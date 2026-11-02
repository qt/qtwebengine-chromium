// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the cross-platform font mapping and selection logic of
// CFX_AndroidFontInfo. It is compiled and run on Linux host tests as the
// logic does not depend on Android-specific system APIs.

#include "core/fxge/android/cfx_androidfontinfo.h"

#include <memory>
#include <utility>

#include "core/fxcrt/fx_codepage.h"
#include "core/fxge/fx_font.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kArial[] = "Arial";
constexpr char kDroidSansFallback[] = "Droid Sans Fallback";
constexpr char kRoboto[] = "Roboto";
constexpr char kSimSun[] = "SimSun";

}  // namespace

class CFXAndroidFontInfoTest : public ::testing::Test {
 public:
  CFXAndroidFontInfoTest() = default;

  void* MapFont(int weight,
                bool bItalic,
                FX_Charset charset,
                int pitch_family,
                const char* family) {
    return font_info_.MapFont(nullptr, weight, bItalic, charset, pitch_family,
                              family);
  }

  ByteString GetFaceName(void* font) {
    return static_cast<CFX_AndroidFontInfo::FontFaceInfo*>(font)->face_name_;
  }

  void AddDummyFont(const char* font_name,
                    FX_CharsetFlag charset,
                    uint32_t glyph_count = 0,
                    uint32_t styles = 0) {
    auto info = std::make_unique<CFX_AndroidFontInfo::FontFaceInfo>(
        /*filePath=*/"", font_name, /*fontTables=*/"",
        /*fontOffset=*/0, /*fileSize=*/0);
    info->charsets_ = charset;
    info->glyph_count_ = glyph_count;
    info->styles_ = styles;
    font_info_.font_list_[font_name] = std::move(info);
  }

 private:
  CFX_AndroidFontInfo font_info_;
};

TEST_F(CFXAndroidFontInfoTest, MapFontPreferOriginalNonCJK) {
  // Install both original (Arial) and substitution (Roboto).
  AddDummyFont(kArial, FX_CharsetFlag::kANSI);
  AddDummyFont(kRoboto, FX_CharsetFlag::kANSI);

  // Map "Arial" -> should prefer "Arial" over "Roboto" (subst).
  void* font = MapFont(/*weight=*/400, /*bItalic=*/false, FX_Charset::kANSI,
                       pdfium::kFontPitchFamilyRoman, kArial);
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), kArial);
}

TEST_F(CFXAndroidFontInfoTest, MapFontFallbackToSubstNonCJK) {
  // Install only substitution (Roboto).
  AddDummyFont(kRoboto, FX_CharsetFlag::kANSI);

  // Map "Arial" -> should fallback to "Roboto".
  void* font = MapFont(/*weight=*/400, /*bItalic=*/false, FX_Charset::kANSI,
                       pdfium::kFontPitchFamilyRoman, kArial);
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), kRoboto);
}

TEST_F(CFXAndroidFontInfoTest, MapFontPreferOriginalCJK) {
  // Install both original (SimSun) and substitution (Droid Sans Fallback).
  // Droid Sans Fallback might have more glyphs, but we should prefer original.
  AddDummyFont(kSimSun, FX_CharsetFlag::kChineseSimplified,
               /*glyph_count=*/1000);
  AddDummyFont(kDroidSansFallback, FX_CharsetFlag::kChineseSimplified,
               /*glyph_count=*/2000);

  // Map "SimSun" -> should prefer "SimSun" (original) over "Droid Sans
  // Fallback" (subst).
  void* font =
      MapFont(/*weight=*/400, /*bItalic=*/false, FX_Charset::kChineseSimplified,
              pdfium::kFontPitchFamilyRoman, kSimSun);
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), kSimSun);
}

TEST_F(CFXAndroidFontInfoTest, MapFontFallbackToSubstCJK) {
  // Install only substitution (Droid Sans Fallback).
  AddDummyFont(kDroidSansFallback, FX_CharsetFlag::kChineseSimplified,
               /*glyph_count=*/2000);

  // Map "SimSun" -> should fallback to "Droid Sans Fallback".
  void* font =
      MapFont(/*weight=*/400, /*bItalic=*/false, FX_Charset::kChineseSimplified,
              pdfium::kFontPitchFamilyRoman, kSimSun);
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), kDroidSansFallback);
}

TEST_F(CFXAndroidFontInfoTest, MapFontCJKFallbackNoMatch) {
  // Install some other CJK font (e.g. Droid Sans Fallback).
  AddDummyFont(kDroidSansFallback, FX_CharsetFlag::kChineseSimplified,
               /*glyph_count=*/2000);

  // Map some random CJK name that has no subst rule (e.g. "MyCJKFont").
  // Since must_match_name will eventually be false, it should find Droid Sans
  // Fallback.
  void* font =
      MapFont(/*weight=*/400, /*bItalic=*/false, FX_Charset::kChineseSimplified,
              pdfium::kFontPitchFamilyRoman, "MyCJKFont");
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), kDroidSansFallback);
}

TEST_F(CFXAndroidFontInfoTest, MapFontCJKDifferentWeights) {
  // Install "SimSun" (Regular) and "SimSun Bold".
  AddDummyFont("SimSun", FX_CharsetFlag::kChineseSimplified, 1000, 0);
  AddDummyFont("SimSun Bold", FX_CharsetFlag::kChineseSimplified, 1000,
               pdfium::kFontStyleForceBold);

  // Map "SimSun" with bold weight (700) -> should prefer "SimSun Bold".
  void* font =
      MapFont(/*weight=*/700, /*bItalic=*/false, FX_Charset::kChineseSimplified,
              pdfium::kFontPitchFamilyRoman, "SimSun");
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), "SimSun Bold");
}

TEST_F(CFXAndroidFontInfoTest, MapFontCJKDifferentGlyphCountsNoNameMatch) {
  // Install two non-matching CJK fonts with different glyph counts.
  AddDummyFont("CJK Font 1", FX_CharsetFlag::kChineseSimplified, 1000);
  AddDummyFont("CJK Font 2", FX_CharsetFlag::kChineseSimplified, 2000);

  // Map "MyCJKFont" (no match) -> should prefer CJK Font 2 (more glyphs).
  void* font =
      MapFont(/*weight=*/400, /*bItalic=*/false, FX_Charset::kChineseSimplified,
              pdfium::kFontPitchFamilyRoman, "MyCJKFont");
  ASSERT_TRUE(font);
  EXPECT_EQ(GetFaceName(font), "CJK Font 2");
}
