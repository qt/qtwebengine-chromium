// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/cfx_read_only_container_stream.h"

#include <stdint.h>

#include <utility>

#include "build/build_config.h"
#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fixed_size_data_vector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <unistd.h>
#include "core/fxcrt/bytestring.h"
#include "testing/temporary_file_test.h"
#endif

TEST(CFX_ReadOnlyContainerStreamTest, FromDataVector) {
  DataVector<uint8_t> data = {'a', 'b', 'c', 'd'};
  auto stream =
      pdfium::MakeRetain<CFX_ReadOnlyDataVectorStream>(std::move(data));
  EXPECT_EQ(4, stream->GetSize());

  uint8_t buffer[2];
  EXPECT_TRUE(stream->ReadBlockAtOffset(buffer, 1));
  EXPECT_THAT(buffer, testing::ElementsAre('b', 'c'));

  EXPECT_THAT(stream->span(), testing::ElementsAre('a', 'b', 'c', 'd'));
}

TEST(CFX_ReadOnlyContainerStreamTest, FromFixedSizeDataVector) {
  auto data = FixedSizeDataVector<uint8_t>::Uninit(4);
  data.span()[0] = 'e';
  data.span()[1] = 'f';
  data.span()[2] = 'g';
  data.span()[3] = 'h';

  auto stream = pdfium::MakeRetain<CFX_ReadOnlyFixedSizeDataVectorStream>(
      std::move(data));
  EXPECT_EQ(4, stream->GetSize());

  uint8_t buffer[2];
  EXPECT_TRUE(stream->ReadBlockAtOffset(buffer, 2));
  EXPECT_THAT(buffer, testing::ElementsAre('g', 'h'));

  EXPECT_THAT(stream->span(), testing::ElementsAre('e', 'f', 'g', 'h'));
}

TEST(CFX_ReadOnlyContainerStreamTest, OutOfBoundsRead) {
  DataVector<uint8_t> data = {'a', 'b', 'c'};
  auto stream =
      pdfium::MakeRetain<CFX_ReadOnlyDataVectorStream>(std::move(data));

  uint8_t buffer[2];
  EXPECT_FALSE(stream->ReadBlockAtOffset(buffer, 2));
  EXPECT_FALSE(stream->ReadBlockAtOffset(buffer, 3));
  EXPECT_FALSE(stream->ReadBlockAtOffset(buffer, 4));
}

#if BUILDFLAG(IS_POSIX)
class CFX_ReadOnlyTemporaryTest : public TemporaryFileTest {};

TEST_F(CFX_ReadOnlyTemporaryTest, FromFile) {
  static const uint8_t kData[] = {'f', 'i', 'l', 'e', 's'};
  WriteAndClose(kData);

  auto stream = pdfium::MakeRetain<CFX_ReadOnlyMappedDataBytesStream>(
      MappedDataBytes::Create(temp_name()));

  ASSERT_TRUE(stream);
  EXPECT_EQ(5, stream->GetSize());
  EXPECT_THAT(stream->span(), testing::ElementsAre('f', 'i', 'l', 'e', 's'));
}
#endif
