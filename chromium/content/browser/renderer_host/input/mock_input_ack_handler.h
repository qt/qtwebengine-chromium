// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"

namespace content {

class InputRouter;

class MockInputAckHandler : public InputAckHandler {
 public:
  MockInputAckHandler();
  virtual ~MockInputAckHandler();

  // InputAckHandler
  virtual void OnKeyboardEventAck(const NativeWebKeyboardEvent& event,
                                  InputEventAckState ack_result) OVERRIDE;
  virtual void OnWheelEventAck(const WebKit::WebMouseWheelEvent& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnGestureEventAck(const WebKit::WebGestureEvent& event,
                                 InputEventAckState ack_result) OVERRIDE;
  virtual void OnUnexpectedEventAck(UnexpectedEventAckType type) OVERRIDE;

  void ExpectAckCalled(int times);

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }

  void set_followup_touch_event(scoped_ptr<GestureEventWithLatencyInfo> event) {
    touch_followup_event_ = event.Pass();
  }

  bool unexpected_event_ack_called() const {
    return unexpected_event_ack_called_;
  }
  InputEventAckState ack_state() const { return ack_state_; }

  const NativeWebKeyboardEvent& acked_keyboard_event() const {
    return acked_key_event_;
  }
  const WebKit::WebMouseWheelEvent& acked_wheel_event() const {
    return acked_wheel_event_;
  }
  const TouchEventWithLatencyInfo& acked_touch_event() const {
    return acked_touch_event_;
  }
  const WebKit::WebGestureEvent& acked_gesture_event() const {
    return acked_gesture_event_;
  }

 private:
  void RecordAckCalled(InputEventAckState ack_result);

  InputRouter* input_router_;

  int ack_count_;
  bool unexpected_event_ack_called_;
  InputEventAckState ack_state_;
  NativeWebKeyboardEvent acked_key_event_;
  WebKit::WebMouseWheelEvent acked_wheel_event_;
  TouchEventWithLatencyInfo acked_touch_event_;
  WebKit::WebGestureEvent acked_gesture_event_;

  scoped_ptr<GestureEventWithLatencyInfo> touch_followup_event_;
};

}  // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ACK_HANDLER_H_
