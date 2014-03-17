// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/quad_culler.h"

#include <vector>

#include "cc/base/math_util.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestOcclusionTrackerImpl
    : public TestOcclusionTrackerBase<LayerImpl, RenderSurfaceImpl> {
 public:
  TestOcclusionTrackerImpl(gfx::Rect scissor_rect_in_screen,
                           bool record_metrics_for_frame = true)
      : TestOcclusionTrackerBase(scissor_rect_in_screen,
                                 record_metrics_for_frame) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOcclusionTrackerImpl);
};

typedef LayerIterator<LayerImpl,
                      LayerImplList,
                      RenderSurfaceImpl,
                      LayerIteratorActions::FrontToBack> LayerIteratorType;

class QuadCullerTest : public testing::Test {
 public:
  QuadCullerTest()
      : host_impl_(&proxy_),
        layer_id_(1) {}

  scoped_ptr<TiledLayerImpl> MakeLayer(TiledLayerImpl* parent,
                                       const gfx::Transform& draw_transform,
                                       gfx::Rect layer_rect,
                                       float opacity,
                                       bool opaque,
                                       gfx::Rect layer_opaque_rect,
                                       LayerImplList& surface_layer_list) {
    scoped_ptr<TiledLayerImpl> layer =
        TiledLayerImpl::Create(host_impl_.active_tree(), layer_id_++);
    scoped_ptr<LayerTilingData> tiler = LayerTilingData::Create(
        gfx::Size(100, 100), LayerTilingData::NO_BORDER_TEXELS);
    tiler->SetBounds(layer_rect.size());
    layer->SetTilingData(*tiler);
    layer->set_skips_draw(false);
    layer->SetDrawsContent(true);
    layer->draw_properties().target_space_transform = draw_transform;
    layer->draw_properties().screen_space_transform = draw_transform;
    layer->draw_properties().visible_content_rect = layer_rect;
    layer->draw_properties().opacity = opacity;
    layer->SetContentsOpaque(opaque);
    layer->SetBounds(layer_rect.size());
    layer->SetContentBounds(layer_rect.size());

    ResourceProvider::ResourceId resource_id = 1;
    for (int i = 0; i < tiler->num_tiles_x(); ++i) {
      for (int j = 0; j < tiler->num_tiles_y(); ++j) {
        gfx::Rect tile_opaque_rect =
            opaque
            ? tiler->tile_bounds(i, j)
            : gfx::IntersectRects(tiler->tile_bounds(i, j), layer_opaque_rect);
        layer->PushTileProperties(i, j, resource_id++, tile_opaque_rect, false);
      }
    }

    gfx::Rect rect_in_target = MathUtil::MapClippedRect(
        layer->draw_transform(), layer->visible_content_rect());
    if (!parent) {
      layer->CreateRenderSurface();
      layer->render_surface()->SetContentRect(rect_in_target);
      surface_layer_list.push_back(layer.get());
      layer->render_surface()->layer_list().push_back(layer.get());
    } else {
      layer->draw_properties().render_target = parent->render_target();
      parent->render_surface()->layer_list().push_back(layer.get());
      rect_in_target.Union(MathUtil::MapClippedRect(
          parent->draw_transform(), parent->visible_content_rect()));
      parent->render_surface()->SetContentRect(rect_in_target);
    }
    layer->draw_properties().drawable_content_rect = rect_in_target;

    return layer.Pass();
  }

  void AppendQuads(QuadList* quad_list,
                   SharedQuadStateList* shared_state_list,
                   TiledLayerImpl* layer,
                   LayerIteratorType* it,
                   OcclusionTrackerImpl* occlusion_tracker) {
    occlusion_tracker->EnterLayer(*it);
    QuadCuller quad_culler(
        quad_list, shared_state_list, layer, *occlusion_tracker, false, false);
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    occlusion_tracker->LeaveLayer(*it);
    ++it;
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  int layer_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuadCullerTest);
};

