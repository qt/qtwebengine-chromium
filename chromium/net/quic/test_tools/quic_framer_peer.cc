// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_framer_peer.h"

#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {
namespace test {

// static
QuicPacketSequenceNumber QuicFramerPeer::CalculatePacketSequenceNumberFromWire(
    QuicFramer* framer,
    QuicSequenceNumberLength sequence_number_length,
    QuicPacketSequenceNumber packet_sequence_number) {
  return framer->CalculatePacketSequenceNumberFromWire(sequence_number_length,
                                                       packet_sequence_number);
}

// static
void QuicFramerPeer::SetLastSerializedGuid(QuicFramer* framer, QuicGuid guid) {
  framer->last_serialized_guid_ = guid;
}

void QuicFramerPeer::SetLastSequenceNumber(
    QuicFramer* framer,
    QuicPacketSequenceNumber packet_sequence_number) {
  framer->last_sequence_number_ = packet_sequence_number;
}

void QuicFramerPeer::SetIsServer(QuicFramer* framer, bool is_server) {
  framer->is_server_ = is_server;
}

}  // namespace test
}  // namespace net
