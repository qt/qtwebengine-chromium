// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/tcp_cubic_sender.h"

namespace net {

namespace {
// Constants based on TCP defaults.
const int64 kHybridStartLowWindow = 16;
const QuicByteCount kMaxSegmentSize = kMaxPacketSize;
const QuicByteCount kDefaultReceiveWindow = 64000;
const int64 kInitialCongestionWindow = 10;
const int kMaxBurstLength = 3;
const int kInitialRttMs = 60;  // At a typical RTT 60 ms.
const float kAlpha = 0.125f;
const float kOneMinusAlpha = (1 - kAlpha);
const float kBeta = 0.25f;
const float kOneMinusBeta = (1 - kBeta);
};  // namespace

TcpCubicSender::TcpCubicSender(
    const QuicClock* clock,
    bool reno,
    QuicTcpCongestionWindow max_tcp_congestion_window)
    : hybrid_slow_start_(clock),
      cubic_(clock),
      reno_(reno),
      congestion_window_count_(0),
      receiver_congestion_window_(kDefaultReceiveWindow),
      last_received_accumulated_number_of_lost_packets_(0),
      bytes_in_flight_(0),
      update_end_sequence_number_(true),
      end_sequence_number_(0),
      congestion_window_(kInitialCongestionWindow),
      slowstart_threshold_(max_tcp_congestion_window),
      max_tcp_congestion_window_(max_tcp_congestion_window),
      delay_min_(QuicTime::Delta::Zero()),
      smoothed_rtt_(QuicTime::Delta::Zero()),
      mean_deviation_(QuicTime::Delta::Zero()) {
}

TcpCubicSender::~TcpCubicSender() {
}

void TcpCubicSender::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& feedback,
    QuicTime feedback_receive_time,
    const SentPacketsMap& /*sent_packets*/) {
  if (last_received_accumulated_number_of_lost_packets_ !=
      feedback.tcp.accumulated_number_of_lost_packets) {
    int recovered_lost_packets =
        last_received_accumulated_number_of_lost_packets_ -
        feedback.tcp.accumulated_number_of_lost_packets;
    last_received_accumulated_number_of_lost_packets_ =
        feedback.tcp.accumulated_number_of_lost_packets;
    if (recovered_lost_packets > 0) {
      OnIncomingLoss(feedback_receive_time);
    }
  }
  receiver_congestion_window_ = feedback.tcp.receive_window;
}

void TcpCubicSender::OnIncomingAck(
    QuicPacketSequenceNumber acked_sequence_number, QuicByteCount acked_bytes,
    QuicTime::Delta rtt) {
  DCHECK_GE(bytes_in_flight_, acked_bytes);
  bytes_in_flight_ -= acked_bytes;
  CongestionAvoidance(acked_sequence_number);
  AckAccounting(rtt);
  if (end_sequence_number_ == acked_sequence_number) {
    DLOG(INFO) << "Start update end sequence number @" << acked_sequence_number;
    update_end_sequence_number_ = true;
  }
}

void TcpCubicSender::OnIncomingLoss(QuicTime /*ack_receive_time*/) {
  // In a normal TCP we would need to know the lowest missing packet to detect
  // if we receive 3 missing packets. Here we get a missing packet for which we
  // enter TCP Fast Retransmit immediately.
  if (reno_) {
    congestion_window_ = congestion_window_ >> 1;
    slowstart_threshold_ = congestion_window_;
  } else {
    congestion_window_ =
        cubic_.CongestionWindowAfterPacketLoss(congestion_window_);
    slowstart_threshold_ = congestion_window_;
  }
  // Sanity, make sure that we don't end up with an empty window.
  if (congestion_window_ == 0) {
    congestion_window_ = 1;
  }
  DLOG(INFO) << "Incoming loss; congestion window:" << congestion_window_;
}

bool TcpCubicSender::SentPacket(QuicTime /*sent_time*/,
                                QuicPacketSequenceNumber sequence_number,
                                QuicByteCount bytes,
                                Retransmission is_retransmission,
                                HasRetransmittableData is_retransmittable) {
  // Only update bytes_in_flight_ for data packets.
  if (is_retransmittable != HAS_RETRANSMITTABLE_DATA) {
    return false;
  }

  bytes_in_flight_ += bytes;
  if (is_retransmission == NOT_RETRANSMISSION && update_end_sequence_number_) {
    end_sequence_number_ = sequence_number;
    if (AvailableCongestionWindow() == 0) {
      update_end_sequence_number_ = false;
      DLOG(INFO) << "Stop update end sequence number @" << sequence_number;
    }
  }
  return true;
}

void TcpCubicSender::AbandoningPacket(QuicPacketSequenceNumber sequence_number,
                                      QuicByteCount abandoned_bytes) {
  DCHECK_GE(bytes_in_flight_, abandoned_bytes);
  bytes_in_flight_ -= abandoned_bytes;
}

QuicTime::Delta TcpCubicSender::TimeUntilSend(
    QuicTime now,
    Retransmission is_retransmission,
    HasRetransmittableData has_retransmittable_data,
    IsHandshake handshake) {
  if (is_retransmission == IS_RETRANSMISSION ||
      has_retransmittable_data == NO_RETRANSMITTABLE_DATA ||
      handshake == IS_HANDSHAKE) {
    // For TCP we can always send a retransmission,  or an ACK immediately.
    // We also immediately send any handshake packet (CHLO, etc.).  We provide
    // this  special dispensation for handshake messages in QUIC, although the
    // concept is not present in TCP.
    return QuicTime::Delta::Zero();
  }
  if (AvailableCongestionWindow() == 0) {
    return QuicTime::Delta::Infinite();
  }
  return QuicTime::Delta::Zero();
}

