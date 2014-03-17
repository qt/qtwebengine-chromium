// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/rtcp/mock_rtcp_receiver_feedback.h"
#include "media/cast/rtcp/mock_rtcp_sender_feedback.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kReceiverSsrc = 0x40506;
static const std::string kCName("test@10.1.1.1");
static const uint32 kRtcpIntervalMs = 500;
static const int64 kStartMillisecond = GG_INT64_C(12345678900000);
static const int64 kAddedDelay = 123;
static const int64 kAddedShortDelay= 100;

class LocalRtcpTransport : public PacedPacketSender {
 public:
  explicit LocalRtcpTransport(scoped_refptr<CastEnvironment> cast_environment,
                              base::SimpleTestTickClock* testing_clock)
      : drop_packets_(false),
        short_delay_(false),
        testing_clock_(testing_clock) {}

  void SetRtcpReceiver(Rtcp* rtcp) { rtcp_ = rtcp; }

  void SetShortDelay() { short_delay_ = true; }

  void SetDropPackets(bool drop_packets) { drop_packets_ = drop_packets; }


  virtual bool SendRtcpPacket(const std::vector<uint8>& packet) OVERRIDE {
    if (short_delay_) {
      testing_clock_->Advance(
          base::TimeDelta::FromMilliseconds(kAddedShortDelay));
    } else {
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(kAddedDelay));
    }
    if (drop_packets_) return true;

    rtcp_->IncomingRtcpPacket(&(packet[0]), packet.size());
    return true;
  }

  virtual bool SendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE {
    return false;
  }

 private:
  bool drop_packets_;
  bool short_delay_;
  Rtcp* rtcp_;
  base::SimpleTestTickClock* testing_clock_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

class RtcpPeer : public Rtcp {
 public:
  RtcpPeer(scoped_refptr<CastEnvironment> cast_environment,
           RtcpSenderFeedback* sender_feedback,
           PacedPacketSender* const paced_packet_sender,
           RtpSenderStatistics* rtp_sender_statistics,
           RtpReceiverStatistics* rtp_receiver_statistics,
           RtcpMode rtcp_mode,
           const base::TimeDelta& rtcp_interval,
           uint32 local_ssrc,
           uint32 remote_ssrc,
           const std::string& c_name)
      : Rtcp(cast_environment,
             sender_feedback,
             paced_packet_sender,
             rtp_sender_statistics,
             rtp_receiver_statistics,
             rtcp_mode,
             rtcp_interval,
             local_ssrc,
             remote_ssrc,
             c_name) {
  }

  using Rtcp::CheckForWrapAround;
  using Rtcp::OnReceivedLipSyncInfo;
};

class RtcpTest : public ::testing::Test {
 protected:
  RtcpTest()
      : task_runner_(new test::FakeTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            GetDefaultCastLoggingConfig())),
        transport_(cast_environment_, &testing_clock_) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual ~RtcpTest() {}

  virtual void SetUp() {
    EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(0);
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  LocalRtcpTransport transport_;
  MockRtcpSenderFeedback mock_sender_feedback_;
};

TEST_F(RtcpTest, TimeToSend) {
  base::TimeTicks start_time;
  start_time += base::TimeDelta::FromMilliseconds(kStartMillisecond);
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kReceiverSsrc,
            kCName);
  transport_.SetRtcpReceiver(&rtcp);
  EXPECT_LE(start_time, rtcp.TimeToSendNextRtcpReport());
  EXPECT_GE(start_time + base::TimeDelta::FromMilliseconds(
                kRtcpIntervalMs * 3 / 2),
            rtcp.TimeToSendNextRtcpReport());
  base::TimeDelta delta = rtcp.TimeToSendNextRtcpReport() - start_time;
  testing_clock_.Advance(delta);
  EXPECT_EQ(testing_clock_.NowTicks(), rtcp.TimeToSendNextRtcpReport());
}

TEST_F(RtcpTest, BasicSenderReport) {
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kReceiverSsrc,
            kCName);
  transport_.SetRtcpReceiver(&rtcp);
  rtcp.SendRtcpFromRtpSender(NULL);
}

TEST_F(RtcpTest, BasicReceiverReport) {
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kReceiverSsrc,
            kCName);
  transport_.SetRtcpReceiver(&rtcp);
  rtcp.SendRtcpFromRtpReceiver(NULL, NULL);
}

