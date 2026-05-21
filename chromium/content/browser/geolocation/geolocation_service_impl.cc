// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

#if BUILDFLAG(IS_IOS)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif

namespace content {

// `GeolocationProxy` acts as a Mojo intermediary between the renderer client
// and the backing `GeolocationImpl`. It forwards all calls and monitors
// the connection lifetime of both ends.
//
// `GeolocationServiceImpl` uses this to track active sessions and manage
// the browser-side activity count (UI location indicator). Disconnections on
// either end trigger `OnProxyDisconnected` to update state.
class GeolocationServiceImpl::GeolocationProxy
    : public device::mojom::Geolocation {
 public:
  GeolocationProxy(
      GeolocationServiceImpl* service_impl,
      mojo::PendingRemote<device::mojom::Geolocation> geolocation_impl_remote,
      mojo::PendingReceiver<device::mojom::Geolocation> renderer_receiver)
      : service_impl_(service_impl),
        geolocation_impl_remote_(std::move(geolocation_impl_remote)),
        renderer_receiver_(this, std::move(renderer_receiver)) {
    renderer_receiver_.set_disconnect_handler(base::BindOnce(
        &GeolocationProxy::OnDisconnect, base::Unretained(this)));
    geolocation_impl_remote_.set_disconnect_handler(base::BindOnce(
        &GeolocationProxy::OnDisconnect, base::Unretained(this)));
  }

  // device::mojom::Geolocation:
  void SetHighAccuracyHint(bool high_accuracy) override {
    geolocation_impl_remote_->SetHighAccuracyHint(high_accuracy);
  }
  void QueryCachedPosition(QueryCachedPositionCallback callback) override {
    geolocation_impl_remote_->QueryCachedPosition(std::move(callback));
  }
  void QueryNextPosition(QueryNextPositionCallback callback) override {
    geolocation_impl_remote_->QueryNextPosition(std::move(callback));
  }

 private:
  void OnDisconnect() { service_impl_->OnProxyDisconnected(this); }

  // `GeolocationServiceImpl` owns `this` (via `active_proxies_`), so
  // `service_impl_` is guaranteed to outlive `this`.
  const raw_ptr<GeolocationServiceImpl> service_impl_;
  mojo::Remote<device::mojom::Geolocation> geolocation_impl_remote_;
  mojo::Receiver<device::mojom::Geolocation> renderer_receiver_;
};

GeolocationServiceImplContext::GeolocationServiceImplContext() = default;

GeolocationServiceImplContext::~GeolocationServiceImplContext() = default;

void GeolocationServiceImplContext::RequestPermission(
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    PermissionCallback callback) {
  if (has_pending_permission_request_) {
    mojo::ReportBadMessage(
        "GeolocationService client may only create one Geolocation at a "
        "time.");
    return;
  }

  has_pending_permission_request_ = true;

  render_frame_host->GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          render_frame_host,
          PermissionRequestDescription(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::GEOLOCATION),
              user_gesture),
          base::BindOnce(&GeolocationServiceImplContext::HandlePermissionStatus,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void GeolocationServiceImplContext::HandlePermissionStatus(
    PermissionCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  has_pending_permission_request_ = false;
  std::move(callback).Run(permission_status);
}

GeolocationServiceImpl::GeolocationServiceImpl(
    device::mojom::GeolocationContext* geolocation_context,
    RenderFrameHost* render_frame_host)
    : geolocation_context_(geolocation_context),
      render_frame_host_(render_frame_host) {
  DCHECK(geolocation_context);
  DCHECK(render_frame_host);
}

GeolocationServiceImpl::~GeolocationServiceImpl() {
  DecrementActivityCount();
}

void GeolocationServiceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::GeolocationService> receiver) {
  receiver_set_.Add(this, std::move(receiver),
                    std::make_unique<GeolocationServiceImplContext>());
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &GeolocationServiceImpl::OnDisconnected, base::Unretained(this)));
#if BUILDFLAG(IS_IOS)
  device::GeolocationSystemPermissionManager*
      geolocation_system_permission_manager =
          device::GeolocationSystemPermissionManager::GetInstance();
  if (geolocation_system_permission_manager) {
    geolocation_system_permission_manager->RequestSystemPermission();
  }
#endif
}

