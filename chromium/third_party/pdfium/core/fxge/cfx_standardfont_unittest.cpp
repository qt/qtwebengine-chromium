// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_standardfont.h"

#include <optional>

#include "core/fxcrt/bytestring.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CFXStandardFontTest, IsStandardFontName) {
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Courier"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Courier-Bold"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Courier-BoldOblique"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Courier-Oblique"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Helvetica"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Helvetica-Bold"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Helvetica-BoldOblique"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Helvetica-Oblique"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Times-Roman"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Times-Bold"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Times-BoldItalic"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Times-Italic"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("Symbol"));
  EXPECT_TRUE(CFX_StandardFont::IsStandardFontName("ZapfDingbats"));

  EXPECT_FALSE(CFX_StandardFont::IsStandardFontName("Courie"));
  EXPECT_FALSE(CFX_StandardFont::IsStandardFontName("Courier-"));
  EXPECT_FALSE(CFX_StandardFont::IsStandardFontName("Helvetica+Bold"));
  EXPECT_FALSE(CFX_StandardFont::IsStandardFontName("YapfDingbats"));
}

TEST(CFXStandardFontTest, GetStandardFontIndex) {
  // Canonical names
  EXPECT_EQ(CFX_StandardFont::kCourier,
            CFX_StandardFont::GetStandardFontIndex("Courier"));
  EXPECT_EQ(CFX_StandardFont::kTimesBold,
            CFX_StandardFont::GetStandardFontIndex("Times-Bold"));
  EXPECT_EQ(CFX_StandardFont::kDingbats,
            CFX_StandardFont::GetStandardFontIndex("ZapfDingbats"));

  // Alternates (aliases)
  EXPECT_EQ(CFX_StandardFont::kHelvetica,
            CFX_StandardFont::GetStandardFontIndex("Arial"));
  EXPECT_EQ(CFX_StandardFont::kCourier,
            CFX_StandardFont::GetStandardFontIndex("CourierNew"));
  EXPECT_EQ(CFX_StandardFont::kTimesBold,
            CFX_StandardFont::GetStandardFontIndex("TimesNewRoman,Bold"));
  EXPECT_EQ(CFX_StandardFont::kHelvetica,
            CFX_StandardFont::GetStandardFontIndex("arial"));

  // Non-standard
  EXPECT_FALSE(CFX_StandardFont::GetStandardFontIndex("ComicSans").has_value());
}

TEST(CFXStandardFontTest, GetCanonicalFontName) {
  EXPECT_EQ("Courier",
            CFX_StandardFont::GetCanonicalFontName(CFX_StandardFont::kCourier));
  EXPECT_EQ("Helvetica-Bold", CFX_StandardFont::GetCanonicalFontName(
                                  CFX_StandardFont::kHelveticaBold));
  EXPECT_EQ("ZapfDingbats", CFX_StandardFont::GetCanonicalFontName(
                                CFX_StandardFont::kDingbats));
}

TEST(CFXStandardFontTest, IsSymbolicFont) {
  EXPECT_TRUE(CFX_StandardFont::IsSymbolicFont(CFX_StandardFont::kSymbol));
  EXPECT_TRUE(CFX_StandardFont::IsSymbolicFont(CFX_StandardFont::kDingbats));

  EXPECT_FALSE(CFX_StandardFont::IsSymbolicFont(CFX_StandardFont::kCourier));
  EXPECT_FALSE(CFX_StandardFont::IsSymbolicFont(CFX_StandardFont::kHelvetica));
  EXPECT_FALSE(CFX_StandardFont::IsSymbolicFont(CFX_StandardFont::kTimes));
}

TEST(CFXStandardFontTest, IsFixedFont) {
  EXPECT_TRUE(CFX_StandardFont::IsFixedFont(CFX_StandardFont::kCourier));
  EXPECT_TRUE(CFX_StandardFont::IsFixedFont(CFX_StandardFont::kCourierBold));
  EXPECT_TRUE(
      CFX_StandardFont::IsFixedFont(CFX_StandardFont::kCourierBoldOblique));
  EXPECT_TRUE(CFX_StandardFont::IsFixedFont(CFX_StandardFont::kCourierOblique));

  EXPECT_FALSE(CFX_StandardFont::IsFixedFont(CFX_StandardFont::kHelvetica));
  EXPECT_FALSE(CFX_StandardFont::IsFixedFont(CFX_StandardFont::kTimes));
  EXPECT_FALSE(CFX_StandardFont::IsFixedFont(CFX_StandardFont::kSymbol));
}
