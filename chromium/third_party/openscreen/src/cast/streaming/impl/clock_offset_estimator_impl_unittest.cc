// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/clock_offset_estimator_impl.h"

#include <stdint.h>

#include <memory>
#include <random>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/trivial_clock_traits.h"
#include "platform/test/fake_clock.h"
#include "testing/util/chrono_test_helpers.h"
#include "util/chrono_helpers.h"

namespace openscreen::cast {

namespace {

FrameEvent CreateFrameEvent(StatisticsEvent::Type type,
                            Clock::time_point timestamp,
                            FrameId frame_id,
                            RtpTimeTicks rtp_timestamp,
                            StatisticsEvent::MediaType media_type) {
  FrameEvent event;
  event.type = type;
  event.media_type = media_type;
  event.timestamp = timestamp;
  event.frame_id = frame_id;
  event.rtp_timestamp = rtp_timestamp;
  return event;
}

PacketEvent CreatePacketEvent(StatisticsEvent::Type type,
                              Clock::time_point timestamp,
                              FrameId frame_id,
                              RtpTimeTicks rtp_timestamp,
                              StatisticsEvent::MediaType media_type) {
  PacketEvent event;
  event.type = type;
  event.media_type = media_type;
  event.timestamp = timestamp;
  event.frame_id = frame_id;
  event.rtp_timestamp = rtp_timestamp;
  event.packet_id = 0;
  event.max_packet_id = 1;
  event.size = 1500;
  return event;
}

}  // namespace

class ClockOffsetEstimatorImplTest : public ::testing::Test {
 public:
  ClockOffsetEstimatorImplTest()
      : sender_time_(Clock::now()),
        receiver_clock_(Clock::now()),
        estimator_() {}

  ~ClockOffsetEstimatorImplTest() override = default;

 protected:
  void AdvanceClocks(Clock::duration time) {
    receiver_clock_.Advance(time);
    sender_time_ += time;
  }

  void SendAndReceiveEvents(FrameId frame_id,
                            RtpTimeTicks rtp,
                            milliseconds network_latency,
                            StatisticsEvent::MediaType media_type) {
    estimator_.OnFrameEvent(
        CreateFrameEvent(StatisticsEvent::Type::kFrameEncoded, sender_time_,
                         frame_id, rtp, media_type));
    estimator_.OnPacketEvent(
        CreatePacketEvent(StatisticsEvent::Type::kPacketSentToNetwork,
                          sender_time_, frame_id, rtp, media_type));
    AdvanceClocks(network_latency);
    estimator_.OnPacketEvent(
        CreatePacketEvent(StatisticsEvent::Type::kPacketReceived,
                          receiver_clock_.now(), frame_id, rtp, media_type));
    estimator_.OnFrameEvent(
        CreateFrameEvent(StatisticsEvent::Type::kFrameAckSent,
                         receiver_clock_.now(), frame_id, rtp, media_type));
    AdvanceClocks(network_latency);
    estimator_.OnFrameEvent(
        CreateFrameEvent(StatisticsEvent::Type::kFrameAckReceived, sender_time_,
                         frame_id, rtp, media_type));
  }

