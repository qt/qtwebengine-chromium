// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_TARGET_ITERATOR_H_
#define UI_EVENTS_EVENT_TARGET_ITERATOR_H_

#include <vector>

namespace ui {

class EventTarget;

// An interface that allows iterating over a set of EventTargets.
class EventTargetIterator {
 public:
  virtual ~EventTargetIterator() {}
  virtual EventTarget* GetNextTarget() = 0;
};

// Provides an EventTargetIterator implementation for iterating over a list of
// EventTargets. The list is iterated in the reverse order, since typically the
// EventTargets are maintained in increasing z-order in the lists.
template<typename T>
class EventTargetIteratorImpl : public EventTargetIterator {
 public:
  explicit EventTargetIteratorImpl(const std::vector<T*>& children)
      : begin_(children.rbegin()),
        end_(children.rend()) {
  }
  virtual ~EventTargetIteratorImpl() {}

  virtual EventTarget* GetNextTarget() OVERRIDE {
    if (begin_ == end_)
      return NULL;
    EventTarget* target = *(begin_);
    ++begin_;
    return target;
  }

 private:
  typename std::vector<T*>::const_reverse_iterator begin_;
  typename std::vector<T*>::const_reverse_iterator end_;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_TARGET_ITERATOR_H_