QuicByteCount TcpCubicSender::AvailableCongestionWindow() {
  if (bytes_in_flight_ > CongestionWindow()) {
    return 0;
  }
  return CongestionWindow() - bytes_in_flight_;
}

QuicByteCount TcpCubicSender::CongestionWindow() {
  // What's the current congestion window in bytes.
  return std::min(receiver_congestion_window_,
                  congestion_window_ * kMaxSegmentSize);
}

QuicBandwidth TcpCubicSender::BandwidthEstimate() {
  // TODO(pwestin): make a long term estimate, based on RTT and loss rate? or
  // instantaneous estimate?
  // Throughput ~= (1/RTT)*sqrt(3/2p)
  return QuicBandwidth::Zero();
}

QuicTime::Delta TcpCubicSender::SmoothedRtt() {
  if (smoothed_rtt_.IsZero()) {
    return QuicTime::Delta::FromMilliseconds(kInitialRttMs);
  }
  return smoothed_rtt_;
}

QuicTime::Delta TcpCubicSender::RetransmissionDelay() {
  return QuicTime::Delta::FromMicroseconds(
      smoothed_rtt_.ToMicroseconds() + 4 * mean_deviation_.ToMicroseconds());
}

void TcpCubicSender::Reset() {
  delay_min_ = QuicTime::Delta::Zero();
  hybrid_slow_start_.Restart();
}

bool TcpCubicSender::IsCwndLimited() const {
  const QuicByteCount congestion_window_bytes = congestion_window_ *
      kMaxSegmentSize;
  if (bytes_in_flight_ >= congestion_window_bytes) {
    return true;
  }
  const QuicByteCount tcp_max_burst = kMaxBurstLength * kMaxSegmentSize;
  const QuicByteCount left = congestion_window_bytes - bytes_in_flight_;
  return left <= tcp_max_burst;
}

// Called when we receive and ack. Normal TCP tracks how many packets one ack
// represents, but quic has a separate ack for each packet.
void TcpCubicSender::CongestionAvoidance(QuicPacketSequenceNumber ack) {
  if (!IsCwndLimited()) {
    // We don't update the congestion window unless we are close to using the
    // window we have available.
    return;
  }
  if (congestion_window_ < slowstart_threshold_) {
    // Slow start.
    if (hybrid_slow_start_.EndOfRound(ack)) {
      hybrid_slow_start_.Reset(end_sequence_number_);
    }
    // congestion_window_cnt is the number of acks since last change of snd_cwnd
    if (congestion_window_ < max_tcp_congestion_window_) {
      // TCP slow start, exponential growth, increase by one for each ACK.
      congestion_window_++;
    }
    DLOG(INFO) << "Slow start; congestion window:" << congestion_window_;
  } else {
    if (congestion_window_ < max_tcp_congestion_window_) {
      if (reno_) {
        // Classic Reno congestion avoidance provided for testing.
        if (congestion_window_count_ >= congestion_window_) {
          congestion_window_++;
          congestion_window_count_ = 0;
        } else {
          congestion_window_count_++;
        }
        DLOG(INFO) << "Reno; congestion window:" << congestion_window_;
      } else {
        congestion_window_ = std::min(
            max_tcp_congestion_window_,
            cubic_.CongestionWindowAfterAck(congestion_window_, delay_min_));
        DLOG(INFO) << "Cubic; congestion window:" << congestion_window_;
      }
    }
  }
}

// TODO(pwestin): what is the timout value?
void TcpCubicSender::OnTimeOut() {
  cubic_.Reset();
  congestion_window_ = 1;
}

void TcpCubicSender::AckAccounting(QuicTime::Delta rtt) {
  if (rtt.IsInfinite() || rtt.IsZero()) {
    return;
  }
  // RTT can't be negative.
  DCHECK_LT(0, rtt.ToMicroseconds());

  // TODO(pwestin): Discard delay samples right after fast recovery,
  // during 1 second?.

  // First time call or link delay decreases.
  if (delay_min_.IsZero() || delay_min_ > rtt) {
    delay_min_ = rtt;
  }
  // First time call.
  if (smoothed_rtt_.IsZero()) {
    smoothed_rtt_ = rtt;
    mean_deviation_ = QuicTime::Delta::FromMicroseconds(
        rtt.ToMicroseconds() / 2);
  } else {
    mean_deviation_ = QuicTime::Delta::FromMicroseconds(
        kOneMinusBeta * mean_deviation_.ToMicroseconds() +
        kBeta * abs(smoothed_rtt_.ToMicroseconds() - rtt.ToMicroseconds()));
    smoothed_rtt_ = QuicTime::Delta::FromMicroseconds(
        kOneMinusAlpha * smoothed_rtt_.ToMicroseconds() +
        kAlpha * rtt.ToMicroseconds());
    DLOG(INFO) << "Cubic; mean_deviation_:" << mean_deviation_.ToMicroseconds();
  }

  // Hybrid start triggers when cwnd is larger than some threshold.
  if (congestion_window_ <= slowstart_threshold_ &&
      congestion_window_ >= kHybridStartLowWindow) {
    if (!hybrid_slow_start_.started()) {
      // Time to start the hybrid slow start.
      hybrid_slow_start_.Reset(end_sequence_number_);
    }
    hybrid_slow_start_.Update(rtt, delay_min_);
    if (hybrid_slow_start_.Exit()) {
      slowstart_threshold_ = congestion_window_;
    }
  }
}

}  // namespace net
