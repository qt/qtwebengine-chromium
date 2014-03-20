// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class InputEvent;

// Interface between the synthetic gesture controller and the RenderWidgetHost.
class CONTENT_EXPORT SyntheticGestureTarget {
 public:
  SyntheticGestureTarget() {}
  virtual ~SyntheticGestureTarget() {}

  // Allows synthetic gestures to insert input events in the highest level of
  // input processing on the target platform (e.g. Java on Android), so that
  // the event traverses the entire input processing stack.
  virtual void DispatchInputEventToPlatform(const InputEvent& event) = 0;

  // Called by SyntheticGestureController when a gesture has finished.
  virtual void OnSyntheticGestureCompleted(
      SyntheticGesture::Result result) = 0;

  // Called by SyntheticGestureController to request a flush at a time
  // appropriate for the platform, e.g. aligned with vsync.
  virtual void SetNeedsFlush() = 0;

  // Returns the default gesture source type for the target.
  virtual SyntheticGestureParams::GestureSourceType
      GetDefaultSyntheticGestureSourceType() const = 0;

  // Check if a particular gesture type is supported by the target.
  virtual bool SupportsSyntheticGestureSourceType(
      SyntheticGestureParams::GestureSourceType gesture_source_type) const = 0;

  // After how much time of inaction does the target assume that a pointer has
  // stopped moving.
  virtual base::TimeDelta PointerAssumedStoppedTime() const = 0;

  // Returns the maximum number of DIPs a touch pointer can move without being
  // considered moving by the platform.
  virtual int GetTouchSlopInDips() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_H_
