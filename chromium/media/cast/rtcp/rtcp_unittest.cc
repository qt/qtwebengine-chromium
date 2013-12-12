// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_defines.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/rtcp/mock_rtcp_receiver_feedback.h"
#include "media/cast/rtcp/mock_rtcp_sender_feedback.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kReceiverSsrc = 0x40506;
static const uint32 kUnknownSsrc = 0xDEAD;
static const std::string kCName("test@10.1.1.1");
static const uint32 kRtcpIntervalMs = 500;
static const int64 kStartMillisecond = 123456789;
static const int64 kAddedDelay = 123;
static const int64 kAddedShortDelay= 100;

class LocalRtcpTransport : public PacedPacketSender {
 public:
  explicit LocalRtcpTransport(base::SimpleTestTickClock* testing_clock)
      : short_delay_(false),
        testing_clock_(testing_clock) {}

  void SetRtcpReceiver(Rtcp* rtcp)  { rtcp_ = rtcp; }

  void SetShortDelay()  { short_delay_ = true; }

  virtual bool SendRtcpPacket(const std::vector<uint8>& packet) OVERRIDE {
    if (short_delay_) {
      testing_clock_->Advance(
          base::TimeDelta::FromMilliseconds(kAddedShortDelay));
    } else {
      testing_clock_->Advance(base::TimeDelta::FromMilliseconds(kAddedDelay));
    }
    rtcp_->IncomingRtcpPacket(&(packet[0]), packet.size());
    return true;
  }

  virtual bool SendPacket(const std::vector<uint8>& packet,
                          int num_of_packets) OVERRIDE {
    return false;
  }

  virtual bool ResendPacket(const std::vector<uint8>& packet,
                            int num_of_packets) OVERRIDE {
    return false;
  }

 private:
  bool short_delay_;
  Rtcp* rtcp_;
  base::SimpleTestTickClock* testing_clock_;
};

class RtcpPeer : public Rtcp {
 public:
  RtcpPeer(RtcpSenderFeedback* sender_feedback,
           PacedPacketSender* const paced_packet_sender,
           RtpSenderStatistics* rtp_sender_statistics,
           RtpReceiverStatistics* rtp_receiver_statistics,
           RtcpMode rtcp_mode,
           const base::TimeDelta& rtcp_interval,
           bool sending_media,
           uint32 local_ssrc,
           const std::string& c_name)
      : Rtcp(sender_feedback,
             paced_packet_sender,
             rtp_sender_statistics,
             rtp_receiver_statistics,
             rtcp_mode,
             rtcp_interval,
             sending_media,
             local_ssrc,
             c_name) {
  }

  using Rtcp::CheckForWrapAround;
  using Rtcp::OnReceivedLipSyncInfo;
};

class RtcpTest : public ::testing::Test {
 protected:
  RtcpTest()
      : transport_(&testing_clock_) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  ~RtcpTest() {}

  void SetUp() {
    EXPECT_CALL(mock_sender_feedback_, OnReceivedReportBlock(_)).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedIntraFrameRequest()).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedRpsi(_, _)).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedRemb(_)).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedNackRequest(_)).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(0);
  }

  base::SimpleTestTickClock testing_clock_;
  LocalRtcpTransport transport_;
  MockRtcpSenderFeedback mock_sender_feedback_;
};

TEST_F(RtcpTest, TimeToSend) {
  base::TimeTicks start_time =
      base::TimeTicks::FromInternalValue(kStartMillisecond * 1000);
  Rtcp rtcp(&mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            true,  // Media sender.
            kSenderSsrc,
            kCName);
  rtcp.set_clock(&testing_clock_);
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
  Rtcp rtcp(&mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            true,  // Media sender.
            kSenderSsrc,
            kCName);
  transport_.SetRtcpReceiver(&rtcp);
  rtcp.SendRtcpReport(kUnknownSsrc);
}

TEST_F(RtcpTest, BasicReceiverReport) {
  EXPECT_CALL(mock_sender_feedback_, OnReceivedReportBlock(_)).Times(1);
  Rtcp rtcp(&mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpCompound,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            false,  // Media receiver.
            kSenderSsrc,
            kCName);
  transport_.SetRtcpReceiver(&rtcp);
  rtcp.SetRemoteSSRC(kSenderSsrc);
  rtcp.SendRtcpReport(kSenderSsrc);
}

TEST_F(RtcpTest, BasicPli) {
  EXPECT_CALL(mock_sender_feedback_, OnReceivedReportBlock(_)).Times(1);
  EXPECT_CALL(mock_sender_feedback_, OnReceivedIntraFrameRequest()).Times(1);

  // Media receiver.
  Rtcp rtcp(&mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpReducedSize,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            false,
            kSenderSsrc,
            kCName);
  rtcp.set_clock(&testing_clock_);
  transport_.SetRtcpReceiver(&rtcp);
  rtcp.SetRemoteSSRC(kSenderSsrc);
  rtcp.SendRtcpPli(kSenderSsrc);
}

TEST_F(RtcpTest, BasicCast) {
  EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(1);

  // Media receiver.
  Rtcp rtcp(&mock_sender_feedback_,
            &transport_,
            NULL,
            NULL,
            kRtcpReducedSize,
            base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
            false,
            kSenderSsrc,
            kCName);
  rtcp.set_clock(&testing_clock_);
  transport_.SetRtcpReceiver(&rtcp);
  rtcp.SetRemoteSSRC(kSenderSsrc);
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id_ = kAckFrameId;
  std::set<uint16_t> missing_packets;
  cast_message.missing_frames_and_packets_[
      kLostFrameId] = missing_packets;

  missing_packets.insert(kLostPacketId1);
  missing_packets.insert(kLostPacketId2);
  missing_packets.insert(kLostPacketId3);
  cast_message.missing_frames_and_packets_[
      kFrameIdWithLostPackets] = missing_packets;
  rtcp.SendRtcpCast(cast_message);
}

