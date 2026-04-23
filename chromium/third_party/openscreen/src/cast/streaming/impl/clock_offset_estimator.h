// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_H_
#define CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_H_

#include <memory>
#include <optional>

#include "cast/streaming/impl/statistics_common.h"
#include "cast/streaming/public/statistics.h"
#include "platform/base/trivial_clock_traits.h"

namespace openscreen::cast {

// Used to estimate the offset between the Sender and Receiver clocks.
class ClockOffsetEstimator {
 public:
  static std::unique_ptr<ClockOffsetEstimator> Create();

  virtual ~ClockOffsetEstimator() {}

  // TODO(issuetracker.google.com/298085631): these should be in a separate
  // header, like Chrome's raw event subscriber pattern.
  // See: //media/cast/logging/raw_event_subscriber.h
  virtual void OnFrameEvent(const FrameEvent& frame_event) = 0;
  virtual void OnPacketEvent(const PacketEvent& packet_event) = 0;

  // Estimates the clock offset between the sender and the receiver.
  //
  // This is calculated by solving a system of two linear equations with two
  // unknowns: the clock offset and the network latency. The two equations are
  // derived from two round-trip time measurements.
  //
  // Let's define:
  //   - latency: the one-way network latency.
  //   - offset: the clock offset, where Clock_Receiver(t) = Clock_Sender(t) +
  //   offset.
  //
  // The estimator measures two bounds:
  //
  // 1. packet_bound (sender -> receiver):
  //    delta = TS_receiver - TS_sender
  //          = (TS_sender + latency + offset) - TS_sender
  //          = latency + offset
  //
  // 2. frame_bound (receiver -> sender):
  //    delta = TS_sender - TS_receiver
  //          = (TS_receiver + latency - offset) - TS_receiver
  //          = latency - offset
  //
  // The offset is then isolated by the formula:
  //    (packet_bound - frame_bound) / 2 =
  //     ( (latency + offset) - (latency - offset) ) / 2 =
  //     (2 * offset) / 2 = offset
  virtual std::optional<Clock::duration> GetEstimatedOffset() const = 0;

  // Estimates the one-way network latency.
  // This uses the same bounds as GetEstimatedOffset().
  //
  // The latency is isolated by the formula:
  //   (packet_bound + frame_bound) / 2 =
  //   ( (latency + offset) + (latency - offset) ) / 2 = (2 * latency) / 2 =
  //   latency
  virtual std::optional<Clock::duration> GetEstimatedLatency() const = 0;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_H_
