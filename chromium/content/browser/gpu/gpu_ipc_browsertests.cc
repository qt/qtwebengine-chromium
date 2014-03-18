// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "content/test/content_browser_test.h"
#include "ui/gl/gl_switches.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace {

using content::WebGraphicsContext3DCommandBufferImpl;

class ContextTestBase : public content::ContentBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    if (!content::BrowserGpuChannelHostFactory::CanUseForTesting())
      return;

    if (!content::BrowserGpuChannelHostFactory::instance())
      content::BrowserGpuChannelHostFactory::Initialize(true);

    content::BrowserGpuChannelHostFactory* factory =
        content::BrowserGpuChannelHostFactory::instance();
    CHECK(factory);
    scoped_refptr<content::GpuChannelHost> gpu_channel_host(
        factory->EstablishGpuChannelSync(
            content::
                CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE));
    context_.reset(
        WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            gpu_channel_host.get(),
            blink::WebGraphicsContext3D::Attributes(),
            GURL(),
            WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits()));
    CHECK(context_.get());
    context_->makeContextCurrent();
    context_support_ = context_->GetContextSupport();
    ContentBrowserTest::SetUpOnMainThread();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    // Must delete the context first.
    context_.reset(NULL);
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
  gpu::ContextSupport* context_support_;
};

}  // namespace

// Include the shared tests.
#define CONTEXT_TEST_F IN_PROC_BROWSER_TEST_F
#include "content/common/gpu/client/gpu_context_tests.h"

namespace content {

class BrowserGpuChannelHostFactoryTest : public ContextTestBase {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    if (!content::BrowserGpuChannelHostFactory::CanUseForTesting())
      return;

    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    if (!content::BrowserGpuChannelHostFactory::instance())
      content::BrowserGpuChannelHostFactory::Initialize(false);

    CHECK(GetFactory());

    ContentBrowserTest::SetUpOnMainThread();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    ContextTestBase::TearDownOnMainThread();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    command_line->AppendSwitch(switches::kDisableGpuProcessPrelaunch);
  }

  void OnContextLost(const base::Closure callback, int* counter) {
    (*counter)++;
    callback.Run();
  }

 protected:
  BrowserGpuChannelHostFactory* GetFactory() {
    return BrowserGpuChannelHostFactory::instance();
  }

  bool IsChannelEstablished() {
    return GetFactory()->GetGpuChannel() != NULL;
  }

  void EstablishAndWait() {
    base::RunLoop run_loop;
    GetFactory()->EstablishGpuChannel(
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE,
        run_loop.QuitClosure());
    run_loop.Run();
  }

  GpuChannelHost* GetGpuChannel() {
    return GetFactory()->GetGpuChannel();
  }

  static void Signal(bool *event) {
    CHECK_EQ(*event, false);
    *event = true;
  }

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContext() {
    return make_scoped_ptr(
        WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            GetGpuChannel(),
            blink::WebGraphicsContext3D::Attributes(),
            GURL(),
            WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits()));
  }
};

IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, Basic) {
  if (!context_)
    return;

  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(GetGpuChannel() != NULL);
}

IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       EstablishAndTerminate) {
  if (!context_)
    return;

  DCHECK(!IsChannelEstablished());
  base::RunLoop run_loop;
  GetFactory()->EstablishGpuChannel(
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE,
      run_loop.QuitClosure());
  GetFactory()->Terminate();

  // The callback should still trigger.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, AlreadyEstablished) {
  if (!context_)
    return;

  DCHECK(!IsChannelEstablished());
  scoped_refptr<GpuChannelHost> gpu_channel =
      GetFactory()->EstablishGpuChannelSync(
          CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE);

  // Expect established callback immediately.
  bool event = false;
  GetFactory()->EstablishGpuChannel(
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE,
      base::Bind(&BrowserGpuChannelHostFactoryTest::Signal, &event));
  EXPECT_TRUE(event);
  EXPECT_EQ(gpu_channel, GetGpuChannel());
}

IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, CrashAndRecover) {
  if (!context_)
    return;

  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  scoped_refptr<GpuChannelHost> host = GetGpuChannel();

  scoped_refptr<ContextProviderCommandBuffer> provider =
      ContextProviderCommandBuffer::Create(CreateContext(),
                                           "BrowserGpuChannelHostFactoryTest");
  base::RunLoop run_loop;
  int counter = 0;
  provider->SetLostContextCallback(
      base::Bind(&BrowserGpuChannelHostFactoryTest::OnContextLost,
                 base::Unretained(this), run_loop.QuitClosure(), &counter));
  EXPECT_TRUE(provider->BindToCurrentThread());
  GpuProcessHostUIShim* shim =
      GpuProcessHostUIShim::FromID(GetFactory()->GpuProcessHostId());
  EXPECT_TRUE(shim != NULL);
  shim->SimulateCrash();
  run_loop.Run();

  EXPECT_EQ(1, counter);
  EXPECT_FALSE(IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(IsChannelEstablished());
}

}  // namespace content
