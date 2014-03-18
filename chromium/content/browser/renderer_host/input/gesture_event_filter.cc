// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_filter.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "content/public/common/content_switches.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {
namespace {

// Default debouncing interval duration: if a scroll is in progress, non-scroll
// events during this interval are deferred to either its end or discarded on
// receipt of another GestureScrollUpdate.
static const int kDebouncingIntervalTimeMs = 30;

}  // namespace

GestureEventFilter::GestureEventFilter(
    GestureEventFilterClient* client,
    TouchpadTapSuppressionControllerClient* touchpad_client)
     : client_(client),
       fling_in_progress_(false),
       scrolling_in_progress_(false),
       ignore_next_ack_(false),
       combined_scroll_pinch_(gfx::Transform()),
       touchpad_tap_suppression_controller_(
           new TouchpadTapSuppressionController(touchpad_client)),
       touchscreen_tap_suppression_controller_(
           new TouchscreenTapSuppressionController(this)),
       debounce_interval_time_ms_(kDebouncingIntervalTimeMs),
       debounce_enabled_(true) {
  DCHECK(client);
  DCHECK(touchpad_tap_suppression_controller_);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGestureDebounce)) {
    debounce_enabled_ = false;
  }
}

GestureEventFilter::~GestureEventFilter() { }

bool GestureEventFilter::ShouldDiscardFlingCancelEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  if (coalesced_gesture_events_.empty() && fling_in_progress_)
    return false;
  GestureEventQueue::const_reverse_iterator it =
      coalesced_gesture_events_.rbegin();
  while (it != coalesced_gesture_events_.rend()) {
    if (it->event.type == WebInputEvent::GestureFlingStart)
      return false;
    if (it->event.type == WebInputEvent::GestureFlingCancel)
      return true;
    it++;
  }
  return true;
}

bool GestureEventFilter::ShouldForwardForBounceReduction(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!debounce_enabled_)
    return true;
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureScrollUpdate:
      if (!scrolling_in_progress_) {
        debounce_deferring_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(debounce_interval_time_ms_),
            this,
            &GestureEventFilter::SendScrollEndingEventsNow);
      } else {
        // Extend the bounce interval.
        debounce_deferring_timer_.Reset();
      }
      scrolling_in_progress_ = true;
      debouncing_deferral_queue_.clear();
      return true;
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
      // TODO(rjkroege): Debounce pinch (http://crbug.com/147647)
      return true;
    default:
      if (scrolling_in_progress_) {
        debouncing_deferral_queue_.push_back(gesture_event);
        return false;
      }
      return true;
  }

  NOTREACHED();
  return false;
}

// NOTE: The filters are applied successively. This simplifies the change.
bool GestureEventFilter::ShouldForward(
    const GestureEventWithLatencyInfo& gesture_event) {
  return ShouldForwardForZeroVelocityFlingStart(gesture_event) &&
      ShouldForwardForBounceReduction(gesture_event) &&
      ShouldForwardForGFCFiltering(gesture_event) &&
      ShouldForwardForTapSuppression(gesture_event) &&
      ShouldForwardForCoalescing(gesture_event);
}

bool GestureEventFilter::ShouldForwardForZeroVelocityFlingStart(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return gesture_event.event.type != WebInputEvent::GestureFlingStart ||
      gesture_event.event.sourceDevice != WebGestureEvent::Touchpad ||
      gesture_event.event.data.flingStart.velocityX != 0 ||
      gesture_event.event.data.flingStart.velocityY != 0;
}

bool GestureEventFilter::ShouldForwardForGFCFiltering(
    const GestureEventWithLatencyInfo& gesture_event) const {
  return gesture_event.event.type != WebInputEvent::GestureFlingCancel ||
      !ShouldDiscardFlingCancelEvent(gesture_event);
}

bool GestureEventFilter::ShouldForwardForTapSuppression(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureFlingCancel:
      if (gesture_event.event.sourceDevice == WebGestureEvent::Touchscreen)
        touchscreen_tap_suppression_controller_->GestureFlingCancel();
      else
        touchpad_tap_suppression_controller_->GestureFlingCancel();
      return true;
    case WebInputEvent::GestureTapDown:
      return !touchscreen_tap_suppression_controller_->
          ShouldDeferGestureTapDown(gesture_event);
    case WebInputEvent::GestureShowPress:
      return !touchscreen_tap_suppression_controller_->
          ShouldDeferGestureShowPress(gesture_event);
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureDoubleTap:
      return !touchscreen_tap_suppression_controller_->
          ShouldSuppressGestureTapEnd();
    default:
      return true;
  }
  NOTREACHED();
  return false;
}

