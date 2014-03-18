// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/returned_resource.h"
#include "cc/test/fake_delegated_renderer_layer.h"
#include "cc/test/fake_delegated_renderer_layer_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace cc {
namespace {

bool ReturnedResourceLower(const ReturnedResource& a,
                           const ReturnedResource& b) {
  return a.id < b.id;
}

// Tests if the list of resources matches an expectation, modulo the order.
bool ResourcesMatch(ReturnedResourceArray actual,
                    unsigned* expected,
                    size_t expected_count) {
  std::sort(actual.begin(), actual.end(), ReturnedResourceLower);
  std::sort(expected, expected + expected_count);
  size_t actual_index = 0;

  // for each element of the expected array, count off one of the actual array
  // (after checking it matches).
  for (size_t expected_index = 0; expected_index < expected_count;
       ++expected_index) {
    EXPECT_LT(actual_index, actual.size());
    if (actual_index >= actual.size())
      return false;
    EXPECT_EQ(actual[actual_index].id, expected[expected_index]);
    if (actual[actual_index].id != expected[expected_index])
      return false;
    EXPECT_GT(actual[actual_index].count, 0);
    if (actual[actual_index].count <= 0) {
      return false;
    } else {
      --actual[actual_index].count;
      if (actual[actual_index].count == 0)
        ++actual_index;
    }
  }
  EXPECT_EQ(actual_index, actual.size());
  return actual_index == actual.size();
}

#define EXPECT_RESOURCES(expected, actual) \
    EXPECT_TRUE(ResourcesMatch(actual, expected, arraysize(expected)));

// These tests deal with delegated renderer layers.
class LayerTreeHostDelegatedTest : public LayerTreeTest {
 protected:
  scoped_ptr<DelegatedFrameData> CreateFrameData(gfx::Rect root_output_rect,
                                                 gfx::Rect root_damage_rect) {
    scoped_ptr<DelegatedFrameData> frame(new DelegatedFrameData);

    scoped_ptr<RenderPass> root_pass(RenderPass::Create());
    root_pass->SetNew(RenderPass::Id(1, 1),
                      root_output_rect,
                      root_damage_rect,
                      gfx::Transform());
    frame->render_pass_list.push_back(root_pass.Pass());
    return frame.Pass();
  }

  scoped_ptr<DelegatedFrameData> CreateInvalidFrameData(
      gfx::Rect root_output_rect,
      gfx::Rect root_damage_rect) {
    scoped_ptr<DelegatedFrameData> frame(new DelegatedFrameData);

    scoped_ptr<RenderPass> root_pass(RenderPass::Create());
    root_pass->SetNew(RenderPass::Id(1, 1),
                      root_output_rect,
                      root_damage_rect,
                      gfx::Transform());

    scoped_ptr<SharedQuadState> shared_quad_state = SharedQuadState::Create();

    gfx::Rect rect = root_output_rect;
    gfx::Rect opaque_rect = root_output_rect;
    // An invalid resource id! The resource isn't part of the frame.
    unsigned resource_id = 5;
    bool premultiplied_alpha = false;
    gfx::PointF uv_top_left = gfx::PointF(0.f, 0.f);
    gfx::PointF uv_bottom_right = gfx::PointF(1.f, 1.f);
    SkColor background_color = 0;
    float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
    bool flipped = false;

    scoped_ptr<TextureDrawQuad> invalid_draw_quad = TextureDrawQuad::Create();
    invalid_draw_quad->SetNew(shared_quad_state.get(),
                              rect,
                              opaque_rect,
                              resource_id,
                              premultiplied_alpha,
                              uv_top_left,
                              uv_bottom_right,
                              background_color,
                              vertex_opacity,
                              flipped);
    root_pass->quad_list.push_back(invalid_draw_quad.PassAs<DrawQuad>());

    root_pass->shared_quad_state_list.push_back(shared_quad_state.Pass());

    frame->render_pass_list.push_back(root_pass.Pass());
    return frame.Pass();
  }

  void AddTransferableResource(DelegatedFrameData* frame,
                               ResourceProvider::ResourceId resource_id) {
    TransferableResource resource;
    resource.id = resource_id;
    resource.target = GL_TEXTURE_2D;
    frame->resource_list.push_back(resource);
  }

  void AddTextureQuad(DelegatedFrameData* frame,
                      ResourceProvider::ResourceId resource_id) {
    scoped_ptr<SharedQuadState> sqs = SharedQuadState::Create();
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    float vertex_opacity[4] = { 1.f, 1.f, 1.f, 1.f };
    quad->SetNew(sqs.get(),
                 gfx::Rect(0, 0, 10, 10),
                 gfx::Rect(0, 0, 10, 10),
                 resource_id,
                 false,
                 gfx::PointF(0.f, 0.f),
                 gfx::PointF(1.f, 1.f),
                 SK_ColorTRANSPARENT,
                 vertex_opacity,
                 false);
    frame->render_pass_list[0]->shared_quad_state_list.push_back(sqs.Pass());
    frame->render_pass_list[0]->quad_list.push_back(quad.PassAs<DrawQuad>());
  }

