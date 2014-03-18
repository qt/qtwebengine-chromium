// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/mock_rtcp_receiver_feedback.h"
#include "media/cast/rtcp/mock_rtcp_sender_feedback.h"
#include "media/cast/rtcp/rtcp_receiver.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/rtcp/test_rtcp_packet_builder.h"
#include "media/cast/test/fake_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

using testing::_;

static const uint32 kSenderSsrc = 0x10203;
static const uint32 kSourceSsrc = 0x40506;
static const uint32 kUnknownSsrc = 0xDEAD;
static const std::string kCName("test@10.1.1.1");

namespace {
class SenderFeedbackCastVerification : public RtcpSenderFeedback {
 public:
  SenderFeedbackCastVerification() : called_(false) {}

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_feedback) OVERRIDE {
    EXPECT_EQ(cast_feedback.media_ssrc_, kSenderSsrc);
    EXPECT_EQ(cast_feedback.ack_frame_id_, kAckFrameId);

    MissingFramesAndPacketsMap::const_iterator frame_it =
        cast_feedback.missing_frames_and_packets_.begin();

    EXPECT_TRUE(frame_it != cast_feedback.missing_frames_and_packets_.end());
    EXPECT_EQ(kLostFrameId, frame_it->first);
    EXPECT_TRUE(frame_it->second.empty());
    ++frame_it;
    EXPECT_TRUE(frame_it != cast_feedback.missing_frames_and_packets_.end());
    EXPECT_EQ(kFrameIdWithLostPackets, frame_it->first);
    EXPECT_EQ(3UL, frame_it->second.size());
    PacketIdSet::const_iterator packet_it = frame_it->second.begin();
    EXPECT_EQ(kLostPacketId1, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId2, *packet_it);
    ++packet_it;
    EXPECT_EQ(kLostPacketId3, *packet_it);
    ++frame_it;
    EXPECT_EQ(frame_it, cast_feedback.missing_frames_and_packets_.end());
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_;
};

class RtcpReceiverCastLogVerification : public RtcpReceiverFeedback {
 public:
  RtcpReceiverCastLogVerification()
      : called_on_received_sender_log_(false),
        called_on_received_receiver_log_(false) {}

  virtual void OnReceivedSenderReport(
      const RtcpSenderInfo& remote_sender_info) OVERRIDE {};

  virtual void OnReceiverReferenceTimeReport(
      const RtcpReceiverReferenceTimeReport& remote_time_report) OVERRIDE {};

  virtual void OnReceivedSendReportRequest() OVERRIDE {};

  virtual void OnReceivedReceiverLog(
      const RtcpReceiverLogMessage& receiver_log) OVERRIDE {
    EXPECT_EQ(expected_receiver_log_.size(), receiver_log.size());
    RtcpReceiverLogMessage::const_iterator expected_it =
       expected_receiver_log_.begin();
    RtcpReceiverLogMessage::const_iterator incoming_it = receiver_log.begin();
    for (; incoming_it != receiver_log.end(); ++incoming_it) {
      EXPECT_EQ(expected_it->rtp_timestamp_, incoming_it->rtp_timestamp_);
      EXPECT_EQ(expected_it->event_log_messages_.size(),
                incoming_it->event_log_messages_.size());

      RtcpReceiverEventLogMessages::const_iterator event_incoming_it =
          incoming_it->event_log_messages_.begin();
      RtcpReceiverEventLogMessages::const_iterator event_expected_it =
          expected_it->event_log_messages_.begin();
      for (; event_incoming_it != incoming_it->event_log_messages_.end();
           ++event_incoming_it, ++event_expected_it) {
        EXPECT_EQ(event_expected_it->type, event_incoming_it->type);
        EXPECT_EQ(event_expected_it->event_timestamp,
                  event_incoming_it->event_timestamp);
        if (event_expected_it->type == kPacketReceived) {
          EXPECT_EQ(event_expected_it->packet_id, event_incoming_it->packet_id);
        } else {
          EXPECT_EQ(event_expected_it->delay_delta,
                    event_incoming_it->delay_delta);
        }
      }
      expected_receiver_log_.pop_front();
      expected_it = expected_receiver_log_.begin();
    }
    called_on_received_receiver_log_ = true;
  }

