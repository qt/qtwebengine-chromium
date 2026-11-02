// Copyright 2021 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_TEST_FONTS_H_
#define TESTING_TEST_FONTS_H_

#include <optional>
#include <string>
#include <vector>

#include "core/fxcrt/span.h"

class TestFonts {
 public:
  TestFonts();
  ~TestFonts();

  const char** font_paths() { return font_paths_.data(); }
  std::optional<pdfium::span<const char* const>> FontPathsSpan() const;
  void InstallFontMapper();

  static std::string RenameFont(const char* face);

 private:
  std::string font_path_;
  std::vector<const char*> font_paths_;
};

#endif  // TESTING_TEST_FONTS_H_
