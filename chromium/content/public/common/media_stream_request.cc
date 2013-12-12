// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_stream_request.h"

#include "base/logging.h"

namespace content {

bool IsAudioMediaType(MediaStreamType type) {
  return (type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
          type == content::MEDIA_TAB_AUDIO_CAPTURE ||
          type == content::MEDIA_SYSTEM_AUDIO_CAPTURE);
}

bool IsVideoMediaType(MediaStreamType type) {
  return (type == content::MEDIA_DEVICE_VIDEO_CAPTURE ||
          type == content::MEDIA_TAB_VIDEO_CAPTURE ||
          type == content::MEDIA_DESKTOP_VIDEO_CAPTURE);
}

MediaStreamDevice::MediaStreamDevice()
    : type(MEDIA_NO_SERVICE),
      video_facing(MEDIA_VIDEO_FACING_NONE) {
}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamType type,
    const std::string& id,
    const std::string& name)
    : type(type),
      id(id),
      video_facing(MEDIA_VIDEO_FACING_NONE),
      name(name) {
#if defined(OS_ANDROID)
  if (name.find("front") != std::string::npos) {
    video_facing = MEDIA_VIDEO_FACING_USER;
  } else if (name.find("back") != std::string::npos) {
    video_facing = MEDIA_VIDEO_FACING_ENVIRONMENT;
  }
#endif
}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamType type,
    const std::string& id,
    const std::string& name,
    int sample_rate,
    int channel_layout,
    int frames_per_buffer)
    : type(type),
      id(id),
      video_facing(MEDIA_VIDEO_FACING_NONE),
      name(name),
      input(sample_rate, channel_layout, frames_per_buffer) {
}

MediaStreamDevice::~MediaStreamDevice() {}

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_view_id,
    int page_request_id,
    const std::string& tab_capture_device_id,
    const GURL& security_origin,
    MediaStreamRequestType request_type,
    const std::string& requested_audio_device_id,
    const std::string& requested_video_device_id,
    MediaStreamType audio_type,
    MediaStreamType video_type)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      page_request_id(page_request_id),
      tab_capture_device_id(tab_capture_device_id),
      security_origin(security_origin),
      request_type(request_type),
      requested_audio_device_id(requested_audio_device_id),
      requested_video_device_id(requested_video_device_id),
      audio_type(audio_type),
      video_type(video_type) {
}

MediaStreamRequest::~MediaStreamRequest() {}

}  // namespace content
