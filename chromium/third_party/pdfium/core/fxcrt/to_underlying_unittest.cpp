// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/to_underlying.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace fxcrt {

TEST(ToUnderlyingTest, Basic) {
  enum class E { kA = 1, kB = 2 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1), to_underlying(E::kA));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(2), to_underlying(E::kB));
}

TEST(ToUnderlyingTest, NonClassEnum) {
  enum E { kA = 1, kB = 2 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1), to_underlying(kA));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(2), to_underlying(kB));
}

TEST(ToUnderlyingTest, UnderlyingType) {
  enum class E : int { kA = 1, kB = 2 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1), to_underlying(E::kA));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(2), to_underlying(E::kB));
}

TEST(ToUnderlyingTest, OutOfOrderIndices) {
  enum class E { kA = 1, kB = 67 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1), to_underlying(E::kA));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(67), to_underlying(E::kB));
}

TEST(ToUnderlyingTest, NegativeValues) {
  enum class E : int { kNeg = -1, kZero = 0, kPos = 1 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(-1), to_underlying(E::kNeg));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(0), to_underlying(E::kZero));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1), to_underlying(E::kPos));
}

TEST(ToUnderlyingTest, LargeValues) {
  enum class E : unsigned long long {
    kMax = 0xFFFFFFFFFFFFFFFF,
    kMid = 1234567890123456789ULL
  };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(0xFFFFFFFFFFFFFFFFULL),
            to_underlying(E::kMax));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1234567890123456789ULL),
            to_underlying(E::kMid));
}

TEST(ToUnderlyingTest, CustomUnderlyingType) {
  enum class E : char { kA = 1, kB = 127 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(1), to_underlying(E::kA));
  EXPECT_EQ(static_cast<std::underlying_type_t<E>>(127), to_underlying(E::kB));
}

TEST(ToUnderlyingTest, MultipleEnums) {
  enum class E1 { kA = 1 };
  enum class E2 { kB = 2 };
  EXPECT_EQ(static_cast<std::underlying_type_t<E1>>(1), to_underlying(E1::kA));
  EXPECT_EQ(static_cast<std::underlying_type_t<E2>>(2), to_underlying(E2::kB));
}

}  // namespace fxcrt
