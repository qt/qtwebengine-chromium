// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/fix_rate_sender.h"

#include <math.h>

#include "base/logging.h"
#include "net/quic/quic_protocol.h"

namespace {
  const int kInitialBitrate = 100000;  // In bytes per second.
  const uint64 kWindowSizeUs = 10000;  // 10 ms.
}

namespace net {

FixRateSender::FixRateSender(const QuicClock* clock)
    : bitrate_(QuicBandwidth::FromBytesPerSecond(kInitialBitrate)),
      fix_rate_leaky_bucket_(bitrate_),
      paced_sender_(bitrate_),
      data_in_flight_(0),
      latest_rtt_(QuicTime::Delta::Zero()) {
  DLOG(INFO) << "FixRateSender";
}

FixRateSender::~FixRateSender() {
}

void FixRateSender::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback,
    QuicTime feedback_receive_time,
    const SentPacketsMap& /*sent_packets*/) {
  DCHECK(feedback.type == kFixRate) <<
      "Invalid incoming CongestionFeedbackType:" << feedback.type;
  if (feedback.type == kFixRate) {
    bitrate_ = feedback.fix_rate.bitrate;
    fix_rate_leaky_bucket_.SetDrainingRate(feedback_receive_time, bitrate_);
    paced_sender_.UpdateBandwidthEstimate(feedback_receive_time, bitrate_);
  }
  // Silently ignore invalid messages in release mode.
}

void FixRateSender::OnIncomingAck(
    QuicPacketSequenceNumber /*acked_sequence_number*/,
    QuicByteCount bytes_acked,
    QuicTime::Delta rtt) {
  // RTT can't be negative.
  DCHECK_LE(0, rtt.ToMicroseconds());

  data_in_flight_ -= bytes_acked;
  if (rtt.IsInfinite()) {
    return;
  }
  latest_rtt_ = rtt;
}

void FixRateSender::OnIncomingLoss(QuicTime /*ack_receive_time*/) {
  // Ignore losses for fix rate sender.
}

bool FixRateSender::SentPacket(
    QuicTime sent_time,
    QuicPacketSequenceNumber /*sequence_number*/,
    QuicByteCount bytes,
    Retransmission is_retransmission,
    HasRetransmittableData /*has_retransmittable_data*/) {
  fix_rate_leaky_bucket_.Add(sent_time, bytes);
  paced_sender_.SentPacket(sent_time, bytes);
  if (is_retransmission == NOT_RETRANSMISSION) {
    data_in_flight_ += bytes;
  }
  return true;
}

void FixRateSender::AbandoningPacket(
    QuicPacketSequenceNumber /*sequence_number*/,
    QuicByteCount /*abandoned_bytes*/) {
}

QuicTime::Delta FixRateSender::TimeUntilSend(
    QuicTime now,
    Retransmission /*is_retransmission*/,
    HasRetransmittableData /*has_retransmittable_data*/,
    IsHandshake /*handshake*/) {
  if (CongestionWindow() > fix_rate_leaky_bucket_.BytesPending(now)) {
    if (CongestionWindow() <= data_in_flight_) {
      // We need an ack before we send more.
      return QuicTime::Delta::Infinite();
    }
    return paced_sender_.TimeUntilSend(now, QuicTime::Delta::Zero());
  }
  QuicTime::Delta time_remaining = fix_rate_leaky_bucket_.TimeRemaining(now);
  if (time_remaining.IsZero()) {
    // We need an ack before we send more.
    return QuicTime::Delta::Infinite();
  }
  return paced_sender_.TimeUntilSend(now, time_remaining);
}

QuicByteCount FixRateSender::CongestionWindow() {
  QuicByteCount window_size_bytes = bitrate_.ToBytesPerPeriod(
      QuicTime::Delta::FromMicroseconds(kWindowSizeUs));
  // Make sure window size is not less than a packet.
  return std::max(kMaxPacketSize, window_size_bytes);
}

QuicBandwidth FixRateSender::BandwidthEstimate() {
  return bitrate_;
}

QuicTime::Delta FixRateSender::SmoothedRtt() {
  // TODO(satyamshekhar): Calculate and return smoothed rtt.
  return latest_rtt_;
}

QuicTime::Delta FixRateSender::RetransmissionDelay() {
  // TODO(pwestin): Calculate and return retransmission delay.
  // Use 2 * the latest RTT for now.
  return latest_rtt_.Add(latest_rtt_);
}

}  // namespace net
