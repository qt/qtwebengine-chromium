// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/basictypes.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/io_surface_layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/filter_operations.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_content_layer_impl.h"
#include "cc/test/fake_delegated_renderer_layer.h"
#include "cc/test/fake_delegated_renderer_layer_impl.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_painted_scrollbar_layer.h"
#include "cc/test/fake_scoped_ui_resource.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/media.h"

using media::VideoFrame;
using blink::WebGraphicsContext3D;

namespace cc {
namespace {

// These tests deal with losing the 3d graphics context.
class LayerTreeHostContextTest : public LayerTreeTest {
 public:
  LayerTreeHostContextTest()
      : LayerTreeTest(),
        context3d_(NULL),
        times_to_fail_create_(0),
        times_to_lose_during_commit_(0),
        times_to_lose_during_draw_(0),
        times_to_fail_recreate_(0),
        times_to_fail_create_offscreen_(0),
        times_to_fail_recreate_offscreen_(0),
        times_to_expect_create_failed_(0),
        times_create_failed_(0),
        times_offscreen_created_(0),
        committed_at_least_once_(false),
        context_should_support_io_surface_(false),
        fallback_context_works_(false) {
    media::InitializeMediaLibraryForTesting();
  }

  void LoseContext() {
    context3d_->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                    GL_INNOCENT_CONTEXT_RESET_ARB);
    context3d_ = NULL;
  }

  virtual scoped_ptr<TestWebGraphicsContext3D> CreateContext3d() {
    return TestWebGraphicsContext3D::Create();
  }

  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE {
    if (times_to_fail_create_) {
      --times_to_fail_create_;
      ExpectCreateToFail();
      return scoped_ptr<OutputSurface>();
    }

    scoped_ptr<TestWebGraphicsContext3D> context3d = CreateContext3d();
    context3d_ = context3d.get();

    if (context_should_support_io_surface_) {
      context3d_->set_have_extension_io_surface(true);
      context3d_->set_have_extension_egl_image(true);
    }

    if (delegating_renderer()) {
      return FakeOutputSurface::CreateDelegating3d(context3d.Pass())
          .PassAs<OutputSurface>();
    }
    return FakeOutputSurface::Create3d(context3d.Pass())
        .PassAs<OutputSurface>();
  }

  scoped_ptr<TestWebGraphicsContext3D> CreateOffscreenContext3d() {
    if (!context3d_)
      return scoped_ptr<TestWebGraphicsContext3D>();

    ++times_offscreen_created_;

    if (times_to_fail_create_offscreen_) {
      --times_to_fail_create_offscreen_;
      ExpectCreateToFail();
      return scoped_ptr<TestWebGraphicsContext3D>();
    }

    scoped_ptr<TestWebGraphicsContext3D> offscreen_context3d =
        TestWebGraphicsContext3D::Create().Pass();
    DCHECK(offscreen_context3d);
    context3d_->add_share_group_context(offscreen_context3d.get());

    return offscreen_context3d.Pass();
  }

  virtual scoped_refptr<ContextProvider> OffscreenContextProvider() OVERRIDE {
    if (!offscreen_contexts_.get() ||
        offscreen_contexts_->DestroyedOnMainThread()) {
      offscreen_contexts_ =
          TestContextProvider::Create(CreateOffscreenContext3d());
    }
    return offscreen_contexts_;
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame,
                                     bool result) OVERRIDE {
    EXPECT_TRUE(result);
    if (!times_to_lose_during_draw_)
      return result;

    --times_to_lose_during_draw_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
    times_to_fail_create_offscreen_ = times_to_fail_recreate_offscreen_;
    times_to_fail_recreate_offscreen_ = 0;

    return result;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    committed_at_least_once_ = true;

    if (!times_to_lose_during_commit_)
      return;
    --times_to_lose_during_commit_;
    LoseContext();

    times_to_fail_create_ = times_to_fail_recreate_;
    times_to_fail_recreate_ = 0;
    times_to_fail_create_offscreen_ = times_to_fail_recreate_offscreen_;
    times_to_fail_recreate_offscreen_ = 0;
  }

  virtual void DidFailToInitializeOutputSurface() OVERRIDE {
    ++times_create_failed_;
  }

  virtual void TearDown() OVERRIDE {
    LayerTreeTest::TearDown();
    EXPECT_EQ(times_to_expect_create_failed_, times_create_failed_);
  }

  void ExpectCreateToFail() {
    ++times_to_expect_create_failed_;
  }

 protected:
  TestWebGraphicsContext3D* context3d_;
  int times_to_fail_create_;
  int times_to_lose_during_commit_;
  int times_to_lose_during_draw_;
  int times_to_fail_recreate_;
  int times_to_fail_create_offscreen_;
  int times_to_fail_recreate_offscreen_;
  int times_to_expect_create_failed_;
  int times_create_failed_;
  int times_offscreen_created_;
  bool committed_at_least_once_;
  bool context_should_support_io_surface_;
  bool fallback_context_works_;

