// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "cc/output/context_provider.h"
#include "content/browser/aura/image_transport_factory.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/test/content_browser_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

class ImageTransportFactoryBrowserTest : public ContentBrowserTest {
 public:
  ImageTransportFactoryBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    UseRealGLContexts();
    ContentBrowserTest::SetUp();
  }
};

class MockImageTransportFactoryObserver : public ImageTransportFactoryObserver {
 public:
  MOCK_METHOD0(OnLostResources, void());
};

// Checks that upon context loss, the observer is called and the created
// resources are reset.
IN_PROC_BROWSER_TEST_F(ImageTransportFactoryBrowserTest, TestLostContext) {
  // This test doesn't make sense in software compositing mode.
  if (!GpuDataManager::GetInstance()->CanUseGpuBrowserCompositor())
    return;

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  scoped_refptr<ui::Texture> texture = factory->CreateTransportClient(1.f);
  ASSERT_TRUE(texture.get());

  MockImageTransportFactoryObserver observer;
  factory->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLostResources())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  ui::ContextFactory* context_factory = ui::ContextFactory::GetInstance();

  gpu::gles2::GLES2Interface* gl =
      context_factory->SharedMainThreadContextProvider()->ContextGL();
  gl->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);

  // We have to flush to make sure that the client side gets a chance to notice
  // the context is gone.
  gl->Flush();

  run_loop.Run();
  EXPECT_EQ(0u, texture->PrepareTexture());

  factory->RemoveObserver(&observer);
}

}  // anonymous namespace
}  // namespace content
