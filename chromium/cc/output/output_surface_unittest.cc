// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include "base/test/test_simple_task_runner.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/frame_time.h"

namespace cc {
namespace {

class TestOutputSurface : public OutputSurface {
 public:
  explicit TestOutputSurface(scoped_refptr<ContextProvider> context_provider)
      : OutputSurface(context_provider),
        retroactive_begin_impl_frame_deadline_enabled_(false),
        override_retroactive_period_(false) {}

  explicit TestOutputSurface(scoped_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(software_device.Pass()),
        retroactive_begin_impl_frame_deadline_enabled_(false),
        override_retroactive_period_(false) {}

  TestOutputSurface(scoped_refptr<ContextProvider> context_provider,
                    scoped_ptr<SoftwareOutputDevice> software_device)
      : OutputSurface(context_provider, software_device.Pass()),
        retroactive_begin_impl_frame_deadline_enabled_(false),
        override_retroactive_period_(false) {}

  bool InitializeNewContext3d(
      scoped_refptr<ContextProvider> new_context_provider) {
    return InitializeAndSetContext3d(new_context_provider,
                                     scoped_refptr<ContextProvider>());
  }

  using OutputSurface::ReleaseGL;

  void OnVSyncParametersChangedForTesting(base::TimeTicks timebase,
                                          base::TimeDelta interval) {
    OnVSyncParametersChanged(timebase, interval);
  }

  void BeginImplFrameForTesting() {
    OutputSurface::BeginImplFrame(BeginFrameArgs::CreateExpiredForTesting());
  }

  void DidSwapBuffersForTesting() {
    DidSwapBuffers();
  }

  int pending_swap_buffers() {
    return pending_swap_buffers_;
  }

  void OnSwapBuffersCompleteForTesting() {
    OnSwapBuffersComplete();
  }

  void EnableRetroactiveBeginImplFrameDeadline(
      bool enable,
      bool override_retroactive_period,
      base::TimeDelta period_override) {
    retroactive_begin_impl_frame_deadline_enabled_ = enable;
    override_retroactive_period_ = override_retroactive_period;
    retroactive_period_override_ = period_override;
  }

 protected:
  virtual void PostCheckForRetroactiveBeginImplFrame() OVERRIDE {
    // For testing purposes, we check immediately rather than posting a task.
    CheckForRetroactiveBeginImplFrame();
  }

  virtual base::TimeTicks RetroactiveBeginImplFrameDeadline() OVERRIDE {
    if (retroactive_begin_impl_frame_deadline_enabled_) {
      if (override_retroactive_period_) {
        return skipped_begin_impl_frame_args_.frame_time +
               retroactive_period_override_;
      } else {
        return OutputSurface::RetroactiveBeginImplFrameDeadline();
      }
    }
    return base::TimeTicks();
  }

  bool retroactive_begin_impl_frame_deadline_enabled_;
  bool override_retroactive_period_;
  base::TimeDelta retroactive_period_override_;
};

class TestSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  TestSoftwareOutputDevice();
  virtual ~TestSoftwareOutputDevice();

  // Overriden from cc:SoftwareOutputDevice
  virtual void DiscardBackbuffer() OVERRIDE;
  virtual void EnsureBackbuffer() OVERRIDE;

  int discard_backbuffer_count() { return discard_backbuffer_count_; }
  int ensure_backbuffer_count() { return ensure_backbuffer_count_; }

 private:
  int discard_backbuffer_count_;
  int ensure_backbuffer_count_;
};

TestSoftwareOutputDevice::TestSoftwareOutputDevice()
    : discard_backbuffer_count_(0), ensure_backbuffer_count_(0) {}

TestSoftwareOutputDevice::~TestSoftwareOutputDevice() {}

void TestSoftwareOutputDevice::DiscardBackbuffer() {
  SoftwareOutputDevice::DiscardBackbuffer();
  discard_backbuffer_count_++;
}

void TestSoftwareOutputDevice::EnsureBackbuffer() {
  SoftwareOutputDevice::EnsureBackbuffer();
  ensure_backbuffer_count_++;
}

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientSuccess) {
  TestOutputSurface output_surface(TestContextProvider::Create());
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClient());
  EXPECT_FALSE(client.deferred_initialize_called());

  // Verify DidLoseOutputSurface callback is hooked up correctly.
  EXPECT_FALSE(client.did_lose_output_surface_called());
  output_surface.context_provider()->Context3d()->loseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  EXPECT_TRUE(client.did_lose_output_surface_called());
}

TEST(OutputSurfaceTest, ClientPointerIndicatesBindToClientFailure) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  // Lose the context so BindToClient fails.
  context_provider->UnboundTestContext3d()->set_context_lost(true);

  TestOutputSurface output_surface(context_provider);
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_FALSE(output_surface.BindToClient(&client));
  EXPECT_FALSE(output_surface.HasClient());
}

class OutputSurfaceTestInitializeNewContext3d : public ::testing::Test {
 public:
  OutputSurfaceTestInitializeNewContext3d()
      : context_provider_(TestContextProvider::Create()),
        output_surface_(
            scoped_ptr<SoftwareOutputDevice>(new SoftwareOutputDevice)) {}

