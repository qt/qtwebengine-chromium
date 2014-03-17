// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_WRITE_BLOCKED_LIST_H_
#define NET_SPDY_WRITE_BLOCKED_LIST_H_

#include <algorithm>
#include <deque>

#include "base/logging.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

const int kHighestPriority = 0;
const int kLowestPriority = 7;

template <typename IdType>
class WriteBlockedList {
 public:
  // 0(1) size lookup.  0(1) insert at front or back.
  typedef std::deque<IdType> BlockedList;
  typedef typename BlockedList::iterator iterator;

  static SpdyPriority ClampPriority(SpdyPriority priority) {
    if (priority < kHighestPriority) {
      LOG(DFATAL) << "Invalid priority: " << static_cast<int>(priority);
      return kHighestPriority;
    }
    if (priority > kLowestPriority) {
      LOG(DFATAL) << "Invalid priority: " << static_cast<int>(priority);
      return kLowestPriority;
    }
    return priority;
  }

  // Returns the priority of the highest priority list with sessions on it.
  SpdyPriority GetHighestPriorityWriteBlockedList() const {
    for (SpdyPriority i = 0; i <= kLowestPriority; ++i) {
      if (write_blocked_lists_[i].size() > 0)
        return i;
    }
    LOG(DFATAL) << "No blocked streams";
    return kHighestPriority;
  }

  IdType PopFront(SpdyPriority priority) {
    priority = ClampPriority(priority);
    DCHECK(!write_blocked_lists_[priority].empty());
    IdType stream_id = write_blocked_lists_[priority].front();
    write_blocked_lists_[priority].pop_front();
    return stream_id;
  }

  bool HasWriteBlockedStreamsGreaterThanPriority(SpdyPriority priority) const {
    priority = ClampPriority(priority);
    for (SpdyPriority i = kHighestPriority; i < priority; ++i) {
      if (!write_blocked_lists_[i].empty()) {
        return true;
      }
    }
    return false;
  }

  bool HasWriteBlockedStreams() const {
    for (SpdyPriority i = kHighestPriority; i <= kLowestPriority; ++i) {
      if (!write_blocked_lists_[i].empty()) {
        return true;
      }
    }
    return false;
  }

  void PushBack(IdType stream_id, SpdyPriority priority) {
    write_blocked_lists_[ClampPriority(priority)].push_back(stream_id);
  }

  void RemoveStreamFromWriteBlockedList(IdType stream_id,
                                        SpdyPriority priority) {
    iterator it = std::find(write_blocked_lists_[priority].begin(),
                            write_blocked_lists_[priority].end(),
                            stream_id);
    while (it != write_blocked_lists_[priority].end()) {
      write_blocked_lists_[priority].erase(it);
      it = std::find(write_blocked_lists_[priority].begin(),
                     write_blocked_lists_[priority].end(),
                     stream_id);
    }
  }

  size_t NumBlockedStreams() const {
    size_t num_blocked_streams = 0;
    for (SpdyPriority i = kHighestPriority; i <= kLowestPriority; ++i) {
      num_blocked_streams += write_blocked_lists_[i].size();
    }
    return num_blocked_streams;
  }

 private:
  BlockedList write_blocked_lists_[kLowestPriority + 1];
};

}  // namespace net

#endif  // NET_SPDY_WRITE_BLOCKED_LIST_H_