  virtual void OnReceivedSenderLog(
      const RtcpSenderLogMessage& sender_log) OVERRIDE {
    EXPECT_EQ(expected_sender_log_.size(), sender_log.size());

    RtcpSenderLogMessage::const_iterator expected_it =
       expected_sender_log_.begin();
    RtcpSenderLogMessage::const_iterator incoming_it = sender_log.begin();
    for (; expected_it != expected_sender_log_.end();
        ++expected_it, ++incoming_it) {
      EXPECT_EQ(expected_it->frame_status, incoming_it->frame_status);
      EXPECT_EQ(0xffffff & expected_it->rtp_timestamp,
                incoming_it->rtp_timestamp);
    }
    called_on_received_sender_log_ = true;
  }

  bool OnReceivedSenderLogCalled() {
    return called_on_received_sender_log_;
  }

  bool OnReceivedReceiverLogCalled() {
    return called_on_received_receiver_log_ && expected_receiver_log_.empty();
  }

  void SetExpectedReceiverLog(const RtcpReceiverLogMessage& receiver_log) {
    expected_receiver_log_ = receiver_log;
  }

  void SetExpectedSenderLog(const RtcpSenderLogMessage& sender_log) {
    expected_sender_log_ = sender_log;
  }

 private:
  RtcpReceiverLogMessage expected_receiver_log_;
  RtcpSenderLogMessage expected_sender_log_;
  bool called_on_received_sender_log_;
  bool called_on_received_receiver_log_;
};

}  // namespace

class RtcpReceiverTest : public ::testing::Test {
 protected:
  RtcpReceiverTest()
      : task_runner_(new test::FakeTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_, task_runner_,
            task_runner_, task_runner_, task_runner_, task_runner_,
            GetDefaultCastLoggingConfig())),
        rtcp_receiver_(new RtcpReceiver(cast_environment_,
                                        &mock_sender_feedback_,
                                        &mock_receiver_feedback_,
                                        &mock_rtt_feedback_,
                                        kSourceSsrc)) {
  }

  virtual ~RtcpReceiverTest() {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(mock_receiver_feedback_, OnReceivedSenderReport(_)).Times(0);
    EXPECT_CALL(mock_receiver_feedback_,
                OnReceiverReferenceTimeReport(_)).Times(0);
    EXPECT_CALL(mock_receiver_feedback_,
                OnReceivedSendReportRequest()).Times(0);
    EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(0);
    EXPECT_CALL(mock_rtt_feedback_,
                OnReceivedDelaySinceLastReport(_, _, _)).Times(0);

    expected_sender_info_.ntp_seconds = kNtpHigh;
    expected_sender_info_.ntp_fraction = kNtpLow;
    expected_sender_info_.rtp_timestamp = kRtpTimestamp;
    expected_sender_info_.send_packet_count = kSendPacketCount;
    expected_sender_info_.send_octet_count = kSendOctetCount;

    expected_report_block_.remote_ssrc = kSenderSsrc;
    expected_report_block_.media_ssrc = kSourceSsrc;
    expected_report_block_.fraction_lost = kLoss >> 24;
    expected_report_block_.cumulative_lost = kLoss & 0xffffff;
    expected_report_block_.extended_high_sequence_number = kExtendedMax;
    expected_report_block_.jitter = kTestJitter;
    expected_report_block_.last_sr = kLastSr;
    expected_report_block_.delay_since_last_sr = kDelayLastSr;
    expected_receiver_reference_report_.remote_ssrc = kSenderSsrc;
    expected_receiver_reference_report_.ntp_seconds = kNtpHigh;
    expected_receiver_reference_report_.ntp_fraction = kNtpLow;
  }

  // Injects an RTCP packet into the receiver.
  void InjectRtcpPacket(const uint8* packet, uint16 length) {
    RtcpParser rtcp_parser(packet, length);
    rtcp_receiver_->IncomingRtcpPacket(&rtcp_parser);
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  MockRtcpReceiverFeedback mock_receiver_feedback_;
  MockRtcpRttFeedback mock_rtt_feedback_;
  MockRtcpSenderFeedback mock_sender_feedback_;
  scoped_ptr<RtcpReceiver> rtcp_receiver_;
  RtcpSenderInfo expected_sender_info_;
  RtcpReportBlock expected_report_block_;
  RtcpReceiverReferenceTimeReport expected_receiver_reference_report_;
};