  void AddRenderPass(DelegatedFrameData* frame,
                     RenderPass::Id id,
                     gfx::Rect output_rect,
                     gfx::Rect damage_rect,
                     const FilterOperations& filters,
                     const FilterOperations& background_filters) {
    for (size_t i = 0; i < frame->render_pass_list.size(); ++i)
      DCHECK(id != frame->render_pass_list[i]->id);

    scoped_ptr<RenderPass> pass(RenderPass::Create());
    pass->SetNew(id,
                 output_rect,
                 damage_rect,
                 gfx::Transform());
    frame->render_pass_list.push_back(pass.Pass());

    scoped_ptr<SharedQuadState> sqs = SharedQuadState::Create();
    scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();

    quad->SetNew(sqs.get(),
                 output_rect,
                 id,
                 false,  // is_replica
                 0,  // mask_resource_id
                 damage_rect,
                 gfx::Rect(0, 0, 1, 1),  // mask_uv_rect
                 filters,
                 background_filters);
    frame->render_pass_list[0]->shared_quad_state_list.push_back(sqs.Pass());
    frame->render_pass_list[0]->quad_list.push_back(quad.PassAs<DrawQuad>());
  }

  static ResourceProvider::ResourceId AppendResourceId(
      std::vector<ResourceProvider::ResourceId>* resources_in_last_sent_frame,
      ResourceProvider::ResourceId resource_id) {
    resources_in_last_sent_frame->push_back(resource_id);
    return resource_id;
  }

  void ReturnUnusedResourcesFromParent(LayerTreeHostImpl* host_impl) {
    DelegatedFrameData* delegated_frame_data =
        output_surface()->last_sent_frame().delegated_frame_data.get();
    if (!delegated_frame_data)
      return;

    std::vector<ResourceProvider::ResourceId> resources_in_last_sent_frame;
    for (size_t i = 0; i < delegated_frame_data->resource_list.size(); ++i) {
      resources_in_last_sent_frame.push_back(
          delegated_frame_data->resource_list[i].id);
    }

    std::vector<ResourceProvider::ResourceId> resources_to_return;

    const TransferableResourceArray& resources_held_by_parent =
        output_surface()->resources_held_by_parent();
    for (size_t i = 0; i < resources_held_by_parent.size(); ++i) {
      ResourceProvider::ResourceId resource_in_parent =
          resources_held_by_parent[i].id;
      bool resource_in_parent_is_not_part_of_frame =
          std::find(resources_in_last_sent_frame.begin(),
                    resources_in_last_sent_frame.end(),
                    resource_in_parent) == resources_in_last_sent_frame.end();
      if (resource_in_parent_is_not_part_of_frame)
        resources_to_return.push_back(resource_in_parent);
    }

    if (resources_to_return.empty())
      return;

    CompositorFrameAck ack;
    for (size_t i = 0; i < resources_to_return.size(); ++i)
      output_surface()->ReturnResource(resources_to_return[i], &ack);
    host_impl->ReclaimResources(&ack);
    host_impl->OnSwapBuffersComplete();
  }
};

class LayerTreeHostDelegatedTestCaseSingleDelegatedLayer
    : public LayerTreeHostDelegatedTest,
      public DelegatedFrameResourceCollectionClient {
 public:
  LayerTreeHostDelegatedTestCaseSingleDelegatedLayer()
      : resource_collection_(new DelegatedFrameResourceCollection),
        available_(false) {
    resource_collection_->SetClient(this);
  }

  virtual void SetupTree() OVERRIDE {
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(gfx::Size(10, 10));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostDelegatedTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    resource_collection_->SetClient(this);
    PostSetNeedsCommitToMainThread();
  }

  void SetFrameData(scoped_ptr<DelegatedFrameData> frame_data) {
    RenderPass* root_pass = frame_data->render_pass_list.back();
    gfx::Size frame_size = root_pass->output_rect.size();

    if (frame_provider_.get() && frame_size == frame_provider_->frame_size()) {
      frame_provider_->SetFrameData(frame_data.Pass());
      return;
    }

    if (delegated_.get()) {
      delegated_->RemoveFromParent();
      delegated_ = NULL;
      frame_provider_ = NULL;
    }

    frame_provider_ = new DelegatedFrameProvider(resource_collection_.get(),
                                                 frame_data.Pass());

    delegated_ = CreateDelegatedLayer(frame_provider_.get());
  }

  scoped_refptr<DelegatedRendererLayer> CreateDelegatedLayer(
      DelegatedFrameProvider* frame_provider) {
    scoped_refptr<DelegatedRendererLayer> delegated =
        FakeDelegatedRendererLayer::Create(frame_provider);
    delegated->SetAnchorPoint(gfx::PointF());
    delegated->SetBounds(gfx::Size(10, 10));
    delegated->SetIsDrawable(true);

    root_->AddChild(delegated);
    return delegated;
  }

  virtual void AfterTest() OVERRIDE { resource_collection_->SetClient(NULL); }

  // DelegatedFrameProviderClient implementation.
  virtual void UnusedResourcesAreAvailable() OVERRIDE { available_ = true; }

  bool TestAndResetAvailable() {
    bool available = available_;
    available_ = false;
    return available;
  }

 protected:
  scoped_refptr<DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<DelegatedFrameProvider> frame_provider_;
  scoped_refptr<Layer> root_;
  scoped_refptr<DelegatedRendererLayer> delegated_;
  bool available_;
};

class LayerTreeHostDelegatedTestCreateChildId
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  LayerTreeHostDelegatedTestCreateChildId()
      : LayerTreeHostDelegatedTestCaseSingleDelegatedLayer(),
        num_activates_(0),
        did_reset_child_id_(false) {}