  scoped_refptr<TestContextProvider> offscreen_contexts_;
};

class LayerTreeHostContextTestLostContextSucceeds
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextSucceeds()
      : LayerTreeHostContextTest(),
        test_case_(0),
        num_losses_(0),
        num_losses_last_test_case_(-1),
        recovered_context_(true),
        first_initialized_(false) {}

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);

    if (first_initialized_)
      ++num_losses_;
    else
      first_initialized_ = true;

    recovered_context_ = true;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(9u, test_case_); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    // If the last frame had a context loss, then we'll commit again to
    // recover.
    if (!recovered_context_)
      return;
    if (times_to_lose_during_commit_)
      return;
    if (times_to_lose_during_draw_)
      return;

    recovered_context_ = false;
    if (NextTestCase())
      InvalidateAndSetNeedsCommit();
    else
      EndTest();
  }

  virtual void InvalidateAndSetNeedsCommit() {
    // Cause damage so we try to draw.
    layer_tree_host()->root_layer()->SetNeedsDisplay();
    layer_tree_host()->SetNeedsCommit();
  }

  bool NextTestCase() {
    static const TestCase kTests[] = {
      // Losing the context and failing to recreate it (or losing it again
      // immediately) a small number of times should succeed.
      { 1,      // times_to_lose_during_commit
        0,      // times_to_lose_during_draw
        0,      // times_to_fail_recreate
        0,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      { 0,      // times_to_lose_during_commit
        1,      // times_to_lose_during_draw
        0,      // times_to_fail_recreate
        0,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      { 1,      // times_to_lose_during_commit
        0,      // times_to_lose_during_draw
        3,      // times_to_fail_recreate
        0,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      { 0,      // times_to_lose_during_commit
        1,      // times_to_lose_during_draw
        3,      // times_to_fail_recreate
        0,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      { 1,      // times_to_lose_during_commit
        0,      // times_to_lose_during_draw
        0,      // times_to_fail_recreate
        3,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      { 0,      // times_to_lose_during_commit
        1,      // times_to_lose_during_draw
        0,      // times_to_fail_recreate
        3,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
             // Losing the context and recreating it any number of times should
      // succeed.
      { 10,  // times_to_lose_during_commit
        0,   // times_to_lose_during_draw
        0,   // times_to_fail_recreate
        0,   // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      { 0,      // times_to_lose_during_commit
        10,     // times_to_lose_during_draw
        0,      // times_to_fail_recreate
        0,      // times_to_fail_recreate_offscreen
        false,  // fallback_context_works
    },
      // Losing the context, failing to reinitialize it, and making a fallback
      // context should work.
      { 0,      // times_to_lose_during_commit
        1,      // times_to_lose_during_draw
        0,      // times_to_fail_recreate
        0,      // times_to_fail_recreate_offscreen
        true,   // fallback_context_works
    },
    };

    if (test_case_ >= arraysize(kTests))
      return false;
    // Make sure that we lost our context at least once in the last test run so
    // the test did something.
    EXPECT_GT(num_losses_, num_losses_last_test_case_);
    num_losses_last_test_case_ = num_losses_;

    times_to_lose_during_commit_ =
        kTests[test_case_].times_to_lose_during_commit;
    times_to_lose_during_draw_ =
        kTests[test_case_].times_to_lose_during_draw;
    times_to_fail_recreate_ = kTests[test_case_].times_to_fail_recreate;
    times_to_fail_recreate_offscreen_ =
        kTests[test_case_].times_to_fail_recreate_offscreen;
    fallback_context_works_ = kTests[test_case_].fallback_context_works;
    ++test_case_;
    return true;
  }

  struct TestCase {
    int times_to_lose_during_commit;
    int times_to_lose_during_draw;
    int times_to_fail_recreate;
    int times_to_fail_recreate_offscreen;
    bool fallback_context_works;
  };

 protected:
  size_t test_case_;
  int num_losses_;
  int num_losses_last_test_case_;
  bool recovered_context_;
  bool first_initialized_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLostContextSucceeds);

class LayerTreeHostContextTestLostContextSucceedsWithContent
    : public LayerTreeHostContextTestLostContextSucceeds {
 public:
  LayerTreeHostContextTestLostContextSucceedsWithContent()
      : LayerTreeHostContextTestLostContextSucceeds() {}

  virtual void SetupTree() OVERRIDE {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetIsDrawable(true);

    content_ = FakeContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(10, 10));
    content_->SetAnchorPoint(gfx::PointF());
    content_->SetIsDrawable(true);
    if (use_surface_) {
      content_->SetForceRenderSurface(true);
      // Filters require us to create an offscreen context.
      FilterOperations filters;
      filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
      content_->SetFilters(filters);
      content_->SetBackgroundFilters(filters);
    }

    root_->AddChild(content_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void InvalidateAndSetNeedsCommit() OVERRIDE {
    // Invalidate the render surface so we don't try to use a cached copy of the
    // surface.  We want to make sure to test the drawing paths for drawing to
    // a child surface.
    content_->SetNeedsDisplay();
    LayerTreeHostContextTestLostContextSucceeds::InvalidateAndSetNeedsCommit();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    FakeContentLayerImpl* content_impl = static_cast<FakeContentLayerImpl*>(
        host_impl->active_tree()->root_layer()->children()[0]);
    // Even though the context was lost, we should have a resource. The
    // TestWebGraphicsContext3D ensures that this resource is created with
    // the active context.
    EXPECT_TRUE(content_impl->HaveResourceForTileAt(0, 0));

    ContextProvider* contexts = host_impl->offscreen_context_provider();
    if (use_surface_) {
      ASSERT_TRUE(contexts);
      EXPECT_TRUE(contexts->Context3d());
      // TODO(danakj): Make a fake GrContext.
      // EXPECT_TRUE(contexts->GrContext());
    } else {
      EXPECT_FALSE(contexts);
    }
  }

  virtual void AfterTest() OVERRIDE {
    LayerTreeHostContextTestLostContextSucceeds::AfterTest();
    if (use_surface_) {
      // 1 create to start with +
      // 4 from test cases that lose the offscreen context directly +
      // 2 from test cases that create a fallback +
      // All the test cases that recreate both contexts only once
      // per time it is lost.
      EXPECT_EQ(4 + 1 + 2 + num_losses_, times_offscreen_created_);
    } else {
      EXPECT_EQ(0, times_offscreen_created_);
    }
  }

 protected:
  bool use_surface_;
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<ContentLayer> content_;
};

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_SingleThread_DirectRenderer) {
  use_surface_ = false;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_SingleThread_DelegatingRenderer) {
  use_surface_ = false;
  RunTest(false, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_MultiThread_DirectRenderer_MainThreadPaint) {
  use_surface_ = false;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       NoSurface_MultiThread_DelegatingRenderer_MainThreadPaint) {
  use_surface_ = false;
  RunTest(true, true, false);
}

// Surfaces don't exist with a delegating renderer.
TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       WithSurface_SingleThread_DirectRenderer) {
  use_surface_ = true;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextSucceedsWithContent,
       WithSurface_MultiThread_DirectRenderer_MainThreadPaint) {
  use_surface_ = true;
  RunTest(true, false, false);
}

class LayerTreeHostContextTestOffscreenContextFails
    : public LayerTreeHostContextTest {
 public:
  virtual void SetupTree() OVERRIDE {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(10, 10));
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetIsDrawable(true);

    content_ = FakeContentLayer::Create(&client_);
    content_->SetBounds(gfx::Size(10, 10));
    content_->SetAnchorPoint(gfx::PointF());
    content_->SetIsDrawable(true);
    content_->SetForceRenderSurface(true);
    // Filters require us to create an offscreen context.
    FilterOperations filters;
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    content_->SetFilters(filters);
    content_->SetBackgroundFilters(filters);

    root_->AddChild(content_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    times_to_fail_create_offscreen_ = 1;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    ContextProvider* contexts = host_impl->offscreen_context_provider();
    EXPECT_FALSE(contexts);

    // This did not lead to create failure.
    times_to_expect_create_failed_ = 0;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<ContentLayer> content_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestOffscreenContextFails);

class LayerTreeHostContextTestLostContextFails
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextFails()
      : LayerTreeHostContextTest(),
        num_commits_(0),
        first_initialized_(false) {
    times_to_lose_during_commit_ = 1;
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    if (first_initialized_) {
      EXPECT_FALSE(succeeded);
      EndTest();
    } else {
      first_initialized_ = true;
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    ++num_commits_;
    if (num_commits_ == 1) {
      // When the context is ok, we should have these things.
      EXPECT_TRUE(host_impl->output_surface());
      EXPECT_TRUE(host_impl->renderer());
      EXPECT_TRUE(host_impl->resource_provider());
      return;
    }

    // When context recreation fails we shouldn't be left with any of them.
    EXPECT_FALSE(host_impl->output_surface());
    EXPECT_FALSE(host_impl->renderer());
    EXPECT_FALSE(host_impl->resource_provider());
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;
  bool first_initialized_;
};

class LayerTreeHostContextTestLostContextAndEvictTextures
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextAndEvictTextures()
      : LayerTreeHostContextTest(),
        layer_(FakeContentLayer::Create(&client_)),
        impl_host_(0),
        num_commits_(0) {}

  virtual void SetupTree() OVERRIDE {
    layer_->SetBounds(gfx::Size(10, 20));
    layer_tree_host()->SetRootLayer(layer_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  void PostEvictTextures() {
    if (HasImplThread()) {
      ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(
              &LayerTreeHostContextTestLostContextAndEvictTextures::
              EvictTexturesOnImplThread,
              base::Unretained(this)));
    } else {
      DebugScopedSetImplThread impl(proxy());
      EvictTexturesOnImplThread();
    }
  }

  void EvictTexturesOnImplThread() {
    impl_host_->EvictTexturesForTesting();
    if (lose_after_evict_)
      LoseContext();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    if (num_commits_ > 1)
      return;
    EXPECT_TRUE(layer_->HaveBackingAt(0, 0));
    PostEvictTextures();
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    if (num_commits_ > 1)
      return;
    ++num_commits_;
    if (!lose_after_evict_)
      LoseContext();
    impl_host_ = impl;
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  bool lose_after_evict_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> layer_;
  LayerTreeHostImpl* impl_host_;
  int num_commits_;
};

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_SingleThread_DirectRenderer) {
  lose_after_evict_ = true;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_SingleThread_DelegatingRenderer) {
  lose_after_evict_ = true;
  RunTest(false, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread_DirectRenderer_MainThreadPaint) {
  lose_after_evict_ = true;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseAfterEvict_MultiThread_DelegatingRenderer_MainThreadPaint) {
  lose_after_evict_ = true;
  RunTest(true, true, false);
}

// Flaky on all platforms, http://crbug.com/310979
TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       DISABLED_LoseAfterEvict_MultiThread_DelegatingRenderer_ImplSidePaint) {
  lose_after_evict_ = true;
  RunTest(true, true, true);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread_DirectRenderer) {
  lose_after_evict_ = false;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_SingleThread_DelegatingRenderer) {
  lose_after_evict_ = false;
  RunTest(false, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DirectRenderer_MainThreadPaint) {
  lose_after_evict_ = false;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DirectRenderer_ImplSidePaint) {
  lose_after_evict_ = false;
  RunTest(true, false, true);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DelegatingRenderer_MainThreadPaint) {
  lose_after_evict_ = false;
  RunTest(true, true, false);
}

TEST_F(LayerTreeHostContextTestLostContextAndEvictTextures,
       LoseBeforeEvict_MultiThread_DelegatingRenderer_ImplSidePaint) {
  lose_after_evict_ = false;
  RunTest(true, true, true);
}

class LayerTreeHostContextTestLostContextWhileUpdatingResources
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLostContextWhileUpdatingResources()
      : parent_(FakeContentLayer::Create(&client_)),
        num_children_(50),
        times_to_lose_on_end_query_(3) {}

  virtual scoped_ptr<TestWebGraphicsContext3D> CreateContext3d() OVERRIDE {
    scoped_ptr<TestWebGraphicsContext3D> context =
        LayerTreeHostContextTest::CreateContext3d();
    if (times_to_lose_on_end_query_) {
      --times_to_lose_on_end_query_;
      context->set_times_end_query_succeeds(5);
    }
    return context.Pass();
  }

  virtual void SetupTree() OVERRIDE {
    parent_->SetBounds(gfx::Size(num_children_, 1));

    for (int i = 0; i < num_children_; i++) {
      scoped_refptr<FakeContentLayer> child =
          FakeContentLayer::Create(&client_);
      child->SetPosition(gfx::PointF(i, 0.f));
      child->SetBounds(gfx::Size(1, 1));
      parent_->AddChild(child);
    }

    layer_tree_host()->SetRootLayer(parent_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_EQ(0, times_to_lose_on_end_query_);
    EndTest();
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(0, times_to_lose_on_end_query_);
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> parent_;
  int num_children_;
  int times_to_lose_on_end_query_;
};

SINGLE_AND_MULTI_THREAD_NOIMPL_TEST_F(
    LayerTreeHostContextTestLostContextWhileUpdatingResources);

class LayerTreeHostContextTestLayersNotified
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestLayersNotified()
      : LayerTreeHostContextTest(),
        num_commits_(0) {}

  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    child_ = FakeContentLayer::Create(&client_);
    grandchild_ = FakeContentLayer::Create(&client_);

    root_->AddChild(child_);
    child_->AddChild(grandchild_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::DidActivateTreeOnThread(host_impl);

    FakeContentLayerImpl* root = static_cast<FakeContentLayerImpl*>(
        host_impl->active_tree()->root_layer());
    FakeContentLayerImpl* child = static_cast<FakeContentLayerImpl*>(
        root->children()[0]);
    FakeContentLayerImpl* grandchild = static_cast<FakeContentLayerImpl*>(
        child->children()[0]);

    ++num_commits_;
    switch (num_commits_) {
      case 1:
        EXPECT_EQ(0u, root->lost_output_surface_count());
        EXPECT_EQ(0u, child->lost_output_surface_count());
        EXPECT_EQ(0u, grandchild->lost_output_surface_count());
        // Lose the context and struggle to recreate it.
        LoseContext();
        times_to_fail_create_ = 1;
        break;
      case 2:
        EXPECT_GE(1u, root->lost_output_surface_count());
        EXPECT_GE(1u, child->lost_output_surface_count());
        EXPECT_GE(1u, grandchild->lost_output_surface_count());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int num_commits_;

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> child_;
  scoped_refptr<FakeContentLayer> grandchild_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestLayersNotified);

class LayerTreeHostContextTestDontUseLostResources
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestDontUseLostResources()
      : lost_context_(false) {
    context_should_support_io_surface_ = true;

    child_output_surface_ = FakeOutputSurface::Create3d();
    child_output_surface_->BindToClient(&output_surface_client_);
    child_resource_provider_ =
        ResourceProvider::Create(child_output_surface_.get(),
                                 NULL,
                                 0,
                                 false,
                                 1);
  }

  static void EmptyReleaseCallback(unsigned sync_point, bool lost) {}

  virtual void SetupTree() OVERRIDE {
    blink::WebGraphicsContext3D* context3d =
        child_output_surface_->context_provider()->Context3d();

    scoped_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);

    scoped_ptr<TestRenderPass> pass_for_quad = TestRenderPass::Create();
    pass_for_quad->SetNew(
        // AppendOneOfEveryQuadType() makes a RenderPass quad with this id.
        RenderPass::Id(2, 1),
        gfx::Rect(0, 0, 10, 10),
        gfx::Rect(0, 0, 10, 10),
        gfx::Transform());

    scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
    pass->SetNew(RenderPass::Id(1, 1),
                 gfx::Rect(0, 0, 10, 10),
                 gfx::Rect(0, 0, 10, 10),
                 gfx::Transform());
    pass->AppendOneOfEveryQuadType(child_resource_provider_.get(),
                                   RenderPass::Id(2, 1));

    frame_data->render_pass_list.push_back(pass_for_quad.PassAs<RenderPass>());
    frame_data->render_pass_list.push_back(pass.PassAs<RenderPass>());

    delegated_resource_collection_ = new DelegatedFrameResourceCollection;
    delegated_frame_provider_ = new DelegatedFrameProvider(
        delegated_resource_collection_.get(), frame_data.Pass());

    ResourceProvider::ResourceId resource =
        child_resource_provider_->CreateResource(
            gfx::Size(4, 4),
            GL_CLAMP_TO_EDGE,
            ResourceProvider::TextureUsageAny,
            RGBA_8888);
    ResourceProvider::ScopedWriteLockGL lock(child_resource_provider_.get(),
                                             resource);

    gpu::Mailbox mailbox;
    context3d->genMailboxCHROMIUM(mailbox.name);
    unsigned sync_point = context3d->insertSyncPoint();

    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    scoped_refptr<FakeDelegatedRendererLayer> delegated =
        FakeDelegatedRendererLayer::Create(delegated_frame_provider_.get());
    delegated->SetBounds(gfx::Size(10, 10));
    delegated->SetAnchorPoint(gfx::PointF());
    delegated->SetIsDrawable(true);
    root->AddChild(delegated);

    scoped_refptr<ContentLayer> content = ContentLayer::Create(&client_);
    content->SetBounds(gfx::Size(10, 10));
    content->SetAnchorPoint(gfx::PointF());
    content->SetIsDrawable(true);
    root->AddChild(content);

    scoped_refptr<TextureLayer> texture = TextureLayer::CreateForMailbox(NULL);
    texture->SetBounds(gfx::Size(10, 10));
    texture->SetAnchorPoint(gfx::PointF());
    texture->SetIsDrawable(true);
    texture->SetTextureMailbox(
        TextureMailbox(mailbox, sync_point),
        SingleReleaseCallback::Create(base::Bind(
            &LayerTreeHostContextTestDontUseLostResources::
                EmptyReleaseCallback)));
    root->AddChild(texture);

    scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client_);
    mask->SetBounds(gfx::Size(10, 10));
    mask->SetAnchorPoint(gfx::PointF());

    scoped_refptr<ContentLayer> content_with_mask =
        ContentLayer::Create(&client_);
    content_with_mask->SetBounds(gfx::Size(10, 10));
    content_with_mask->SetAnchorPoint(gfx::PointF());
    content_with_mask->SetIsDrawable(true);
    content_with_mask->SetMaskLayer(mask.get());
    root->AddChild(content_with_mask);

    scoped_refptr<VideoLayer> video_color =
        VideoLayer::Create(&color_frame_provider_);
    video_color->SetBounds(gfx::Size(10, 10));
    video_color->SetAnchorPoint(gfx::PointF());
    video_color->SetIsDrawable(true);
    root->AddChild(video_color);

    scoped_refptr<VideoLayer> video_hw =
        VideoLayer::Create(&hw_frame_provider_);
    video_hw->SetBounds(gfx::Size(10, 10));
    video_hw->SetAnchorPoint(gfx::PointF());
    video_hw->SetIsDrawable(true);
    root->AddChild(video_hw);

    scoped_refptr<VideoLayer> video_scaled_hw =
        VideoLayer::Create(&scaled_hw_frame_provider_);
    video_scaled_hw->SetBounds(gfx::Size(10, 10));
    video_scaled_hw->SetAnchorPoint(gfx::PointF());
    video_scaled_hw->SetIsDrawable(true);
    root->AddChild(video_scaled_hw);

    color_video_frame_ = VideoFrame::CreateColorFrame(
        gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
    hw_video_frame_ = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(new VideoFrame::MailboxHolder(
            mailbox,
            sync_point,
            VideoFrame::MailboxHolder::TextureNoLongerNeededCallback())),
        GL_TEXTURE_2D,
        gfx::Size(4, 4),
        gfx::Rect(0, 0, 4, 4),
        gfx::Size(4, 4),
        base::TimeDelta(),
        VideoFrame::ReadPixelsCB(),
        base::Closure());
    scaled_hw_video_frame_ = VideoFrame::WrapNativeTexture(
        make_scoped_ptr(new VideoFrame::MailboxHolder(
            mailbox,
            sync_point,
            VideoFrame::MailboxHolder::TextureNoLongerNeededCallback())),
        GL_TEXTURE_2D,
        gfx::Size(4, 4),
        gfx::Rect(0, 0, 3, 2),
        gfx::Size(4, 4),
        base::TimeDelta(),
        VideoFrame::ReadPixelsCB(),
        base::Closure());

    color_frame_provider_.set_frame(color_video_frame_);
    hw_frame_provider_.set_frame(hw_video_frame_);
    scaled_hw_frame_provider_.set_frame(scaled_hw_video_frame_);

    if (!delegating_renderer()) {
      // TODO(danakj): IOSurface layer can not be transported. crbug.com/239335
      scoped_refptr<IOSurfaceLayer> io_surface = IOSurfaceLayer::Create();
      io_surface->SetBounds(gfx::Size(10, 10));
      io_surface->SetAnchorPoint(gfx::PointF());
      io_surface->SetIsDrawable(true);
      io_surface->SetIOSurfaceProperties(1, gfx::Size(10, 10));
      root->AddChild(io_surface);
    }

    // Enable the hud.
    LayerTreeDebugState debug_state;
    debug_state.show_property_changed_rects = true;
    layer_tree_host()->SetDebugState(debug_state);

    scoped_refptr<PaintedScrollbarLayer> scrollbar =
        PaintedScrollbarLayer::Create(
            scoped_ptr<Scrollbar>(new FakeScrollbar).Pass(), content->id());
    scrollbar->SetBounds(gfx::Size(10, 10));
    scrollbar->SetAnchorPoint(gfx::PointF());
    scrollbar->SetIsDrawable(true);
    root->AddChild(scrollbar);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(host_impl);

    if (host_impl->active_tree()->source_frame_number() == 3) {
      // On the third commit we're recovering from context loss. Hardware
      // video frames should not be reused by the VideoFrameProvider, but
      // software frames can be.
      hw_frame_provider_.set_frame(NULL);
      scaled_hw_frame_provider_.set_frame(NULL);
    }
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame,
                                     bool result) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() == 2) {
      // Lose the context during draw on the second commit. This will cause
      // a third commit to recover.
      context3d_->set_times_bind_texture_succeeds(0);
    }
    return true;
  }

  virtual scoped_ptr<OutputSurface> CreateOutputSurface(
      bool fallback) OVERRIDE {
    if (layer_tree_host()) {
      lost_context_ = true;
      EXPECT_EQ(layer_tree_host()->source_frame_number(), 3);
    }
    return LayerTreeHostContextTest::CreateOutputSurface(fallback);
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ASSERT_TRUE(layer_tree_host()->hud_layer());
    // End the test once we know the 3nd frame drew.
    if (layer_tree_host()->source_frame_number() < 4) {
      layer_tree_host()->root_layer()->SetNeedsDisplay();
      layer_tree_host()->SetNeedsCommit();
    } else {
      EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_TRUE(lost_context_);
  }

 private:
  FakeContentLayerClient client_;
  bool lost_context_;

  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> child_output_surface_;
  scoped_ptr<ResourceProvider> child_resource_provider_;

  scoped_refptr<DelegatedFrameResourceCollection>
      delegated_resource_collection_;
  scoped_refptr<DelegatedFrameProvider> delegated_frame_provider_;

  scoped_refptr<VideoFrame> color_video_frame_;
  scoped_refptr<VideoFrame> hw_video_frame_;
  scoped_refptr<VideoFrame> scaled_hw_video_frame_;

  FakeVideoFrameProvider color_frame_provider_;
  FakeVideoFrameProvider hw_frame_provider_;
  FakeVideoFrameProvider scaled_hw_frame_provider_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestDontUseLostResources);

class LayerTreeHostContextTestCompositeAndReadbackBeforeOutputSurfaceInit
    : public LayerTreeHostContextTest {
 public:
  virtual void BeginTest() OVERRIDE {
    // This must be called immediately after creating LTH, before the first
    // OutputSurface is initialized.
    ASSERT_TRUE(layer_tree_host()->output_surface_lost());

    times_output_surface_created_ = 0;

    // Post the SetNeedsCommit before the readback to make sure it is run
    // on the main thread before the readback's replacement commit when
    // we have a threaded compositor.
    PostSetNeedsCommitToMainThread();

    char pixels[4];
    bool result = layer_tree_host()->CompositeAndReadback(
        &pixels, gfx::Rect(1, 1));
    EXPECT_EQ(!delegating_renderer(), result);
    EXPECT_EQ(1, times_output_surface_created_);
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    ++times_output_surface_created_;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {
    // Should not try to create output surface again after successfully
    // created by CompositeAndReadback.
    EXPECT_EQ(1, times_output_surface_created_);
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    EXPECT_GE(host_impl->active_tree()->source_frame_number(), 0);
    EXPECT_LE(host_impl->active_tree()->source_frame_number(), 1);
    return true;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    // We should only draw for the readback and the replacement commit.
    // The replacement commit will also be the first commit after output
    // surface initialization.
    EXPECT_GE(host_impl->active_tree()->source_frame_number(), 0);
    EXPECT_LE(host_impl->active_tree()->source_frame_number(), 1);
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    // We should only swap for the replacement commit.
    EXPECT_EQ(host_impl->active_tree()->source_frame_number(), 1);
    EndTest();
  }

 private:
  int times_output_surface_created_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestCompositeAndReadbackBeforeOutputSurfaceInit);

// This test verifies that losing an output surface during a
// simultaneous readback and forced redraw works and does not deadlock.
class LayerTreeHostContextTestLoseOutputSurfaceDuringReadbackAndForcedDraw
    : public LayerTreeHostContextTest {
 protected:
  static const int kFirstOutputSurfaceInitSourceFrameNumber = 0;
  static const int kReadbackSourceFrameNumber = 1;
  static const int kReadbackReplacementSourceFrameNumber = 2;
  static const int kSecondOutputSurfaceInitSourceFrameNumber = 3;

  LayerTreeHostContextTestLoseOutputSurfaceDuringReadbackAndForcedDraw()
      : did_react_to_first_commit_(false) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // This enables forced draws after a single prepare to draw failure.
    settings->timeout_and_draw_when_animation_checkerboards = true;
    settings->maximum_number_of_failed_draws_before_draw_is_forced_ = 1;
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == kFirstOutputSurfaceInitSourceFrameNumber ||
                sfn == kSecondOutputSurfaceInitSourceFrameNumber ||
                sfn == kReadbackSourceFrameNumber)
        << sfn;

    // Before we react to the failed draw by initiating the forced draw
    // sequence, start a readback on the main thread and then lose the context
    // to start output surface initialization all at the same time.
    if (sfn == kFirstOutputSurfaceInitSourceFrameNumber &&
        !did_react_to_first_commit_) {
      did_react_to_first_commit_ = true;
      PostReadbackToMainThread();
      LoseContext();
    }

    return false;
  }

  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) OVERRIDE {
    // -1 is for the first output surface initialization.
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == -1 || sfn == kReadbackReplacementSourceFrameNumber)
        << sfn;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    // We should only draw the first commit after output surface initialization
    // and attempt to draw the readback commit (which will fail).
    // All others should abort because the output surface is lost.
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == kSecondOutputSurfaceInitSourceFrameNumber ||
                sfn == kReadbackSourceFrameNumber)
        << sfn;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    // We should only swap the first commit after the second output surface
    // initialization.
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == kSecondOutputSurfaceInitSourceFrameNumber) << sfn;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  int did_react_to_first_commit_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestLoseOutputSurfaceDuringReadbackAndForcedDraw);

// This test verifies that losing an output surface right before a
// simultaneous readback and forced redraw works and does not deadlock.
class LayerTreeHostContextTestReadbackWithForcedDrawAndOutputSurfaceInit
    : public LayerTreeHostContextTest {
 protected:
  static const int kFirstOutputSurfaceInitSourceFrameNumber = 0;
  static const int kReadbackSourceFrameNumber = 1;
  static const int kForcedDrawCommitSourceFrameNumber = 2;
  static const int kSecondOutputSurfaceInitSourceFrameNumber = 2;

  LayerTreeHostContextTestReadbackWithForcedDrawAndOutputSurfaceInit()
      : did_lose_context_(false) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // This enables forced draws after a single prepare to draw failure.
    settings->timeout_and_draw_when_animation_checkerboards = true;
    settings->maximum_number_of_failed_draws_before_draw_is_forced_ = 1;
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == kFirstOutputSurfaceInitSourceFrameNumber ||
                sfn == kSecondOutputSurfaceInitSourceFrameNumber ||
                sfn == kReadbackSourceFrameNumber)
        << sfn;

    // Before we react to the failed draw by initiating the forced draw
    // sequence, start a readback on the main thread and then lose the context
    // to start output surface initialization all at the same time.
    if (sfn == kFirstOutputSurfaceInitSourceFrameNumber && !did_lose_context_) {
      did_lose_context_ = true;
      LoseContext();
    }

    // Returning false will result in a forced draw.
    return false;
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    if (layer_tree_host()->source_frame_number() > 0) {
      // Perform a readback right after the second output surface
      // initialization.
      char pixels[4];
      layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
    }
  }

  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) OVERRIDE {
    // -1 is for the first output surface initialization.
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == -1 || sfn == kFirstOutputSurfaceInitSourceFrameNumber)
        << sfn;
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    // We should only draw the first commit after output surface initialization
    // and attempt to draw the readback commit (which will fail).
    // All others should abort because the output surface is lost.
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == kForcedDrawCommitSourceFrameNumber ||
                sfn == kReadbackSourceFrameNumber)
        << sfn;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    // We should only swap the first commit after the second output surface
    // initialization.
    int sfn = host_impl->active_tree()->source_frame_number();
    EXPECT_TRUE(sfn == kForcedDrawCommitSourceFrameNumber) << sfn;
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  int did_lose_context_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostContextTestReadbackWithForcedDrawAndOutputSurfaceInit);

