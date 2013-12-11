// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/framer/framer.h"
#include "media/cast/rtp_common/mock_rtp_payload_feedback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const int64 kFrameTimeMillisecond = 33;

class FramerTest : public ::testing::Test {
 protected:
  FramerTest()
      : mock_rtp_payload_feedback_(),
        framer_(&mock_rtp_payload_feedback_, 0, true, 0) {
    framer_.set_clock(&testing_clock_);
  }

  ~FramerTest() {}

  void SetUp() {
    // Build a default one packet frame - populate webrtc header.
    rtp_header_.is_key_frame = false;
    rtp_header_.frame_id = 0;
    rtp_header_.packet_id = 0;
    rtp_header_.max_packet_id = 0;
    rtp_header_.is_reference = false;
    rtp_header_.reference_frame_id = 0;
    payload_.assign(kIpPacketSize, 0);

    EXPECT_CALL(mock_rtp_payload_feedback_,
                CastFeedback(testing::_)).WillRepeatedly(testing::Return());
  }

  std::vector<uint8> payload_;
  RtpCastHeader rtp_header_;
  MockRtpPayloadFeedback mock_rtp_payload_feedback_;
  Framer framer_;
  base::SimpleTestTickClock testing_clock_;
};


TEST_F(FramerTest, EmptyState) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
}

TEST_F(FramerTest, AlwaysStartWithKey) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;

  // Insert non key first frame.
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
  rtp_header_.frame_id = 1;
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(1, frame.frame_id);
  EXPECT_TRUE(frame.key_frame);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, CompleteFrame) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;

  // start with a complete key frame.
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0, frame.frame_id);
  EXPECT_TRUE(frame.key_frame);
  framer_.ReleaseFrame(frame.frame_id);

  // Incomplete delta.
  ++rtp_header_.frame_id;
  rtp_header_.is_key_frame = false;
  rtp_header_.max_packet_id = 2;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));

  // Complete delta - can't skip, as incomplete sequence.
  ++rtp_header_.frame_id;
  rtp_header_.max_packet_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
}

TEST_F(FramerTest, ContinuousSequence) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;

  // start with a complete key frame.
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0, frame.frame_id);
  EXPECT_TRUE(frame.key_frame);
  framer_.ReleaseFrame(frame.frame_id);

  // Complete - not continuous.
  rtp_header_.frame_id = 2;
  rtp_header_.is_key_frame = false;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
}

TEST_F(FramerTest, Wrap) {
  // Insert key frame, frame_id = 255 (will jump to that)
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 255;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(255, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Insert wrapped delta frame - should be continuous.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, Reset) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  framer_.Reset();
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
}

TEST_F(FramerTest, RequireKeyAfterReset) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;
  framer_.Reset();

  // Start with a complete key frame.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
  rtp_header_.frame_id = 1;
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
}

TEST_F(FramerTest, BasicNonLastReferenceId) {
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);

  base::TimeTicks timeout;
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  framer_.ReleaseFrame(frame.frame_id);

  rtp_header_.is_key_frame = false;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  rtp_header_.frame_id = 5;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);

  timeout += base::TimeDelta::FromMilliseconds(kFrameTimeMillisecond);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
  testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kFrameTimeMillisecond));

  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_FALSE(next_frame);
}

TEST_F(FramerTest, InOrderReferenceFrameSelection) {
  // Create pattern: 0, 1, 4, 5.
  EncodedVideoFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = 1;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);

  // Insert frame #2 partially.
  rtp_header_.frame_id = 2;
  rtp_header_.max_packet_id = 1;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  rtp_header_.frame_id = 4;
  rtp_header_.max_packet_id = 0;
  rtp_header_.is_reference = true;
  rtp_header_.reference_frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_EQ(0, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(1, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_FALSE(next_frame);
  EXPECT_EQ(4, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  // Insert remaining packet of frame #2 - should no be continuous.
  rtp_header_.frame_id = 2;
  rtp_header_.packet_id = 1;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_FALSE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                            &next_frame));
  rtp_header_.is_reference = false;
  rtp_header_.frame_id = 5;
  rtp_header_.packet_id = 0;
  rtp_header_.max_packet_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedVideoFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(5, frame.frame_id);
}

TEST_F(FramerTest, AudioWrap) {
  // All audio frames are marked as key frames.
  EncodedAudioFrame frame;
  uint32_t rtp_timestamp;
  base::TimeTicks timeout;
  bool next_frame = false;
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 254;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(254, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  rtp_header_.frame_id = 255;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);

  // Insert wrapped frame - should be continuous.
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);

  EXPECT_TRUE(framer_.GetEncodedAudioFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(255, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  EXPECT_TRUE(framer_.GetEncodedAudioFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, AudioWrapWithMissingFrame) {
  // All audio frames are marked as key frames.
  EncodedAudioFrame frame;
  uint32_t rtp_timestamp;
  bool next_frame = false;
  base::TimeTicks timeout;

  // Insert and get first packet.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = 253;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(253, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Insert third and fourth packets.
  rtp_header_.frame_id = 255;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);
  rtp_header_.frame_id = 0;
  framer_.InsertPacket(payload_.data(), payload_.size(), rtp_header_);

  // Get third and fourth packets.
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_FALSE(next_frame);
  EXPECT_EQ(255, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedAudioFrame(timeout, &frame, &rtp_timestamp,
                                           &next_frame));
  EXPECT_TRUE(next_frame);
  EXPECT_EQ(0, frame.frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

}  // namespace cast
}  // namespace media