  virtual void DidCommit() OVERRIDE {
    if (TestEnded())
      return;
    SetFrameData(CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1)));
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() < 1)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    ContextProvider* context_provider =
        host_impl->output_surface()->context_provider();

    ++num_activates_;
    switch (num_activates_) {
      case 2:
        EXPECT_TRUE(delegated_impl->ChildId());
        EXPECT_FALSE(did_reset_child_id_);

        context_provider->Context3d()->loseContextCHROMIUM(
            GL_GUILTY_CONTEXT_RESET_ARB,
            GL_INNOCENT_CONTEXT_RESET_ARB);
        break;
      case 3:
        EXPECT_TRUE(delegated_impl->ChildId());
        EXPECT_TRUE(did_reset_child_id_);
        EndTest();
        break;
    }
  }

  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) OVERRIDE {
    EXPECT_TRUE(success);

    if (num_activates_ < 2)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    EXPECT_EQ(2, num_activates_);
    EXPECT_FALSE(delegated_impl->ChildId());
    did_reset_child_id_ = true;
  }

 protected:
  int num_activates_;
  bool did_reset_child_id_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestCreateChildId);

class LayerTreeHostDelegatedTestOffscreenContext_NoFilters
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 protected:
  virtual void BeginTest() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame =
        CreateFrameData(gfx::Rect(0, 0, 1, 1),
                        gfx::Rect(0, 0, 1, 1));
    SetFrameData(frame.Pass());

    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    EXPECT_FALSE(host_impl->offscreen_context_provider());
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestOffscreenContext_NoFilters);

class LayerTreeHostDelegatedTestOffscreenContext_Filters
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 protected:
  virtual void BeginTest() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame =
        CreateFrameData(gfx::Rect(0, 0, 1, 1),
                        gfx::Rect(0, 0, 1, 1));

    FilterOperations filters;
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    AddRenderPass(frame.get(),
                  RenderPass::Id(2, 1),
                  gfx::Rect(0, 0, 1, 1),
                  gfx::Rect(0, 0, 1, 1),
                  filters,
                  FilterOperations());
    SetFrameData(frame.Pass());

    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    bool expect_context = !delegating_renderer();
    EXPECT_EQ(expect_context, !!host_impl->offscreen_context_provider());
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestOffscreenContext_Filters);

class LayerTreeHostDelegatedTestOffscreenContext_BackgroundFilters
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 protected:
  virtual void BeginTest() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame =
        CreateFrameData(gfx::Rect(0, 0, 1, 1),
                        gfx::Rect(0, 0, 1, 1));

    FilterOperations filters;
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    AddRenderPass(frame.get(),
                  RenderPass::Id(2, 1),
                  gfx::Rect(0, 0, 1, 1),
                  gfx::Rect(0, 0, 1, 1),
                  FilterOperations(),
                  filters);
    SetFrameData(frame.Pass());

    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    bool expect_context = !delegating_renderer();
    EXPECT_EQ(expect_context, !!host_impl->offscreen_context_provider());
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestOffscreenContext_BackgroundFilters);

class LayerTreeHostDelegatedTestOffscreenContext_Filters_AddedToTree
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 protected:
  virtual void BeginTest() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame_no_filters =
        CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));

    scoped_ptr<DelegatedFrameData> frame_with_filters =
        CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));

    FilterOperations filters;
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    AddRenderPass(frame_with_filters.get(),
                  RenderPass::Id(2, 1),
                  gfx::Rect(0, 0, 1, 1),
                  gfx::Rect(0, 0, 1, 1),
                  filters,
                  FilterOperations());

    SetFrameData(frame_no_filters.Pass());
    delegated_->RemoveFromParent();
    SetFrameData(frame_with_filters.Pass());
    layer_tree_host()->root_layer()->AddChild(delegated_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    bool expect_context = !delegating_renderer();
    EXPECT_EQ(expect_context, !!host_impl->offscreen_context_provider());
    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestOffscreenContext_Filters_AddedToTree);

