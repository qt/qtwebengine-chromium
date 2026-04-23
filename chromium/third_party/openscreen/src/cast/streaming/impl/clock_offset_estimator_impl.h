// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_IMPL_H_
#define CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <utility>

#include "cast/streaming/impl/clock_offset_estimator.h"
#include "cast/streaming/impl/statistics_common.h"
#include "cast/streaming/rtp_time.h"
#include "platform/base/trivial_clock_traits.h"
#include "util/chrono_helpers.h"

namespace openscreen::cast {

// This implementation listens to two pairs of events:
//     1. FrameAckSent / FrameAckReceived (receiver->sender)
//     2. PacketSentToNetwork / PacketReceived (sender->receiver)
//
// There is a causal relationship between these events in that these events
// must happen in order. This class obtains the lower and upper bounds for
// the offset by taking the difference of timestamps.
class ClockOffsetEstimatorImpl final : public ClockOffsetEstimator {
 public:
  ClockOffsetEstimatorImpl();
  ClockOffsetEstimatorImpl(ClockOffsetEstimatorImpl&&) noexcept;
  ClockOffsetEstimatorImpl(const ClockOffsetEstimatorImpl&) = delete;
  ClockOffsetEstimatorImpl& operator=(ClockOffsetEstimatorImpl&&);
  ClockOffsetEstimatorImpl& operator=(const ClockOffsetEstimatorImpl&) = delete;
  ~ClockOffsetEstimatorImpl() final;

  void OnFrameEvent(const FrameEvent& frame_event) final;
  void OnPacketEvent(const PacketEvent& packet_event) final;

  bool GetReceiverOffsetBounds(Clock::duration& frame_bound,
                               Clock::duration& packet_bound) const;

  // ClockOffsetEstimator overrides.
  std::optional<Clock::duration> GetEstimatedOffset() const final;
  std::optional<Clock::duration> GetEstimatedLatency() const final;

 private:
  // These values are chosen based on common network conditions.
  //
  // Q (process_noise): We expect latency to drift by up to 5ms between
  // measurements.
  static constexpr Clock::duration kProcessNoise = milliseconds(5);
  //
  // R (measurement_noise): We expect jitter of up to 30ms.
  static constexpr Clock::duration kMeasurementNoise = milliseconds(30);

  // Simplified 1D Kalman Filter for latency estimation.
  class KalmanFilter {
   public:
    // Q: process_noise - Represents the expected variance of the latency
    //    itself between time steps. A higher value makes the filter adapt
    //    more quickly to real changes in latency.
    // R: measurement_noise - Represents the variance of the measurement
    //    noise (jitter). A higher value makes the filter trust its own
    //    prediction more and smooth out noisy measurements.
    KalmanFilter(Clock::duration process_noise,
                 Clock::duration measurement_noise);
    KalmanFilter(KalmanFilter&&) noexcept = default;
    KalmanFilter& operator=(KalmanFilter&&) = default;

    Clock::duration GetEstimate() const { return estimated_latency_; }
    bool HasEstimate() const { return has_estimate_; }
    void Update(Clock::duration measurement);

   private:
    double q_nanos_squared_;
    double r_nanos_squared_;

    bool has_estimate_ = false;
    Clock::duration estimated_latency_{};
    double error_covariance_nanos_squared_ = 0.0;
  };

  // This helper uses the difference between sent and received event
  // to calculate an upper bound on the difference between the clocks
  // on the sender and receiver. Note that this difference can take
  // very large positive or negative values, but the smaller value is
  // always the better estimate, since a receive event cannot possibly
  // happen before a send event.  Note that we use this to calculate
  // both upper and lower bounds by reversing the sender/receiver
  // relationship.
  class BoundCalculator {
   public:
    typedef std::pair<std::optional<Clock::time_point>,
                      std::optional<Clock::time_point>>
        TimeTickPair;
    typedef std::map<uint64_t, TimeTickPair> EventMap;

    BoundCalculator();
    BoundCalculator(BoundCalculator&&) noexcept;
    BoundCalculator(const BoundCalculator&) = delete;
    BoundCalculator& operator=(BoundCalculator&&);
    BoundCalculator& operator=(const BoundCalculator&) = delete;
    ~BoundCalculator();
    bool has_bound() const { return filter_.HasEstimate(); }
    Clock::duration bound() const { return filter_.GetEstimate(); }

    void SetSent(RtpTimeTicks rtp,
                 uint16_t packet_id,
                 bool audio,
                 Clock::time_point t);

    void SetReceived(RtpTimeTicks rtp,
                     uint16_t packet_id,
                     bool audio,
                     Clock::time_point t);

   private:
    void UpdateBound(Clock::time_point a, Clock::time_point b);
    void CheckUpdate(uint64_t key);

   private:
    EventMap events_;
    KalmanFilter filter_;
  };

  // Fixed size storage to store event times for recent frames and packets.
  BoundCalculator packet_bound_;
  BoundCalculator frame_bound_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_IMPL_H_
