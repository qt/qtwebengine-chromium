// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/bit_vector.h"

#include <algorithm>

#include "gtest/gtest.h"
#include "platform/base/span.h"

namespace openscreen {
namespace {

constexpr uint8_t kBitPatterns[] = {0b00000000, 0b11111111, 0b01010101,
                                    0b10101010, 0b00100100, 0b01001001,
                                    0b10010010, 0b00110110};

// These are used for testing various vector sizes, begins/ends of ranges, etc.
// They will exercise both the small case and the multiple integer case.
const size_t kTestSizes[] = {0,  1,  2,  3,  5,  7,  11, 13, 17,  19,
                             23, 29, 31, 37, 41, 43, 47, 53, 59,  61,
                             64, 67, 71, 73, 79, 83, 89, 97, 127, 128};

// Returns a subspan of `kTestSizes` that contains all values in the range
// [first,last].
Span<const size_t> GetTestSizesInRange(size_t first, size_t last) {
  const auto begin =
      std::lower_bound(std::cbegin(kTestSizes), std::cend(kTestSizes), first);
  const auto end =
      std::upper_bound(std::cbegin(kTestSizes), std::cend(kTestSizes), last);
  return Span<const size_t>(&*begin, std::distance(begin, end));
}

// Returns true if an infinitely-repeating `pattern` has a bit set at the given
// `position`.
constexpr bool IsSetInPattern(uint8_t pattern, size_t position) {
  constexpr size_t kRepeatPeriod = sizeof(pattern) * CHAR_BIT;
  return !!((pattern >> (position % kRepeatPeriod)) & 1);
}

// Fills an infinitely-repeating `pattern` in `v`, but only modifies the bits at
// and after the given `from` position.
void FillWithPattern(uint8_t pattern, size_t from, BitVector* v) {
  for (size_t i = from; i < v->size(); ++i) {
    if (IsSetInPattern(pattern, i)) {
      v->Set(i);
    } else {
      v->Clear(i);
    }
  }
}

// Tests that construction and resizes initialize the vector to the correct size
// and set or clear all of its bits, as requested.
TEST(BitVectorTest, ConstructsAndResizes) {
  BitVector v;
  ASSERT_EQ(v.size(), 0u);

  for (int fill_set = 0; fill_set <= 1; ++fill_set) {
    for (size_t size : kTestSizes) {
      const bool all_bits_should_be_set = !!fill_set;
      v.Resize(size,
               all_bits_should_be_set ? BitVector::SET : BitVector::CLEARED);
      ASSERT_EQ(size, v.size());
      for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(all_bits_should_be_set, v.IsSet(i));
      }
    }
  }
}

// Tests that individual bits can be set and cleared for various vector sizes
// and bit patterns.
TEST(BitVectorTest, SetsAndClearsIndividualBits) {
  BitVector v;
  for (int fill_set = 0; fill_set <= 1; ++fill_set) {
    for (size_t size : kTestSizes) {
      v.Resize(size, fill_set ? BitVector::SET : BitVector::CLEARED);

      for (uint8_t pattern : kBitPatterns) {
        FillWithPattern(pattern, 0, &v);
        for (size_t i = 0; i < size; ++i) {
          ASSERT_EQ(IsSetInPattern(pattern, i), v.IsSet(i));
        }
      }
    }
  }
}

// Tests the FindFirstSet() operation, for various vector sizes and bit
// patterns.
TEST(BitVectorTest, FindsTheFirstBitSet) {
  BitVector v;

  // For various sizes of vector where no bits are set, the FFS operation should
  // always return size().
  for (size_t size : kTestSizes) {
    v.Resize(size, BitVector::CLEARED);
    ASSERT_EQ(size, v.FindFirstSet());
  }

  // For various sizes of vector where only one bit is set, the FFS operation
  // should always return the position of that bit.
  for (size_t size : kTestSizes) {
    v.Resize(size, BitVector::CLEARED);

    for (size_t position_plus_one : GetTestSizesInRange(1, size)) {
      const size_t position = position_plus_one - 1;
      v.Set(position);
      ASSERT_EQ(position, v.FindFirstSet());
      v.Clear(position);
    }
  }

  // For various sizes of vector where a pattern of bits are set, the FFS
  // operation should always return the first one set.
  for (size_t size : kTestSizes) {
    v.Resize(size, BitVector::CLEARED);

    for (size_t position_plus_one : GetTestSizesInRange(1, size)) {
      const size_t position = position_plus_one - 1;
      v.Resize(size, BitVector::CLEARED);
      v.Set(position);
      for (uint8_t pattern : kBitPatterns) {
        FillWithPattern(pattern, position_plus_one, &v);
        ASSERT_EQ(position, v.FindFirstSet());
      }
      v.Clear(position);
    }
  }
}

}  // namespace
}  // namespace openscreen