class LayerTreeHostDelegatedTestLayerUsesFrameDamage
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  LayerTreeHostDelegatedTestLayerUsesFrameDamage()
      : LayerTreeHostDelegatedTestCaseSingleDelegatedLayer(),
        first_draw_for_source_frame_(true) {}

  virtual void DidCommit() OVERRIDE {
    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // The first time the layer gets a frame the whole layer should be
        // damaged.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1)));
        break;
      case 2:
        // A different frame size will damage the whole layer.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 20, 20), gfx::Rect(0, 0, 0, 0)));
        break;
      case 3:
        // Should create a total amount of gfx::Rect(2, 2, 10, 6) damage.
        // The frame size is 20x20 while the layer is 10x10, so this should
        // produce a gfx::Rect(1, 1, 5, 3) damage rect.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 20, 20), gfx::Rect(2, 2, 5, 5)));
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 20, 20), gfx::Rect(7, 2, 5, 6)));
        break;
      case 4:
        // Should create zero damage.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        // Should damage the full viewport.
        delegated_->SetBounds(gfx::Size(2, 2));
        break;
      case 6:
        // Should create zero damage.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 7:
        // Should damage the full layer, tho the frame size is not changing.
        delegated_->SetBounds(gfx::Size(6, 6));
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 20, 20), gfx::Rect(1, 1, 2, 2)));
        break;
      case 8:
        // Should create zero damage.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 9:
        // Should damage the full layer.
        delegated_->SetDisplaySize(gfx::Size(10, 10));
        break;
      case 10:
        // Should create zero damage.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 11:
        // Changing the frame size damages the full layer.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 5, 5), gfx::Rect(4, 4, 1, 1)));
        break;
      case 12:
        // An invalid frame isn't used, so it should not cause damage.
        SetFrameData(CreateInvalidFrameData(gfx::Rect(0, 0, 5, 5),
                                            gfx::Rect(4, 4, 1, 1)));
        break;
      case 13:
        // Should create gfx::Rect(1, 1, 2, 2) of damage. The frame size is
        // 5x5 and the display size is now set to 10x10, so this should result
        // in a gfx::Rect(2, 2, 4, 4) damage rect.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 5, 5), gfx::Rect(1, 1, 2, 2)));
        break;
      case 14:
        // Should create zero damage.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 15:
        // Moving the layer out of the tree and back in will damage the whole
        // impl layer.
        delegated_->RemoveFromParent();
        layer_tree_host()->root_layer()->AddChild(delegated_);
        break;
      case 16:
        // Make a larger frame with lots of damage. Then a frame smaller than
        // the first frame's damage. The entire layer should be damaged, but
        // nothing more.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 10, 10), gfx::Rect(0, 0, 10, 10)));
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 5, 5), gfx::Rect(1, 1, 2, 2)));
        break;
      case 17:
        // Make a frame with lots of damage. Then replace it with a frame with
        // no damage. The entire layer should be damaged, but nothing more.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 10, 10), gfx::Rect(0, 0, 10, 10)));
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 10, 10), gfx::Rect(0, 0, 0, 0)));
        break;
      case 18:
        // Make another layer that uses the same frame provider. The new layer
        // should be damaged.
        delegated_copy_ = CreateDelegatedLayer(frame_provider_);
        delegated_copy_->SetPosition(gfx::Point(5, 0));

        // Also set a new frame.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 10, 10), gfx::Rect(4, 0, 1, 1)));
        break;
      case 19:
        // Set another new frame, both layers should be damaged in the same
        // ways.
        SetFrameData(
            CreateFrameData(gfx::Rect(0, 0, 10, 10), gfx::Rect(3, 3, 1, 1)));
    }
    first_draw_for_source_frame_ = true;
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame,
                                     bool result) OVERRIDE {
    EXPECT_TRUE(result);

    if (!first_draw_for_source_frame_)
      return result;

    gfx::RectF damage_rect;
    if (!frame->has_no_damage) {
      damage_rect = frame->render_passes.back()->damage_rect;
    } else {
      // If there is no damage, then we have no render passes to send.
      EXPECT_TRUE(frame->render_passes.empty());
    }

    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        // First frame is damaged because of viewport resize.
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 1:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 2:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 3:
        EXPECT_EQ(gfx::RectF(1.f, 1.f, 5.f, 3.f).ToString(),
                  damage_rect.ToString());
        break;
      case 4:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 5:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 6:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 7:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 6.f).ToString(),
                  damage_rect.ToString());
        break;
      case 8:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 9:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 6.f).ToString(),
                  damage_rect.ToString());
        break;
      case 10:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 11:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 12:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 13:
        EXPECT_EQ(gfx::RectF(2.f, 2.f, 4.f, 4.f).ToString(),
                  damage_rect.ToString());
        break;
      case 14:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 0.f, 0.f).ToString(),
                  damage_rect.ToString());
        break;
      case 15:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 16:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 17:
        EXPECT_EQ(gfx::RectF(0.f, 0.f, 10.f, 10.f).ToString(),
                  damage_rect.ToString());
        break;
      case 18:
        EXPECT_EQ(gfx::UnionRects(gfx::RectF(5.f, 0.f, 10.f, 10.f),
                                  gfx::RectF(4.f, 0.f, 1.f, 1.f)).ToString(),
                  damage_rect.ToString());
        break;
      case 19:
        EXPECT_EQ(gfx::RectF(3.f, 3.f, 6.f, 1.f).ToString(),
                  damage_rect.ToString());
        EndTest();
        break;
    }

    return result;
  }

 protected:
  scoped_refptr<DelegatedRendererLayer> delegated_copy_;
  bool first_draw_for_source_frame_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestLayerUsesFrameDamage);

class LayerTreeHostDelegatedTestMergeResources
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    // Push two frames to the delegated renderer layer with no commit between.

    // The first frame has resource 999.
    scoped_ptr<DelegatedFrameData> frame1 =
        CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
    AddTextureQuad(frame1.get(), 999);
    AddTransferableResource(frame1.get(), 999);
    SetFrameData(frame1.Pass());

    // The second frame uses resource 999 still, but also adds 555.
    scoped_ptr<DelegatedFrameData> frame2 =
        CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
    AddTextureQuad(frame2.get(), 999);
    AddTransferableResource(frame2.get(), 999);
    AddTextureQuad(frame2.get(), 555);
    AddTransferableResource(frame2.get(), 555);
    SetFrameData(frame2.Pass());

    // The resource 999 from frame1 is returned since it is still on the main
    // thread.
    ReturnedResourceArray returned_resources;
    resource_collection_->TakeUnusedResourcesForChildCompositor(
        &returned_resources);
    {
      unsigned expected[] = {999};
      EXPECT_RESOURCES(expected, returned_resources);
      EXPECT_TRUE(TestAndResetAvailable());
    }

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // Both frames' resources should be in the parent's resource provider.
    EXPECT_EQ(2u, map.size());
    EXPECT_EQ(1u, map.count(999));
    EXPECT_EQ(1u, map.count(555));

    EXPECT_EQ(2u, delegated_impl->Resources().size());
    EXPECT_EQ(1u, delegated_impl->Resources().count(999));
    EXPECT_EQ(1u, delegated_impl->Resources().count(555));

    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestMergeResources);

