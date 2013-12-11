// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_INERTIAL_SENSOR_CONSTS_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_INERTIAL_SENSOR_CONSTS_H_

namespace content {

// Constants related to the Device Motion/Device Orientation APIs.

enum ConsumerType {
  CONSUMER_TYPE_MOTION = 1 << 0,
  CONSUMER_TYPE_ORIENTATION = 1 << 1,
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_INERTIAL_SENSOR_CONSTS_H_
