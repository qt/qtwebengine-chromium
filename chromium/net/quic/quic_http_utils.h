// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP_UTILS_H_
#define NET_QUIC_QUIC_HTTP_UTILS_H_

#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/quic/quic_protocol.h"

namespace net {

NET_EXPORT_PRIVATE QuicPriority ConvertRequestPriorityToQuicPriority(
    RequestPriority priority);

NET_EXPORT_PRIVATE RequestPriority ConvertQuicPriorityToRequestPriority(
    QuicPriority priority);

}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP_UTILS_H_