class ImplSidePaintingLayerTreeHostContextTest
    : public LayerTreeHostContextTest {
 public:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->impl_side_painting = true;
  }
};

class LayerTreeHostContextTestImplSidePainting
    : public ImplSidePaintingLayerTreeHostContextTest {
 public:
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    scoped_refptr<PictureLayer> picture = PictureLayer::Create(&client_);
    picture->SetBounds(gfx::Size(10, 10));
    picture->SetAnchorPoint(gfx::PointF());
    picture->SetIsDrawable(true);
    root->AddChild(picture);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    times_to_lose_during_commit_ = 1;
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
    EndTest();
  }

 private:
  FakeContentLayerClient client_;
};

MULTI_THREAD_TEST_F(LayerTreeHostContextTestImplSidePainting);

class ScrollbarLayerLostContext : public LayerTreeHostContextTest {
 public:
  ScrollbarLayerLostContext() : commits_(0) {}

  virtual void BeginTest() OVERRIDE {
    scoped_refptr<Layer> scroll_layer = Layer::Create();
    scrollbar_layer_ = FakePaintedScrollbarLayer::Create(
        false, true, scroll_layer->id());
    scrollbar_layer_->SetBounds(gfx::Size(10, 100));
    layer_tree_host()->root_layer()->AddChild(scrollbar_layer_);
    layer_tree_host()->root_layer()->AddChild(scroll_layer);
    PostSetNeedsCommitToMainThread();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);

