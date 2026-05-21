// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/cpp/fpdf_scopers.h"
#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class JBig2EmbedderTest : public EmbedderTest {};

TEST_F(JBig2EmbedderTest, Bug631912) {
  // Test jbig2 image in PDF file can be loaded successfully.
  // Should not crash.
  ASSERT_TRUE(OpenDocument("bug_631912.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  ScopedFPDFBitmap bitmap = RenderLoadedPage(page.get());
  CompareBitmapToPngWithExpectationSuffix(bitmap.get(), "bug_631912");
}