bool GestureEventFilter::ShouldForwardForCoalescing(
    const GestureEventWithLatencyInfo& gesture_event) {
  switch (gesture_event.event.type) {
    case WebInputEvent::GestureFlingCancel:
      fling_in_progress_ = false;
      break;
    case WebInputEvent::GestureFlingStart:
      fling_in_progress_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureScrollUpdate:
      MergeOrInsertScrollAndPinchEvent(gesture_event);
      return ShouldHandleEventNow();
    default:
      break;
  }
  EnqueueEvent(gesture_event);
  return ShouldHandleEventNow();
}

void GestureEventFilter::ProcessGestureAck(InputEventAckState ack_result,
                                           WebInputEvent::Type type,
                                           const ui::LatencyInfo& latency) {
  if (coalesced_gesture_events_.empty()) {
    DLOG(ERROR) << "Received unexpected ACK for event type " << type;
    return;
  }

  // Ack'ing an event may enqueue additional gesture events.  By ack'ing the
  // event before the forwarding of queued events below, such additional events
  // can be coalesced with existing queued events prior to dispatch.
  GestureEventWithLatencyInfo event_with_latency =
      coalesced_gesture_events_.front();
  DCHECK_EQ(event_with_latency.event.type, type);
  event_with_latency.latency.AddNewLatencyFrom(latency);
  client_->OnGestureEventAck(event_with_latency, ack_result);

  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (type == WebInputEvent::GestureFlingCancel) {
    if (event_with_latency.event.sourceDevice == WebGestureEvent::Touchscreen)
      touchscreen_tap_suppression_controller_->GestureFlingCancelAck(processed);
    else
      touchpad_tap_suppression_controller_->GestureFlingCancelAck(processed);
  }
  coalesced_gesture_events_.pop_front();

  if (ignore_next_ack_) {
    ignore_next_ack_ = false;
    return;
  }

  if (coalesced_gesture_events_.empty())
    return;

  const GestureEventWithLatencyInfo& first_gesture_event =
      coalesced_gesture_events_.front();

  // TODO(yusufo): Introduce GesturePanScroll so that these can be combined
  // into one gesture and kept inside the queue that way.
  // Check for the coupled GesturePinchUpdate before sending either event,
  // handling the case where the first GestureScrollUpdate ack is synchronous.
  GestureEventWithLatencyInfo second_gesture_event;
  if (first_gesture_event.event.type == WebInputEvent::GestureScrollUpdate &&
      coalesced_gesture_events_.size() > 1 &&
      coalesced_gesture_events_[1].event.type ==
          WebInputEvent::GesturePinchUpdate) {
    second_gesture_event = coalesced_gesture_events_[1];
    ignore_next_ack_ = true;
  }

  client_->SendGestureEventImmediately(first_gesture_event);
  if (second_gesture_event.event.type != WebInputEvent::Undefined)
    client_->SendGestureEventImmediately(second_gesture_event);
}

TouchpadTapSuppressionController*
    GestureEventFilter::GetTouchpadTapSuppressionController() {
  return touchpad_tap_suppression_controller_.get();
}

bool GestureEventFilter::HasQueuedGestureEvents() const {
  return !coalesced_gesture_events_.empty();
}

void GestureEventFilter::FlingHasBeenHalted() {
  fling_in_progress_ = false;
}

bool GestureEventFilter::ShouldHandleEventNow() const {
  return coalesced_gesture_events_.size() == 1;
}

void GestureEventFilter::ForwardGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (ShouldForwardForCoalescing(gesture_event))
    client_->SendGestureEventImmediately(gesture_event);
}

void GestureEventFilter::SendScrollEndingEventsNow() {
  scrolling_in_progress_ = false;
  GestureEventQueue debouncing_deferral_queue;
  debouncing_deferral_queue.swap(debouncing_deferral_queue_);
  for (GestureEventQueue::const_iterator it = debouncing_deferral_queue.begin();
       it != debouncing_deferral_queue.end(); it++) {
    if (ShouldForwardForGFCFiltering(*it) &&
        ShouldForwardForTapSuppression(*it) &&
        ShouldForwardForCoalescing(*it)) {
      client_->SendGestureEventImmediately(*it);
    }
  }
}