TEST_F(RtcpReceiverTest, BrokenPacketIsIgnored) {
  const uint8 bad_packet[] = {0, 0, 0, 0};
  InjectRtcpPacket(bad_packet, sizeof(bad_packet));
}

TEST_F(RtcpReceiverTest, InjectSenderReportPacket) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);

  // Expected to be ignored since the sender ssrc does not match our
  // remote ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Expected to be pass through since the sender ssrc match our remote ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiveReportPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectSenderReportWithReportBlockPacket) {
  TestRtcpPacketBuilder p1;
  p1.AddSr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Report block expected to be ignored since the source ssrc does not match
  // our local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_receiver_feedback_, OnReceivedSenderReport(_)).Times(0);
  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
        kLastSr,
        kDelayLastSr)).Times(1);

  rtcp_receiver_->SetRemoteSSRC(0);

  TestRtcpPacketBuilder p2;
  p2.AddSr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);

  // Sender report expected to be ignored since the sender ssrc does not match
  // our remote ssrc.
  // Receiver report expected to be pass through since the sender ssrc match
  // our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Sender report expected to be pass through since the sender ssrc match our
  // remote ssrc.
  // Receiver report expected to be pass through since the sender ssrc match
  // our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectSenderReportPacketWithDlrr) {
  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);
  p.AddXrHeader(kSenderSsrc);
  p.AddXrUnknownBlock();
  p.AddXrExtendedDlrrBlock(kSenderSsrc);
  p.AddXrUnknownBlock();
  p.AddSdesCname(kSenderSsrc, kCName);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());

  EXPECT_CALL(mock_receiver_feedback_,
              OnReceivedSenderReport(expected_sender_info_)).Times(1);
  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSenderSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  // Enable receiving sender report.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p.Packet(), p.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithRrtr) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddXrHeader(kSenderSsrc);
  p1.AddXrRrtrBlock();

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);
  EXPECT_CALL(mock_receiver_feedback_, OnReceiverReferenceTimeReport(
      expected_receiver_reference_report_)).Times(1);

  // Enable receiving reference time report.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddXrHeader(kSenderSsrc);
  p2.AddXrRrtrBlock();

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithIntraFrameRequest) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddPli(kSenderSsrc, kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddPli(kSenderSsrc, kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithCastFeedback) {
  TestRtcpPacketBuilder p1;
  p1.AddRr(kSenderSsrc, 1);
  p1.AddRb(kUnknownSsrc);
  p1.AddCast(kSenderSsrc, kUnknownSsrc);

  // Expected to be ignored since the source ssrc does not match our
  // local ssrc.
  InjectRtcpPacket(p1.Packet(), p1.Length());

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);
  EXPECT_CALL(mock_sender_feedback_, OnReceivedCastFeedback(_)).Times(1);

  // Enable receiving the cast feedback.
  rtcp_receiver_->SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p2;
  p2.AddRr(kSenderSsrc, 1);
  p2.AddRb(kSourceSsrc);
  p2.AddCast(kSenderSsrc, kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  InjectRtcpPacket(p2.Packet(), p2.Length());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportPacketWithCastVerification) {
  SenderFeedbackCastVerification sender_feedback_cast_verification;
  RtcpReceiver rtcp_receiver(cast_environment_,
                             &sender_feedback_cast_verification,
                             &mock_receiver_feedback_,
                             &mock_rtt_feedback_,
                             kSourceSsrc);

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc,
          kLastSr,
          kDelayLastSr)).Times(1);

  // Enable receiving the cast feedback.
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddCast(kSenderSsrc, kSourceSsrc);

  // Expected to be pass through since the sender ssrc match our local ssrc.
  RtcpParser rtcp_parser(p.Packet(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(sender_feedback_cast_verification.called());
}

TEST_F(RtcpReceiverTest, InjectSenderReportWithCastSenderLogVerification) {
  RtcpReceiverCastLogVerification cast_log_verification;
  RtcpReceiver rtcp_receiver(cast_environment_,
                             &mock_sender_feedback_,
                             &cast_log_verification,
                             &mock_rtt_feedback_,
                             kSourceSsrc);
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  RtcpSenderLogMessage sender_log;
  for (int j = 0; j < 359; ++j) {
    RtcpSenderFrameLogMessage sender_frame_log;
    sender_frame_log.frame_status = kRtcpSenderFrameStatusSentToNetwork;
    sender_frame_log.rtp_timestamp = kRtpTimestamp + j * 90;
    sender_log.push_back(sender_frame_log);
  }
  cast_log_verification.SetExpectedSenderLog(sender_log);

  TestRtcpPacketBuilder p;
  p.AddSr(kSenderSsrc, 0);
  p.AddSdesCname(kSenderSsrc, kCName);
  p.AddSenderLog(kSenderSsrc);

  for (int i = 0; i < 359; ++i) {
    p.AddSenderFrameLog(kRtcpSenderFrameStatusSentToNetwork,
                        kRtpTimestamp + i * 90);
  }
  RtcpParser rtcp_parser(p.Packet(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(cast_log_verification.OnReceivedSenderLogCalled());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportWithReceiverLogVerificationBase) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  RtcpReceiverCastLogVerification cast_log_verification;
  RtcpReceiver rtcp_receiver(cast_environment_,
                             &mock_sender_feedback_,
                             &cast_log_verification,
                             &mock_rtt_feedback_,
                             kSourceSsrc);
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  RtcpReceiverLogMessage receiver_log;
  RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
  RtcpReceiverEventLogMessage event_log;

  event_log.type = kAckSent;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
  frame_log.event_log_messages_.push_back(event_log);

  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  event_log.type = kPacketReceived;
  event_log.event_timestamp = testing_clock.NowTicks();
  event_log.packet_id = kLostPacketId1;
  frame_log.event_log_messages_.push_back(event_log);
  receiver_log.push_back(frame_log);

  cast_log_verification.SetExpectedReceiverLog(receiver_log);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddReceiverLog(kSenderSsrc);
  p.AddReceiverFrameLog(kRtpTimestamp, 2, kTimeBaseMs);
  p.AddReceiverEventLog(kDelayDeltaMs, 1, 0);
  p.AddReceiverEventLog(kLostPacketId1, 6, kTimeDelayMs);

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc, kLastSr, kDelayLastSr)).
          Times(1);

  RtcpParser rtcp_parser(p.Packet(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(cast_log_verification.OnReceivedReceiverLogCalled());
}

TEST_F(RtcpReceiverTest, InjectReceiverReportWithReceiverLogVerificationMulti) {
  static const uint32 kTimeBaseMs = 12345678;
  static const uint32 kTimeDelayMs = 10;
  static const uint32 kDelayDeltaMs = 123;
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeBaseMs));

  RtcpReceiverCastLogVerification cast_log_verification;
  RtcpReceiver rtcp_receiver(cast_environment_,
                             &mock_sender_feedback_,
                             &cast_log_verification,
                             &mock_rtt_feedback_,
                             kSourceSsrc);
  rtcp_receiver.SetRemoteSSRC(kSenderSsrc);

  RtcpReceiverLogMessage receiver_log;

  for (int j = 0; j < 100; ++j) {
    RtcpReceiverFrameLogMessage frame_log(kRtpTimestamp);
    RtcpReceiverEventLogMessage event_log;
    event_log.type = kAckSent;
    event_log.event_timestamp = testing_clock.NowTicks();
    event_log.delay_delta = base::TimeDelta::FromMilliseconds(kDelayDeltaMs);
    frame_log.event_log_messages_.push_back(event_log);
    receiver_log.push_back(frame_log);
    testing_clock.Advance(base::TimeDelta::FromMilliseconds(kTimeDelayMs));
  }

  cast_log_verification.SetExpectedReceiverLog(receiver_log);

  TestRtcpPacketBuilder p;
  p.AddRr(kSenderSsrc, 1);
  p.AddRb(kSourceSsrc);
  p.AddReceiverLog(kSenderSsrc);
  for (int i = 0; i < 100; ++i) {
    p.AddReceiverFrameLog(kRtpTimestamp, 1, kTimeBaseMs +  i * kTimeDelayMs);
    p.AddReceiverEventLog(kDelayDeltaMs, 1, 0);
  }

  EXPECT_CALL(mock_rtt_feedback_,
      OnReceivedDelaySinceLastReport(kSourceSsrc, kLastSr, kDelayLastSr)).
          Times(1);

  RtcpParser rtcp_parser(p.Packet(), p.Length());
  rtcp_receiver.IncomingRtcpPacket(&rtcp_parser);

  EXPECT_TRUE(cast_log_verification.OnReceivedReceiverLogCalled());
}



}  // namespace cast
}  // namespace media