TEST_F(RtcpTest, BasicCast) {
  EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(1);

  // Media receiver.
  Rtcp rtcp(cast_environment_,
            &mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpReducedSize,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            kSenderSsrc,
            kSenderSsrc,
            kCName);
  transport_.SetRtcpReceiver(&rtcp);
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  PacketIdSet missing_packets;
  cast_message.missing_frames_and_packets_[
      kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[kFrameIdWithLostPackets] =
      missing_packets;
  rtcp.SendRtcpFromRtpReceiver(&cast_message, NULL);
}

TEST_F(RtcpTest, RttReducedSizeRtcp) {
  // Media receiver.
  LocalRtcpTransport receiver_transport(cast_environment_, &testing_clock_);
  Rtcp rtcp_receiver(cast_environment_,
                     &mock_sender_feedback_,
                     &receiver_transport,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName);

  // Media sender.
  LocalRtcpTransport sender_transport(cast_environment_, &testing_clock_);
  Rtcp rtcp_sender(cast_environment_,
                   &mock_sender_feedback_,
                   &sender_transport,
                   NULL,
                   NULL,
                   kRtcpReducedSize,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   kSenderSsrc,
                   kReceiverSsrc,
                   kCName);

  receiver_transport.SetRtcpReceiver(&rtcp_sender);
  sender_transport.SetRtcpReceiver(&rtcp_receiver);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  rtcp_sender.SendRtcpFromRtpSender(NULL);
  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);
  rtcp_sender.SendRtcpFromRtpSender(NULL);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);
}

TEST_F(RtcpTest, Rtt) {
  // Media receiver.
  LocalRtcpTransport receiver_transport(cast_environment_, &testing_clock_);
  Rtcp rtcp_receiver(cast_environment_,
                     &mock_sender_feedback_,
                     &receiver_transport,
                     NULL,
                     NULL,
                     kRtcpCompound,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName);

  // Media sender.
  LocalRtcpTransport sender_transport(cast_environment_, &testing_clock_);
  Rtcp rtcp_sender(cast_environment_,
                   &mock_sender_feedback_,
                   &sender_transport,
                   NULL,
                   NULL,
                   kRtcpCompound,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   kSenderSsrc,
                   kReceiverSsrc,
                   kCName);

  receiver_transport.SetRtcpReceiver(&rtcp_sender);
  sender_transport.SetRtcpReceiver(&rtcp_receiver);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  rtcp_sender.SendRtcpFromRtpSender(NULL);
  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  rtcp_sender.SendRtcpFromRtpSender(NULL);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  receiver_transport.SetShortDelay();
  sender_transport.SetShortDelay();
  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR((kAddedShortDelay + 3 * kAddedDelay) / 2,
               avg_rtt.InMilliseconds(),
               1);
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  rtcp_sender.SendRtcpFromRtpSender(NULL);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR((2 * kAddedShortDelay + 2 * kAddedDelay) / 2,
               avg_rtt.InMilliseconds(),
               1);
  EXPECT_NEAR(2 * kAddedShortDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedShortDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedShortDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);
}

TEST_F(RtcpTest, RttWithPacketLoss) {
  // Media receiver.
  LocalRtcpTransport receiver_transport(cast_environment_, &testing_clock_);
  Rtcp rtcp_receiver(cast_environment_,
                     &mock_sender_feedback_,
                     &receiver_transport,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kSenderSsrc,
                     kReceiverSsrc,
                     kCName);

  // Media sender.
  LocalRtcpTransport sender_transport(cast_environment_, &testing_clock_);
  Rtcp rtcp_sender(cast_environment_,
                   &mock_sender_feedback_,
                   &sender_transport,
                   NULL,
                   NULL,
                   kRtcpReducedSize,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   kReceiverSsrc,
                   kSenderSsrc,
                   kCName);

  receiver_transport.SetRtcpReceiver(&rtcp_sender);
  sender_transport.SetRtcpReceiver(&rtcp_receiver);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  rtcp_sender.SendRtcpFromRtpSender(NULL);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  receiver_transport.SetShortDelay();
  sender_transport.SetShortDelay();
  receiver_transport.SetDropPackets(true);

  rtcp_receiver.SendRtcpFromRtpReceiver(NULL, NULL);
  rtcp_sender.SendRtcpFromRtpSender(NULL);

  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, rtt.InMilliseconds(), 1);
}

