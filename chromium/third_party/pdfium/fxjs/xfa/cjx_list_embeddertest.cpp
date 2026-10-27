// Copyright 2019 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test_environment.h"
#include "testing/embedder_test_timer_handling_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/v8_test_environment.h"
#include "testing/xfa_js_embedder_test.h"

class CJXListEmbedderTest : public XFAJSEmbedderTest {};

// Should not crash.
TEST_F(CJXListEmbedderTest, Bug1263) {
  ASSERT_TRUE(OpenDocument("simple_xfa.pdf"));

  EXPECT_FALSE(Execute("nodes.insert($form,$)"));
}

// Should not crash.
TEST_F(CJXListEmbedderTest, Bug504416752) {
  EmbedderTestTimerHandlingDelegate delegate;
  SetDelegate(&delegate);

  ASSERT_TRUE(OpenDocument("bug_504416752.pdf"));
  ScopedPage page = LoadScopedPage(0);
  ASSERT_TRUE(page);
  DoOpenActions();

  ForceCppGarbageCollection();

  for (int i = 0; i < 1000; ++i) {
    delegate.AdvanceTime(10);
    V8TestEnvironment::PumpPlatformMessageLoop(
        V8TestEnvironment::GetInstance()->isolate());
  }
}
