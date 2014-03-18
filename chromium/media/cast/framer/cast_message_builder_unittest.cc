// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/framer/cast_message_builder.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const uint32 kSsrc = 0x1234;
static const uint32 kShortTimeIncrementMs = 10;
static const uint32 kLongTimeIncrementMs = 40;
static const int64 kStartMillisecond = GG_INT64_C(12345678900000);

namespace {
typedef std::map<uint32, size_t> MissingPacketsMap;

class NackFeedbackVerification : public RtpPayloadFeedback {
 public:
  NackFeedbackVerification()
      : triggered_(false),
        missing_packets_(),
        last_frame_acked_(0) {}


  virtual void CastFeedback(const RtcpCastMessage& cast_feedback) OVERRIDE {
    EXPECT_EQ(kSsrc, cast_feedback.media_ssrc_);

    last_frame_acked_ = cast_feedback.ack_frame_id_;

    MissingFramesAndPacketsMap::const_iterator frame_it =
        cast_feedback.missing_frames_and_packets_.begin();

    // Keep track of the number of missing packets per frame.
    missing_packets_.clear();
    while (frame_it != cast_feedback.missing_frames_and_packets_.end()) {
      // Check for complete frame lost.
      if ((frame_it->second.size() == 1) &&
          (*frame_it->second.begin() == kRtcpCastAllPacketsLost)) {
        missing_packets_.insert(
          std::make_pair(frame_it->first, kRtcpCastAllPacketsLost));
      } else {
      missing_packets_.insert(
          std::make_pair(frame_it->first, frame_it->second.size()));
      }
      ++frame_it;
    }
    triggered_ = true;
  }

  size_t num_missing_packets(uint32 frame_id) {
    MissingPacketsMap::iterator it;
    it = missing_packets_.find(frame_id);
    if (it == missing_packets_.end()) return 0;

    return it->second;
  }

  // Holds value for one call.
  bool triggered() {
    bool ret_val =  triggered_;
    triggered_ = false;
    return ret_val;
  }

  uint32 last_frame_acked() { return last_frame_acked_; }

 private:
  bool triggered_;
  MissingPacketsMap missing_packets_;  // Missing packets per frame.
  uint32 last_frame_acked_;
};
}  // namespace

