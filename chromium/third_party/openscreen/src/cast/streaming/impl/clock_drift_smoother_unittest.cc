// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/clock_drift_smoother.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

#include "platform/base/trivial_clock_traits.h"
#include "testing/util/chrono_test_helpers.h"

namespace openscreen::cast {

TEST(ClockDriftSmootherTest, InitializesToNullopt) {
  ClockDriftSmoother smoother(seconds(1));
  EXPECT_EQ(smoother.Current(), std::nullopt);
}

TEST(ClockDriftSmootherTest, ResetSetsOffset) {
  ClockDriftSmoother smoother(seconds(1));
  const Clock::time_point now = Clock::now();
  const Clock::duration offset = milliseconds(100);
  smoother.Reset(now, offset);
  ASSERT_TRUE(smoother.Current().has_value());
  EXPECT_EQ(smoother.Current().value(), offset);
}

TEST(ClockDriftSmootherTest, BasicSmoothing) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += seconds(1);
  smoother.Update(now, milliseconds(200));

  // After 1 time constant, the value should be a weighted average.
  // alpha = 1 - exp(-1/1) = 1 - exp(-1) = 0.632
  // new_value = 0.632 * 200 + (1 - 0.632) * 100 = 126.4 + 36.8 = 163.2
  constexpr auto kExpectedOffset = milliseconds(163);
  ASSERT_TRUE(smoother.Current().has_value());
  ExpectDurationNear(smoother.Current().value(), kExpectedOffset,
                     milliseconds(1));
}

TEST(ClockDriftSmootherTest, TimeProgression) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += milliseconds(100);
  smoother.Update(now, milliseconds(1000));
  ASSERT_TRUE(smoother.Current().has_value());
  const Clock::duration first_update = smoother.Current().value();

  now += seconds(2);
  smoother.Update(now, milliseconds(1000));
  ASSERT_TRUE(smoother.Current().has_value());
  const Clock::duration second_update = smoother.Current().value();

  // The second update should be closer to the target because more time has
  // passed.
  EXPECT_GT(second_update, first_update);
}

TEST(ClockDriftSmootherTest, HandlesZeroOffset) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += seconds(1);
  smoother.Update(now, Clock::duration::zero());
  ASSERT_TRUE(smoother.Current().has_value());
  EXPECT_LT(smoother.Current().value(), milliseconds(100));
}

TEST(ClockDriftSmootherTest, HandlesNegativeOffset) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += seconds(1);
  smoother.Update(now, milliseconds(-100));
  ASSERT_TRUE(smoother.Current().has_value());
  EXPECT_LT(smoother.Current().value(), milliseconds(100));
}

TEST(ClockDriftSmootherTest, StabilityWithJitter) {
  ClockDriftSmoother smoother(seconds(5));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  for (int i = 0; i < 100; ++i) {
    now += milliseconds(100);
    const auto offset = (i % 2 == 0) ? milliseconds(105) : milliseconds(95);
    smoother.Update(now, offset);
  }

  // After many updates, the smoother should converge to the average, despite
  // the jitter.
  ASSERT_TRUE(smoother.Current().has_value());
  ExpectDurationNear(smoother.Current().value(), milliseconds(100),
                     milliseconds(5));
}

TEST(ClockDriftSmootherTest, ConvergenceAfterSuddenJump) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += seconds(1);
  smoother.Update(now, milliseconds(1000));

  // After a jump, the value should be closer to the new value.
  ASSERT_TRUE(smoother.Current().has_value());
  EXPECT_GT(smoother.Current().value(), milliseconds(100));
  EXPECT_LT(smoother.Current().value(), milliseconds(1000));
}

TEST(ClockDriftSmootherTest, UpdateWithZeroElapsedTime) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));
  ASSERT_TRUE(smoother.Current().has_value());
  const Clock::duration initial_value = smoother.Current().value();

  smoother.Update(now, milliseconds(1000));

  // With zero elapsed time, the value should not change.
  ASSERT_TRUE(smoother.Current().has_value());
  EXPECT_EQ(smoother.Current().value(), initial_value);
}

TEST(ClockDriftSmootherTest, HeavyWeightingAfterLongGap) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += seconds(100);
  smoother.Update(now, milliseconds(1000));

  // After a long gap, the new value should be very close to the new
  // measurement.
  ASSERT_TRUE(smoother.Current().has_value());
  ExpectDurationNear(smoother.Current().value(), milliseconds(1000),
                     milliseconds(1));
}

TEST(ClockDriftSmootherTest, Responsiveness) {
  ClockDriftSmoother smoother(seconds(1));
  Clock::time_point now = Clock::now();
  smoother.Reset(now, milliseconds(100));

  now += milliseconds(500);
  smoother.Update(now, milliseconds(200));

  // After 500ms, the value should be a weighted average.
  // alpha = 1 - exp(-500/1000) = 1 - exp(-0.5) = 0.393
  // new_value = 0.393 * 200 + (1 - 0.393) * 100 = 78.6 + 60.7 = 139.3
  constexpr auto kExpectedOffset = milliseconds(139);
  ASSERT_TRUE(smoother.Current().has_value());
  ExpectDurationNear(smoother.Current().value(), kExpectedOffset,
                     milliseconds(1));
}

}  // namespace openscreen::cast
