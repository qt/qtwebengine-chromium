// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/gestures/gesture_sequence.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {

namespace {

template <typename T>
void TransferConsumer(GestureConsumer* current_consumer,
                      GestureConsumer* new_consumer,
                      std::map<GestureConsumer*, T>* map) {
  if (map->count(current_consumer)) {
    (*map)[new_consumer] = (*map)[current_consumer];
    map->erase(current_consumer);
  }
}

void RemoveConsumerFromMap(GestureConsumer* consumer,
                           GestureRecognizerImpl::TouchIdToConsumerMap* map) {
  for (GestureRecognizerImpl::TouchIdToConsumerMap::iterator i = map->begin();
       i != map->end();) {
    if (i->second == consumer)
      map->erase(i++);
    else
      ++i;
  }
}

void TransferTouchIdToConsumerMap(
    GestureConsumer* old_consumer,
    GestureConsumer* new_consumer,
    GestureRecognizerImpl::TouchIdToConsumerMap* map) {
  for (GestureRecognizerImpl::TouchIdToConsumerMap::iterator i = map->begin();
       i != map->end(); ++i) {
    if (i->second == old_consumer)
      i->second = new_consumer;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, public:

GestureRecognizerImpl::GestureRecognizerImpl() {
}

GestureRecognizerImpl::~GestureRecognizerImpl() {
  STLDeleteValues(&consumer_sequence_);
}

// Checks if this finger is already down, if so, returns the current target.
// Otherwise, returns NULL.
GestureConsumer* GestureRecognizerImpl::GetTouchLockedTarget(
    const TouchEvent& event) {
  return touch_id_target_[event.touch_id()];
}

GestureConsumer* GestureRecognizerImpl::GetTargetForGestureEvent(
    const GestureEvent& event) {
  GestureConsumer* target = NULL;
  int touch_id = event.GetLowestTouchId();
  target = touch_id_target_for_gestures_[touch_id];
  return target;
}

GestureConsumer* GestureRecognizerImpl::GetTargetForLocation(
    const gfx::Point& location) {
  const GesturePoint* closest_point = NULL;
  int64 closest_distance_squared = 0;
  std::map<GestureConsumer*, GestureSequence*>::iterator i;
  for (i = consumer_sequence_.begin(); i != consumer_sequence_.end(); ++i) {
    const GesturePoint* points = i->second->points();
    for (int j = 0; j < GestureSequence::kMaxGesturePoints; ++j) {
      if (!points[j].in_use())
        continue;
      gfx::Vector2d delta = points[j].last_touch_position() - location;
      // Relative distance is all we need here, so LengthSquared() is
      // appropriate, and cheaper than Length().
      int64 distance_squared = delta.LengthSquared();
      if (!closest_point || distance_squared < closest_distance_squared) {
        closest_point = &points[j];
        closest_distance_squared = distance_squared;
      }
    }
  }

  const int max_distance =
      GestureConfiguration::max_separation_for_gesture_touches_in_pixels();

  if (closest_distance_squared < max_distance * max_distance && closest_point)
    return touch_id_target_[closest_point->touch_id()];
  else
    return NULL;
}

void GestureRecognizerImpl::TransferEventsTo(GestureConsumer* current_consumer,
                                             GestureConsumer* new_consumer) {
  // Send cancel to all those save |new_consumer| and |current_consumer|.
  // Don't send a cancel to |current_consumer|, unless |new_consumer| is NULL.
  // Dispatching a touch-cancel event can end up altering |touch_id_target_|
  // (e.g. when the target of the event is destroyed, causing it to be removed
  // from |touch_id_target_| in |CleanupStateForConsumer()|). So create a list
  // of the touch-ids that need to be cancelled, and dispatch the cancel events
  // for them at the end.
  std::vector<std::pair<int, GestureConsumer*> > ids;
  for (TouchIdToConsumerMap::iterator i = touch_id_target_.begin();
       i != touch_id_target_.end(); ++i) {
    if (i->second && i->second != new_consumer &&
        (i->second != current_consumer || new_consumer == NULL) &&
        i->second) {
      ids.push_back(std::make_pair(i->first, i->second));
    }
  }

  CancelTouches(&ids);

  // Transfer events from |current_consumer| to |new_consumer|.
  if (current_consumer && new_consumer) {
    TransferTouchIdToConsumerMap(current_consumer, new_consumer,
                                 &touch_id_target_);
    TransferTouchIdToConsumerMap(current_consumer, new_consumer,
                                 &touch_id_target_for_gestures_);
    TransferConsumer(current_consumer, new_consumer, &consumer_sequence_);
  }
}

bool GestureRecognizerImpl::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::Point* point) {
  if (consumer_sequence_.count(consumer) == 0)
    return false;

  *point = consumer_sequence_[consumer]->last_touch_location();
  return true;
}

void GestureRecognizerImpl::CancelActiveTouches(
    GestureConsumer* consumer) {
  std::vector<std::pair<int, GestureConsumer*> > ids;
  for (TouchIdToConsumerMap::const_iterator i = touch_id_target_.begin();
       i != touch_id_target_.end(); ++i) {
    if (i->second == consumer)
      ids.push_back(std::make_pair(i->first, i->second));
  }
  CancelTouches(&ids);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, protected:

GestureSequence* GestureRecognizerImpl::CreateSequence(
    GestureSequenceDelegate* delegate) {
  return new GestureSequence(delegate);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, private:

GestureSequence* GestureRecognizerImpl::GetGestureSequenceForConsumer(
    GestureConsumer* consumer) {
  GestureSequence* gesture_sequence = consumer_sequence_[consumer];
  if (!gesture_sequence) {
    gesture_sequence = CreateSequence(this);
    consumer_sequence_[consumer] = gesture_sequence;
  }
  return gesture_sequence;
}

void GestureRecognizerImpl::SetupTargets(const TouchEvent& event,
                                         GestureConsumer* target) {
  if (event.type() == ui::ET_TOUCH_RELEASED ||
      event.type() == ui::ET_TOUCH_CANCELLED) {
    touch_id_target_.erase(event.touch_id());
  } else if (event.type() == ui::ET_TOUCH_PRESSED) {
    touch_id_target_[event.touch_id()] = target;
    if (target)
      touch_id_target_for_gestures_[event.touch_id()] = target;
  }
}

void GestureRecognizerImpl::CancelTouches(
    std::vector<std::pair<int, GestureConsumer*> >* touches) {
  while (!touches->empty()) {
    int touch_id = touches->begin()->first;
    GestureConsumer* target = touches->begin()->second;
    TouchEvent touch_event(ui::ET_TOUCH_CANCELLED, gfx::Point(0, 0),
                           ui::EF_IS_SYNTHESIZED, touch_id,
                           ui::EventTimeForNow(), 0.0f, 0.0f, 0.0f, 0.0f);
    GestureEventHelper* helper = FindDispatchHelperForConsumer(target);
    if (helper)
      helper->DispatchCancelTouchEvent(&touch_event);
    touches->erase(touches->begin());
  }
}

GestureSequence::Gestures* GestureRecognizerImpl::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::EventResult result,
    GestureConsumer* target) {
  SetupTargets(event, target);
  GestureSequence* gesture_sequence = GetGestureSequenceForConsumer(target);
  return gesture_sequence->ProcessTouchEventForGesture(event, result);
}

void GestureRecognizerImpl::CleanupStateForConsumer(GestureConsumer* consumer) {
  if (consumer_sequence_.count(consumer)) {
    delete consumer_sequence_[consumer];
    consumer_sequence_.erase(consumer);
  }

  RemoveConsumerFromMap(consumer, &touch_id_target_);
  RemoveConsumerFromMap(consumer, &touch_id_target_for_gestures_);
}

void GestureRecognizerImpl::AddGestureEventHelper(GestureEventHelper* helper) {
  helpers_.push_back(helper);
}

void GestureRecognizerImpl::RemoveGestureEventHelper(
    GestureEventHelper* helper) {
  std::vector<GestureEventHelper*>::iterator it = std::find(helpers_.begin(),
      helpers_.end(), helper);
  if (it != helpers_.end())
    helpers_.erase(it);
}

void GestureRecognizerImpl::DispatchPostponedGestureEvent(GestureEvent* event) {
  GestureConsumer* consumer = GetTargetForGestureEvent(*event);
  if (consumer) {
    GestureEventHelper* helper = FindDispatchHelperForConsumer(consumer);
    if (helper)
      helper->DispatchPostponedGestureEvent(event);
  }
}

GestureEventHelper* GestureRecognizerImpl::FindDispatchHelperForConsumer(
    GestureConsumer* consumer) {
  std::vector<GestureEventHelper*>::iterator it;
  for (it = helpers_.begin(); it != helpers_.end(); ++it) {
    if ((*it)->CanDispatchToConsumer(consumer))
      return (*it);
  }
  return NULL;
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create() {
  return new GestureRecognizerImpl();
}

static GestureRecognizerImpl* g_gesture_recognizer_instance = NULL;

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Get() {
  if (!g_gesture_recognizer_instance)
    g_gesture_recognizer_instance = new GestureRecognizerImpl();
  return g_gesture_recognizer_instance;
}

void SetGestureRecognizerForTesting(GestureRecognizer* gesture_recognizer) {
  // Transfer helpers to the new GR.
  std::vector<GestureEventHelper*>& helpers =
      g_gesture_recognizer_instance->helpers();
  std::vector<GestureEventHelper*>::iterator it;
  for (it = helpers.begin(); it != helpers.end(); ++it)
    gesture_recognizer->AddGestureEventHelper(*it);

  helpers.clear();
  g_gesture_recognizer_instance =
      static_cast<GestureRecognizerImpl*>(gesture_recognizer);
}

}  // namespace ui
