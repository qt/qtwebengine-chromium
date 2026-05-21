// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/stringprintf.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(StringFormat, ProducesFormattedStrings) {
  EXPECT_EQ("no args", StringFormat("no args"));
  EXPECT_EQ("", StringFormat("{}", ""));
  EXPECT_EQ("42", StringFormat("{}", 42));
  EXPECT_EQ(
      "The result of foo(1, 2) looks good!",
      StringFormat("The result of foo({}, {}) looks {}{}", 1, 2, "good", '!'));
}

TEST(HexEncode, ProducesEmptyStringFromEmptyByteArray) {
  const uint8_t kSomeMemoryLocation = 0;
  EXPECT_EQ("", HexEncode(&kSomeMemoryLocation, 0));
}

TEST(HexEncode, ProducesHexStringsFromBytes) {
  const uint8_t kMessage[] = "Hello world!";
  const char kMessageInHex[] = "48656c6c6f20776f726c642100";
  EXPECT_EQ(kMessageInHex, HexEncode(kMessage, sizeof(kMessage)));
}

}  // namespace
}  // namespace openscreen
