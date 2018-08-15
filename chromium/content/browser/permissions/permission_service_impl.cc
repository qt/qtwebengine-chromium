// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"

using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionObserverPtr;
using blink::mojom::PermissionStatus;

namespace content {

namespace {


bool PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor,
    PermissionType* permission_type) {
  switch (descriptor->name) {
    case PermissionName::GEOLOCATION:
      *permission_type = PermissionType::GEOLOCATION;
      return true;
    case PermissionName::NOTIFICATIONS:
      *permission_type = PermissionType::NOTIFICATIONS;
      return true;
    case PermissionName::PUSH_NOTIFICATIONS:
      *permission_type = PermissionType::PUSH_MESSAGING;
      return true;

    case PermissionName::MIDI: {
      if (descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex) {
        *permission_type = PermissionType::MIDI_SYSEX;
        return true;
      }
      *permission_type = PermissionType::MIDI;
      return true;
    }
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
      *permission_type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
      return true;
    case PermissionName::DURABLE_STORAGE:
      *permission_type = PermissionType::DURABLE_STORAGE;
      return true;
    case PermissionName::AUDIO_CAPTURE:
      *permission_type = PermissionType::AUDIO_CAPTURE;
      return true;
    case PermissionName::VIDEO_CAPTURE:
      *permission_type = PermissionType::VIDEO_CAPTURE;
      return true;
    case PermissionName::BACKGROUND_SYNC:
      *permission_type = PermissionType::BACKGROUND_SYNC;
      return true;
  }

  NOTREACHED();
  return false;
}

// This function allows the usage of the the multiple request map
// with single requests.
void PermissionRequestResponseCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  callback.Run(vector[0]);
}

} // anonymous namespace

PermissionServiceImpl::PendingRequest::PendingRequest(
    const RequestPermissionsCallback& callback,
    int request_count)
    : callback(callback),
      request_count(request_count) {
}

PermissionServiceImpl::PendingRequest::~PendingRequest() {
  if (callback.is_null())
    return;

  std::vector<PermissionStatus> result(request_count, PermissionStatus::DENIED);
  callback.Run(result);
}

PermissionServiceImpl::PermissionServiceImpl(
    PermissionServiceContext* context,
    mojo::InterfaceRequest<blink::mojom::PermissionService> request)
    : context_(context),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  binding_.set_connection_error_handler(
      base::Bind(&PermissionServiceImpl::OnConnectionError,
                 base::Unretained(this)));
}

PermissionServiceImpl::~PermissionServiceImpl() {
  DCHECK(pending_requests_.IsEmpty());
}

void PermissionServiceImpl::OnConnectionError() {
  CancelPendingOperations();
  context_->ServiceHadConnectionError(this);
  // After that call, |this| will be deleted.
}

void PermissionServiceImpl::RequestPermission(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    bool user_gesture,
    const PermissionStatusCallback& callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned.
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!context_->render_frame_host() ||
      !browser_context->GetPermissionManager()) {
    callback.Run(GetPermissionStatus(permission, origin));
    return;
  }

  PermissionType type;
  if (!PermissionDescriptorToPermissionType(permission, &type)) {
    ReceivedBadMessage();
    return;
  }

  int pending_request_id = pending_requests_.Add(new PendingRequest(
      base::Bind(&PermissionRequestResponseCallbackWrapper, callback), 1));
  int id = browser_context->GetPermissionManager()->RequestPermission(
      type,
      context_->render_frame_host(), origin.GetURL(), user_gesture,
      base::Bind(&PermissionServiceImpl::OnRequestPermissionResponse,
                 weak_factory_.GetWeakPtr(), pending_request_id));

  // Check if the request still exists. It might have been removed by the
  // callback if it was run synchronously.
  PendingRequest* pending_request = pending_requests_.Lookup(
      pending_request_id);
  if (!pending_request)
      return;
  pending_request->id = id;
}

void PermissionServiceImpl::OnRequestPermissionResponse(
    int pending_request_id,
    PermissionStatus status) {
  OnRequestPermissionsResponse(pending_request_id,
                               std::vector<PermissionStatus>(1, status));
}