 protected:
  void BindOutputSurface() {
    EXPECT_TRUE(output_surface_.BindToClient(&client_));
    EXPECT_TRUE(output_surface_.HasClient());
  }

  void InitializeNewContextExpectFail() {
    EXPECT_FALSE(output_surface_.InitializeNewContext3d(context_provider_));
    EXPECT_TRUE(output_surface_.HasClient());

    EXPECT_FALSE(output_surface_.context_provider());
    EXPECT_TRUE(output_surface_.software_device());
  }

  scoped_refptr<TestContextProvider> context_provider_;
  TestOutputSurface output_surface_;
  FakeOutputSurfaceClient client_;
};

TEST_F(OutputSurfaceTestInitializeNewContext3d, Success) {
  BindOutputSurface();
  EXPECT_FALSE(client_.deferred_initialize_called());

  EXPECT_TRUE(output_surface_.InitializeNewContext3d(context_provider_));
  EXPECT_TRUE(client_.deferred_initialize_called());
  EXPECT_EQ(context_provider_, output_surface_.context_provider());

  EXPECT_FALSE(client_.did_lose_output_surface_called());
  context_provider_->Context3d()->loseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  EXPECT_TRUE(client_.did_lose_output_surface_called());

  output_surface_.ReleaseGL();
  EXPECT_FALSE(output_surface_.context_provider());
}

TEST_F(OutputSurfaceTestInitializeNewContext3d, Context3dMakeCurrentFails) {
  BindOutputSurface();

  context_provider_->UnboundTestContext3d()->set_context_lost(true);
  InitializeNewContextExpectFail();
}

TEST_F(OutputSurfaceTestInitializeNewContext3d, ClientDeferredInitializeFails) {
  BindOutputSurface();
  client_.set_deferred_initialize_result(false);
  InitializeNewContextExpectFail();
}

TEST(OutputSurfaceTest, BeginImplFrameEmulation) {
  TestOutputSurface output_surface(TestContextProvider::Create());
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClient());
  EXPECT_FALSE(client.deferred_initialize_called());

  // Initialize BeginImplFrame emulation
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  bool throttle_frame_production = true;
  const base::TimeDelta display_refresh_interval =
      BeginFrameArgs::DefaultInterval();

  output_surface.InitializeBeginImplFrameEmulation(
      task_runner.get(),
      throttle_frame_production,
      display_refresh_interval);

  output_surface.SetMaxFramesPending(2);
  output_surface.EnableRetroactiveBeginImplFrameDeadline(
      false, false, base::TimeDelta());

  // We should start off with 0 BeginImplFrames
  EXPECT_EQ(client.begin_impl_frame_count(), 0);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // We should not have a pending task until a BeginImplFrame has been
  // requested.
  EXPECT_FALSE(task_runner->HasPendingTask());
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_TRUE(task_runner->HasPendingTask());

  // BeginImplFrame should be called on the first tick.
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_impl_frame_count(), 1);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // BeginImplFrame should not be called when there is a pending BeginImplFrame.
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_impl_frame_count(), 1);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // SetNeedsBeginImplFrame should clear the pending BeginImplFrame after
  // a SwapBuffers.
  output_surface.DidSwapBuffersForTesting();
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_EQ(client.begin_impl_frame_count(), 1);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_impl_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // BeginImplFrame should be throttled by pending swap buffers.
  output_surface.DidSwapBuffersForTesting();
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_EQ(client.begin_impl_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 2);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_impl_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 2);

  // SwapAck should decrement pending swap buffers and unblock BeginImplFrame
  // again.
  output_surface.OnSwapBuffersCompleteForTesting();
  EXPECT_EQ(client.begin_impl_frame_count(), 2);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_impl_frame_count(), 3);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // Calling SetNeedsBeginImplFrame again indicates a swap did not occur but
  // the client still wants another BeginImplFrame.
  output_surface.SetNeedsBeginImplFrame(true);
  task_runner->RunPendingTasks();
  EXPECT_EQ(client.begin_impl_frame_count(), 4);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // Disabling SetNeedsBeginImplFrame should prevent further BeginImplFrames.
  output_surface.SetNeedsBeginImplFrame(false);
  task_runner->RunPendingTasks();
  EXPECT_FALSE(task_runner->HasPendingTask());
  EXPECT_EQ(client.begin_impl_frame_count(), 4);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);
}