void GestureEventFilter::MergeOrInsertScrollAndPinchEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (coalesced_gesture_events_.size() <= 1) {
    EnqueueEvent(gesture_event);
    return;
  }
  GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
  if (last_event->CanCoalesceWith(gesture_event)) {
    last_event->CoalesceWith(gesture_event);
    if (!combined_scroll_pinch_.IsIdentity()) {
      combined_scroll_pinch_.ConcatTransform(
          GetTransformForEvent(gesture_event));
    }
    return;
  }
  if (coalesced_gesture_events_.size() == 2 ||
      (coalesced_gesture_events_.size() == 3 && ignore_next_ack_) ||
      !ShouldTryMerging(gesture_event, *last_event)) {
    EnqueueEvent(gesture_event);
    return;
  }
  GestureEventWithLatencyInfo scroll_event;
  GestureEventWithLatencyInfo pinch_event;
  scroll_event.event.modifiers |= gesture_event.event.modifiers;
  scroll_event.event.timeStampSeconds = gesture_event.event.timeStampSeconds;
  // Keep the oldest LatencyInfo.
  DCHECK_LE(last_event->latency.trace_id, gesture_event.latency.trace_id);
  scroll_event.latency = last_event->latency;
  pinch_event = scroll_event;
  scroll_event.event.type = WebInputEvent::GestureScrollUpdate;
  pinch_event.event.type = WebInputEvent::GesturePinchUpdate;
  pinch_event.event.x = gesture_event.event.type ==
      WebInputEvent::GesturePinchUpdate ?
          gesture_event.event.x : last_event->event.x;
  pinch_event.event.y = gesture_event.event.type ==
      WebInputEvent::GesturePinchUpdate ?
          gesture_event.event.y : last_event->event.y;

  combined_scroll_pinch_.ConcatTransform(GetTransformForEvent(gesture_event));
  GestureEventWithLatencyInfo* second_last_event = &coalesced_gesture_events_
      [coalesced_gesture_events_.size() - 2];
  if (ShouldTryMerging(gesture_event, *second_last_event)) {
    // Keep the oldest LatencyInfo.
    DCHECK_LE(second_last_event->latency.trace_id,
              scroll_event.latency.trace_id);
    scroll_event.latency = second_last_event->latency;
    pinch_event.latency = second_last_event->latency;
    coalesced_gesture_events_.pop_back();
  } else {
    DCHECK(combined_scroll_pinch_ == GetTransformForEvent(gesture_event));
    combined_scroll_pinch_.
        PreconcatTransform(GetTransformForEvent(*last_event));
  }
  coalesced_gesture_events_.pop_back();
  float combined_scale =
      SkMScalarToFloat(combined_scroll_pinch_.matrix().get(0, 0));
  float combined_scroll_pinch_x =
      SkMScalarToFloat(combined_scroll_pinch_.matrix().get(0, 3));
  float combined_scroll_pinch_y =
      SkMScalarToFloat(combined_scroll_pinch_.matrix().get(1, 3));
  scroll_event.event.data.scrollUpdate.deltaX =
      (combined_scroll_pinch_x + pinch_event.event.x) / combined_scale -
      pinch_event.event.x;
  scroll_event.event.data.scrollUpdate.deltaY =
      (combined_scroll_pinch_y + pinch_event.event.y) / combined_scale -
      pinch_event.event.y;
  coalesced_gesture_events_.push_back(scroll_event);
  pinch_event.event.data.pinchUpdate.scale = combined_scale;
  coalesced_gesture_events_.push_back(pinch_event);
}

bool GestureEventFilter::ShouldTryMerging(
    const GestureEventWithLatencyInfo& new_event,
    const GestureEventWithLatencyInfo& event_in_queue) const {
  DLOG_IF(WARNING,
          new_event.event.timeStampSeconds <
          event_in_queue.event.timeStampSeconds)
          << "Event time not monotonic?\n";
  return (event_in_queue.event.type == WebInputEvent::GestureScrollUpdate ||
      event_in_queue.event.type == WebInputEvent::GesturePinchUpdate) &&
      event_in_queue.event.modifiers == new_event.event.modifiers;
}

gfx::Transform GestureEventFilter::GetTransformForEvent(
    const GestureEventWithLatencyInfo& gesture_event) const {
  gfx::Transform gesture_transform = gfx::Transform();
  if (gesture_event.event.type == WebInputEvent::GestureScrollUpdate) {
    gesture_transform.Translate(gesture_event.event.data.scrollUpdate.deltaX,
                                gesture_event.event.data.scrollUpdate.deltaY);
  } else if (gesture_event.event.type == WebInputEvent::GesturePinchUpdate) {
    float scale = gesture_event.event.data.pinchUpdate.scale;
    gesture_transform.Translate(-gesture_event.event.x, -gesture_event.event.y);
    gesture_transform.Scale(scale,scale);
    gesture_transform.Translate(gesture_event.event.x, gesture_event.event.y);
  }
  return gesture_transform;
}

void GestureEventFilter::EnqueueEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  coalesced_gesture_events_.push_back(gesture_event);
  // Scroll and pinch events contributing to |combined_scroll_pinch_| will be
  // manually added to the queue in |MergeOrInsertScrollAndPinchEvent()|.
  combined_scroll_pinch_ = gfx::Transform();
}

}  // namespace content