#define DECLARE_AND_INITIALIZE_TEST_QUADS()                                    \
  QuadList quad_list;                                                          \
  SharedQuadStateList shared_state_list;                                       \
  LayerImplList render_surface_layer_list;                                     \
  gfx::Transform child_transform;                                              \
  gfx::Size root_size = gfx::Size(300, 300);                                   \
  gfx::Rect root_rect = gfx::Rect(root_size);                                  \
  gfx::Size child_size = gfx::Size(200, 200);                                  \
  gfx::Rect child_rect = gfx::Rect(child_size);

TEST_F(QuadCullerTest, NoCulling) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1.f,
                                                     false,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(13u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              40000,
              1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, CullChildLinesUpTopLeft) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(9u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              40000,
              1);
}

TEST_F(QuadCullerTest, CullWhenChildOpacityNotOne) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     0.9f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(13u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              40000,
              1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, CullWhenChildOpaqueFlagFalse) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     false,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(13u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              40000,
              1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, CullCenterTileOnly) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 50);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  ASSERT_EQ(quad_list.size(), 12u);

  gfx::Rect quad_visible_rect1 = quad_list[5]->visible_rect;
  EXPECT_EQ(50, quad_visible_rect1.height());

  gfx::Rect quad_visible_rect3 = quad_list[7]->visible_rect;
  EXPECT_EQ(50, quad_visible_rect3.width());

  // Next index is 8, not 9, since centre quad culled.
  gfx::Rect quad_visible_rect4 = quad_list[8]->visible_rect;
  EXPECT_EQ(50, quad_visible_rect4.width());
  EXPECT_EQ(250, quad_visible_rect4.x());

  gfx::Rect quad_visible_rect6 = quad_list[10]->visible_rect;
  EXPECT_EQ(50, quad_visible_rect6.height());
  EXPECT_EQ(250, quad_visible_rect6.y());

  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 100000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              30000,
              1);
}

TEST_F(QuadCullerTest, CullCenterTileNonIntegralSize1) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(100, 100);

  // Make the root layer's quad have extent (99.1, 99.1) -> (200.9, 200.9) to
  // make sure it doesn't get culled due to transform rounding.
  gfx::Transform root_transform;
  root_transform.Translate(99.1f, 99.1f);
  root_transform.Scale(1.018f, 1.018f);

  root_rect = child_rect = gfx::Rect(0, 0, 100, 100);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    root_transform,
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(2u, quad_list.size());

  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 20363, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, CullCenterTileNonIntegralSize2) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  // Make the child's quad slightly smaller than, and centred over, the root
  // layer tile.  Verify the child does not cause the quad below to be culled
  // due to rounding.
  child_transform.Translate(100.1f, 100.1f);
  child_transform.Scale(0.982f, 0.982f);

  gfx::Transform root_transform;
  root_transform.Translate(100, 100);

  root_rect = child_rect = gfx::Rect(0, 0, 100, 100);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    root_transform,
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(2u, quad_list.size());

  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 19643, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

TEST_F(QuadCullerTest, CullChildLinesUpBottomRight) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(100, 100);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(9u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              40000,
              1);
}

TEST_F(QuadCullerTest, CullSubRegion) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 50);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  gfx::Rect child_opaque_rect(child_rect.x() + child_rect.width() / 4,
                              child_rect.y() + child_rect.height() / 4,
                              child_rect.width() / 2,
                              child_rect.height() / 2);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     false,
                                                     child_opaque_rect,
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(12u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              30000,
              1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              10000,
              1);
}

TEST_F(QuadCullerTest, CullSubRegion2) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 10);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  gfx::Rect child_opaque_rect(child_rect.x() + child_rect.width() / 4,
                              child_rect.y() + child_rect.height() / 4,
                              child_rect.width() / 2,
                              child_rect.height() * 3 / 4);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     false,
                                                     child_opaque_rect,
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(12u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              25000,
              1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              15000,
              1);
}

TEST_F(QuadCullerTest, CullSubRegionCheckOvercull) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  child_transform.Translate(50, 49);
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  gfx::Rect child_opaque_rect(child_rect.x() + child_rect.width() / 4,
                              child_rect.y() + child_rect.height() / 4,
                              child_rect.width() / 2,
                              child_rect.height() / 2);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     false,
                                                     child_opaque_rect,
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(13u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 90000, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(),
              30000,
              1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              10000,
              1);
}

