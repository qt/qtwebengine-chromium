// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_data_stream_peer.h"

#include "net/quic/quic_data_stream.h"

namespace net {
namespace test {

// static
void QuicDataStreamPeer::SetHeadersDecompressed(QuicDataStream* stream,
                                                bool headers_decompressed) {
  stream->headers_decompressed_ = headers_decompressed;
}

}  // namespace test
}  // namespace net
