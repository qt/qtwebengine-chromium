// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_clock.h"

#include "net/tools/epoll_server/epoll_server.h"

namespace net {
namespace tools {

QuicEpollClock::QuicEpollClock(EpollServer* epoll_server)
    : epoll_server_(epoll_server) {
}

QuicEpollClock::~QuicEpollClock() {}

QuicTime QuicEpollClock::ApproximateNow() const {
  return QuicTime::Zero().Add(
      QuicTime::Delta::FromMicroseconds(epoll_server_->ApproximateNowInUsec()));
}

QuicTime QuicEpollClock::Now() const {
  return QuicTime::Zero().Add(
      QuicTime::Delta::FromMicroseconds(epoll_server_->NowInUsec()));
}

}  // namespace tools
}  // namespace net
