// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/clock_offset_estimator_impl.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>

#include "platform/base/trivial_clock_traits.h"
#include "util/chrono_helpers.h"

namespace openscreen::cast {
namespace {

// This should be large enough so that we can collect all 3 events before
// the entry gets removed from the map.
constexpr size_t kMaxEventTimesMapSize = 500;

// Bitwise merging of values to produce an ordered key for entries in the
// BoundCalculator::events_ map. Since std::map is sorted by key value, we
// ensure that the Packet ID is first (since the RTP timestamp may roll over
// eventually).
//
//  0         1         2         3         4         5         6
//  0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |   Packet ID   |               RTP Timestamp                 |*| (is_audio)
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
uint64_t MakeEventKey(RtpTimeTicks rtp, uint16_t packet_id, bool audio) {
  return (static_cast<uint64_t>(packet_id) << 48) |
         (static_cast<uint64_t>(rtp.lower_32_bits()) << 1) |
         static_cast<uint64_t>(audio ? 1 : 0);
}

}  // namespace

std::unique_ptr<ClockOffsetEstimator> ClockOffsetEstimator::Create() {
  return std::make_unique<ClockOffsetEstimatorImpl>();
}

ClockOffsetEstimatorImpl::ClockOffsetEstimatorImpl() = default;
ClockOffsetEstimatorImpl::ClockOffsetEstimatorImpl(
    ClockOffsetEstimatorImpl&&) noexcept = default;
ClockOffsetEstimatorImpl& ClockOffsetEstimatorImpl::operator=(
    ClockOffsetEstimatorImpl&&) = default;
ClockOffsetEstimatorImpl::~ClockOffsetEstimatorImpl() = default;

void ClockOffsetEstimatorImpl::OnFrameEvent(const FrameEvent& frame_event) {
  switch (frame_event.type) {
    case StatisticsEvent::Type::kFrameAckSent:
      frame_bound_.SetSent(
          frame_event.rtp_timestamp, 0,
          frame_event.media_type == StatisticsEvent::MediaType::kAudio,
          frame_event.timestamp);
      break;
    case StatisticsEvent::Type::kFrameAckReceived:
      frame_bound_.SetReceived(
          frame_event.rtp_timestamp, 0,
          frame_event.media_type == StatisticsEvent::MediaType::kAudio,
          frame_event.timestamp);
      break;
    default:
      // Ignored
      break;
  }
}

void ClockOffsetEstimatorImpl::OnPacketEvent(const PacketEvent& packet_event) {
  switch (packet_event.type) {
    case StatisticsEvent::Type::kPacketSentToNetwork:
      packet_bound_.SetSent(
          packet_event.rtp_timestamp, packet_event.packet_id,
          packet_event.media_type == StatisticsEvent::MediaType::kAudio,
          packet_event.timestamp);
      break;
    case StatisticsEvent::Type::kPacketReceived:
      packet_bound_.SetReceived(
          packet_event.rtp_timestamp, packet_event.packet_id,
          packet_event.media_type == StatisticsEvent::MediaType::kAudio,
          packet_event.timestamp);
      break;
    default:
      // Ignored
      break;
  }
}

bool ClockOffsetEstimatorImpl::GetReceiverOffsetBounds(
    Clock::duration& frame_bound,
    Clock::duration& packet_bound) const {
  if (!frame_bound_.has_bound() || !packet_bound_.has_bound()) {
    return false;
  }

  frame_bound = -frame_bound_.bound();
  packet_bound = packet_bound_.bound();

  return true;
}

std::optional<Clock::duration> ClockOffsetEstimatorImpl::GetEstimatedOffset()
    const {
  Clock::duration frame_bound;
  Clock::duration packet_bound;
  if (!GetReceiverOffsetBounds(frame_bound, packet_bound)) {
    return {};
  }
  return (packet_bound + frame_bound) / 2;
}

std::optional<Clock::duration> ClockOffsetEstimatorImpl::GetEstimatedLatency()
    const {
  Clock::duration frame_bound;
  Clock::duration packet_bound;
  if (!GetReceiverOffsetBounds(frame_bound, packet_bound)) {
    return {};
  }
  return (packet_bound - frame_bound) / 2;
}

ClockOffsetEstimatorImpl::KalmanFilter::KalmanFilter(
    Clock::duration process_noise,
    Clock::duration measurement_noise)
    : q_nanos_squared_(
          static_cast<double>(std::chrono::nanoseconds(process_noise).count()) *
          std::chrono::nanoseconds(process_noise).count()),
      r_nanos_squared_(
          static_cast<double>(
              std::chrono::nanoseconds(measurement_noise).count()) *
          std::chrono::nanoseconds(measurement_noise).count()) {}

void ClockOffsetEstimatorImpl::KalmanFilter::Update(
    Clock::duration measurement) {
  if (!has_estimate_) {
    // First measurement, initialize the state.
    estimated_latency_ = measurement;
    error_covariance_nanos_squared_ = r_nanos_squared_;
    has_estimate_ = true;
    return;
  }

  // --- 1. PREDICT ---
  // The predicted state is the same as the previous state.
  // The uncertainty (covariance) increases by the process noise.
  const double predicted_error_covariance =
      error_covariance_nanos_squared_ + q_nanos_squared_;

  // --- 2. UPDATE ---
  // Calculate Kalman Gain.
  const double kalman_gain = predicted_error_covariance /
                             (predicted_error_covariance + r_nanos_squared_);

  // Update the estimate with the new measurement.
  const double measurement_nanos =
      static_cast<double>(std::chrono::nanoseconds(measurement).count());
  const double estimate_nanos =
      static_cast<double>(std::chrono::nanoseconds(estimated_latency_).count());
  const double new_estimate_nanos =
      estimate_nanos + kalman_gain * (measurement_nanos - estimate_nanos);
  estimated_latency_ =
      std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds(
          static_cast<Clock::duration::rep>(new_estimate_nanos)));

