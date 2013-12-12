// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicProtocolTest, MakeQuicTag) {
  QuicTag tag = MakeQuicTag('A', 'B', 'C', 'D');
  char bytes[4];
  memcpy(bytes, &tag, 4);
  EXPECT_EQ('A', bytes[0]);
  EXPECT_EQ('B', bytes[1]);
  EXPECT_EQ('C', bytes[2]);
  EXPECT_EQ('D', bytes[3]);
}

TEST(QuicProtocolTest, IsAawaitingPacket) {
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 10u;
  EXPECT_TRUE(IsAwaitingPacket(received_info, 11u));
  EXPECT_FALSE(IsAwaitingPacket(received_info, 1u));

  received_info.missing_packets.insert(10);
  EXPECT_TRUE(IsAwaitingPacket(received_info, 10u));
}

TEST(QuicProtocolTest, InsertMissingPacketsBetween) {
  ReceivedPacketInfo received_info;
  InsertMissingPacketsBetween(&received_info, 4u, 10u);
  EXPECT_EQ(6u, received_info.missing_packets.size());

  QuicPacketSequenceNumber i = 4;
  for (SequenceNumberSet::iterator it = received_info.missing_packets.begin();
       it != received_info.missing_packets.end(); ++it, ++i) {
    EXPECT_EQ(i, *it);
  }
}

TEST(QuicProtocolTest, QuicVersionToQuicTag) {
  // If you add a new version to the QuicVersion enum you will need to add a new
  // case to QuicVersionToQuicTag, otherwise this test will fail.

  // TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  // Any logs would indicate an unsupported version which we don't expect.
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  log.StartCapturingLogs();
#endif

  // Explicitly test a specific version.
  EXPECT_EQ(MakeQuicTag('Q', '0', '1', '0'),
            QuicVersionToQuicTag(QUIC_VERSION_10));

  // Loop over all supported versions and make sure that we never hit the
  // default case (i.e. all supported versions should be successfully converted
  // to valid QuicTags).
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    const QuicVersion& version = kSupportedQuicVersions[i];
    EXPECT_LT(0u, QuicVersionToQuicTag(version));
  }
}

TEST(QuicProtocolTest, QuicVersionToQuicTagUnsupported) {
  // TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  // TODO(rjshade): Change to DFATAL once we actually support multiple versions,
  // and QuicConnectionTest::SendVersionNegotiationPacket can be changed to use
  // mis-matched versions rather than relying on QUIC_VERSION_UNSUPPORTED.
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _, "Unsupported QuicVersion: 0")).Times(1);
  log.StartCapturingLogs();
#endif

  EXPECT_EQ(0u, QuicVersionToQuicTag(QUIC_VERSION_UNSUPPORTED));
}

TEST(QuicProtocolTest, QuicTagToQuicVersion) {
  // If you add a new version to the QuicVersion enum you will need to add a new
  // case to QuicTagToQuicVersion, otherwise this test will fail.

  // TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  // Any logs would indicate an unsupported version which we don't expect.
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  log.StartCapturingLogs();
#endif

  // Explicitly test specific versions.
  EXPECT_EQ(QUIC_VERSION_10,
            QuicTagToQuicVersion(MakeQuicTag('Q', '0', '1', '0')));

  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    const QuicVersion& version = kSupportedQuicVersions[i];

    // Get the tag from the version (we can loop over QuicVersions easily).
    QuicTag tag = QuicVersionToQuicTag(version);
    EXPECT_LT(0u, tag);

    // Now try converting back.
    QuicVersion tag_to_quic_version = QuicTagToQuicVersion(tag);
    EXPECT_EQ(version, tag_to_quic_version);
    EXPECT_NE(QUIC_VERSION_UNSUPPORTED, tag_to_quic_version);
  }
}

TEST(QuicProtocolTest, QuicTagToQuicVersionUnsupported) {
  // TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  ScopedMockLog log(kDoNotCaptureLogsYet);
#ifndef NDEBUG
  EXPECT_CALL(log, Log(INFO, _, "Unsupported QuicTag version: FAKE")).Times(1);
#endif
  log.StartCapturingLogs();
#endif

  EXPECT_EQ(QUIC_VERSION_UNSUPPORTED,
            QuicTagToQuicVersion(MakeQuicTag('F', 'A', 'K', 'E')));
}

TEST(QuicProtocolTest, QuicVersionToString) {
  EXPECT_EQ("QUIC_VERSION_8",
            QuicVersionToString(QUIC_VERSION_8));
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED",
            QuicVersionToString(QUIC_VERSION_UNSUPPORTED));

  QuicVersion single_version[] = {QUIC_VERSION_8};
  EXPECT_EQ("QUIC_VERSION_8,", QuicVersionArrayToString(
                                   single_version, arraysize(single_version)));
  QuicVersion multiple_versions[] =
      {QUIC_VERSION_10, QUIC_VERSION_9, QUIC_VERSION_8};
  EXPECT_EQ("QUIC_VERSION_10,QUIC_VERSION_9,QUIC_VERSION_8,",
            QuicVersionArrayToString(multiple_versions,
                                     arraysize(multiple_versions)));
}

}  // namespace
}  // namespace test
}  // namespace net