class CastMessageBuilderTest : public ::testing::Test {
 protected:
  CastMessageBuilderTest()
      : cast_msg_builder_(new CastMessageBuilder(&testing_clock_,
                                                 &feedback_,
                                                 &frame_id_map_,
                                                 kSsrc,
                                                 true,
                                                 0)) {
    rtp_header_.webrtc.header.ssrc = kSsrc;
    rtp_header_.is_key_frame = false;
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  virtual ~CastMessageBuilderTest() {}

  void SetFrameId(uint32 frame_id) {
    rtp_header_.frame_id = frame_id;
  }

  void SetPacketId(uint16 packet_id) {
    rtp_header_.packet_id = packet_id;
  }

  void SetMaxPacketId(uint16 max_packet_id) {
    rtp_header_.max_packet_id = max_packet_id;
  }

  void SetKeyFrame(bool is_key) {
    rtp_header_.is_key_frame = is_key;
  }

  void SetReferenceFrameId(uint32 reference_frame_id) {
    rtp_header_.is_reference = true;
    rtp_header_.reference_frame_id = reference_frame_id;
  }

  void InsertPacket() {
    bool complete = false;
    frame_id_map_.InsertPacket(rtp_header_, &complete);
    if (complete) {
      cast_msg_builder_->CompleteFrameReceived(rtp_header_.frame_id,
                                               rtp_header_.is_key_frame);
    }
    cast_msg_builder_->UpdateCastMessage();
  }

  void SetDecoderSlowerThanMaxFrameRate(int max_unacked_frames) {
    cast_msg_builder_.reset(new CastMessageBuilder(&testing_clock_,
                                                   &feedback_,
                                                   &frame_id_map_,
                                                   kSsrc,
                                                   false,
                                                   max_unacked_frames));
  }

  NackFeedbackVerification feedback_;
  scoped_ptr<CastMessageBuilder> cast_msg_builder_;
  RtpCastHeader rtp_header_;
  FrameIdMap frame_id_map_;
  base::SimpleTestTickClock testing_clock_;
};

TEST_F(CastMessageBuilderTest, StartWithAKeyFrame) {
  SetFrameId(3);
  SetPacketId(0);
  SetMaxPacketId(0);
  InsertPacket();
  // Should not trigger ack.
  EXPECT_FALSE(feedback_.triggered());
  SetFrameId(5);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  frame_id_map_.RemoveOldFrames(5);  // Simulate 5 being pulled for rendering.
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(5u, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, OneFrameNackList) {
  SetFrameId(0);
  SetPacketId(4);
  SetMaxPacketId(10);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(0));
}

TEST_F(CastMessageBuilderTest, CompleteFrameMissing) {
  SetFrameId(0);
  SetPacketId(2);
  SetMaxPacketId(5);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(2);
  SetPacketId(2);
  SetMaxPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(kRtcpCastAllPacketsLost, feedback_.num_missing_packets(1));
}

TEST_F(CastMessageBuilderTest, FastForwardAck) {
  SetFrameId(1);
  SetPacketId(0);
  SetMaxPacketId(0);
  InsertPacket();
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(2);
  SetPacketId(0);
  SetMaxPacketId(0);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(kStartFrameId, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(2u, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, RemoveOldFrames) {
  SetFrameId(1);
  SetPacketId(0);
  SetMaxPacketId(1);
  InsertPacket();
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(2);
  SetPacketId(0);
  SetMaxPacketId(0);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(3);
  SetPacketId(0);
  SetMaxPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(kStartFrameId, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(5);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  frame_id_map_.RemoveOldFrames(5);  // Simulate 5 being pulled for rendering.
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(5u, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  SetFrameId(1);
  SetPacketId(1);
  SetMaxPacketId(1);
  InsertPacket();
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(5u, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, WrapFastForward) {
  SetFrameId(254);
  SetPacketId(0);
  SetMaxPacketId(1);
  SetKeyFrame(true);
  InsertPacket();
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(255);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(false);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(253u, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(256);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(false);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(253u, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameId(254);
  SetPacketId(1);
  SetMaxPacketId(1);
  SetKeyFrame(true);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(256u, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, NackUntilMaxReceivedPacket) {
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(20);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(0));
}

TEST_F(CastMessageBuilderTest, NackUntilMaxReceivedPacketNextFrame) {
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(20);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(0));
  SetFrameId(1);
  SetMaxPacketId(2);
  SetPacketId(0);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(19u, feedback_.num_missing_packets(0));
}

TEST_F(CastMessageBuilderTest, NackUntilMaxReceivedPacketNextKey) {
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(20);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(0));
  SetFrameId(1);
  SetMaxPacketId(0);
  SetPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.num_missing_packets(0));
}

TEST_F(CastMessageBuilderTest, Reset) {
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  cast_msg_builder_->Reset();
  frame_id_map_.Clear();
  // Should reset nack list state and request a key frame.
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.num_missing_packets(0));
}

TEST_F(CastMessageBuilderTest, DeltaAfterReset) {
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.num_missing_packets(0));
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  cast_msg_builder_->Reset();
  SetFrameId(1);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  EXPECT_FALSE(feedback_.triggered());
}

TEST_F(CastMessageBuilderTest, BasicRps) {
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.last_frame_acked());
  SetFrameId(3);
  SetKeyFrame(false);
  SetReferenceFrameId(0);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  frame_id_map_.RemoveOldFrames(3);  // Simulate 3 being pulled for rendering.
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(3u, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, InOrderRps) {
  // Create a pattern - skip to rps, and don't look back.
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.last_frame_acked());
  SetFrameId(1);
  SetPacketId(0);
  SetMaxPacketId(1);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_FALSE(feedback_.triggered());
  SetFrameId(3);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(false);
  SetReferenceFrameId(0);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  frame_id_map_.RemoveOldFrames(3);  // Simulate 3 being pulled for rendering.
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(3u, feedback_.last_frame_acked());
  // Make an old frame complete - should not trigger an ack.
  SetFrameId(1);
  SetPacketId(1);
  SetMaxPacketId(1);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_FALSE(feedback_.triggered());
  EXPECT_EQ(3u, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, SlowDownAck) {
  SetDecoderSlowerThanMaxFrameRate(3);
  SetFrameId(0);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();

  uint32 frame_id;
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  SetKeyFrame(false);
  for (frame_id = 1; frame_id < 3; ++frame_id) {
    EXPECT_TRUE(feedback_.triggered());
    EXPECT_EQ(frame_id - 1, feedback_.last_frame_acked());
    SetFrameId(frame_id);
    InsertPacket();
    testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  }
  // We should now have entered the slowdown ACK state.
  uint32 expected_frame_id = 1;
  for (; frame_id < 10; ++frame_id) {
    if (frame_id % 2)  ++expected_frame_id;
    EXPECT_TRUE(feedback_.triggered());
    EXPECT_EQ(expected_frame_id, feedback_.last_frame_acked());
    SetFrameId(frame_id);
    InsertPacket();
    testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  }
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(expected_frame_id, feedback_.last_frame_acked());

  // Simulate frame_id being pulled for rendering.
  frame_id_map_.RemoveOldFrames(frame_id);
  // We should now leave the slowdown ACK state.
  ++frame_id;
  SetFrameId(frame_id);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(frame_id, feedback_.last_frame_acked());
}

}  // namespace cast
}  // namespace media
