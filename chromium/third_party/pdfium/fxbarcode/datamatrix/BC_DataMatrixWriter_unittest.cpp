// Copyright 2018 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fxbarcode/datamatrix/BC_DataMatrixWriter.h"

#include <stdint.h>

#include "core/fxcrt/data_vector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

class CBCDataMatrixWriterTest : public testing::Test {
 public:
  CBCDataMatrixWriterTest() = default;
  ~CBCDataMatrixWriterTest() override = default;

  // testing::Test:
  void SetUp() override { BC_Library_Init(); }
  void TearDown() override { BC_Library_Destroy(); }
};

TEST_F(CBCDataMatrixWriterTest, Encode) {
  CBC_DataMatrixWriter writer;

  {
    static constexpr int kExpectedDimension = 10;
    // clang-format off
    static constexpr uint8_t kExpectedData[] = {
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
        1, 1, 0, 1, 1, 0, 1, 0, 0, 1,
        1, 1, 0, 1, 0, 0, 0, 0, 1, 0,
        1, 1, 1, 1, 0, 0, 0, 1, 0, 1,
        1, 0, 1, 0, 0, 0, 1, 0, 0, 0,
        1, 1, 1, 0, 1, 0, 0, 0, 0, 1,
        1, 0, 0, 1, 0, 1, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 1, 1, 1, 0, 1,
        1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    // clang-format on
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(L"");
    ASSERT_EQ(std::size(kExpectedData), result.code.size());
    ASSERT_EQ(kExpectedDimension, result.width);
    ASSERT_EQ(kExpectedDimension, result.height);
    EXPECT_THAT(result.code, ElementsAreArray(kExpectedData));
  }
  {
    static constexpr int kExpectedDimension = 14;
    // clang-format off
    static constexpr uint8_t kExpectedData[] = {
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
        1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1,
        1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0,
        1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
        1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0,
        1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1,
        1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1,
        1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0,
        1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    // clang-format on
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(L"helloworld");
    ASSERT_EQ(std::size(kExpectedData), result.code.size());
    ASSERT_EQ(kExpectedDimension, result.width);
    ASSERT_EQ(kExpectedDimension, result.height);
    EXPECT_THAT(result.code, ElementsAreArray(kExpectedData));
  }
  {
    static constexpr int kExpectedDimension = 10;
    // clang-format off
    static constexpr uint8_t kExpectedData[] = {
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
        1, 1, 0, 1, 1, 0, 0, 1, 1, 1,
        1, 1, 0, 0, 0, 1, 0, 1, 1, 0,
        1, 1, 0, 0, 1, 1, 0, 1, 0, 1,
        1, 1, 0, 0, 1, 1, 1, 0, 0, 0,
        1, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        1, 1, 0, 1, 0, 1, 1, 1, 1, 0,
        1, 1, 1, 0, 0, 0, 0, 1, 1, 1,
        1, 1, 0, 1, 1, 0, 0, 1, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    // clang-format on
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(L"12345");
    ASSERT_EQ(std::size(kExpectedData), result.code.size());
    ASSERT_EQ(kExpectedDimension, result.width);
    ASSERT_EQ(kExpectedDimension, result.height);
    EXPECT_THAT(result.code, ElementsAreArray(kExpectedData));
  }
  {
    static constexpr int kExpectedDimension = 18;
    // clang-format off
    static constexpr uint8_t kExpectedData[] = {
        1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1,
        1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0,
        1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
        1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0,
        1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1,
        1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1,
        1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0,
        1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1,
        1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1,
        1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0,
        1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1,
        1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0,
        1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
        1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    // clang-format on
    CBC_TwoDimWriter::EncodeResult result =
        writer.Encode(L"abcdefghijklmnopqrst");
    ASSERT_EQ(std::size(kExpectedData), result.code.size());
    ASSERT_EQ(kExpectedDimension, result.width);
    ASSERT_EQ(kExpectedDimension, result.height);
    EXPECT_THAT(result.code, ElementsAreArray(kExpectedData));
  }
  {
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(L"hello world");
    ASSERT_TRUE(result.code.empty());
    EXPECT_EQ(result.height, 0);
    EXPECT_EQ(result.width, 0);
  }
}

TEST_F(CBCDataMatrixWriterTest, EncodeLimitAlphaNumeric) {
  CBC_DataMatrixWriter writer;

  static constexpr int kMaxInputLength = 2335;  // Per spec.
  WideString input;
  for (size_t i = 0; i < kMaxInputLength; ++i) {
    input.InsertAtBack(L'a');
  }

  {
    static constexpr int kExpectedDimension = 144;
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(input.c_str());
    EXPECT_EQ(20736u, result.code.size());
    EXPECT_EQ(kExpectedDimension, result.width);
    EXPECT_EQ(kExpectedDimension, result.height);
  }

  // Go over the limit.
  input.InsertAtBack(L'a');
  {
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(input.c_str());
    EXPECT_EQ(0u, result.code.size());
    EXPECT_EQ(0, result.width);
    EXPECT_EQ(0, result.height);
  }
}

TEST_F(CBCDataMatrixWriterTest, EncodeLimitNumbers) {
  CBC_DataMatrixWriter writer;

  static constexpr int kMaxInputLength = 3116;  // Per spec.
  WideString input;
  for (size_t i = 0; i < kMaxInputLength; ++i) {
    input.InsertAtBack(L'1');
  }

  {
    static constexpr int kExpectedDimension = 144;
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(input.c_str());
    EXPECT_EQ(20736u, result.code.size());
    EXPECT_EQ(kExpectedDimension, result.width);
    EXPECT_EQ(kExpectedDimension, result.height);
  }

  // Go over the limit.
  input.InsertAtBack(L'1');
  {
    CBC_TwoDimWriter::EncodeResult result = writer.Encode(input.c_str());
    EXPECT_EQ(0u, result.code.size());
    EXPECT_EQ(0, result.width);
    EXPECT_EQ(0, result.height);
  }
}
