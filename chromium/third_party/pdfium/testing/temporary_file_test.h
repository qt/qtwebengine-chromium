// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_TEMPORARY_FILE_TEST_H_
#define TESTING_TEMPORARY_FILE_TEST_H_

#include <string>

#include "build/build_config.h"
#include "core/fxcrt/span.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_POSIX)
#error "Built on wrong platform"
#endif

class TemporaryFileTest : public testing::Test {
 public:
  TemporaryFileTest();
  ~TemporaryFileTest() override;

  void SetUp() override;
  void TearDown() override;

  void WriteAndClose(pdfium::span<const uint8_t> data);

  const char* temp_name() const { return temp_name_.c_str(); }

 private:
  std::string temp_name_;
  int fd_;
};

#endif  // TESTING_TEMPORARY_FILE_TEST_H_