TEST_F(QuadCullerTest, NonAxisAlignedQuadsDontOcclude) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  // Use a small rotation so as to not disturb the geometry significantly.
  child_transform.Rotate(1);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     child_transform,
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(13u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 130000, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(), 0, 1);
}

// This test requires some explanation: here we are rotating the quads to be
// culled.  The 2x2 tile child layer remains in the top-left corner, unrotated,
// but the 3x3 tile parent layer is rotated by 1 degree. Of the four tiles the
// child would normally occlude, three will move (slightly) out from under the
// child layer, and one moves further under the child. Only this last tile
// should be culled.
TEST_F(QuadCullerTest, NonAxisAlignedQuadsSafelyCulled) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  // Use a small rotation so as to not disturb the geometry significantly.
  gfx::Transform parent_transform;
  parent_transform.Rotate(1);

  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    parent_transform,
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(12u, quad_list.size());
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque(), 100600, 1);
  EXPECT_NEAR(
      occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent(), 0, 1);
  EXPECT_NEAR(occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing(),
              29400,
              1);
}

TEST_F(QuadCullerTest, WithoutMetrics) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();
  scoped_ptr<TiledLayerImpl> root_layer = MakeLayer(NULL,
                                                    gfx::Transform(),
                                                    root_rect,
                                                    1.f,
                                                    true,
                                                    gfx::Rect(),
                                                    render_surface_layer_list);
  scoped_ptr<TiledLayerImpl> child_layer = MakeLayer(root_layer.get(),
                                                     gfx::Transform(),
                                                     child_rect,
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);
  bool record_metrics = false;
  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(-100, -100, 1000, 1000),
                                             record_metrics);
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  AppendQuads(&quad_list,
              &shared_state_list,
              child_layer.get(),
              &it,
              &occlusion_tracker);
  AppendQuads(&quad_list,
              &shared_state_list,
              root_layer.get(),
              &it,
              &occlusion_tracker);
  EXPECT_EQ(9u, quad_list.size());
  EXPECT_EQ(0.f,
            occlusion_tracker.overdraw_metrics()->pixels_drawn_opaque());
  EXPECT_EQ(0.f,
            occlusion_tracker.overdraw_metrics()->pixels_drawn_translucent());
  EXPECT_EQ(0.f,
            occlusion_tracker.overdraw_metrics()->pixels_culled_for_drawing());
}

TEST_F(QuadCullerTest, PartialCullingNotDestroyed) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> dummy_layer = MakeLayer(NULL,
                                                     gfx::Transform(),
                                                     gfx::Rect(),
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);

  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  QuadCuller culler(&quad_list,
                    &shared_state_list,
                    dummy_layer.get(),
                    occlusion_tracker,
                    false,
                    false);

  SharedQuadState* sqs = culler.UseSharedQuadState(SharedQuadState::Create());

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(sqs, gfx::Rect(100, 100), SK_ColorRED, false);

  scoped_ptr<RenderPassDrawQuad> pass_quad = RenderPassDrawQuad::Create();
  pass_quad->SetNew(sqs,
                    gfx::Rect(100, 100),
                    RenderPass::Id(10, 10),
                    false,
                    0,
                    gfx::Rect(),
                    gfx::RectF(),
                    FilterOperations(),
                    FilterOperations());

  scoped_ptr<RenderPassDrawQuad> replica_quad = RenderPassDrawQuad::Create();
  replica_quad->SetNew(sqs,
                       gfx::Rect(100, 100),
                       RenderPass::Id(10, 10),
                       true,
                       0,
                       gfx::Rect(),
                       gfx::RectF(),
                       FilterOperations(),
                       FilterOperations());

  // Set a visible rect on the quads.
  color_quad->visible_rect = gfx::Rect(20, 30, 10, 11);
  pass_quad->visible_rect = gfx::Rect(50, 60, 13, 14);
  replica_quad->visible_rect = gfx::Rect(30, 40, 15, 16);

  // Nothing is occluding.
  occlusion_tracker.EnterLayer(it);

  EXPECT_EQ(0u, quad_list.size());

  AppendQuadsData data;
  culler.Append(color_quad.PassAs<DrawQuad>(), &data);
  culler.Append(pass_quad.PassAs<DrawQuad>(), &data);
  culler.Append(replica_quad.PassAs<DrawQuad>(), &data);

  ASSERT_EQ(3u, quad_list.size());

  // The partial culling is preserved.
  EXPECT_EQ(gfx::Rect(20, 30, 10, 11).ToString(),
            quad_list[0]->visible_rect.ToString());
  EXPECT_EQ(gfx::Rect(50, 60, 13, 14).ToString(),
            quad_list[1]->visible_rect.ToString());
  EXPECT_EQ(gfx::Rect(30, 40, 15, 16).ToString(),
            quad_list[2]->visible_rect.ToString());
}

