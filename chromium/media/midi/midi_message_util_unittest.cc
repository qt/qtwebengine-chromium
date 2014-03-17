// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_message_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

const uint8 kGMOn[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7 };
const uint8 kNoteOn[] = { 0x90, 0x3c, 0x7f };
const uint8 kChannelPressure[] = { 0xd0, 0x01 };
const uint8 kTimingClock[] = { 0xf8 };

TEST(GetMIDIMessageLengthTest, BasicTest) {
  // Check basic functionarity
  EXPECT_EQ(arraysize(kNoteOn), GetMIDIMessageLength(kNoteOn[0]));
  EXPECT_EQ(arraysize(kChannelPressure),
            GetMIDIMessageLength(kChannelPressure[0]));
  EXPECT_EQ(arraysize(kTimingClock), GetMIDIMessageLength(kTimingClock[0]));

  // SysEx message should be mapped to 0-length
  EXPECT_EQ(0u, GetMIDIMessageLength(kGMOn[0]));

  // Any data byte should be mapped to 0-length
  EXPECT_EQ(0u, GetMIDIMessageLength(kGMOn[1]));
  EXPECT_EQ(0u, GetMIDIMessageLength(kNoteOn[1]));
  EXPECT_EQ(0u, GetMIDIMessageLength(kChannelPressure[1]));
}

}  // namespace
}  // namespace media
