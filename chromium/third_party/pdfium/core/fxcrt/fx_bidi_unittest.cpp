// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/fx_bidi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr wchar_t kNeutralChar = 32;   // ' '
constexpr wchar_t kLeftChar = 65;      // 'A'
constexpr wchar_t kRightChar = 1488;   // 'א'
constexpr wchar_t kLeftWeakChar = 46;  // '.'

}  // namespace

TEST(fxcrt, BidiCharEmpty) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, info.direction);
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(0u, info.count);
  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeft) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(0u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));

  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, info.direction);
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(0u, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, info.direction);
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(3u, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeftNeutralRight) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(0u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_TRUE(bidi.AppendChar(kNeutralChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(3u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kNeutralChar));
  EXPECT_FALSE(bidi.AppendChar(kNeutralChar));
  EXPECT_FALSE(bidi.AppendChar(kNeutralChar));
  EXPECT_TRUE(bidi.AppendChar(kRightChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, info.direction);
  EXPECT_EQ(3u, info.start);
  EXPECT_EQ(4u, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, info.direction);
  EXPECT_EQ(7u, info.start);
  EXPECT_EQ(1u, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeftLeftWeakRight) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(0u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_TRUE(bidi.AppendChar(kLeftWeakChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(3u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftWeakChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftWeakChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftWeakChar));
  EXPECT_TRUE(bidi.AppendChar(kRightChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kLeftWeak, info.direction);
  EXPECT_EQ(3u, info.start);
  EXPECT_EQ(4u, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, info.direction);
  EXPECT_EQ(7u, info.start);
  EXPECT_EQ(1u, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiCharLeftRightLeft) {
  CFX_BidiChar bidi;
  CFX_BidiChar::Segment info;

  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(0u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_FALSE(bidi.AppendChar(kLeftChar));
  EXPECT_TRUE(bidi.AppendChar(kRightChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(0u, info.start);
  EXPECT_EQ(3u, info.count);

  EXPECT_FALSE(bidi.AppendChar(kRightChar));
  EXPECT_FALSE(bidi.AppendChar(kRightChar));
  EXPECT_FALSE(bidi.AppendChar(kRightChar));
  EXPECT_TRUE(bidi.AppendChar(kLeftChar));
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, info.direction);
  EXPECT_EQ(3u, info.start);
  EXPECT_EQ(4u, info.count);

  EXPECT_TRUE(bidi.EndChar());
  info = bidi.GetSegmentInfo();
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, info.direction);
  EXPECT_EQ(7u, info.start);
  EXPECT_EQ(1u, info.count);

  EXPECT_FALSE(bidi.EndChar());
}

TEST(fxcrt, BidiStringEmpty) {
  CFX_BidiString bidi(L"", /*auto_order=*/true);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());
  EXPECT_TRUE(bidi.begin() == bidi.end());
}

TEST(fxcrt, BidiStringAllNeutral) {
  {
    constexpr wchar_t kStr[] = {kNeutralChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(1u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
    ++it;
    EXPECT_EQ(it, bidi.end());
  }
  {
    constexpr wchar_t kStr[] = {kNeutralChar, kNeutralChar, kNeutralChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(3u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
    ++it;
    EXPECT_EQ(it, bidi.end());
  }
}

TEST(fxcrt, BidiStringAllLeft) {
  {
    constexpr wchar_t kStr[] = {kLeftChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(0u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(1u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    EXPECT_EQ(it, bidi.end());
  }
  {
    constexpr wchar_t kStr[] = {kLeftChar, kLeftChar, kLeftChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(0u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(3u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    EXPECT_EQ(it, bidi.end());
  }
}

TEST(fxcrt, BidiStringAllLeftWeak) {
  {
    constexpr wchar_t kStr[] = {kLeftWeakChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(0u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

    ++it;
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(1u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeftWeak, it->direction);

    ++it;
    EXPECT_EQ(it, bidi.end());
  }
  {
    constexpr wchar_t kStr[] = {kLeftWeakChar, kLeftWeakChar, kLeftWeakChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

    auto it = bidi.begin();
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(0u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

    ++it;
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(3u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kLeftWeak, it->direction);

    ++it;
    EXPECT_EQ(it, bidi.end());
  }
}

TEST(fxcrt, BidiStringAllRight) {
  {
    constexpr wchar_t kStr[] = {kRightChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kRight, bidi.OverallDirection());

    auto it = bidi.begin();
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(1u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(0u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    EXPECT_EQ(it, bidi.end());
  }
  {
    constexpr wchar_t kStr[] = {kRightChar, kRightChar, kRightChar, 0};
    CFX_BidiString bidi(kStr, /*auto_order=*/true);
    EXPECT_EQ(CFX_BidiChar::Direction::kRight, bidi.OverallDirection());

    auto it = bidi.begin();
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(3u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    ASSERT_NE(it, bidi.end());
    EXPECT_EQ(0u, it->start);
    EXPECT_EQ(0u, it->count);
    EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
    ASSERT_NE(it, bidi.end());

    ++it;
    EXPECT_EQ(it, bidi.end());
  }
}

TEST(fxcrt, BidiStringLeftNeutralLeftRight) {
  constexpr wchar_t kStr[] = {kLeftChar, kNeutralChar, kLeftChar, kRightChar,
                              0};
  CFX_BidiString bidi(kStr, /*auto_order=*/true);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

  auto it = bidi.begin();
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(0u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(1u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(2u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(3u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(it, bidi.end());
}

TEST(fxcrt, BidiStringRightNeutralLeftRight) {
  constexpr wchar_t kStr[] = {kRightChar, kNeutralChar, kLeftChar, kRightChar,
                              0};
  CFX_BidiString bidi(kStr, /*auto_order=*/true);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, bidi.OverallDirection());

  auto it = bidi.begin();
  EXPECT_EQ(3u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(2u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(1u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(0u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);
  ASSERT_NE(it, bidi.end());

  ++it;
  EXPECT_EQ(it, bidi.end());
}

TEST(fxcrt, BidiStringRightLeftWeakLeftRight) {
  constexpr wchar_t kStr[] = {kRightChar, kLeftWeakChar, kLeftChar, kRightChar,
                              0};
  CFX_BidiString bidi(kStr, /*auto_order=*/true);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, bidi.OverallDirection());

  auto it = bidi.begin();
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(3u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(2u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(1u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeftWeak, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(0u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  EXPECT_EQ(it, bidi.end());
}

TEST(fxcrt, BidiStringReverse) {
  constexpr wchar_t kStr[] = {kLeftChar, kNeutralChar, kRightChar,
                              kLeftWeakChar, kLeftChar, 0};
  CFX_BidiString bidi(kStr, /*auto_order=*/true);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());
  bidi.SetOverallDirectionRight();

  auto it = bidi.begin();
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(4u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(3u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeftWeak, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(2u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(1u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(0u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  EXPECT_EQ(it, bidi.end());
}

TEST(fxcrt, BidiStringLeftRightEqual) {
  constexpr wchar_t kStr[] = {kLeftChar, kRightChar, 0};
  CFX_BidiString bidi(kStr, /*auto_order=*/true);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

  auto it = bidi.begin();
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(0u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(1u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);

  ++it;
  EXPECT_EQ(it, bidi.end());
}

TEST(fxcrt, BidiStringNoAutoOrder) {
  // Line with 2 RTL and 1 LTR segments.
  constexpr wchar_t kStr[] = {kRightChar, kRightChar,   kNeutralChar,
                              kLeftChar,  kNeutralChar, kRightChar,
                              0};
  CFX_BidiString bidi(kStr, /*auto_order=*/false);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, bidi.OverallDirection());

  auto it = bidi.begin();
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(0u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(0u, it->start);
  EXPECT_EQ(2u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(2u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(3u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kLeft, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(4u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kNeutral, it->direction);

  ++it;
  ASSERT_NE(it, bidi.end());
  EXPECT_EQ(5u, it->start);
  EXPECT_EQ(1u, it->count);
  EXPECT_EQ(CFX_BidiChar::Direction::kRight, it->direction);

  ++it;
  EXPECT_EQ(it, bidi.end());
}