    ++commits_;
    switch (commits_) {
      case 1:
        // First (regular) update, we should upload 2 resources (thumb, and
        // backtrack).
        EXPECT_EQ(1, scrollbar_layer_->update_count());
        LoseContext();
        break;
      case 2:
        // Second update, after the lost context, we should still upload 2
        // resources even if the contents haven't changed.
        EXPECT_EQ(2, scrollbar_layer_->update_count());
        EndTest();
        break;
      case 3:
        // Single thread proxy issues extra commits after context lost.
        // http://crbug.com/287250
        if (HasImplThread())
          NOTREACHED();
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  int commits_;
  scoped_refptr<FakePaintedScrollbarLayer> scrollbar_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(ScrollbarLayerLostContext);

// Not reusing LayerTreeTest because it expects creating LTH to always succeed.
class LayerTreeHostTestCannotCreateIfCannotCreateOutputSurface
    : public testing::Test,
      public FakeLayerTreeHostClient {
 public:
  LayerTreeHostTestCannotCreateIfCannotCreateOutputSurface()
      : FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D) {}

  // FakeLayerTreeHostClient implementation.
  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE {
    return scoped_ptr<OutputSurface>();
  }

  void RunTest(bool threaded,
               bool delegating_renderer,
               bool impl_side_painting) {
    LayerTreeSettings settings;
    settings.impl_side_painting = impl_side_painting;
    if (threaded) {
      scoped_ptr<base::Thread> impl_thread(new base::Thread("LayerTreeTest"));
      ASSERT_TRUE(impl_thread->Start());
      ASSERT_TRUE(impl_thread->message_loop_proxy().get());
      scoped_ptr<LayerTreeHost> layer_tree_host = LayerTreeHost::CreateThreaded(
          this, NULL, settings, impl_thread->message_loop_proxy());
      EXPECT_FALSE(layer_tree_host);
    } else {
      scoped_ptr<LayerTreeHost> layer_tree_host =
          LayerTreeHost::CreateSingleThreaded(this, this, NULL, settings);
      EXPECT_FALSE(layer_tree_host);
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostTestCannotCreateIfCannotCreateOutputSurface);

class UIResourceLostTest : public LayerTreeHostContextTest {
 public:
  UIResourceLostTest() : time_step_(0) {}
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    settings->texture_id_allocation_chunk_size = 1;
  }
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }
  virtual void AfterTest() OVERRIDE {}

  // This is called on the main thread after each commit and
  // DidActivateTreeOnThread, with the value of time_step_ at the time
  // of the call to DidActivateTreeOnThread. Similar tests will do
  // work on the main thread in DidCommit but that is unsuitable because
  // the main thread work for these tests must happen after
  // DidActivateTreeOnThread, which happens after DidCommit with impl-side
  // painting.
  virtual void StepCompleteOnMainThread(int time_step) = 0;

  // Called after DidActivateTreeOnThread. If this is done during the commit,
  // the call to StepCompleteOnMainThread will not occur until after
  // the commit completes, because the main thread is blocked.
  void PostStepCompleteToMainThread() {
    proxy()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(
            &UIResourceLostTest::StepCompleteOnMainThreadInternal,
            base::Unretained(this),
            time_step_));
  }

