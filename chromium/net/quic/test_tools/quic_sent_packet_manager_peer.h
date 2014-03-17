// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_

#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"

namespace net {

class SendAlgorithmInterface;

namespace test {

class QuicSentPacketManagerPeer {
 public:
  static void SetSendAlgorithm(QuicSentPacketManager* sent_packet_manager,
                               SendAlgorithmInterface* send_algorithm);

  static size_t GetNackCount(
      const QuicSentPacketManager* sent_packet_manager,
      QuicPacketSequenceNumber sequence_number);

  static QuicTime GetSentTime(const QuicSentPacketManager* sent_packet_manager,
                              QuicPacketSequenceNumber sequence_number);

  static QuicTime::Delta rtt(QuicSentPacketManager* sent_packet_manager);

  // Returns true if |sequence_number| is a retransmission of a packet.
  static bool IsRetransmission(QuicSentPacketManager* sent_packet_manager,
                               QuicPacketSequenceNumber sequence_number);

  static void MarkForRetransmission(QuicSentPacketManager* sent_packet_manager,
                                    QuicPacketSequenceNumber sequence_number,
                                    TransmissionType transmission_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManagerPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_
