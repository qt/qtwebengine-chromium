// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_PINCH_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_PINCH_GESTURE_PARAMS_H_

#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/gfx/point.h"

namespace content {

struct CONTENT_EXPORT SyntheticPinchGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticPinchGestureParams();
  SyntheticPinchGestureParams(
      const SyntheticPinchGestureParams& other);
  virtual ~SyntheticPinchGestureParams();

  virtual GestureType GetGestureType() const OVERRIDE;

  bool zoom_in;
  int total_num_pixels_covered;
  gfx::Point anchor;
  int relative_pointer_speed_in_pixels_s;

  static const SyntheticPinchGestureParams* Cast(
      const SyntheticGestureParams* gesture_params);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_PINCH_GESTURE_PARAMS_H_
