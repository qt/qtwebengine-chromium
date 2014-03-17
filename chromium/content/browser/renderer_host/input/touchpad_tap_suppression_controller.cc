// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"

#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"

// The default implementation of the TouchpadTapSuppressionController does not
// suppress taps. Touchpad tap suppression is needed only on CrOS.

namespace content {

TouchpadTapSuppressionController::TouchpadTapSuppressionController(
    TouchpadTapSuppressionControllerClient* /* client */)
    : client_(NULL) {}

TouchpadTapSuppressionController::~TouchpadTapSuppressionController() {}

void TouchpadTapSuppressionController::GestureFlingCancel() {}

void TouchpadTapSuppressionController::GestureFlingCancelAck(
    bool /*processed*/) {
}

bool TouchpadTapSuppressionController::ShouldDeferMouseDown(
    const MouseEventWithLatencyInfo& /*event*/) {
  return false;
}

bool TouchpadTapSuppressionController::ShouldSuppressMouseUp() { return false; }

int TouchpadTapSuppressionController::MaxCancelToDownTimeInMs() {
  return 0;
}

int TouchpadTapSuppressionController::MaxTapGapTimeInMs() {
  return 0;
}

void TouchpadTapSuppressionController::DropStashedTapDown() {
}

void TouchpadTapSuppressionController::ForwardStashedTapDown() {
}

}  // namespace content