TEST_F(RtcpTest, NtpAndTime) {
  const int64 kSecondsbetweenYear1900and2010 = GG_INT64_C(40176 * 24 * 60 * 60);
  const int64 kSecondsbetweenYear1900and2030 = GG_INT64_C(47481 * 24 * 60 * 60);

  uint32 ntp_seconds_1 = 0;
  uint32 ntp_fractions_1 = 0;
  base::TimeTicks input_time = base::TimeTicks::Now();
  ConvertTimeTicksToNtp(input_time, &ntp_seconds_1, &ntp_fractions_1);

  // Verify absolute value.
  EXPECT_GT(ntp_seconds_1, kSecondsbetweenYear1900and2010);
  EXPECT_LT(ntp_seconds_1, kSecondsbetweenYear1900and2030);

  base::TimeTicks out_1 = ConvertNtpToTimeTicks(ntp_seconds_1, ntp_fractions_1);
  EXPECT_EQ(input_time, out_1);  // Verify inverse.

  base::TimeDelta time_delta = base::TimeDelta::FromMilliseconds(1000);
  input_time += time_delta;

  uint32 ntp_seconds_2 = 0;
  uint32 ntp_fractions_2 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_2, &ntp_fractions_2);
  base::TimeTicks out_2 = ConvertNtpToTimeTicks(ntp_seconds_2, ntp_fractions_2);
  EXPECT_EQ(input_time, out_2);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_2 - out_1), time_delta);
  EXPECT_EQ((ntp_seconds_2 - ntp_seconds_1), GG_UINT32_C(1));
  EXPECT_NEAR(ntp_fractions_2, ntp_fractions_1, 1);

  time_delta = base::TimeDelta::FromMilliseconds(500);
  input_time += time_delta;

  uint32 ntp_seconds_3 = 0;
  uint32 ntp_fractions_3 = 0;

  ConvertTimeTicksToNtp(input_time, &ntp_seconds_3, &ntp_fractions_3);
  base::TimeTicks out_3 = ConvertNtpToTimeTicks(ntp_seconds_3, ntp_fractions_3);
  EXPECT_EQ(input_time, out_3);  // Verify inverse.

  // Verify delta.
  EXPECT_EQ((out_3 - out_2), time_delta);
  EXPECT_NEAR((ntp_fractions_3 - ntp_fractions_2), 0xffffffff / 2, 1);
}

TEST_F(RtcpTest, WrapAround) {
  RtcpPeer rtcp_peer(cast_environment_,
                     &mock_sender_feedback_,
                     NULL,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName);
  uint32 new_timestamp = 0;
  uint32 old_timestamp = 0;
  EXPECT_EQ(0, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
  new_timestamp = 1234567890;
  old_timestamp = 1234567000;
  EXPECT_EQ(0, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
  new_timestamp = 1234567000;
  old_timestamp = 1234567890;
  EXPECT_EQ(0, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
  new_timestamp = 123;
  old_timestamp = 4234567890u;
  EXPECT_EQ(1, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
  new_timestamp = 4234567890u;
  old_timestamp = 123;
  EXPECT_EQ(-1, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
}

TEST_F(RtcpTest, RtpTimestampInSenderTime) {
  RtcpPeer rtcp_peer(cast_environment_,
                     &mock_sender_feedback_,
                     NULL,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     kReceiverSsrc,
                     kSenderSsrc,
                     kCName);
  int frequency = 32000;
  uint32 rtp_timestamp = 64000;
  base::TimeTicks rtp_timestamp_in_ticks;

  // Test fail before we get a OnReceivedLipSyncInfo.
  EXPECT_FALSE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                   &rtp_timestamp_in_ticks));

  uint32 ntp_seconds = 0;
  uint32 ntp_fractions = 0;
  uint64 input_time_us = 12345678901000LL;
  base::TimeTicks input_time;
  input_time += base::TimeDelta::FromMicroseconds(input_time_us);

  // Test exact match.
  ConvertTimeTicksToNtp(input_time, &ntp_seconds, &ntp_fractions);
  rtcp_peer.OnReceivedLipSyncInfo(rtp_timestamp, ntp_seconds, ntp_fractions);
  EXPECT_TRUE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                 &rtp_timestamp_in_ticks));
  EXPECT_EQ(input_time, rtp_timestamp_in_ticks);

  // Test older rtp_timestamp.
  rtp_timestamp = 32000;
  EXPECT_TRUE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                 &rtp_timestamp_in_ticks));
  EXPECT_EQ(input_time - base::TimeDelta::FromMilliseconds(1000),
            rtp_timestamp_in_ticks);

  // Test older rtp_timestamp with wrap.
  rtp_timestamp = 4294903296u;
  EXPECT_TRUE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                 &rtp_timestamp_in_ticks));
  EXPECT_EQ(input_time - base::TimeDelta::FromMilliseconds(4000),
            rtp_timestamp_in_ticks);

  // Test newer rtp_timestamp.
  rtp_timestamp = 128000;
  EXPECT_TRUE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                 &rtp_timestamp_in_ticks));
  EXPECT_EQ(input_time + base::TimeDelta::FromMilliseconds(2000),
            rtp_timestamp_in_ticks);

  // Test newer rtp_timestamp with wrap.
  rtp_timestamp = 4294903296u;
  rtcp_peer.OnReceivedLipSyncInfo(rtp_timestamp, ntp_seconds, ntp_fractions);
  rtp_timestamp = 64000;
  EXPECT_TRUE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                 &rtp_timestamp_in_ticks));
  EXPECT_EQ(input_time + base::TimeDelta::FromMilliseconds(4000),
            rtp_timestamp_in_ticks);
}

}  // namespace cast
}  // namespace media
