// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xfa/fgas/font/cfgas_fontmgr.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_WIN)
TEST(CFGASFontMgr, LazyEnumeration) {
  CFGAS_FontMgr font_mgr;

  // Fonts should not be enumerated until explicitly requested. This allows
  // embedders to configure the font mapper before any enumeration occurs.
  EXPECT_FALSE(font_mgr.fonts_enumerated_);

  font_mgr.EnsureFontsEnumerated();
  EXPECT_TRUE(font_mgr.fonts_enumerated_);

  // Calling again should be a no-op.
  font_mgr.EnsureFontsEnumerated();
  EXPECT_TRUE(font_mgr.fonts_enumerated_);
}
#endif  // !BUILDFLAG(IS_WIN)
