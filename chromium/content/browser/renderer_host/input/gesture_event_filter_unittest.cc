// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/gesture_event_filter.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/port/common/input_event_ack_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

class GestureEventFilterTest : public testing::Test,
                               public GestureEventFilterClient,
                               public TouchpadTapSuppressionControllerClient {
 public:
  GestureEventFilterTest()
      : acked_gesture_event_count_(0),
        sent_gesture_event_count_(0) {}

  virtual ~GestureEventFilterTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
    filter_.reset(new GestureEventFilter(this, this));
  }

  virtual void TearDown() OVERRIDE {
    // Process all pending tasks to avoid leaks.
    RunUntilIdle();
    filter_.reset();
  }

  // GestureEventFilterClient
  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) OVERRIDE {
    ++sent_gesture_event_count_;
    if (sync_ack_result_) {
      scoped_ptr<InputEventAckState> ack_result = sync_ack_result_.Pass();
      SendInputEventACK(event.event.type, *ack_result);
    }
  }

  virtual void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      InputEventAckState ack_result) OVERRIDE {
    ++acked_gesture_event_count_;
    last_acked_event_ = event.event;
    if (sync_followup_event_)
      SimulateGestureEvent(*sync_followup_event_.Pass());
  }

  // TouchpadTapSuppressionControllerClient
  virtual void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& event) OVERRIDE {
  }

 protected:

  // Returns the result of |GestureEventFilter::ShouldForward()|.
  bool SimulateGestureEvent(const WebGestureEvent& gesture) {
    GestureEventWithLatencyInfo gesture_with_latency(gesture,
                                                     ui::LatencyInfo());
    if (filter()->ShouldForward(gesture_with_latency)) {
      SendGestureEventImmediately(gesture_with_latency);
      return true;
    }
    return false;
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureEvent::SourceDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dX, dY, modifiers));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildPinchUpdate(scale,
                                                          anchorX,
                                                          anchorY,
                                                          modifiers));
  }

  void SimulateGestureFlingStartEvent(
      float velocityX,
      float velocityY,
      WebGestureEvent::SourceDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildFling(velocityX,
                                                    velocityY,
                                                    sourceDevice));
  }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack) {
    filter()->ProcessGestureAck(ack, type, ui::LatencyInfo());
  }

  void RunUntilIdle() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  size_t GetAndResetSentGestureEventCount() {
    size_t count = sent_gesture_event_count_;
    sent_gesture_event_count_ = 0;
    return count;
  }

  size_t GetAndResetAckedGestureEventCount() {
    size_t count = acked_gesture_event_count_;
    acked_gesture_event_count_ = 0;
    return count;
  }

  const WebGestureEvent& last_acked_event() const {
    return last_acked_event_;
  }

  void DisableDebounce() {
    filter()->set_debounce_enabled_for_testing(false);
  }

  void set_debounce_interval_time_ms(int ms) {
    filter()->set_debounce_interval_time_ms_for_testing(ms);
  }

  void set_synchronous_ack(InputEventAckState ack_result) {
    sync_ack_result_.reset(new InputEventAckState(ack_result));
  }

  void set_sync_followup_event(WebInputEvent::Type type,
                               WebGestureEvent::SourceDevice sourceDevice) {
    sync_followup_event_.reset(new WebGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice)));
  }

  unsigned GestureEventQueueSize() {
    return filter()->coalesced_gesture_events_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return filter()->coalesced_gesture_events_.at(
        GestureEventQueueSize() - 2).event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return filter()->coalesced_gesture_events_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return filter()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return filter()->coalesced_gesture_events_.at(i).event;
  }

  bool ScrollingInProgress() {
    return filter()->scrolling_in_progress_;
  }

  bool FlingInProgress() {
    return filter()->fling_in_progress_;
  }

  bool WillIgnoreNextACK() {
    return filter()->ignore_next_ack_;
  }

  GestureEventFilter* filter() const {
    return filter_.get();
  }

 private:
  scoped_ptr<GestureEventFilter> filter_;
  size_t acked_gesture_event_count_;
  size_t sent_gesture_event_count_;
  WebGestureEvent last_acked_event_;
  scoped_ptr<InputEventAckState> sync_ack_result_;
  scoped_ptr<WebGestureEvent> sync_followup_event_;
  base::MessageLoopForUI message_loop_;
};

#if GTEST_HAS_PARAM_TEST
// This is for tests that are to be run for all source devices.
class GestureEventFilterWithSourceTest
    : public GestureEventFilterTest,
      public testing::WithParamInterface<WebGestureEvent::SourceDevice> {
};
#endif  // GTEST_HAS_PARAM_TEST

TEST_F(GestureEventFilterTest, CoalescesScrollGestureEvents) {
  // Turn off debounce handling for test isolation.
  DisableDebounce();

  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);

  // Check that coalescing updated the correct values.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(0, merged_event.modifiers);
  EXPECT_EQ(16, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-11, merged_event.data.scrollUpdate.deltaY);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 1);

  // Check that we didn't wrongly coalesce.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.modifiers);

  // Different.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);

  // Check that only the first event was sent.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Ack for queued coalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Ack for queued uncoalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