class LayerTreeHostDelegatedTestRemapResourcesInQuads
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    // Generate a frame with two resources in it.
    scoped_ptr<DelegatedFrameData> frame =
        CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
    AddTextureQuad(frame.get(), 999);
    AddTransferableResource(frame.get(), 999);
    AddTextureQuad(frame.get(), 555);
    AddTransferableResource(frame.get(), 555);
    SetFrameData(frame.Pass());

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // The frame's resource should be in the parent's resource provider.
    EXPECT_EQ(2u, map.size());
    EXPECT_EQ(1u, map.count(999));
    EXPECT_EQ(1u, map.count(555));

    ResourceProvider::ResourceId parent_resource_id1 = map.find(999)->second;
    EXPECT_NE(parent_resource_id1, 999u);
    ResourceProvider::ResourceId parent_resource_id2 = map.find(555)->second;
    EXPECT_NE(parent_resource_id2, 555u);

    // The resources in the quads should be remapped to the parent's namespace.
    const TextureDrawQuad* quad1 = TextureDrawQuad::MaterialCast(
        delegated_impl->RenderPassesInDrawOrder()[0]->quad_list[0]);
    EXPECT_EQ(parent_resource_id1, quad1->resource_id);
    const TextureDrawQuad* quad2 = TextureDrawQuad::MaterialCast(
        delegated_impl->RenderPassesInDrawOrder()[0]->quad_list[1]);
    EXPECT_EQ(parent_resource_id2, quad2->resource_id);

    EndTest();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestRemapResourcesInQuads);

class LayerTreeHostDelegatedTestReturnUnusedResources
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // Generate a frame with two resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        // All of the resources are in use.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Keep using 999 but stop using 555.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
      case 3:
        // 555 is no longer in use.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        // Stop using any resources.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());
        break;
      case 4:
        // Postpone collecting resources for a frame. They should still be there
        // the next frame.
        layer_tree_host()->SetNeedsCommit();
        return;
      case 5:
        // 444 and 999 are no longer in use. We sent two refs to 999, so we
        // should get two back.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {444, 999, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }

    // Resources are never immediately released.
    ReturnedResourceArray empty_resources;
    resource_collection_->TakeUnusedResourcesForChildCompositor(
        &empty_resources);
    EXPECT_EQ(0u, empty_resources.size());
    EXPECT_FALSE(TestAndResetAvailable());
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestReturnUnusedResources);

class LayerTreeHostDelegatedTestReusedResources
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // Generate a frame with some resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
      case 2:
        // All of the resources are in use.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Keep using 999 but stop using 555 and 444.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        SetFrameData(frame.Pass());

        // Resource are not immediately released.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Now using 555 and 444 again, but not 999.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
      case 3:
        // The 999 resource is the only unused one. Two references were sent, so
        // two should be returned.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {999, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestReusedResources);

class LayerTreeHostDelegatedTestFrameBeforeAck
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // Generate a frame with some resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
      case 2:
        // All of the resources are in use.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Keep using 999 but stop using 555 and 444.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        SetFrameData(frame.Pass());

        // Resource are not immediately released.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // The parent compositor (this one) does a commit.
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {444, 555};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        // The child compositor sends a frame referring to resources not in the
        // frame.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() != 3)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // The bad frame should be dropped. So we should only have one quad (the
    // one with resource 999) on the impl tree. And only 999 will be present
    // in the parent's resource provider.
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(1u, map.count(999));

    EXPECT_EQ(1u, delegated_impl->Resources().size());
    EXPECT_EQ(1u, delegated_impl->Resources().count(999));

    const RenderPass* pass = delegated_impl->RenderPassesInDrawOrder()[0];
    EXPECT_EQ(1u, pass->quad_list.size());
    const TextureDrawQuad* quad = TextureDrawQuad::MaterialCast(
        pass->quad_list[0]);
    EXPECT_EQ(map.find(999)->second, quad->resource_id);

    EndTest();
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestFrameBeforeAck);

class LayerTreeHostDelegatedTestFrameBeforeTakeResources
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // Generate a frame with some resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
      case 2:
        // All of the resources are in use.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Keep using 999 but stop using 555 and 444.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        SetFrameData(frame.Pass());

        // Resource are not immediately released.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // The parent compositor (this one) does a commit.
        break;
      case 3:
        // The child compositor sends a frame before taking resources back
        // from the previous commit. This frame makes use of the resources 555
        // and 444, which were just released during commit.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());

        // The resources are used by the new frame but are returned anyway since
        // we passed them again.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {444, 555};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        break;
      case 4:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());
        EndTest();
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() != 3)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // The third frame has all of the resources in it again, the delegated
    // renderer layer should continue to own the resources for it.
    EXPECT_EQ(3u, map.size());
    EXPECT_EQ(1u, map.count(999));
    EXPECT_EQ(1u, map.count(555));
    EXPECT_EQ(1u, map.count(444));

    EXPECT_EQ(3u, delegated_impl->Resources().size());
    EXPECT_EQ(1u, delegated_impl->Resources().count(999));
    EXPECT_EQ(1u, delegated_impl->Resources().count(555));
    EXPECT_EQ(1u, delegated_impl->Resources().count(444));

    const RenderPass* pass = delegated_impl->RenderPassesInDrawOrder()[0];
    EXPECT_EQ(3u, pass->quad_list.size());
    const TextureDrawQuad* quad1 = TextureDrawQuad::MaterialCast(
        pass->quad_list[0]);
    EXPECT_EQ(map.find(999)->second, quad1->resource_id);
    const TextureDrawQuad* quad2 = TextureDrawQuad::MaterialCast(
        pass->quad_list[1]);
    EXPECT_EQ(map.find(555)->second, quad2->resource_id);
    const TextureDrawQuad* quad3 = TextureDrawQuad::MaterialCast(
        pass->quad_list[2]);
    EXPECT_EQ(map.find(444)->second, quad3->resource_id);
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestFrameBeforeTakeResources);

class LayerTreeHostDelegatedTestBadFrame
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // Generate a frame with some resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        // All of the resources are in use.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Generate a bad frame with a resource the layer doesn't have. The
        // 885 and 775 resources are unknown, while ownership of the legit 444
        // resource is passed in here. The bad frame does not use any of the
        // previous resources, 999 or 555.
        // A bad quad is present both before and after the good quad.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 885);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        AddTextureQuad(frame.get(), 775);
        SetFrameData(frame.Pass());

        // The parent compositor (this one) does a commit.
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Now send a good frame with 999 again.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        SetFrameData(frame.Pass());

        // The bad frame's resource is given back to the child compositor.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {444};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        break;
      case 4:
        // The unused 555 from the last good frame is now released.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() < 1)
      return;

    ReturnUnusedResourcesFromParent(host_impl);

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    switch (host_impl->active_tree()->source_frame_number()) {
      case 1: {
        // We have the first good frame with just 990 and 555 in it.
        // layer.
        EXPECT_EQ(2u, map.size());
        EXPECT_EQ(1u, map.count(999));
        EXPECT_EQ(1u, map.count(555));

        EXPECT_EQ(2u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(999));
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));

        const RenderPass* pass = delegated_impl->RenderPassesInDrawOrder()[0];
        EXPECT_EQ(2u, pass->quad_list.size());
        const TextureDrawQuad* quad1 = TextureDrawQuad::MaterialCast(
            pass->quad_list[0]);
        EXPECT_EQ(map.find(999)->second, quad1->resource_id);
        const TextureDrawQuad* quad2 = TextureDrawQuad::MaterialCast(
            pass->quad_list[1]);
        EXPECT_EQ(map.find(555)->second, quad2->resource_id);
        break;
      }
      case 2: {
        // We only keep resources from the last valid frame.
        EXPECT_EQ(2u, map.size());
        EXPECT_EQ(1u, map.count(999));
        EXPECT_EQ(1u, map.count(555));

        EXPECT_EQ(2u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(999));
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));

        // The bad frame is dropped though, we still have the frame with 999 and
        // 555 in it.
        const RenderPass* pass = delegated_impl->RenderPassesInDrawOrder()[0];
        EXPECT_EQ(2u, pass->quad_list.size());
        const TextureDrawQuad* quad1 = TextureDrawQuad::MaterialCast(
            pass->quad_list[0]);
        EXPECT_EQ(map.find(999)->second, quad1->resource_id);
        const TextureDrawQuad* quad2 = TextureDrawQuad::MaterialCast(
            pass->quad_list[1]);
        EXPECT_EQ(map.find(555)->second, quad2->resource_id);
        break;
      }
      case 3: {
        // We have the new good frame with just 999 in it.
        EXPECT_EQ(1u, map.size());
        EXPECT_EQ(1u, map.count(999));

        EXPECT_EQ(1u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(999));

        const RenderPass* pass = delegated_impl->RenderPassesInDrawOrder()[0];
        EXPECT_EQ(1u, pass->quad_list.size());
        const TextureDrawQuad* quad1 = TextureDrawQuad::MaterialCast(
            pass->quad_list[0]);
        EXPECT_EQ(map.find(999)->second, quad1->resource_id);
        break;
      }
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestBadFrame);

class LayerTreeHostDelegatedTestUnnamedResource
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // This frame includes two resources in it, but only uses one.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Now send an empty frame.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());

        // The unused resource should be returned.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        EndTest();
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() != 1)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // The layer only held on to the resource that was used.
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(1u, map.count(555));

    EXPECT_EQ(1u, delegated_impl->Resources().size());
    EXPECT_EQ(1u, delegated_impl->Resources().count(555));
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestUnnamedResource);

class LayerTreeHostDelegatedTestDontLeakResource
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // This frame includes two resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());

        // But then we immediately stop using 999.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        // The unused resources should be returned. 555 is still used, but it's
        // returned once to account for the first frame.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        // Send a frame with no resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());
        break;
      case 3:
        // The now unused resource 555 should be returned.
        resources.clear();
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() != 1)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // The layer only held on to the resource that was used.
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(1u, map.count(555));

    EXPECT_EQ(1u, delegated_impl->Resources().size());
    EXPECT_EQ(1u, delegated_impl->Resources().count(555));
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestDontLeakResource);

class LayerTreeHostDelegatedTestResourceSentToParent
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        // This frame includes two resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // 999 is in use in the grandparent compositor, generate a frame without
        // it present.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 3:
        // Since 999 is in the grandparent it is not returned.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // The impl side will get back the resource at some point.
        ImplThreadTaskRunner()->PostTask(FROM_HERE,
                                         receive_resource_on_thread_);
        break;
    }
  }

  void ReceiveResourceOnThread(LayerTreeHostImpl* host_impl) {
    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    // Receive 999 back from the grandparent.
    CompositorFrameAck ack;
    output_surface()->ReturnResource(map.find(999)->second, &ack);
    host_impl->ReclaimResources(&ack);
    host_impl->OnSwapBuffersComplete();
  }

  virtual void UnusedResourcesAreAvailable() OVERRIDE {
    EXPECT_EQ(3, layer_tree_host()->source_frame_number());

    ReturnedResourceArray resources;

    // 999 was returned from the grandparent and could be released.
    resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
    {
      unsigned expected[] = {999};
      EXPECT_RESOURCES(expected, resources);
    }

    EndTest();
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() < 1)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    switch (host_impl->active_tree()->source_frame_number()) {
      case 1: {
        EXPECT_EQ(2u, map.size());
        EXPECT_EQ(1u, map.count(999));
        EXPECT_EQ(1u, map.count(555));

        EXPECT_EQ(2u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(999));
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));

        // The 999 resource will be sent to a grandparent compositor.
        break;
      }
      case 2: {
        EXPECT_EQ(2u, map.size());
        EXPECT_EQ(1u, map.count(999));
        EXPECT_EQ(1u, map.count(555));

        // 999 is in the parent, so not held by delegated renderer layer.
        EXPECT_EQ(1u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));

        receive_resource_on_thread_ =
            base::Bind(&LayerTreeHostDelegatedTestResourceSentToParent::
                            ReceiveResourceOnThread,
                       base::Unretained(this),
                       host_impl);
        break;
      }
      case 3:
        // 999 should be released.
        EXPECT_EQ(1u, map.size());
        EXPECT_EQ(1u, map.count(555));

        EXPECT_EQ(1u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(map.find(555)->second));
        break;
    }
  }

  base::Closure receive_resource_on_thread_;
};