TEST_F(QuadCullerTest, PartialCullingWithOcclusionNotDestroyed) {
  DECLARE_AND_INITIALIZE_TEST_QUADS();

  scoped_ptr<TiledLayerImpl> dummy_layer = MakeLayer(NULL,
                                                     gfx::Transform(),
                                                     gfx::Rect(),
                                                     1.f,
                                                     true,
                                                     gfx::Rect(),
                                                     render_surface_layer_list);

  TestOcclusionTrackerImpl occlusion_tracker(gfx::Rect(1000, 1000));
  LayerIteratorType it = LayerIteratorType::Begin(&render_surface_layer_list);

  QuadCuller culler(&quad_list,
                    &shared_state_list,
                    dummy_layer.get(),
                    occlusion_tracker,
                    false,
                    false);

  SharedQuadState* sqs = culler.UseSharedQuadState(SharedQuadState::Create());

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(sqs, gfx::Rect(100, 100), SK_ColorRED, false);

  scoped_ptr<RenderPassDrawQuad> pass_quad = RenderPassDrawQuad::Create();
  pass_quad->SetNew(sqs,
                    gfx::Rect(100, 100),
                    RenderPass::Id(10, 10),
                    false,
                    0,
                    gfx::Rect(),
                    gfx::RectF(),
                    FilterOperations(),
                    FilterOperations());

  scoped_ptr<RenderPassDrawQuad> replica_quad = RenderPassDrawQuad::Create();
  replica_quad->SetNew(sqs,
                       gfx::Rect(100, 100),
                       RenderPass::Id(10, 10),
                       true,
                       0,
                       gfx::Rect(),
                       gfx::RectF(),
                       FilterOperations(),
                       FilterOperations());

  // Set a visible rect on the quads.
  color_quad->visible_rect = gfx::Rect(10, 10, 10, 11);
  pass_quad->visible_rect = gfx::Rect(10, 20, 13, 14);
  replica_quad->visible_rect = gfx::Rect(10, 30, 15, 16);

  // Occlude the left part of the visible rects.
  occlusion_tracker.EnterLayer(it);
  occlusion_tracker.set_occlusion_from_outside_target(gfx::Rect(0, 0, 15, 100));

  EXPECT_EQ(0u, quad_list.size());

  AppendQuadsData data;
  culler.Append(color_quad.PassAs<DrawQuad>(), &data);
  culler.Append(pass_quad.PassAs<DrawQuad>(), &data);
  culler.Append(replica_quad.PassAs<DrawQuad>(), &data);

  ASSERT_EQ(3u, quad_list.size());

  // The partial culling is preserved, while the left side of the quads is newly
  // occluded.
  EXPECT_EQ(gfx::Rect(15, 10, 5, 11).ToString(),
            quad_list[0]->visible_rect.ToString());
  EXPECT_EQ(gfx::Rect(15, 20, 8, 14).ToString(),
            quad_list[1]->visible_rect.ToString());
  EXPECT_EQ(gfx::Rect(15, 30, 10, 16).ToString(),
            quad_list[2]->visible_rect.ToString());
}

}  // namespace
}  // namespace cc
