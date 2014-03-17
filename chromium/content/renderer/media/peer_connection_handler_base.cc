// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler_base.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

PeerConnectionHandlerBase::PeerConnectionHandlerBase(
    MediaStreamDependencyFactory* dependency_factory)
    : dependency_factory_(dependency_factory),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
}

PeerConnectionHandlerBase::~PeerConnectionHandlerBase() {
}

bool PeerConnectionHandlerBase::AddStream(
    const blink::WebMediaStream& stream,
    const webrtc::MediaConstraintsInterface* constraints) {
  webrtc::MediaStreamInterface* native_stream =
      MediaStreamDependencyFactory::GetNativeMediaStream(stream);
  if (!native_stream)
    return false;
  return native_peer_connection_->AddStream(native_stream, constraints);
}

void PeerConnectionHandlerBase::RemoveStream(
    const blink::WebMediaStream& stream) {
  webrtc::MediaStreamInterface* native_stream =
      MediaStreamDependencyFactory::GetNativeMediaStream(stream);
  if (native_stream)
    native_peer_connection_->RemoveStream(native_stream);
  DCHECK(native_stream);
}

}  // namespace content