  Clock::time_point sender_time_;
  FakeClock receiver_clock_;
  ClockOffsetEstimatorImpl estimator_;
};

TEST_F(ClockOffsetEstimatorImplTest, ReturnsNulloptWhenNoEvents) {
  EXPECT_FALSE(estimator_.GetEstimatedOffset());
  EXPECT_FALSE(estimator_.GetEstimatedLatency());
}

TEST_F(ClockOffsetEstimatorImplTest, CalculatesOffsetAndLatencyAfterOneTrip) {
  constexpr milliseconds kTrueOffset(100);
  constexpr milliseconds kNetworkLatency(10);
  receiver_clock_.Advance(kTrueOffset);

  SendAndReceiveEvents(FrameId::first(), RtpTimeTicks(), kNetworkLatency,
                       StatisticsEvent::MediaType::kVideo);

  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_EQ(kTrueOffset, to_milliseconds(*estimator_.GetEstimatedOffset()));
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_EQ(kNetworkLatency,
            to_milliseconds(*estimator_.GetEstimatedLatency()));
}

TEST_F(ClockOffsetEstimatorImplTest,
       CalculatesOffsetAndLatencyWithOutOfOrderEvents) {
  constexpr milliseconds kTrueOffset(100);
  receiver_clock_.Advance(kTrueOffset);

  const RtpTimeTicks rtp_timestamp;
  const FrameId frame_id = FrameId::first();

  AdvanceClocks(milliseconds(20));
  estimator_.OnFrameEvent(CreateFrameEvent(
      StatisticsEvent::Type::kFrameEncoded, sender_time_, frame_id,
      rtp_timestamp, StatisticsEvent::MediaType::kVideo));
  estimator_.OnPacketEvent(CreatePacketEvent(
      StatisticsEvent::Type::kPacketSentToNetwork, sender_time_, frame_id,
      rtp_timestamp, StatisticsEvent::MediaType::kVideo));

  AdvanceClocks(milliseconds(10));
  const auto event_b_time = receiver_clock_.now();
  AdvanceClocks(milliseconds(30));
  const auto event_c_time = sender_time_;

  estimator_.OnFrameEvent(CreateFrameEvent(
      StatisticsEvent::Type::kFrameAckReceived, event_c_time, frame_id,
      rtp_timestamp, StatisticsEvent::MediaType::kVideo));
  estimator_.OnPacketEvent(CreatePacketEvent(
      StatisticsEvent::Type::kPacketReceived, event_b_time, frame_id,
      rtp_timestamp, StatisticsEvent::MediaType::kVideo));
  estimator_.OnFrameEvent(CreateFrameEvent(
      StatisticsEvent::Type::kFrameAckSent, event_b_time, frame_id,
      rtp_timestamp, StatisticsEvent::MediaType::kVideo));

  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_EQ(milliseconds(90),
            to_milliseconds(*estimator_.GetEstimatedOffset()));
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_EQ(milliseconds(20),
            to_milliseconds(*estimator_.GetEstimatedLatency()));
}

TEST_F(ClockOffsetEstimatorImplTest,
       UpdatesOffsetAndLatencyAfterMultipleRoundTrips) {
  constexpr milliseconds kTrueOffset(100);
  constexpr milliseconds kNetworkLatency(5);
  receiver_clock_.Advance(kTrueOffset);

  SendAndReceiveEvents(FrameId::first(), RtpTimeTicks(), kNetworkLatency,
                       StatisticsEvent::MediaType::kVideo);
  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_EQ(kTrueOffset, to_milliseconds(*estimator_.GetEstimatedOffset()));
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_EQ(kNetworkLatency,
            to_milliseconds(*estimator_.GetEstimatedLatency()));

  AdvanceClocks(milliseconds(100));
  SendAndReceiveEvents(FrameId::first() + 1,
                       RtpTimeTicks() + RtpTimeDelta::FromTicks(90),
                       kNetworkLatency, StatisticsEvent::MediaType::kVideo);
  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_EQ(kTrueOffset, to_milliseconds(*estimator_.GetEstimatedOffset()));
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_EQ(kNetworkLatency,
            to_milliseconds(*estimator_.GetEstimatedLatency()));
}

TEST_F(ClockOffsetEstimatorImplTest,
       CalculatesLatencyWithVaryingNetworkConditions) {
  constexpr milliseconds kTrueOffset(100);
  receiver_clock_.Advance(kTrueOffset);

  // Start with a baseline latency.
  constexpr milliseconds kBaselineLatency(10);
  SendAndReceiveEvents(FrameId::first(), RtpTimeTicks(), kBaselineLatency,
                       StatisticsEvent::MediaType::kVideo);
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_THAT(to_milliseconds(*estimator_.GetEstimatedLatency()),
              EqualsDuration(kBaselineLatency));

  // Test with zero latency.
  constexpr milliseconds kZeroLatency(0);
  for (int i = 0; i < 10; ++i) {
    SendAndReceiveEvents(FrameId::first() + 1 + i,
                         RtpTimeTicks() + RtpTimeDelta::FromTicks(90 * (i + 1)),
                         kZeroLatency, StatisticsEvent::MediaType::kVideo);
  }
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kZeroLatency.count(), 5);

  // Test with high latency.
  constexpr milliseconds kHighLatency(100);
  for (int i = 0; i < 10; ++i) {
    SendAndReceiveEvents(
        FrameId::first() + 11 + i,
        RtpTimeTicks() + RtpTimeDelta::FromTicks(90 * (i + 11)), kHighLatency,
        StatisticsEvent::MediaType::kVideo);
  }
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kHighLatency.count(), 20);
}

TEST_F(ClockOffsetEstimatorImplTest, ConvergesToMeanLatencyWithJitter) {
  constexpr milliseconds kTrueOffset(100);
  constexpr milliseconds kMeanNetworkLatency(50);
  constexpr milliseconds kJitter(40);
  receiver_clock_.Advance(kTrueOffset);

  std::minstd_rand prng;
  std::uniform_int_distribution<int> jitter_dist(-kJitter.count(),
                                                 kJitter.count());

  for (int i = 0; i < 50; ++i) {
    const milliseconds jitter = milliseconds(jitter_dist(prng));
    const milliseconds network_latency = kMeanNetworkLatency + jitter;
    SendAndReceiveEvents(FrameId::first() + i,
                         RtpTimeTicks() + RtpTimeDelta::FromTicks(i * 90),
                         network_latency, StatisticsEvent::MediaType::kVideo);
  }

  // After many measurements, the estimate should be very close to the mean.
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kMeanNetworkLatency.count(), 20);
}

