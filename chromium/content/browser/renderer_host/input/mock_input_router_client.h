// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/input/input_router_client.h"

namespace content {

class InputEvent;
class InputRouter;

class MockInputRouterClient : public InputRouterClient {
 public:
  MockInputRouterClient();
  virtual ~MockInputRouterClient();

  // InputRouterClient
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void IncrementInFlightEventCount() OVERRIDE;
  virtual void DecrementInFlightEventCount() OVERRIDE;
  virtual void OnHasTouchEventHandlers(bool has_handlers) OVERRIDE;
  virtual OverscrollController* GetOverscrollController() const OVERRIDE;
  virtual void DidFlush() OVERRIDE;
  virtual void SetNeedsFlush() OVERRIDE;

  bool GetAndResetFilterEventCalled();

  void set_input_router(InputRouter* input_router) {
    input_router_ = input_router;
  }

  bool has_touch_handler() const { return has_touch_handler_; }
  void set_filter_state(InputEventAckState filter_state) {
    filter_state_ = filter_state;
  }
  int in_flight_event_count() const {
    return in_flight_event_count_;
  }
  void set_allow_send_event(bool allow) {
    filter_state_ = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }

  bool did_flush_called() const { return did_flush_called_; }
  bool needs_flush_called() const { return set_needs_flush_called_; }

 private:
  InputRouter* input_router_;
  int in_flight_event_count_;
  bool has_touch_handler_;

  InputEventAckState filter_state_;

  bool filter_input_event_called_;
  scoped_ptr<InputEvent> last_filter_event_;

  bool did_flush_called_;
  bool set_needs_flush_called_;
};

}  // namespace content

#endif // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOCK_INPUT_ROUTER_CLIENT_H_