TEST_F(RtcpTest, Rtt) {
  // Media receiver.
  LocalRtcpTransport receiver_transport(&testing_clock_);
  Rtcp rtcp_receiver(&mock_sender_feedback_,
                     &receiver_transport,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     false,
                     kReceiverSsrc,
                     kCName);
  rtcp_receiver.set_clock(&testing_clock_);

  // Media sender.
  LocalRtcpTransport sender_transport(&testing_clock_);
  Rtcp rtcp_sender(&mock_sender_feedback_,
                   &sender_transport,
                   NULL,
                   NULL,
                   kRtcpReducedSize,
                   base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                   true,
                   kSenderSsrc,
                   kCName);

  rtcp_sender.set_clock(&testing_clock_);
  receiver_transport.SetRtcpReceiver(&rtcp_sender);
  sender_transport.SetRtcpReceiver(&rtcp_receiver);

  rtcp_sender.SetRemoteSSRC(kReceiverSsrc);
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  EXPECT_CALL(mock_sender_feedback_, OnReceivedReportBlock(_)).Times(2);

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  EXPECT_FALSE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  rtcp_sender.SendRtcpReport(kSenderSsrc);
  rtcp_receiver.SendRtcpReport(kSenderSsrc);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_FALSE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));
  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);
  rtcp_sender.SendRtcpReport(kSenderSsrc);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  EXPECT_NEAR(2 * kAddedDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, avg_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);

  receiver_transport.SetShortDelay();
  sender_transport.SetShortDelay();
  rtcp_receiver.SendRtcpReport(kSenderSsrc);
  EXPECT_TRUE(rtcp_sender.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR((kAddedShortDelay + 3 * kAddedDelay) / 2,
               avg_rtt.InMilliseconds(),
               1);
  EXPECT_NEAR(kAddedDelay + kAddedShortDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);
  rtcp_sender.SendRtcpReport(kSenderSsrc);
  EXPECT_TRUE(rtcp_receiver.Rtt(&rtt, &avg_rtt, &min_rtt,  &max_rtt));

  EXPECT_NEAR(2 * kAddedShortDelay, rtt.InMilliseconds(), 1);
  EXPECT_NEAR((2 * kAddedShortDelay + 2 * kAddedDelay) / 2,
               avg_rtt.InMilliseconds(),
               1);
  EXPECT_NEAR(2 *  kAddedShortDelay, min_rtt.InMilliseconds(), 1);
  EXPECT_NEAR(2 * kAddedDelay, max_rtt.InMilliseconds(), 1);
}

TEST_F(RtcpTest, NtpAndTime) {
  RtcpPeer rtcp_peer(&mock_sender_feedback_,
                     NULL,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     false,
                     kReceiverSsrc,
                     kCName);
  rtcp_peer.set_clock(&testing_clock_);
  uint32 ntp_seconds = 0;
  uint32 ntp_fractions = 0;
  base::TimeTicks input_time = base::TimeTicks::FromInternalValue(
    12345678901000LL + kNtpEpochDeltaMicroseconds);
  ConvertTimeToNtp(input_time, &ntp_seconds, &ntp_fractions);
  EXPECT_EQ(12345678u, ntp_seconds);
  EXPECT_EQ(input_time,
            ConvertNtpToTime(ntp_seconds, ntp_fractions));
}

TEST_F(RtcpTest, WrapAround) {
  RtcpPeer rtcp_peer(&mock_sender_feedback_,
                     NULL,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     false,
                     kReceiverSsrc,
                     kCName);
  rtcp_peer.set_clock(&testing_clock_);
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
  old_timestamp = 4234567890;
  EXPECT_EQ(1, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
  new_timestamp = 4234567890;
  old_timestamp = 123;
  EXPECT_EQ(-1, rtcp_peer.CheckForWrapAround(new_timestamp, old_timestamp));
}

TEST_F(RtcpTest, RtpTimestampInSenderTime) {
  RtcpPeer rtcp_peer(&mock_sender_feedback_,
                     NULL,
                     NULL,
                     NULL,
                     kRtcpReducedSize,
                     base::TimeDelta::FromMilliseconds(kRtcpIntervalMs),
                     false,
                     kReceiverSsrc,
                     kCName);
  rtcp_peer.set_clock(&testing_clock_);
  int frequency = 32000;
  uint32 rtp_timestamp = 64000;
  base::TimeTicks rtp_timestamp_in_ticks;

  // Test fail before we get a OnReceivedLipSyncInfo.
  EXPECT_FALSE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                   &rtp_timestamp_in_ticks));

  uint32 ntp_seconds = 0;
  uint32 ntp_fractions = 0;
  base::TimeTicks input_time = base::TimeTicks::FromInternalValue(
      12345678901000LL + kNtpEpochDeltaMicroseconds);

  // Test exact match.
  ConvertTimeToNtp(input_time, &ntp_seconds, &ntp_fractions);
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
  rtp_timestamp = 4294903296;
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
  rtp_timestamp = 4294903296;
  rtcp_peer.OnReceivedLipSyncInfo(rtp_timestamp, ntp_seconds, ntp_fractions);
  rtp_timestamp = 64000;
  EXPECT_TRUE(rtcp_peer.RtpTimestampInSenderTime(frequency, rtp_timestamp,
                                                  &rtp_timestamp_in_ticks));
  EXPECT_EQ(input_time + base::TimeDelta::FromMilliseconds(4000),
            rtp_timestamp_in_ticks);
}

}  // namespace cast
}  // namespace media
