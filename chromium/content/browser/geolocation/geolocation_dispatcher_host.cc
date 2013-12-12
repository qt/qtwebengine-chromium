// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_dispatcher_host.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "content/public/common/geoposition.h"
#include "content/common/geolocation_messages.h"

namespace content {
namespace {

void NotifyGeolocationProviderPermissionGranted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationProviderImpl::GetInstance()->UserDidOptIntoLocationServices();
}

void SendGeolocationPermissionResponse(int render_process_id,
                                       int render_view_id,
                                       int bridge_id,
                                       bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return;
  render_view_host->Send(
      new GeolocationMsg_PermissionSet(render_view_id, bridge_id, allowed));

  if (allowed) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&NotifyGeolocationProviderPermissionGranted));
  }
}

class GeolocationDispatcherHostImpl : public GeolocationDispatcherHost {
 public:
  GeolocationDispatcherHostImpl(
      int render_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  // GeolocationDispatcherHost
  virtual bool OnMessageReceived(const IPC::Message& msg,
                                 bool* msg_was_ok) OVERRIDE;

 private:
  virtual ~GeolocationDispatcherHostImpl();

  void OnRequestPermission(int render_view_id,
                           int bridge_id,
                           const GURL& requesting_frame);
  void OnCancelPermissionRequest(int render_view_id,
                                 int bridge_id,
                                 const GURL& requesting_frame);
  void OnStartUpdating(int render_view_id,
                       const GURL& requesting_frame,
      bool enable_high_accuracy);
  void OnStopUpdating(int render_view_id);

  // Updates the |geolocation_provider_| with the currently required update
  // options, based on |renderer_high_accuracy_|.
  void RefreshHighAccuracy();

  void OnLocationUpdate(const Geoposition& position);

  int render_process_id_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;

  // Iterated when sending location updates to renderer processes. The fan out
  // to individual bridge IDs happens renderer side, in order to minimize
  // context switches.
  // Only used on the IO thread.
  std::set<int> geolocation_renderer_ids_;
  // Maps renderer_id to whether high accuracy is requested for this particular
  // bridge.
  std::map<int, bool> renderer_high_accuracy_;
  // Only set whilst we are registered with the geolocation provider.
  GeolocationProviderImpl* geolocation_provider_;

  GeolocationProviderImpl::LocationUpdateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHostImpl);
};

GeolocationDispatcherHostImpl::GeolocationDispatcherHostImpl(
    int render_process_id,
    GeolocationPermissionContext* geolocation_permission_context)
    : render_process_id_(render_process_id),
      geolocation_permission_context_(geolocation_permission_context),
      geolocation_provider_(NULL) {
  callback_ = base::Bind(
      &GeolocationDispatcherHostImpl::OnLocationUpdate, base::Unretained(this));
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here, defer to OnRegisterBridge which is triggered whenever
  // a javascript geolocation object is actually initialized.
}

GeolocationDispatcherHostImpl::~GeolocationDispatcherHostImpl() {
  if (geolocation_provider_)
    geolocation_provider_->RemoveLocationUpdateCallback(callback_);
}

bool GeolocationDispatcherHostImpl::OnMessageReceived(
    const IPC::Message& msg, bool* msg_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  *msg_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GeolocationDispatcherHostImpl, msg, *msg_was_ok)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_CancelPermissionRequest,
                        OnCancelPermissionRequest)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_RequestPermission,
                        OnRequestPermission)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_StartUpdating, OnStartUpdating)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_StopUpdating, OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeolocationDispatcherHostImpl::OnLocationUpdate(
    const Geoposition& geoposition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (std::set<int>::iterator it = geolocation_renderer_ids_.begin();
       it != geolocation_renderer_ids_.end(); ++it) {
    Send(new GeolocationMsg_PositionUpdated(*it, geoposition));
  }
}

void GeolocationDispatcherHostImpl::OnRequestPermission(
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id << ":" << bridge_id;
  if (geolocation_permission_context_.get()) {
    geolocation_permission_context_->RequestGeolocationPermission(
        render_process_id_,
        render_view_id,
        bridge_id,
        requesting_frame,
        base::Bind(&SendGeolocationPermissionResponse,
                   render_process_id_,
                   render_view_id,
                   bridge_id));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SendGeolocationPermissionResponse, render_process_id_,
                   render_view_id, bridge_id, true));
  }
}

void GeolocationDispatcherHostImpl::OnCancelPermissionRequest(
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id << ":" << bridge_id;
  if (geolocation_permission_context_.get()) {
    geolocation_permission_context_->CancelGeolocationPermissionRequest(
        render_process_id_, render_view_id, bridge_id, requesting_frame);
  }
}

void GeolocationDispatcherHostImpl::OnStartUpdating(
    int render_view_id,
    const GURL& requesting_frame,
    bool enable_high_accuracy) {
  // StartUpdating() can be invoked as a result of high-accuracy mode
  // being enabled / disabled. No need to record the dispatcher again.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id;
  if (!geolocation_renderer_ids_.count(render_view_id))
    geolocation_renderer_ids_.insert(render_view_id);

  renderer_high_accuracy_[render_view_id] = enable_high_accuracy;
  RefreshHighAccuracy();
}

void GeolocationDispatcherHostImpl::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id;
  if (renderer_high_accuracy_.erase(render_view_id))
    RefreshHighAccuracy();

  DCHECK_EQ(1U, geolocation_renderer_ids_.count(render_view_id));
  geolocation_renderer_ids_.erase(render_view_id);
}

void GeolocationDispatcherHostImpl::RefreshHighAccuracy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (renderer_high_accuracy_.empty()) {
    if (geolocation_provider_) {
      geolocation_provider_->RemoveLocationUpdateCallback(callback_);
      geolocation_provider_ = NULL;
    }
  } else {
    if (!geolocation_provider_)
      geolocation_provider_ = GeolocationProviderImpl::GetInstance();
    // Re-add to re-establish our options, in case they changed.
    bool use_high_accuracy = false;
    std::map<int, bool>::iterator i = renderer_high_accuracy_.begin();
    for (; i != renderer_high_accuracy_.end(); ++i) {
      if (i->second) {
        use_high_accuracy = true;
        break;
      }
    }
    geolocation_provider_->AddLocationUpdateCallback(
        callback_, use_high_accuracy);
  }
}
}  // namespace


// GeolocationDispatcherHost --------------------------------------------------

// static
GeolocationDispatcherHost* GeolocationDispatcherHost::New(
    int render_process_id,
    GeolocationPermissionContext* geolocation_permission_context) {
  return new GeolocationDispatcherHostImpl(
      render_process_id,
      geolocation_permission_context);
}

GeolocationDispatcherHost::GeolocationDispatcherHost() {
}

GeolocationDispatcherHost::~GeolocationDispatcherHost() {
}

}  // namespace content
