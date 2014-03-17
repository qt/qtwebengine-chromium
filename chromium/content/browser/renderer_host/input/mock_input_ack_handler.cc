// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_ack_handler.h"

#include "content/browser/renderer_host/input/input_router.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

MockInputAckHandler::MockInputAckHandler()
  : input_router_(NULL),
    ack_count_(0),
    unexpected_event_ack_called_(false),
    ack_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {}

MockInputAckHandler::~MockInputAckHandler() {}

void MockInputAckHandler::OnKeyboardEventAck(
    const NativeWebKeyboardEvent& event,
    InputEventAckState ack_result)  {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_key_event_ = event;
  RecordAckCalled(ack_result);
}

void MockInputAckHandler::OnWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_wheel_event_ = event.event;
  RecordAckCalled(ack_result);
}

void MockInputAckHandler::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_touch_event_ = event;
  RecordAckCalled(ack_result);
  if (touch_followup_event_)
    input_router_->SendTouchEvent(*touch_followup_event_);
  if (gesture_followup_event_)
    input_router_->SendGestureEvent(*gesture_followup_event_);
}

void MockInputAckHandler::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  VLOG(1) << __FUNCTION__ << " called!";
  acked_gesture_event_ = event.event;
  RecordAckCalled(ack_result);
}

void MockInputAckHandler::OnUnexpectedEventAck(UnexpectedEventAckType type)  {
  VLOG(1) << __FUNCTION__ << " called!";
  unexpected_event_ack_called_ = true;
}

size_t MockInputAckHandler::GetAndResetAckCount() {
  size_t ack_count = ack_count_;
  ack_count_ = 0;
  return ack_count;
}

void MockInputAckHandler::RecordAckCalled(InputEventAckState ack_result) {
  ++ack_count_;
  ack_state_ = ack_result;
}

}  // namespace content
