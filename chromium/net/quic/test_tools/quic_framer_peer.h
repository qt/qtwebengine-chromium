// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {

class QuicFramer;

namespace test {

class QuicFramerPeer {
 public:
  static QuicPacketSequenceNumber CalculatePacketSequenceNumberFromWire(
      QuicFramer* framer,
      QuicSequenceNumberLength sequence_number_length,
      QuicPacketSequenceNumber packet_sequence_number);
  static void SetLastSerializedGuid(QuicFramer* framer, QuicGuid guid);
  static void SetLastSequenceNumber(
      QuicFramer* framer,
      QuicPacketSequenceNumber packet_sequence_number);
  static void SetIsServer(QuicFramer* framer, bool is_server);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicFramerPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
