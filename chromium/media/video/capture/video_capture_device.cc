// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device.h"
#include "base/strings/string_util.h"

namespace media {

const std::string VideoCaptureDevice::Name::GetNameAndModel() const {
  const std::string model_id = GetModel();
  if (model_id.empty())
    return device_name_;
  const std::string suffix = " (" + model_id + ")";
  if (EndsWith(device_name_, suffix, true))  // |true| means case-sensitive.
    return device_name_;
  return device_name_ + suffix;
}

VideoCaptureDevice::Name*
VideoCaptureDevice::Names::FindById(const std::string& id) {
  for (iterator it = begin(); it != end(); ++it) {
    if (it->id() == id)
      return &(*it);
  }
  return NULL;
}

VideoCaptureDevice::~VideoCaptureDevice() {}

VideoCaptureDevice1::VideoCaptureDevice1() {}

VideoCaptureDevice1::~VideoCaptureDevice1() {}

void VideoCaptureDevice1::AllocateAndStart(
    const VideoCaptureCapability& capture_format,
    scoped_ptr<EventHandler> client) {
  client_ = client.Pass();
  Allocate(capture_format, client_.get());
  Start();
}

void VideoCaptureDevice1::StopAndDeAllocate() {
  Stop();
  DeAllocate();
  client_.reset();
};


}  // namespace media
