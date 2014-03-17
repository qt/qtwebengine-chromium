// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DEVICE_ORIENTATION_DEVICE_ORIENTATION_HARDWARE_BUFFER_H_
#define CONTENT_COMMON_DEVICE_ORIENTATION_DEVICE_ORIENTATION_HARDWARE_BUFFER_H_

#include "content/common/shared_memory_seqlock_buffer.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationData.h"

namespace content {

typedef SharedMemorySeqLockBuffer<blink::WebDeviceOrientationData>
    DeviceOrientationHardwareBuffer;

}  // namespace content

#endif  // CONTENT_COMMON_DEVICE_ORIENTATION_DEVICE_ORIENTATION_HARDWARE_BUFFER_H_