void PermissionServiceImpl::RequestPermissions(
    std::vector<PermissionDescriptorPtr> permissions,
    const url::Origin& origin,
    bool user_gesture,
    const RequestPermissionsCallback& callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned for each permission.
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!context_->render_frame_host() ||
      !browser_context->GetPermissionManager()) {
    std::vector<PermissionStatus> result(permissions.size());
    for (size_t i = 0; i < permissions.size(); ++i)
      result[i] = GetPermissionStatus(permissions[i], origin);
    callback.Run(result);
    return;
  }

  std::vector<PermissionType> types(permissions.size());
  std::set<PermissionType> duplicates_check;
  for (size_t i = 0; i < types.size(); ++i) {
    if (!PermissionDescriptorToPermissionType(permissions[i], &types[i])) {
      ReceivedBadMessage();
      return;
    }
    // Each permission should appear at most once in the message.
    bool inserted = duplicates_check.insert(types[i]).second;
    if (!inserted) {
      ReceivedBadMessage();
      return;
    }
  }

  int pending_request_id = pending_requests_.Add(
      new PendingRequest(callback, permissions.size()));
  int id = browser_context->GetPermissionManager()->RequestPermissions(
      types, context_->render_frame_host(), origin.GetURL(), user_gesture,
      base::Bind(&PermissionServiceImpl::OnRequestPermissionsResponse,
                 weak_factory_.GetWeakPtr(), pending_request_id));

  // Check if the request still exists. It may have been removed by the
  // the response callback.
  PendingRequest* pending_request = pending_requests_.Lookup(
      pending_request_id);
  if (!pending_request)
      return;
  pending_request->id = id;
}

void PermissionServiceImpl::OnRequestPermissionsResponse(
    int pending_request_id,
    const std::vector<PermissionStatus>& result) {
  PendingRequest* request = pending_requests_.Lookup(pending_request_id);
  RequestPermissionsCallback callback(request->callback);
  request->callback.Reset();
  pending_requests_.Remove(pending_request_id);
  callback.Run(result);
}

void PermissionServiceImpl::CancelPendingOperations() {
  DCHECK(context_->GetBrowserContext());

  PermissionManager* permission_manager =
      context_->GetBrowserContext()->GetPermissionManager();
  if (!permission_manager)
    return;

  // Cancel pending requests.
  for (RequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    permission_manager->CancelPermissionRequest(
        it.GetCurrentValue()->id);
  }
  pending_requests_.Clear();
}

void PermissionServiceImpl::HasPermission(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    const PermissionStatusCallback& callback) {
  callback.Run(GetPermissionStatus(permission, origin));
}

void PermissionServiceImpl::RevokePermission(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    const PermissionStatusCallback& callback) {
  PermissionType permission_type;
  if (!PermissionDescriptorToPermissionType(permission, &permission_type)) {
    ReceivedBadMessage();
    return;
  }
  PermissionStatus status = GetPermissionStatusFromType(permission_type, origin);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PermissionStatus::GRANTED) {
    callback.Run(status);
    return;
  }

  ResetPermissionStatus(permission_type, origin);

  callback.Run(GetPermissionStatusFromType(permission_type, origin));
}

void PermissionServiceImpl::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    PermissionStatus last_known_status,
    PermissionObserverPtr observer) {
  PermissionStatus current_status = GetPermissionStatus(permission, origin);
  if (current_status != last_known_status) {
    observer->OnPermissionStatusChange(current_status);
    last_known_status = current_status;
  }

  PermissionType type;
  if (!PermissionDescriptorToPermissionType(permission, &type)) {
    ReceivedBadMessage();
    return;
  }

  context_->CreateSubscription(type, origin, std::move(observer));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatus(
    const PermissionDescriptorPtr& permission,
    const url::Origin& origin) {
  PermissionType type;
  if (!PermissionDescriptorToPermissionType(permission, &type)) {
    ReceivedBadMessage();
    return PermissionStatus::DENIED;
  }
  return GetPermissionStatusFromType(type, origin);
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    PermissionType type,
    const url::Origin& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return PermissionStatus::DENIED;

  GURL requesting_origin(origin.Serialize());
  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  return browser_context->GetPermissionManager()->GetPermissionStatus(
      type, requesting_origin,
      embedding_origin.is_empty() ? requesting_origin : embedding_origin);
}

void PermissionServiceImpl::ResetPermissionStatus(PermissionType type,
                                                  const url::Origin& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return;

  GURL requesting_origin(origin.Serialize());
  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  browser_context->GetPermissionManager()->ResetPermission(
      type, requesting_origin,
      embedding_origin.is_empty() ? requesting_origin : embedding_origin);
}

void PermissionServiceImpl::ReceivedBadMessage() {
  if (context_->render_frame_host()) {
    bad_message::ReceivedBadMessage(
        context_->render_frame_host()->GetProcess(),
        bad_message::PERMISSION_SERVICE_BAD_PERMISSION_DESCRIPTOR);
  } else {
    bad_message::ReceivedBadMessage(
        context_->render_process_host(),
        bad_message::PERMISSION_SERVICE_BAD_PERMISSION_DESCRIPTOR);
  }
}

}  // namespace content
