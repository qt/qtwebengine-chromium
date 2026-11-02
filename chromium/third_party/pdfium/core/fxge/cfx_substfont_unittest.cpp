// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_substfont.h"

#include "core/fxge/fx_font.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CFXSubstFontTest, EffectiveSkew) {
  CFX_SubstFont font;
  font.SetItalicAngle(-12);
  EXPECT_EQ(-21, font.GetEffectiveSkew(/*font_style=*/false));

  font.SetSubstCJK(true);
  font.SetItalicCJK(true);
  EXPECT_EQ(-27, font.GetEffectiveSkew(/*font_style=*/true));
}

TEST(CFXSubstFontTest, EffectiveWeight) {
  CFX_SubstFont font;
  font.SetWeight(400);
  font.SetWeightCJK(700);
  font.SetSubstCJK(true);

  EXPECT_EQ(400, font.GetEffectiveWeight(/*font_style=*/false));
  EXPECT_EQ(700, font.GetEffectiveWeight(/*font_style=*/true));
}

TEST(CFXSubstFontTest, EmboldenLevels) {
  CFX_SubstFont font;
  font.SetWeight(700);  // index = (700-400)/10 = 30

  EXPECT_EQ(font.GetEmboldenLevelForRender(/*font_style=*/false, 1024, 0), 1);
  EXPECT_EQ(font.GetEmboldenLevelForLoad(), 70);

  // Test intermediate overflow handling (should not overflow final result).
  // 39 * (30000000 + 30000000) = 2,340,000,000 > INT_MAX (2,147,483,647)
  // 2,340,000,000 / 36655 = 63838
  EXPECT_EQ(63838, font.GetEmboldenLevelForRender(/*font_style=*/false,
                                                  30000000, 30000000));

  font.SetIsBuiltInGenericFont();
  EXPECT_EQ(0, font.GetEmboldenLevelForRender(/*font_style=*/false, 1024, 0));
  EXPECT_EQ(0, font.GetEmboldenLevelForLoad());
}

TEST(CFXSubstFontTest, EstimatedStemV) {
  CFX_SubstFont font;
  font.SetWeight(700);
  EXPECT_EQ(140, font.GetEstimatedStemV());
}

TEST(CFXSubstFontTest, IsActualFontLoaded) {
  CFX_SubstFont font;
  font.SetFamily("Times New Roman");

  EXPECT_TRUE(font.IsActualFontLoaded("timesnewroman,bold"));
  EXPECT_TRUE(font.IsActualFontLoaded("timesnewromanps-bold"));
  EXPECT_FALSE(font.IsActualFontLoaded("arial,bold"));
}