  void PostLoseContextToImplThread() {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    base::SingleThreadTaskRunner* task_runner =
        HasImplThread() ? ImplThreadTaskRunner()
                        : base::MessageLoopProxy::current();
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(
            &LayerTreeHostContextTest::LoseContext,
            base::Unretained(this)));
  }

 protected:
  int time_step_;
  scoped_ptr<FakeScopedUIResource> ui_resource_;

 private:
  void StepCompleteOnMainThreadInternal(int step) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    StepCompleteOnMainThread(step);
  }
};

class UIResourceLostTestSimple : public UIResourceLostTest {
 public:
  // This is called when the commit is complete and the new layer tree has been
  // activated.
  virtual void StepCompleteOnImplThread(LayerTreeHostImpl* impl) = 0;

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (!layer_tree_host()->settings().impl_side_painting) {
      StepCompleteOnImplThread(impl);
      PostStepCompleteToMainThread();
      ++time_step_;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (layer_tree_host()->settings().impl_side_painting) {
      StepCompleteOnImplThread(impl);
      PostStepCompleteToMainThread();
      ++time_step_;
    }
  }
};

// Losing context after an UI resource has been created.
class UIResourceLostAfterCommit : public UIResourceLostTestSimple {
 public:
  virtual void StepCompleteOnMainThread(int step) OVERRIDE {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        // Expects a valid UIResourceId.
        EXPECT_NE(0, ui_resource_->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Release resource before ending the test.
        ui_resource_.reset();
        EndTest();
        break;
      case 5:
        // Single thread proxy issues extra commits after context lost.
        // http://crbug.com/287250
        if (HasImplThread())
          NOTREACHED();
        break;
      case 6:
        NOTREACHED();
    }
  }

