// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_protocol.h"

namespace net {

struct QuicAckFrame;
struct QuicPacketHeader;
class QuicAlarm;
class QuicConnection;
class QuicConnectionHelperInterface;
class QuicConnectionVisitorInterface;
class QuicFecGroup;
class QuicFramer;
class QuicPacketCreator;
class ReceiveAlgorithmInterface;
class SendAlgorithmInterface;

namespace test {

// Peer to make public a number of otherwise private QuicConnection methods.
class QuicConnectionPeer {
 public:
  static void SendAck(QuicConnection* connection);

  static void SetReceiveAlgorithm(QuicConnection* connection,
                                  ReceiveAlgorithmInterface* receive_algorithm);

  static void SetSendAlgorithm(QuicConnection* connection,
                               SendAlgorithmInterface* send_algorithm);

  static QuicAckFrame* CreateAckFrame(QuicConnection* connection);

  static QuicConnectionVisitorInterface* GetVisitor(
      QuicConnection* connection);

  static QuicPacketCreator* GetPacketCreator(QuicConnection* connection);

  static bool GetReceivedTruncatedAck(QuicConnection* connection);

  static size_t GetNumRetransmissionTimeouts(QuicConnection* connection);

  static QuicTime::Delta GetNetworkTimeout(QuicConnection* connection);

  static bool IsSavedForRetransmission(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static size_t GetRetransmissionCount(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static QuicPacketEntropyHash GetSentEntropyHash(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static bool IsValidEntropy(QuicConnection* connection,
                             QuicPacketSequenceNumber largest_observed,
                             const SequenceNumberSet& missing_packets,
                             QuicPacketEntropyHash entropy_hash);

  static QuicPacketEntropyHash ReceivedEntropyHash(
      QuicConnection* connection,
      QuicPacketSequenceNumber sequence_number);

  static bool IsServer(QuicConnection* connection);

  static void SetIsServer(QuicConnection* connection, bool is_server);

  static void SetSelfAddress(QuicConnection* connection,
                             const IPEndPoint& self_address);

  static void SetPeerAddress(QuicConnection* connection,
                             const IPEndPoint& peer_address);

  static void SwapCrypters(QuicConnection* connection, QuicFramer* framer);

  static void SetMaxPacketsPerRetransmissionAlarm(QuicConnection* connection,
                                                  int max_packets);

  static QuicConnectionHelperInterface* GetHelper(QuicConnection* connection);

  static QuicFramer* GetFramer(QuicConnection* connection);

  // Set last_header_->fec_group = fec_group and return connection->GetFecGroup
  static QuicFecGroup* GetFecGroup(QuicConnection* connection, int fec_group);

  static QuicAlarm* GetAckAlarm(QuicConnection* connection);
  static QuicAlarm* GetRetransmissionAlarm(QuicConnection* connection);
  static QuicAlarm* GetSendAlarm(QuicConnection* connection);
  static QuicAlarm* GetTimeoutAlarm(QuicConnection* connection);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_PEER_H_
