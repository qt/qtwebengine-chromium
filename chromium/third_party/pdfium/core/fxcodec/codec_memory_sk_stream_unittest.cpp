// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcodec/codec_memory_sk_stream.h"

#include <stdint.h>

#include "core/fxcodec/cfx_codec_memory.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/span.h"
#include "core/fxcrt/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fxcodec {

TEST(CodecMemorySkStreamTest, ReadAndEof) {
  constexpr uint8_t kData[] = {1, 2, 3, 4, 5};
  auto memory = pdfium::MakeRetain<CFX_CodecMemory>(sizeof(kData));
  fxcrt::Copy(kData, memory->GetBufferSpan());

  CodecMemorySkStream stream(memory);
  EXPECT_TRUE(stream.hasPosition());
  EXPECT_TRUE(stream.hasLength());
  EXPECT_EQ(stream.getLength(), 5u);
  EXPECT_EQ(stream.getPosition(), 0u);
  EXPECT_FALSE(stream.isAtEnd());

  uint8_t buf[10] = {};
  EXPECT_EQ(stream.read(buf, 2), 2u);
  EXPECT_EQ(buf[0], 1);
  EXPECT_EQ(buf[1], 2);
  EXPECT_EQ(stream.getPosition(), 2u);
  EXPECT_FALSE(stream.isAtEnd());

  EXPECT_EQ(stream.read(buf, 10), 3u);
  EXPECT_EQ(buf[0], 3);
  EXPECT_EQ(buf[1], 4);
  EXPECT_EQ(buf[2], 5);
  EXPECT_EQ(stream.getPosition(), 5u);
  EXPECT_TRUE(stream.isAtEnd());

  EXPECT_EQ(stream.read(buf, 2), 0u);
  EXPECT_TRUE(stream.isAtEnd());
}

TEST(CodecMemorySkStreamTest, SeekAndRewind) {
  constexpr uint8_t kData[] = {10, 20, 30, 40, 50};
  auto memory = pdfium::MakeRetain<CFX_CodecMemory>(sizeof(kData));
  fxcrt::Copy(kData, memory->GetBufferSpan());

  CodecMemorySkStream stream(memory);
  uint8_t buf[2] = {};
  EXPECT_EQ(stream.read(buf, 2), 2u);
  EXPECT_EQ(stream.getPosition(), 2u);

  EXPECT_TRUE(stream.rewind());
  EXPECT_EQ(stream.getPosition(), 0u);
  EXPECT_FALSE(stream.isAtEnd());

  EXPECT_TRUE(stream.seek(3));
  EXPECT_EQ(stream.getPosition(), 3u);
  EXPECT_EQ(stream.read(buf, 1), 1u);
  EXPECT_EQ(buf[0], 40);
}

}  // namespace fxcodec