SINGLE_AND_MULTI_THREAD_DELEGATING_RENDERER_TEST_F(
    LayerTreeHostDelegatedTestResourceSentToParent);

class LayerTreeHostDelegatedTestCommitWithoutTake
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE {
    // Prevent drawing with resources that are sent to the grandparent.
    layer_tree_host()->SetViewportSize(gfx::Size());
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        AddTextureQuad(frame.get(), 444);
        AddTransferableResource(frame.get(), 444);
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Stop using 999 and 444 in this frame and commit.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        // 999 and 444 will be returned for frame 1, but not 555 since it's in
        // the current frame.
        break;
      case 3:
        // Don't take resources here, but set a new frame that uses 999 again.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 4:
        // 555 from frame 1 and 2 isn't returned since it's still in use. 999
        // from frame 1 is returned though.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {444, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());
        // 555 will be returned 3 times for frames 1 2 and 3, and 999 will be
        // returned once for frame 3.
        break;
      case 5:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 555, 555, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        EndTest();
        break;
    }
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    if (host_impl->active_tree()->source_frame_number() < 1)
      return;

    LayerImpl* root_impl = host_impl->active_tree()->root_layer();
    FakeDelegatedRendererLayerImpl* delegated_impl =
        static_cast<FakeDelegatedRendererLayerImpl*>(root_impl->children()[0]);

    const ResourceProvider::ResourceIdMap& map =
        host_impl->resource_provider()->GetChildToParentMap(
            delegated_impl->ChildId());

    switch (host_impl->active_tree()->source_frame_number()) {
      case 1:
        EXPECT_EQ(3u, map.size());
        EXPECT_EQ(1u, map.count(999));
        EXPECT_EQ(1u, map.count(555));
        EXPECT_EQ(1u, map.count(444));

        EXPECT_EQ(3u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(999));
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));
        EXPECT_EQ(1u, delegated_impl->Resources().count(444));
        break;
      case 2:
        EXPECT_EQ(1u, map.size());
        EXPECT_EQ(1u, map.count(555));

        EXPECT_EQ(1u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));
        break;
      case 3:
        EXPECT_EQ(2u, map.size());
        EXPECT_EQ(1u, map.count(999));
        EXPECT_EQ(1u, map.count(555));

        EXPECT_EQ(2u, delegated_impl->Resources().size());
        EXPECT_EQ(1u, delegated_impl->Resources().count(999));
        EXPECT_EQ(1u, delegated_impl->Resources().count(555));
    }
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestCommitWithoutTake);

class DelegatedFrameIsActivatedDuringCommit
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 protected:
  DelegatedFrameIsActivatedDuringCommit()
      : wait_thread_("WAIT"),
        wait_event_(false, false),
        returned_resource_count_(0) {
    wait_thread_.Start();
  }

  virtual void BeginTest() OVERRIDE {
    activate_count_ = 0;

    scoped_ptr<DelegatedFrameData> frame =
        CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
    AddTextureQuad(frame.get(), 999);
    AddTransferableResource(frame.get(), 999);
    SetFrameData(frame.Pass());

    PostSetNeedsCommitToMainThread();
  }

  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Slow down activation so the main thread DidCommit() will run if
    // not blocked.
    wait_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal,
                   base::Unretained(&wait_event_)),
        base::TimeDelta::FromMilliseconds(10));
    wait_event_.Wait();

    base::AutoLock lock(activate_lock_);
    ++activate_count_;
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // The main thread is awake now, and will run DidCommit() immediately.
    // Run DidActivate() afterwards by posting it now.
    proxy()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&DelegatedFrameIsActivatedDuringCommit::DidActivate,
                   base::Unretained(this)));
  }

  void DidActivate() {
    base::AutoLock lock(activate_lock_);
    switch (activate_count_) {
      case 1: {
        // The first frame has been activated. Set a new frame, and
        // expect the next commit to finish *after* it is activated.
        scoped_ptr<DelegatedFrameData> frame =
            CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        // So this commit number should complete after the second activate.
        EXPECT_EQ(1, layer_tree_host()->source_frame_number());
        break;
      }
      case 2:
        // The second frame has been activated. Remove the layer from
        // the tree to cause another commit/activation. The commit should
        // finish *after* the layer is removed from the active tree.
        delegated_->RemoveFromParent();
        // So this commit number should complete after the third activate.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
        break;
    }
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 2: {
        // The activate for the 2nd frame should have happened before now.
        base::AutoLock lock(activate_lock_);
        EXPECT_EQ(2, activate_count_);
        break;
      }
      case 3: {
        // The activate to remove the layer should have happened before now.
        base::AutoLock lock(activate_lock_);
        EXPECT_EQ(3, activate_count_);

        scoped_ptr<DelegatedFrameData> frame =
            CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());
        break;
      }
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }

  virtual void UnusedResourcesAreAvailable() OVERRIDE {
    LayerTreeHostDelegatedTestCaseSingleDelegatedLayer::
        UnusedResourcesAreAvailable();
    ReturnedResourceArray resources;
    resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
    EXPECT_TRUE(TestAndResetAvailable());
    returned_resource_count_ += resources.size();
    if (returned_resource_count_ == 2)
      EndTest();
  }

  base::Thread wait_thread_;
  base::WaitableEvent wait_event_;
  base::Lock activate_lock_;
  int activate_count_;
  size_t returned_resource_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    DelegatedFrameIsActivatedDuringCommit);

