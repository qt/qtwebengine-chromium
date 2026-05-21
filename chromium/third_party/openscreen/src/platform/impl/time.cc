// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/time.h"

#include <chrono>
#include <ctime>
#include <ratio>

#include "util/chrono_helpers.h"
#include "util/osp_logging.h"

using std::chrono::high_resolution_clock;
using std::chrono::steady_clock;
using std::chrono::system_clock;

namespace openscreen {

Clock::time_point Clock::now() noexcept {
  constexpr bool kSteadyIsGoodEnough =
      std::ratio_less_equal_v<steady_clock::period, Clock::kRequiredResolution>;
  constexpr bool kHighResIsGoodEnough =
      std::ratio_less_equal_v<high_resolution_clock::period,
                              Clock::kRequiredResolution> &&
      high_resolution_clock::is_steady;
  static_assert(kSteadyIsGoodEnough || kHighResIsGoodEnough,
                "No suitable default clock (steady + high enough resolution) "
                "on this platform");

  // 'if constexpr' guarantees compile-time branching.
  // We prefer steady_clock if it meets the requirements (usually cheaper).
  if constexpr (kSteadyIsGoodEnough) {
    return Clock::time_point(
        Clock::to_duration(steady_clock::now().time_since_epoch()));
  } else {
    return Clock::time_point(
        Clock::to_duration(high_resolution_clock::now().time_since_epoch()));
  }
}

std::chrono::seconds GetWallTimeSinceUnixEpoch() noexcept {
  // C++20 guarantees that system_clock uses the Unix Epoch (1970-01-01).
  // Use floor to truncate sub-second precision safely.
  return std::chrono::floor<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());
}

}  // namespace openscreen
