// Copyright 2018 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "core/fxcrt/byteorder.h"
#include "core/fxcrt/check.h"
#include "core/fxge/cfx_folderfontinfo.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/fx_font.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/path_service.h"

TEST(FXFontTest, UnicodeFromAdobeName) {
  EXPECT_EQ(static_cast<wchar_t>(0x0000), UnicodeFromAdobeName("nonesuch"));
  EXPECT_EQ(static_cast<wchar_t>(0x0000), UnicodeFromAdobeName(""));
  EXPECT_EQ(static_cast<wchar_t>(0x00b6), UnicodeFromAdobeName("paragraph"));
  EXPECT_EQ(static_cast<wchar_t>(0x00d3), UnicodeFromAdobeName("Oacute"));
  EXPECT_EQ(static_cast<wchar_t>(0x00fe), UnicodeFromAdobeName("thorn"));
  EXPECT_EQ(static_cast<wchar_t>(0x0384), UnicodeFromAdobeName("tonos"));
  EXPECT_EQ(static_cast<wchar_t>(0x2022), UnicodeFromAdobeName("bullet"));
}

TEST(FXFontTest, AdobeNameFromUnicode) {
  EXPECT_EQ("", AdobeNameFromUnicode(0x0000));
  EXPECT_EQ("divide", AdobeNameFromUnicode(0x00f7));
  EXPECT_EQ("Lslash", AdobeNameFromUnicode(0x0141));
  EXPECT_EQ("tonos", AdobeNameFromUnicode(0x0384));
  EXPECT_EQ("afii57513", AdobeNameFromUnicode(0x0691));
  EXPECT_EQ("angkhankhuthai", AdobeNameFromUnicode(0x0e5a));
  EXPECT_EQ("Euro", AdobeNameFromUnicode(0x20ac));
}

TEST(FXFontTest, ReadFontNameFromMicrosoftEntries) {
  std::string test_data_dir = PathService::GetTestDataDir();
  ASSERT_FALSE(test_data_dir.empty());

  CFX_FontMapper font_mapper;

  {
    // |folder_font_info| has to be deallocated before the |font_mapper| or we
    // run into UnownedPtr class issues with ASAN.
    CFX_FolderFontInfo folder_font_info;
    folder_font_info.AddPath(
        (test_data_dir + PATH_SEPARATOR + "font_tests").c_str());

    font_mapper.SetSystemFontInfo(
        CFX_GEModule::Get()->GetPlatform()->CreateDefaultSystemFontInfo());
    folder_font_info.EnumFontList(&font_mapper);
  }

  ASSERT_EQ(1u, font_mapper.GetFaceSize());
  ASSERT_EQ("Test", font_mapper.GetFaceName(0));
}

TEST(FXFontTest, FindFontTableLocation) {
  std::vector<uint8_t> table_dir = {
      // Table Directory Entry 1 ('head')
      'h', 'e', 'a', 'd', 0x00, 0x00, 0x00, 0x00,  // checksum
      0x00, 0x00, 0x00, 0x2C,                      // offset = 44
      0x00, 0x00, 0x00, 0x0A,                      // length = 10

      // Table Directory Entry 2 ('name')
      'n', 'a', 'm', 'e', 0x00, 0x00, 0x00, 0x00,  // checksum
      0x00, 0x00, 0x00, 0x36,                      // offset = 54
      0x00, 0x00, 0x00, 0x14,                      // length = 20
  };

  // Test finding 'head' tag
  uint32_t head_tag = CFX_FontMapper::MakeTag('h', 'e', 'a', 'd');
  std::optional<FontTableLocation> head_loc =
      FindFontTableLocation(table_dir, head_tag);
  ASSERT_TRUE(head_loc.has_value());
  EXPECT_EQ(44u, head_loc->offset);
  EXPECT_EQ(10u, head_loc->size);

  // Test finding 'name' tag
  uint32_t name_tag = CFX_FontMapper::MakeTag('n', 'a', 'm', 'e');
  std::optional<FontTableLocation> name_loc =
      FindFontTableLocation(table_dir, name_tag);
  ASSERT_TRUE(name_loc.has_value());
  EXPECT_EQ(54u, name_loc->offset);
  EXPECT_EQ(20u, name_loc->size);

  // Test 'gasp' tag (not found)
  uint32_t gasp_tag = CFX_FontMapper::MakeTag('g', 'a', 's', 'p');
  std::optional<FontTableLocation> gasp_loc =
      FindFontTableLocation(table_dir, gasp_tag);
  EXPECT_FALSE(gasp_loc.has_value());

  // Test truncated `table_dir` span
  pdfium::span<const uint8_t> truncated_span =
      pdfium::span<const uint8_t>(table_dir).first(20u);

  std::optional<FontTableLocation> head_loc_trunc =
      FindFontTableLocation(truncated_span, head_tag);
  ASSERT_TRUE(head_loc_trunc.has_value());
  EXPECT_EQ(44u, head_loc_trunc->offset);
  EXPECT_EQ(10u, head_loc_trunc->size);

  std::optional<FontTableLocation> name_loc_trunc =
      FindFontTableLocation(truncated_span, name_tag);
  EXPECT_FALSE(name_loc_trunc.has_value());

  // Test completely empty/too short span (less than 16 bytes)
  pdfium::span<const uint8_t> short_span =
      pdfium::span<const uint8_t>(table_dir).first(10u);
  std::optional<FontTableLocation> head_loc_short =
      FindFontTableLocation(short_span, head_tag);
  EXPECT_FALSE(head_loc_short.has_value());
}

TEST(FXFontTest, GetCodePageRangeFromOS2) {
  // OS/2 table version 1+ must be at least 86 bytes to contain codepage ranges
  std::vector<uint8_t> short_os2(85);
  EXPECT_EQ(0u, GetCodePageRangeFromOS2(short_os2));

  std::vector<uint8_t> mock_os2(86);
  // ulCodePageRange1 is at offset 78 (4 bytes)
  mock_os2[78] = 0x12;
  mock_os2[79] = 0x34;
  mock_os2[80] = 0x56;
  mock_os2[81] = 0x78;
  EXPECT_EQ(0x12345678u, GetCodePageRangeFromOS2(mock_os2));
}

TEST(FXFontTest, GetGlyphCountFromMaxp) {
  // maxp table must be at least 6 bytes to contain glyph count
  std::vector<uint8_t> short_maxp(5);
  EXPECT_EQ(0u, GetGlyphCountFromMaxp(short_maxp));

  std::vector<uint8_t> mock_maxp(6);
  // numGlyphs is at offset 4 (2 bytes)
  mock_maxp[4] = 0x12;
  mock_maxp[5] = 0x34;
  EXPECT_EQ(0x1234u, GetGlyphCountFromMaxp(mock_maxp));
}