class LayerTreeHostDelegatedTestTwoImplLayers
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Remove the delegated layer and replace it with a new one. Use the
        // same frame and resources for it.
        delegated_->RemoveFromParent();
        delegated_ = CreateDelegatedLayer(frame_provider_.get());
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Use a frame with no resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());
        break;
      case 4:
        // We gave one frame to the frame provider, so we should get one
        // ref back for each resource.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestTwoImplLayers);

class LayerTreeHostDelegatedTestTwoImplLayersTwoFrames
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);

        // Remove the delegated layer and replace it with a new one. Make a new
        // frame but with the same resources for it.
        delegated_->RemoveFromParent();
        delegated_ = NULL;

        frame_provider_->SetFrameData(frame.Pass());
        delegated_ = CreateDelegatedLayer(frame_provider_.get());
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Use a frame with no resources in it.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        SetFrameData(frame.Pass());
        break;
      case 4:
        // We gave two frames to the frame provider, so we should get two
        // refs back for each resource.
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 555, 999, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestTwoImplLayersTwoFrames);

class LayerTreeHostDelegatedTestTwoLayers
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);

        // Create a DelegatedRendererLayer using the frame.
        SetFrameData(frame.Pass());
        break;
      case 2:
        // Create a second DelegatedRendererLayer using the same frame provider.
        delegated_thief_ = CreateDelegatedLayer(frame_provider_.get());
        root_->AddChild(delegated_thief_);

        // And drop our ref on the frame provider so only the layers keep it
        // alive.
        frame_provider_ = NULL;
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Remove one delegated layer from the tree. No resources should be
        // returned yet.
        delegated_->RemoveFromParent();
        break;
      case 4:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Put the first layer back, and remove the other layer and destroy it.
        // No resources should be returned yet.
        root_->AddChild(delegated_);
        delegated_thief_->RemoveFromParent();
        delegated_thief_ = NULL;
        break;
      case 5:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Remove the first layer from the tree again. The resources are still
        // held by the main thread layer.
        delegated_->RemoveFromParent();
        break;
      case 6:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Destroy the layer and the resources should be returned immediately.
        delegated_ = NULL;

        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }

  scoped_refptr<DelegatedRendererLayer> delegated_thief_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestTwoLayers);

class LayerTreeHostDelegatedTestRemoveAndAddToTree
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);

        // Create a DelegatedRendererLayer using the frame.
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Remove the layer from the tree. The resources should not be returned
        // since they are still on the main thread layer.
        delegated_->RemoveFromParent();
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Add the layer back to the tree.
        layer_tree_host()->root_layer()->AddChild(delegated_);
        break;
      case 4:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Set a new frame. Resources should be returned.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 888);
        AddTransferableResource(frame.get(), 888);
        AddTextureQuad(frame.get(), 777);
        AddTransferableResource(frame.get(), 777);
        SetFrameData(frame.Pass());
        break;
      case 5:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }

        // Destroy the layer.
        delegated_->RemoveFromParent();
        delegated_ = NULL;
        break;
      case 6:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Destroy the frame provider. Resources should be returned.
        frame_provider_ = NULL;

        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {777, 888};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }

  scoped_refptr<DelegatedRendererLayer> delegated_thief_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostDelegatedTestRemoveAndAddToTree);

class LayerTreeHostDelegatedTestRemoveAndChangeResources
    : public LayerTreeHostDelegatedTestCaseSingleDelegatedLayer {
 public:
  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    scoped_ptr<DelegatedFrameData> frame;
    ReturnedResourceArray resources;

    int next_source_frame_number = layer_tree_host()->source_frame_number();
    switch (next_source_frame_number) {
      case 1:
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 999);
        AddTransferableResource(frame.get(), 999);
        AddTextureQuad(frame.get(), 555);
        AddTransferableResource(frame.get(), 555);

        // Create a DelegatedRendererLayer using the frame.
        SetFrameData(frame.Pass());
        break;
      case 2:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Remove the layer from the tree. The resources should not be returned
        // since they are still on the main thread layer.
        delegated_->RemoveFromParent();
        break;
      case 3:
        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Set a new frame. Resources should be returned immediately.
        frame = CreateFrameData(gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1));
        AddTextureQuad(frame.get(), 888);
        AddTransferableResource(frame.get(), 888);
        AddTextureQuad(frame.get(), 777);
        AddTransferableResource(frame.get(), 777);
        SetFrameData(frame.Pass());

        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {555, 999};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
          resources.clear();
        }

        // Destroy the frame provider.
        frame_provider_ = NULL;

        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        EXPECT_EQ(0u, resources.size());
        EXPECT_FALSE(TestAndResetAvailable());

        // Destroy the layer. Resources should be returned.
        delegated_ = NULL;

        resource_collection_->TakeUnusedResourcesForChildCompositor(&resources);
        {
          unsigned expected[] = {777, 888};
          EXPECT_RESOURCES(expected, resources);
          EXPECT_TRUE(TestAndResetAvailable());
        }
        EndTest();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ReturnUnusedResourcesFromParent(host_impl);
  }

  scoped_refptr<DelegatedRendererLayer> delegated_thief_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostDelegatedTestRemoveAndChangeResources);

}  // namespace
}  // namespace cc
