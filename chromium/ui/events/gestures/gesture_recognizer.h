// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {
// A GestureRecognizer is an abstract base class for conversion of touch events
// into gestures.
class EVENTS_EXPORT GestureRecognizer {
 public:
  static GestureRecognizer* Create();
  static GestureRecognizer* Get();

  // List of GestureEvent*.
  typedef ScopedVector<GestureEvent> Gestures;

  virtual ~GestureRecognizer() {}

  // Invoked for each touch event that could contribute to the current gesture.
  // Returns list of zero or more GestureEvents identified after processing
  // TouchEvent.
  // Caller would be responsible for freeing up Gestures.
  virtual Gestures* ProcessTouchEventForGesture(const TouchEvent& event,
                                                ui::EventResult result,
                                                GestureConsumer* consumer) = 0;

  // This is called when the consumer is destroyed. So this should cleanup any
  // internal state maintained for |consumer|.
  virtual void CleanupStateForConsumer(GestureConsumer* consumer) = 0;

  // Return the window which should handle this TouchEvent, in the case where
  // the touch is already associated with a target.
  // Otherwise, returns null.
  virtual GestureConsumer* GetTouchLockedTarget(const TouchEvent& event) = 0;

  // Return the window which should handle this GestureEvent.
  virtual GestureConsumer* GetTargetForGestureEvent(
      const GestureEvent& event) = 0;

  // If there is an active touch within
  // GestureConfiguration::max_separation_for_gesture_touches_in_pixels,
  // of |location|, returns the target of the nearest active touch.
  virtual GestureConsumer* GetTargetForLocation(const gfx::Point& location) = 0;

  // Makes |new_consumer| the target for events previously targeting
  // |current_consumer|. All other targets are canceled.
  // The caller is responsible for updating the state of the consumers to
  // be aware of this transfer of control (there are no ENTERED/EXITED events).
  // If |new_consumer| is NULL, all events are canceled.
  // If |old_consumer| is NULL, all events not already targeting |new_consumer|
  // are canceled.
  virtual void TransferEventsTo(GestureConsumer* current_consumer,
                                GestureConsumer* new_consumer) = 0;

  // If a gesture is underway for |consumer| |point| is set to the last touch
  // point and true is returned. If no touch events have been processed for
  // |consumer| false is returned and |point| is untouched.
  virtual bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                          gfx::Point* point) = 0;

  // Sends a touch cancel event for every active touch.
  virtual void CancelActiveTouches(GestureConsumer* consumer) = 0;

  // Subscribes |helper| for dispatching async gestures such as long press.
  // The Gesture Recognizer does NOT take ownership of |helper| and it is the
  // responsibility of the |helper| to call |RemoveGestureEventHelper()| on
  // destruction.
  virtual void AddGestureEventHelper(GestureEventHelper* helper) = 0;

  // Unsubscribes |helper| from async gesture dispatch.
  // Since the GestureRecognizer does not own the |helper|, it is not deleted
  // and must be cleaned up appropriately by the caller.
  virtual void RemoveGestureEventHelper(GestureEventHelper* helper) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_H_