  virtual void StepCompleteOnImplThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // The resource should have been created on LTHI after the commit.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        LoseContext();
        break;
      case 3:
        // The resources should have been recreated. The bitmap callback should
        // have been called once with the resource_lost flag set to true.
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        // Resource Id on the impl-side have been recreated as well. Note
        // that the same UIResourceId persists after the context lost.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        PostSetNeedsCommitToMainThread();
        break;
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostAfterCommit);

// Losing context before UI resource requests can be commited.  Three sequences
// of creation/deletion are considered:
// 1. Create one resource -> Context Lost => Expect the resource to have been
// created.
// 2. Delete an exisiting resource (test_id0_) -> create a second resource
// (test_id1_) -> Context Lost => Expect the test_id0_ to be removed and
// test_id1_ to have been created.
// 3. Create one resource -> Delete that same resource -> Context Lost => Expect
// the resource to not exist in the manager.
class UIResourceLostBeforeCommit : public UIResourceLostTestSimple {
 public:
  UIResourceLostBeforeCommit()
      : test_id0_(0),
        test_id1_(0) {}

  virtual void StepCompleteOnMainThread(int step) OVERRIDE {
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        // Lose the context on the impl thread before the commit.
        PostLoseContextToImplThread();
        break;
      case 2:
        // Sequence 2:
        // Currently one resource has been created.
        test_id0_ = ui_resource_->id();
        // Delete this resource.
        ui_resource_.reset();
        // Create another resource.
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        test_id1_ = ui_resource_->id();
        // Sanity check that two resource creations return different ids.
        EXPECT_NE(test_id0_, test_id1_);
        // Lose the context on the impl thread before the commit.
        PostLoseContextToImplThread();
        break;
      case 3:
        // Clear the manager of resources.
        ui_resource_.reset();
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        // Sequence 3:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        test_id0_ = ui_resource_->id();
        // Sanity check the UIResourceId should not be 0.
        EXPECT_NE(0, test_id0_);
        // Usually ScopedUIResource are deleted from the manager in their
        // destructor (so usually ui_resource_.reset()).  But here we need
        // ui_resource_ for the next step, so call DeleteUIResource directly.
        layer_tree_host()->DeleteUIResource(test_id0_);
        // Delete the resouce and then lose the context.
        PostLoseContextToImplThread();
        break;
      case 5:
        // Release resource before ending the test.
        ui_resource_.reset();
        EndTest();
        break;
      case 6:
        // Single thread proxy issues extra commits after context lost.
        // http://crbug.com/287250
        if (HasImplThread())
          NOTREACHED();
        break;
      case 8:
        NOTREACHED();
    }
  }

  virtual void StepCompleteOnImplThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // Sequence 1 (continued):
        // The first context lost happens before the resources were created,
        // and because it resulted in no resources being destroyed, it does not
        // trigger resource re-creation.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // Resource Id on the impl-side has been created.
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        // Sequence 2 (continued):
        // The previous resource should have been deleted.
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id0_));
        if (HasImplThread()) {
          // The second resource should have been created.
          EXPECT_NE(0u, impl->ResourceIdForUIResource(test_id1_));
        } else {
          // The extra commit that happens at context lost in the single thread
          // proxy changes the timing so that the resource has been destroyed.
          // http://crbug.com/287250
          EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id1_));
        }
        // The second resource called the resource callback once and since the
        // context is lost, a "resource lost" callback was also issued.
        EXPECT_EQ(2, ui_resource_->resource_create_count);
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        break;
      case 5:
        // Sequence 3 (continued):
        // Expect the resource callback to have been called once.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        // No "resource lost" callbacks.
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // The UI resource id should not be valid
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id0_));
        break;
    }
  }

 private:
  UIResourceId test_id0_;
  UIResourceId test_id1_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostBeforeCommit);

