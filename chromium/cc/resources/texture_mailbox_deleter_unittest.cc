// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/texture_mailbox_deleter.h"

#include "cc/debug/test_context_provider.h"
#include "cc/debug/test_web_graphics_context_3d.h"
#include "cc/resources/single_release_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(TextureMailboxDeleterTest, Destroy) {
  scoped_ptr<TextureMailboxDeleter> deleter(new TextureMailboxDeleter);

  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();
  context_provider->BindToCurrentThread();

  unsigned texture_id = context_provider->Context3d()->createTexture();

  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(1u, context_provider->TestContext3d()->NumTextures());

  scoped_ptr<SingleReleaseCallback> cb =
      deleter->GetReleaseCallback(context_provider, texture_id).Pass();
  EXPECT_FALSE(context_provider->HasOneRef());
  EXPECT_EQ(1u, context_provider->TestContext3d()->NumTextures());

  // When the deleter is destroyed, it immediately drops its ref on the
  // ContextProvider, and deletes the texture.
  deleter.reset();
  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(0u, context_provider->TestContext3d()->NumTextures());

  // Run the scoped release callback before destroying it, but it won't do
  // anything.
  cb->Run(0, false);
}

}  // namespace
}  // namespace cc
