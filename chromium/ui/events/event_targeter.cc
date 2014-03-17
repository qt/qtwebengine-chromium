// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_targeter.h"

#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"

namespace ui {

EventTargeter::~EventTargeter() {
}

EventTarget* EventTargeter::FindTargetForEvent(EventTarget* root,
                                               Event* event) {
  if (event->IsMouseEvent() ||
      event->IsScrollEvent() ||
      event->IsTouchEvent() ||
      event->IsGestureEvent()) {
    return FindTargetForLocatedEvent(root,
                                     static_cast<LocatedEvent*>(event));
  }
  return root;
}

EventTarget* EventTargeter::FindTargetForLocatedEvent(EventTarget* root,
                                                      LocatedEvent* event) {
  scoped_ptr<EventTargetIterator> iter = root->GetChildIterator();
  if (iter) {
    EventTarget* target = root;
    EventTarget* child = NULL;
    while ((child = iter->GetNextTarget())) {
      if (!SubtreeShouldBeExploredForEvent(child, *event))
        continue;
      target->ConvertEventToTarget(child, event);
      EventTargeter* targeter = child->GetEventTargeter();
      EventTarget* child_target = targeter ?
          targeter->FindTargetForLocatedEvent(child, event) :
          FindTargetForLocatedEvent(child, event);
      if (child_target)
        return child_target;
      target = child;
    }
    target->ConvertEventToTarget(root, event);
  }
  return root->CanAcceptEvent(*event) ? root : NULL;
}

bool EventTargeter::SubtreeShouldBeExploredForEvent(EventTarget* target,
                                                    const LocatedEvent& event) {
  return true;
}

}  // namespace ui