// Losing UI resource before the pending trees is activated but after the
// commit.  Impl-side-painting only.
class UIResourceLostBeforeActivateTree : public UIResourceLostTest {
  virtual void StepCompleteOnMainThread(int step) OVERRIDE {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        PostSetNeedsCommitToMainThread();
        break;
      case 3:
        test_id_ = ui_resource_->id();
        ui_resource_.reset();
        PostSetNeedsCommitToMainThread();
        break;
      case 5:
        // Release resource before ending the test.
        ui_resource_.reset();
        EndTest();
        break;
      case 6:
        // Make sure no extra commits happened.
        NOTREACHED();
    }
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 2:
        PostSetNeedsCommitToMainThread();
        break;
      case 4:
        PostSetNeedsCommitToMainThread();
        break;
    }
  }

  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    switch (time_step_) {
      case 1:
        // The resource creation callback has been called.
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        // The resource is not yet lost (sanity check).
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        // The resource should not have been created yet on the impl-side.
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        LoseContext();
        break;
      case 3:
        LoseContext();
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::DidActivateTreeOnThread(impl);
    switch (time_step_) {
      case 1:
        // The pending requests on the impl-side should have been processed.
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        break;
      case 2:
        // The "lost resource" callback should have been called once.
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        break;
      case 4:
        // The resource is deleted and should not be in the manager.  Use
        // test_id_ since ui_resource_ has been deleted.
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(test_id_));
        break;
    }

    PostStepCompleteToMainThread();
    ++time_step_;
  }

 private:
  UIResourceId test_id_;
};

