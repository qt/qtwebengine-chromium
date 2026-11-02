// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_color.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(CFX_ColorTest, OperatorMinusUnusedComponents) {
  // Construct a Gray color. Unused components (2, 3, 4) should be 0.0f.
  CFX_Color c1 = CFX_Color::MakeGray(0.2f);
  EXPECT_EQ(c1.nColorType, CFX_Color::Type::kGray);
  EXPECT_FLOAT_EQ(c1.fColor1, 0.2f);
  EXPECT_FLOAT_EQ(c1.fColor2, 0.0f);
  EXPECT_FLOAT_EQ(c1.fColor3, 0.0f);
  EXPECT_FLOAT_EQ(c1.fColor4, 0.0f);

  // Subtract a negative value (effectively adding).
  // This should only affect the active component (fColor1).
  CFX_Color c2 = c1 - (-0.5f);

  EXPECT_EQ(c2.nColorType, CFX_Color::Type::kGray);
  EXPECT_FLOAT_EQ(c2.fColor1, 0.7f);

  // These failed before the fix because they became 0.5f.
  EXPECT_FLOAT_EQ(c2.fColor2, 0.0f);
  EXPECT_FLOAT_EQ(c2.fColor3, 0.0f);
  EXPECT_FLOAT_EQ(c2.fColor4, 0.0f);

  // Construct a Gray color with the expected value directly.
  CFX_Color c3 = CFX_Color::MakeGray(0.7f);

  // Should be equal.
  EXPECT_EQ(c2, c3);

  // Construct a Gray color with non-zero unused components.
  CFX_Color c4 = CFX_Color::MakeGray(0.7f);
  c4.fColor2 = 0.5f;
  c4.fColor3 = 0.5f;
  c4.fColor4 = 0.5f;

  // Equality should ignore fColor2, fColor3, and fColor4.
  EXPECT_EQ(c3, c4);
}

TEST(CFX_ColorTest, OperatorSlashUnusedComponents) {
  // Construct a Gray color. Unused components (2, 3, 4) should be 0.0f.
  CFX_Color c1 = CFX_Color::MakeGray(0.8f);

  // Divide.
  CFX_Color c2 = c1 / 2.0f;

  EXPECT_EQ(c2.nColorType, CFX_Color::Type::kGray);
  EXPECT_FLOAT_EQ(c2.fColor1, 0.4f);
  EXPECT_FLOAT_EQ(c2.fColor2, 0.0f);
  EXPECT_FLOAT_EQ(c2.fColor3, 0.0f);
  EXPECT_FLOAT_EQ(c2.fColor4, 0.0f);

  CFX_Color c3 = CFX_Color::MakeGray(0.4f);
  EXPECT_EQ(c2, c3);
}
