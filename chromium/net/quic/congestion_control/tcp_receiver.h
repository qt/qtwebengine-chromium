// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TCP receiver side congestion algorithm, emulates the behaviour of TCP.

#ifndef NET_QUIC_CONGESTION_CONTROL_TCP_RECEIVER_H_
#define NET_QUIC_CONGESTION_CONTROL_TCP_RECEIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"

namespace net {

class NET_EXPORT_PRIVATE TcpReceiver : public ReceiveAlgorithmInterface {
 public:
  TcpReceiver();

  // Size of the (currently fixed) receive window.
  static const QuicByteCount kReceiveWindowTCP;

  // Start implementation of SendAlgorithmInterface.
  virtual bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* feedback) OVERRIDE;

  virtual void RecordIncomingPacket(QuicByteCount bytes,
                                    QuicPacketSequenceNumber sequence_number,
                                    QuicTime timestamp,
                                    bool revived) OVERRIDE;

 private:
  // We need to keep track of FEC recovered packets.
  int accumulated_number_of_recoverd_lost_packets_;
  QuicByteCount receive_window_;

  DISALLOW_COPY_AND_ASSIGN(TcpReceiver);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_TCP_RECEIVER_H_