TEST_F(GestureEventFilterTest, CoalescesScrollAndPinchEvents) {
  // Turn off debounce handling for test isolation.
  DisableDebounce();

  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);

  // Sent.
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -4, 1);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that only the first event was sent.
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -6, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(13, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-7, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // At this point ACKs shouldn't be getting ignored.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Check that the ACK sends both scroll and pinch updates.
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // The next ACK should be getting ignored.
  EXPECT_TRUE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(1, -1, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-1, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 1);

  // Coalescing scrolls should still work.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(0.5, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that the ACK gets ignored.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  // The flag should have been flipped back to false.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 2);

  // Shouldn't coalesce with different modifiers.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(2, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-2, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(2, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);

  // Check that the ACK sends the next scroll pinch pair.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second event.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Check that the queue is empty after ACK and no events get sent.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventFilterTest, CoalescesMultiplePinchEventSequences) {
  // Turn off debounce handling for test isolation.
  DisableDebounce();

  // Simulate a pinch sequence.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchscreen);

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  size_t expected_events_in_queue = 3;
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(++expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Now start another sequence before the previous sequence has been ack'ed.
  SimulateGestureEvent(WebInputEvent::GesturePinchEnd,
                       WebGestureEvent::Touchscreen);
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchscreen);

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  expected_events_in_queue += 5;
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 30, 30, 1);
  EXPECT_EQ(++expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
}

// Tests a single event with an synchronous ack.
TEST_F(GestureEventFilterTest, SimpleSyncAck) {
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an synchronous ack which enqueues an additional event.
TEST_F(GestureEventFilterTest, SyncAckQueuesEvent) {
  scoped_ptr<WebGestureEvent> queued_event;
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  set_sync_followup_event(WebInputEvent::GestureShowPress,
                          WebGestureEvent::Touchscreen);
  // This event enqueues the show press event.
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::GestureShowPress,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an async ack followed by an event with a sync ack.
TEST_F(GestureEventFilterTest, AsyncThenSyncAck) {
  // Turn off debounce handling for test isolation.
  DisableDebounce();

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);

  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::GestureTapDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetAckedGestureEventCount());
}

TEST_F(GestureEventFilterTest, CoalescesScrollAndPinchEventWithSyncAck) {
  // Turn off debounce handling for test isolation.
  DisableDebounce();

  // Simulate a pinch sequence.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Ack the PinchBegin, and schedule a synchronous ack for GestureScrollUpdate.
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Both GestureScrollUpdate and GesturePinchUpdate should have been sent.
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // Ack the final GesturePinchUpdate.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

#if GTEST_HAS_PARAM_TEST
TEST_P(GestureEventFilterWithSourceTest, GestureFlingCancelsFiltered) {
  WebGestureEvent::SourceDevice source_device = GetParam();

  // Turn off debounce handling for test isolation.
  DisableDebounce();
  // GFC without previous GFS is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  // GFC after previous GFS is dispatched and acked.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  // GFC before previous GFS is acked.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Advance state realistically.
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  // GFS is added to the queue if another event is pending
  SimulateGestureScrollUpdateEvent(8, -7, 0);
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event =GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Adding a second GFC is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Adding another GFS will add it to the queue.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(4U, GestureEventQueueSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventQueueSize());

  // Adding another GFC with a GFC already there is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventQueueSize());
}

INSTANTIATE_TEST_CASE_P(AllSources,
                        GestureEventFilterWithSourceTest,
                        testing::Values(WebGestureEvent::Touchscreen,
                                        WebGestureEvent::Touchpad));
#endif  // GTEST_HAS_PARAM_TEST

// Test that a GestureScrollEnd | GestureFlingStart are deferred during the
// debounce interval, that Scrolls are not and that the deferred events are
// sent after that timer fires.
TEST_F(GestureEventFilterTest, DebounceDefersFollowingGestureEvents) {
  set_debounce_interval_time_ms(3);

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureFlingStartEvent(0, 10, WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(3U, GestureEventDebouncingQueueSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(5));
  base::MessageLoop::current()->Run();

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(5U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollEnd,
      WebInputEvent::GestureFlingStart};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(GestureEventFilterTest, DebounceDropsDeferredEvents) {
  set_debounce_interval_time_ms(3);
  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       WebGestureEvent::Touchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

TEST_F(GestureEventFilterTest, DropZeroVelocityFlings) {
  WebGestureEvent gesture_event;
  gesture_event.type = WebInputEvent::GestureFlingStart;
  gesture_event.sourceDevice = WebGestureEvent::Touchpad;
  gesture_event.data.flingStart.velocityX = 0.f;
  gesture_event.data.flingStart.velocityY = 0.f;
  ASSERT_EQ(0U, GetAndResetSentGestureEventCount());
  ASSERT_EQ(0U, GestureEventQueueSize());
  EXPECT_FALSE(SimulateGestureEvent(gesture_event));
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
}

}  // namespace content