void GeolocationServiceImpl::CreateGeolocation(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    bool user_gesture,
    CreateGeolocationCallback callback) {
  if (!render_frame_host_->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kGeolocation)) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  // If the geolocation service is destroyed before the callback is run, ensure
  // it is called with DENIED status.
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::mojom::PermissionStatus::DENIED);

  receiver_set_.current_context()->RequestPermission(
      render_frame_host_, user_gesture,
      // The owning RenderFrameHost might be destroyed before the permission
      // request finishes. To avoid calling a callback on a destroyed object,
      // use a WeakPtr and skip the callback if the object is invalid.
      base::BindOnce(
          &GeolocationServiceImpl::CreateGeolocationWithPermissionStatus,
          weak_factory_.GetWeakPtr(), std::move(receiver),
          std::move(scoped_callback)));
}

void GeolocationServiceImpl::CreateGeolocationWithPermissionStatus(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    CreateGeolocationCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  std::move(callback).Run(permission_status);
  if (permission_status != blink::mojom::PermissionStatus::GRANTED)
    return;
  requesting_origin_ =
      render_frame_host_->GetMainFrame()->GetLastCommittedOrigin();
  auto requesting_url =
      render_frame_host_->GetMainFrame()->GetLastCommittedURL();

  if (base::FeatureList::IsEnabled(features::kGeolocationProxy)) {
    mojo::PendingRemote<device::mojom::Geolocation> geolocation_impl_remote;
    geolocation_context_->BindGeolocation(
        geolocation_impl_remote.InitWithNewPipeAndPassReceiver(),
        requesting_url,
        device::mojom::GeolocationClientId::kGeolocationServiceImpl);

    if (active_proxies_.empty()) {
      IncrementActivityCount();
    }
    active_proxies_.push_back(std::make_unique<GeolocationProxy>(
        this, std::move(geolocation_impl_remote), std::move(receiver)));
  } else {
    // NOTE: Legacy behavior may leak the active frame count if multiple
    // connections are created. Kept as-is for compatibility.
    IncrementActivityCount();

    geolocation_context_->BindGeolocation(
        std::move(receiver), requesting_url,
        device::mojom::GeolocationClientId::kGeolocationServiceImpl);
  }

  subscription_id_ =
      PermissionControllerImpl::FromBrowserContext(
          render_frame_host_->GetBrowserContext())
          ->SubscribeToPermissionStatusChange(
              blink::PermissionType::GEOLOCATION,
              /*render_process_host=*/nullptr, render_frame_host_,
              requesting_url,
              /*should_include_device_status=*/false,
              base::BindRepeating(
                  &GeolocationServiceImpl::HandlePermissionStatusChange,
                  weak_factory_.GetWeakPtr()));
}

void GeolocationServiceImpl::HandlePermissionStatusChange(
    blink::mojom::PermissionStatus permission_status) {
  if (permission_status != blink::mojom::PermissionStatus::GRANTED &&
      subscription_id_.value()) {
    PermissionControllerImpl::FromBrowserContext(
        render_frame_host_->GetBrowserContext())
        ->UnsubscribeFromPermissionStatusChange(subscription_id_);
    geolocation_context_->OnPermissionRevoked(requesting_origin_);

    // When kGeolocationProxy is enabled, DecrementActivityCount is managed
    // by the GeolocationProxy lifecycle.
    if (!base::FeatureList::IsEnabled(features::kGeolocationProxy)) {
      DecrementActivityCount();
    }
  }
}

void GeolocationServiceImpl::OnDisconnected() {
  // When kGeolocationProxy is enabled, we do not need to perform any cleanup
  // here because active connection lifetimes are managed by the proxies.
  if (!base::FeatureList::IsEnabled(features::kGeolocationProxy) &&
      receiver_set_.empty()) {
    DecrementActivityCount();
  }
}

void GeolocationServiceImpl::OnProxyDisconnected(GeolocationProxy* proxy) {
  std::erase_if(active_proxies_, [proxy](const auto& active_proxy) {
    return active_proxy.get() == proxy;
  });
  if (active_proxies_.empty()) {
    DecrementActivityCount();
  }
}

void GeolocationServiceImpl::IncrementActivityCount() {
  is_sending_updates_ = true;
  auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host_);
  static_cast<WebContentsImpl*>(web_contents)
      ->IncrementGeolocationActiveFrameCount();
}

void GeolocationServiceImpl::DecrementActivityCount() {
  if (is_sending_updates_) {
    is_sending_updates_ = false;
    auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host_);
    static_cast<WebContentsImpl*>(web_contents)
        ->DecrementGeolocationActiveFrameCount();
  }
}

}  // namespace content