TEST_F(ClockOffsetEstimatorImplTest, TracksClockDrift) {
  constexpr milliseconds kInitialOffset(100);
  constexpr milliseconds kNetworkLatency(10);
  constexpr microseconds kDriftPerFrame(100);
  constexpr int kNumFrames = 50;

  receiver_clock_.Advance(kInitialOffset);

  for (int i = 0; i < kNumFrames; ++i) {
    SendAndReceiveEvents(FrameId::first() + i,
                         RtpTimeTicks() + RtpTimeDelta::FromTicks(i * 90),
                         kNetworkLatency, StatisticsEvent::MediaType::kVideo);
    receiver_clock_.Advance(kDriftPerFrame);
  }

  const milliseconds kFinalOffset =
      kInitialOffset +
      std::chrono::duration_cast<milliseconds>(kDriftPerFrame * kNumFrames);
  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedOffset()).count(),
              kFinalOffset.count(), 5);
}

TEST_F(ClockOffsetEstimatorImplTest, IsStableWithPacketLoss) {
  constexpr milliseconds kTrueOffset(100);
  constexpr milliseconds kNetworkLatency(10);
  receiver_clock_.Advance(kTrueOffset);

  // Simulate a burst of 1000 lost packets.
  for (int i = 0; i < 1000; ++i) {
    estimator_.OnPacketEvent(CreatePacketEvent(
        StatisticsEvent::Type::kPacketSentToNetwork, sender_time_,
        FrameId::first() + i, RtpTimeTicks() + RtpTimeDelta::FromTicks(i * 90),
        StatisticsEvent::MediaType::kVideo));
    AdvanceClocks(milliseconds(1));
  }

  // Send a final, successful round-trip.
  SendAndReceiveEvents(FrameId::first() + 1000,
                       RtpTimeTicks() + RtpTimeDelta::FromTicks(1000 * 90),
                       kNetworkLatency, StatisticsEvent::MediaType::kVideo);

  // The estimator should still produce a valid estimate.
  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedOffset()).count(),
              kTrueOffset.count(), 5);
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kNetworkLatency.count(), 5);
}

TEST_F(ClockOffsetEstimatorImplTest, RecoversFromLatencySpike) {
  constexpr milliseconds kTrueOffset(100);
  constexpr milliseconds kBaselineLatency(10);
  receiver_clock_.Advance(kTrueOffset);

  // Establish a baseline estimate.
  for (int i = 0; i < 10; ++i) {
    SendAndReceiveEvents(FrameId::first() + i,
                         RtpTimeTicks() + RtpTimeDelta::FromTicks(i * 90),
                         kBaselineLatency, StatisticsEvent::MediaType::kVideo);
  }
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kBaselineLatency.count(), 5);

  // Introduce a large latency spike.
  const auto kSpikeLatency = milliseconds(500);
  SendAndReceiveEvents(FrameId::first() + 10,
                       RtpTimeTicks() + RtpTimeDelta::FromTicks(10 * 90),
                       kSpikeLatency, StatisticsEvent::MediaType::kVideo);
  ASSERT_TRUE(estimator_.GetEstimatedLatency());

  // Ensure that there is a significant jump in the estimate, but not all the
  // way to the entire spike value.
  EXPECT_GT(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
            kBaselineLatency.count() * 5);
  EXPECT_LT(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
            kSpikeLatency.count() / 2);

  // After several more measurements, the estimate should recover.
  for (int i = 11; i < 25; ++i) {
    SendAndReceiveEvents(FrameId::first() + i,
                         RtpTimeTicks() + RtpTimeDelta::FromTicks(i * 90),
                         kBaselineLatency, StatisticsEvent::MediaType::kVideo);
  }
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kBaselineLatency.count(), 10);
}

TEST_F(ClockOffsetEstimatorImplTest, HandlesMixedAudioAndVideoEvents) {
  constexpr milliseconds kTrueOffset(50);
  constexpr milliseconds kNetworkLatency(20);
  receiver_clock_.Advance(kTrueOffset);

  // Send a video frame and check the estimate.
  SendAndReceiveEvents(FrameId::first(), RtpTimeTicks(), kNetworkLatency,
                       StatisticsEvent::MediaType::kVideo);
  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedOffset()).count(),
              kTrueOffset.count(), 5);
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kNetworkLatency.count(), 5);

  // Now send an audio frame and check that the estimate is updated.
  SendAndReceiveEvents(FrameId::first() + 1,
                       RtpTimeTicks() + RtpTimeDelta::FromTicks(90),
                       kNetworkLatency, StatisticsEvent::MediaType::kAudio);
  ASSERT_TRUE(estimator_.GetEstimatedOffset());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedOffset()).count(),
              kTrueOffset.count(), 5);
  ASSERT_TRUE(estimator_.GetEstimatedLatency());
  EXPECT_NEAR(to_milliseconds(*estimator_.GetEstimatedLatency()).count(),
              kNetworkLatency.count(), 5);
}

}  // namespace openscreen::cast
