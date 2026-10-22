// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/mapped_data_bytes.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>

#include "build/build_config.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/temporary_file_test.h"

#if !BUILDFLAG(IS_POSIX)
#error "Built on wrong platform"
#endif

namespace fxcrt {

class MappedDataBytesTest : public TemporaryFileTest {};

TEST_F(MappedDataBytesTest, CreateNotFound) {
  auto mapping = MappedDataBytes::Create("non_existent_file_asdfghjkl");
  EXPECT_FALSE(mapping);
}

TEST_F(MappedDataBytesTest, CreateEmpty) {
  WriteAndClose({});

  auto mapping = MappedDataBytes::Create(temp_name());
  ASSERT_TRUE(mapping);
  EXPECT_TRUE(mapping->empty());
  EXPECT_EQ(0u, mapping->size());
  EXPECT_TRUE(mapping->span().empty());
}

TEST_F(MappedDataBytesTest, CreateNormal) {
  static const uint8_t kData[] = {'h', 'e', 'l', 'l', 'o'};
  WriteAndClose(kData);

  auto mapping = MappedDataBytes::Create(temp_name());
  ASSERT_TRUE(mapping);
  EXPECT_FALSE(mapping->empty());
  EXPECT_EQ(sizeof(kData), mapping->size());
  EXPECT_EQ(mapping->span(), pdfium::span(kData));
}

}  // namespace fxcrt
