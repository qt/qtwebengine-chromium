// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/clock_drift_smoother.h"

#include <cmath>

#include "util/chrono_helpers.h"
#include "util/osp_logging.h"
#include "util/saturate_cast.h"

namespace openscreen::cast {
namespace {

constexpr Clock::time_point kNullTime = Clock::time_point::min();
}

using clock_operators::operator<<;

ClockDriftSmoother::ClockDriftSmoother(Clock::duration time_constant)
    : time_constant_(time_constant),
      last_update_time_(kNullTime),
      estimated_tick_offset_(0.0) {
  OSP_CHECK(time_constant_ > decltype(time_constant_)::zero());
}

ClockDriftSmoother::~ClockDriftSmoother() = default;

std::optional<Clock::duration> ClockDriftSmoother::Current() const {
  if (last_update_time_ == kNullTime) {
    return std::nullopt;
  }
  return Clock::duration(
      rounded_saturate_cast<Clock::duration::rep>(estimated_tick_offset_));
}

void ClockDriftSmoother::Reset(Clock::time_point now,
                               Clock::duration measured_offset) {
  OSP_CHECK_NE(now, kNullTime);
  last_update_time_ = now;
  estimated_tick_offset_ = static_cast<double>(measured_offset.count());
}

void ClockDriftSmoother::Update(Clock::time_point now,
                                Clock::duration measured_offset) {
  OSP_CHECK_NE(now, kNullTime);
  if (last_update_time_ == kNullTime) {
    Reset(now, measured_offset);
    return;
  }

  if (now < last_update_time_) {
    // `now` is not monotonically non-decreasing.
    OSP_NOTREACHED();
  }

  const double elapsed_ticks =
      static_cast<double>((now - last_update_time_).count());
  last_update_time_ = now;

  // This is a standard exponential moving average (EMA) filter.
  // The alpha value is calculated such that the filter has the desired time
  // constant.
  const double alpha = 1.0 - std::exp(-elapsed_ticks / time_constant_.count());
  estimated_tick_offset_ =
      alpha * static_cast<double>(measured_offset.count()) +
      (1.0 - alpha) * estimated_tick_offset_;

  const auto current = Current();
  OSP_VLOG << "Local clock is ahead of the remote clock by: measured = "
           << measured_offset << ", "
           << "filtered = " << (current ? ToString(*current) : "null") << ".";
}

// static
constexpr std::chrono::seconds ClockDriftSmoother::kDefaultTimeConstant;

}  // namespace openscreen::cast
