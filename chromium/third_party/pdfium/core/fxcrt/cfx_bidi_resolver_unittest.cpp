// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/cfx_bidi_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(fxcrt, BidiResolverPureLTR) {
  auto resolver = CFX_BidiResolver::Create(
      u"abc", CFX_BidiResolver::ParagraphDirection::kLeftToRight);
  ASSERT_TRUE(resolver);
  auto runs = resolver->GetVisualRunsForLine(0, 3);
  ASSERT_EQ(1u, runs.size());
  EXPECT_EQ(0, runs[0].start);
  EXPECT_EQ(3, runs[0].length);
  EXPECT_FALSE(runs[0].is_rtl);
}

TEST(fxcrt, BidiResolverPureRTL) {
  // Hebrew characters Aleph, Bet, Gimel (RTL characters).
  auto resolver = CFX_BidiResolver::Create(
      u"\x05D0\x05D1\x05D2",
      CFX_BidiResolver::ParagraphDirection::kRightToLeft);
  ASSERT_TRUE(resolver);
  auto runs = resolver->GetVisualRunsForLine(0, 3);
  ASSERT_EQ(1u, runs.size());
  EXPECT_EQ(0, runs[0].start);
  EXPECT_EQ(3, runs[0].length);
  EXPECT_TRUE(runs[0].is_rtl);
}

TEST(fxcrt, BidiResolverMixed) {
  auto resolver = CFX_BidiResolver::Create(
      u"ab \x05D0\x05D1 cd",
      CFX_BidiResolver::ParagraphDirection::kLeftToRight);
  ASSERT_TRUE(resolver);
  auto runs = resolver->GetVisualRunsForLine(0, 8);
  ASSERT_EQ(3u, runs.size());
  // "ab " (LTR) -> "Aleph Bet" (RTL) -> " cd" (LTR)
  EXPECT_EQ(0, runs[0].start);
  EXPECT_EQ(3, runs[0].length);
  EXPECT_FALSE(runs[0].is_rtl);

  EXPECT_EQ(3, runs[1].start);
  EXPECT_EQ(2, runs[1].length);
  EXPECT_TRUE(runs[1].is_rtl);

  EXPECT_EQ(5, runs[2].start);
  EXPECT_EQ(3, runs[2].length);
  EXPECT_FALSE(runs[2].is_rtl);
}

TEST(fxcrt, BidiResolverLineWrap) {
  auto resolver = CFX_BidiResolver::Create(
      u"abc def ghi", CFX_BidiResolver::ParagraphDirection::kLeftToRight);
  ASSERT_TRUE(resolver);
  // Get runs for the middle word "def "
  auto runs = resolver->GetVisualRunsForLine(4, 4);
  ASSERT_EQ(1u, runs.size());
  EXPECT_EQ(4, runs[0].start);
  EXPECT_EQ(4, runs[0].length);
  EXPECT_FALSE(runs[0].is_rtl);
}

TEST(fxcrt, BidiResolverEmpty) {
  auto resolver = CFX_BidiResolver::Create(
      std::u16string(), CFX_BidiResolver::ParagraphDirection::kAuto);
  // Just proving it doesn't crash or leak memory.
  EXPECT_FALSE(resolver);
}