  // Update the error covariance.
  error_covariance_nanos_squared_ =
      (1.0 - kalman_gain) * predicted_error_covariance;
}

ClockOffsetEstimatorImpl::BoundCalculator::BoundCalculator()
    : filter_(kProcessNoise, kMeasurementNoise) {}

ClockOffsetEstimatorImpl::BoundCalculator::BoundCalculator(
    BoundCalculator&&) noexcept = default;
ClockOffsetEstimatorImpl::BoundCalculator&
ClockOffsetEstimatorImpl::BoundCalculator::operator=(BoundCalculator&&) =
    default;
ClockOffsetEstimatorImpl::BoundCalculator::~BoundCalculator() = default;

void ClockOffsetEstimatorImpl::BoundCalculator::SetSent(RtpTimeTicks rtp,
                                                        uint16_t packet_id,
                                                        bool audio,
                                                        Clock::time_point t) {
  const uint64_t key = MakeEventKey(rtp, packet_id, audio);
  events_[key].first = t;
  CheckUpdate(key);
}

void ClockOffsetEstimatorImpl::BoundCalculator::SetReceived(
    RtpTimeTicks rtp,
    uint16_t packet_id,
    bool audio,
    Clock::time_point t) {
  const uint64_t key = MakeEventKey(rtp, packet_id, audio);
  events_[key].second = t;
  CheckUpdate(key);
}

void ClockOffsetEstimatorImpl::BoundCalculator::UpdateBound(
    Clock::time_point sent,
    Clock::time_point received) {
  filter_.Update(received - sent);
}

void ClockOffsetEstimatorImpl::BoundCalculator::CheckUpdate(uint64_t key) {
  const TimeTickPair& ticks = events_[key];
  if (ticks.first && ticks.second) {
    UpdateBound(ticks.first.value(), ticks.second.value());
    events_.erase(key);
    return;
  }

  if (events_.size() > kMaxEventTimesMapSize) {
    // We can make use of the fact that std::map sorts by key and just erase
    // the first entry.
    events_.erase(events_.begin());
  }
}

}  // namespace openscreen::cast