TEST(OutputSurfaceTest, OptimisticAndRetroactiveBeginImplFrames) {
  TestOutputSurface output_surface(TestContextProvider::Create());
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClient());
  EXPECT_FALSE(client.deferred_initialize_called());

  output_surface.SetMaxFramesPending(2);
  output_surface.EnableRetroactiveBeginImplFrameDeadline(
      true, false, base::TimeDelta());

  // Optimistically injected BeginImplFrames should be throttled if
  // SetNeedsBeginImplFrame is false...
  output_surface.SetNeedsBeginImplFrame(false);
  output_surface.BeginImplFrameForTesting();
  EXPECT_EQ(client.begin_impl_frame_count(), 0);
  // ...and retroactively triggered by a SetNeedsBeginImplFrame.
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_EQ(client.begin_impl_frame_count(), 1);

  // Optimistically injected BeginImplFrames should be throttled by pending
  // BeginImplFrames...
  output_surface.BeginImplFrameForTesting();
  EXPECT_EQ(client.begin_impl_frame_count(), 1);
  // ...and retroactively triggered by a SetNeedsBeginImplFrame.
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_EQ(client.begin_impl_frame_count(), 2);
  // ...or retroactively triggered by a Swap.
  output_surface.BeginImplFrameForTesting();
  EXPECT_EQ(client.begin_impl_frame_count(), 2);
  output_surface.DidSwapBuffersForTesting();
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_EQ(client.begin_impl_frame_count(), 3);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 1);

  // Optimistically injected BeginImplFrames should be by throttled by pending
  // swap buffers...
  output_surface.DidSwapBuffersForTesting();
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_EQ(client.begin_impl_frame_count(), 3);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 2);
  output_surface.BeginImplFrameForTesting();
  EXPECT_EQ(client.begin_impl_frame_count(), 3);
  // ...and retroactively triggered by OnSwapBuffersComplete
  output_surface.OnSwapBuffersCompleteForTesting();
  EXPECT_EQ(client.begin_impl_frame_count(), 4);
}

TEST(OutputSurfaceTest,
     RetroactiveBeginImplFrameDoesNotDoubleTickWhenEmulating) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  TestOutputSurface output_surface(context_provider);
  EXPECT_FALSE(output_surface.HasClient());

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  EXPECT_TRUE(output_surface.HasClient());
  EXPECT_FALSE(client.deferred_initialize_called());

  base::TimeDelta big_interval = base::TimeDelta::FromSeconds(10);

  // Initialize BeginImplFrame emulation
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  bool throttle_frame_production = true;
  const base::TimeDelta display_refresh_interval = big_interval;

  output_surface.InitializeBeginImplFrameEmulation(
      task_runner.get(),
      throttle_frame_production,
      display_refresh_interval);

  // We need to subtract an epsilon from Now() because some platforms have
  // a slow clock.
  output_surface.OnVSyncParametersChangedForTesting(
      gfx::FrameTime::Now() - base::TimeDelta::FromSeconds(1), big_interval);

  output_surface.SetMaxFramesPending(2);
  output_surface.EnableRetroactiveBeginImplFrameDeadline(
      true, true, big_interval);

  // We should start off with 0 BeginImplFrames
  EXPECT_EQ(client.begin_impl_frame_count(), 0);
  EXPECT_EQ(output_surface.pending_swap_buffers(), 0);

  // The first SetNeedsBeginImplFrame(true) should start a retroactive
  // BeginImplFrame.
  EXPECT_FALSE(task_runner->HasPendingTask());
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_GT(task_runner->NextPendingTaskDelay(), big_interval / 2);
  EXPECT_EQ(client.begin_impl_frame_count(), 1);

  output_surface.SetNeedsBeginImplFrame(false);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(client.begin_impl_frame_count(), 1);

  // The second SetNeedBeginImplFrame(true) should not retroactively start a
  // BeginImplFrame if the timestamp would be the same as the previous
  // BeginImplFrame.
  output_surface.SetNeedsBeginImplFrame(true);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(client.begin_impl_frame_count(), 1);
}

TEST(OutputSurfaceTest, MemoryAllocation) {
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create();

  TestOutputSurface output_surface(context_provider);

  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));

  ManagedMemoryPolicy policy(0);
  policy.bytes_limit_when_visible = 1234;
  policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;

  context_provider->SetMemoryAllocation(policy);
  EXPECT_EQ(1234u, client.memory_policy().bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
            client.memory_policy().priority_cutoff_when_visible);

  policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;
  context_provider->SetMemoryAllocation(policy);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING,
            client.memory_policy().priority_cutoff_when_visible);

  // 0 bytes limit should be ignored.
  policy.bytes_limit_when_visible = 0;
  context_provider->SetMemoryAllocation(policy);
  EXPECT_EQ(1234u, client.memory_policy().bytes_limit_when_visible);
}

TEST(OutputSurfaceTest, SoftwareOutputDeviceBackbufferManagement) {
  TestSoftwareOutputDevice* software_output_device =
      new TestSoftwareOutputDevice();

  // TestOutputSurface now owns software_output_device and has responsibility to
  // free it.
  scoped_ptr<TestSoftwareOutputDevice> p(software_output_device);
  TestOutputSurface output_surface(p.PassAs<SoftwareOutputDevice>());

  EXPECT_EQ(0, software_output_device->ensure_backbuffer_count());
  EXPECT_EQ(0, software_output_device->discard_backbuffer_count());

  output_surface.EnsureBackbuffer();
  EXPECT_EQ(1, software_output_device->ensure_backbuffer_count());
  EXPECT_EQ(0, software_output_device->discard_backbuffer_count());
  output_surface.DiscardBackbuffer();

  EXPECT_EQ(1, software_output_device->ensure_backbuffer_count());
  EXPECT_EQ(1, software_output_device->discard_backbuffer_count());
}

}  // namespace
}  // namespace cc