TEST_F(UIResourceLostBeforeActivateTree,
       RunMultiThread_DirectRenderer_ImplSidePaint) {
  RunTest(true, false, true);
}

TEST_F(UIResourceLostBeforeActivateTree,
       RunMultiThread_DelegatingRenderer_ImplSidePaint) {
  RunTest(true, true, true);
}

// Resources evicted explicitly and by visibility changes.
class UIResourceLostEviction : public UIResourceLostTestSimple {
 public:
  virtual void StepCompleteOnMainThread(int step) OVERRIDE {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    switch (step) {
      case 0:
        ui_resource_ = FakeScopedUIResource::Create(layer_tree_host());
        EXPECT_NE(0, ui_resource_->id());
        PostSetNeedsCommitToMainThread();
        break;
      case 2:
        // Make the tree not visible.
        PostSetVisibleToMainThread(false);
        break;
      case 3:
        // Release resource before ending the test.
        ui_resource_.reset();
        EndTest();
        break;
      case 4:
        NOTREACHED();
    }
  }

  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* impl,
                                       bool visible) OVERRIDE {
    TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(
        impl->output_surface()->context_provider()->Context3d());
    if (!visible) {
      // All resources should have been evicted.
      ASSERT_EQ(0u, context->NumTextures());
      EXPECT_EQ(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
      EXPECT_EQ(2, ui_resource_->resource_create_count);
      EXPECT_EQ(1, ui_resource_->lost_resource_count);
      // Drawing is disabled both because of the evicted resources and
      // because the renderer is not visible.
      EXPECT_FALSE(impl->CanDraw());
      // Make the renderer visible again.
      PostSetVisibleToMainThread(true);
    }
  }

  virtual void StepCompleteOnImplThread(LayerTreeHostImpl* impl) OVERRIDE {
    TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(
        impl->output_surface()->context_provider()->Context3d());
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (time_step_) {
      case 1:
        // The resource should have been created on LTHI after the commit.
        ASSERT_EQ(1u, context->NumTextures());
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        // Evict all UI resources. This will trigger a commit.
        impl->EvictAllUIResources();
        ASSERT_EQ(0u, context->NumTextures());
        EXPECT_EQ(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(1, ui_resource_->resource_create_count);
        EXPECT_EQ(0, ui_resource_->lost_resource_count);
        EXPECT_FALSE(impl->CanDraw());
        break;
      case 2:
        // The resource should have been recreated.
        ASSERT_EQ(1u, context->NumTextures());
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(2, ui_resource_->resource_create_count);
        EXPECT_EQ(1, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        break;
      case 3:
        // The resource should have been recreated after visibility was
        // restored.
        ASSERT_EQ(1u, context->NumTextures());
        EXPECT_NE(0u, impl->ResourceIdForUIResource(ui_resource_->id()));
        EXPECT_EQ(3, ui_resource_->resource_create_count);
        EXPECT_EQ(2, ui_resource_->lost_resource_count);
        EXPECT_TRUE(impl->CanDraw());
        break;
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(UIResourceLostEviction);

class LayerTreeHostContextTestSurfaceCreateCallback
    : public LayerTreeHostContextTest {
 public:
  LayerTreeHostContextTestSurfaceCreateCallback()
      : LayerTreeHostContextTest(),
        layer_(FakeContentLayer::Create(&client_)),
        num_commits_(0) {}

  virtual void SetupTree() OVERRIDE {
    layer_->SetBounds(gfx::Size(10, 20));
    layer_tree_host()->SetRootLayer(layer_);
    LayerTreeHostContextTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    switch (num_commits_) {
      case 0:
        EXPECT_EQ(1u, layer_->output_surface_created_count());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 1:
        EXPECT_EQ(1u, layer_->output_surface_created_count());
        layer_tree_host()->SetNeedsCommit();
        break;
      case 2:
        EXPECT_EQ(1u, layer_->output_surface_created_count());
        break;
      case 3:
        EXPECT_EQ(2u, layer_->output_surface_created_count());
        layer_tree_host()->SetNeedsCommit();
        break;
    }
    ++num_commits_;
  }

  virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerTreeHostContextTest::CommitCompleteOnThread(impl);
    switch (num_commits_) {
      case 0:
        break;
      case 1:
        break;
      case 2:
        LoseContext();
        break;
      case 3:
        EndTest();
        break;
    }
  }

  virtual void DidInitializeOutputSurface(bool succeeded) OVERRIDE {
    EXPECT_TRUE(succeeded);
  }

  virtual void AfterTest() OVERRIDE {}

 protected:
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> layer_;
  int num_commits_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostContextTestSurfaceCreateCallback);

}  // namespace
}  // namespace cc
