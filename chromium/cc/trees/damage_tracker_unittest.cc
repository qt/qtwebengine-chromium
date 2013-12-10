// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/damage_tracker.h"

#include "cc/base/math_util.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/quad_f.h"

namespace cc {
namespace {

void ExecuteCalculateDrawProperties(LayerImpl* root,
                                    LayerImplList& render_surface_layer_list) {
  // Sanity check: The test itself should create the root layer's render
  //               surface, so that the surface (and its damage tracker) can
  //               persist across multiple calls to this function.
  ASSERT_TRUE(root->render_surface());
  ASSERT_FALSE(render_surface_layer_list.size());

  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, root->bounds(), &render_surface_layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
}

void ClearDamageForAllSurfaces(LayerImpl* layer) {
  if (layer->render_surface())
    layer->render_surface()->damage_tracker()->DidDrawDamagedArea();

  // Recursively clear damage for any existing surface.
  for (size_t i = 0; i < layer->children().size(); ++i)
    ClearDamageForAllSurfaces(layer->children()[i]);
}

void EmulateDrawingOneFrame(LayerImpl* root) {
  // This emulates only steps that are relevant to testing the damage tracker:
  //   1. computing the render passes and layerlists
  //   2. updating all damage trackers in the correct order
  //   3. resetting all update_rects and property_changed flags for all layers
  //      and surfaces.

  LayerImplList render_surface_layer_list;
  ExecuteCalculateDrawProperties(root, render_surface_layer_list);

  // Iterate back-to-front, so that damage correctly propagates from descendant
  // surfaces to ancestors.
  for (int i = render_surface_layer_list.size() - 1; i >= 0; --i) {
    RenderSurfaceImpl* target_surface =
            render_surface_layer_list[i]->render_surface();
    target_surface->damage_tracker()->UpdateDamageTrackingState(
        target_surface->layer_list(),
        target_surface->OwningLayerId(),
        target_surface->SurfacePropertyChangedOnlyFromDescendant(),
        target_surface->content_rect(),
        render_surface_layer_list[i]->mask_layer(),
        render_surface_layer_list[i]->filters(),
        render_surface_layer_list[i]->filter().get());
  }

  root->ResetAllChangeTrackingForSubtree();
}

class DamageTrackerTest : public testing::Test {
 public:
  DamageTrackerTest() : host_impl_(&proxy_) {}

  scoped_ptr<LayerImpl> CreateTestTreeWithOneSurface() {
    scoped_ptr<LayerImpl> root =
            LayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<LayerImpl> child =
            LayerImpl::Create(host_impl_.active_tree(), 2);

    root->SetPosition(gfx::PointF());
    root->SetAnchorPoint(gfx::PointF());
    root->SetBounds(gfx::Size(500, 500));
    root->SetContentBounds(gfx::Size(500, 500));
    root->SetDrawsContent(true);
    root->CreateRenderSurface();
    root->render_surface()->SetContentRect(gfx::Rect(0, 0, 500, 500));

    child->SetPosition(gfx::PointF(100.f, 100.f));
    child->SetAnchorPoint(gfx::PointF());
    child->SetBounds(gfx::Size(30, 30));
    child->SetContentBounds(gfx::Size(30, 30));
    child->SetDrawsContent(true);
    root->AddChild(child.Pass());

    return root.Pass();
  }

  scoped_ptr<LayerImpl> CreateTestTreeWithTwoSurfaces() {
    // This test tree has two render surfaces: one for the root, and one for
    // child1. Additionally, the root has a second child layer, and child1 has
    // two children of its own.

    scoped_ptr<LayerImpl> root =
            LayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<LayerImpl> child1 =
            LayerImpl::Create(host_impl_.active_tree(), 2);
    scoped_ptr<LayerImpl> child2 =
            LayerImpl::Create(host_impl_.active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child1 =
            LayerImpl::Create(host_impl_.active_tree(), 4);
    scoped_ptr<LayerImpl> grand_child2 =
            LayerImpl::Create(host_impl_.active_tree(), 5);

    root->SetPosition(gfx::PointF());
    root->SetAnchorPoint(gfx::PointF());
    root->SetBounds(gfx::Size(500, 500));
    root->SetContentBounds(gfx::Size(500, 500));
    root->SetDrawsContent(true);
    root->CreateRenderSurface();
    root->render_surface()->SetContentRect(gfx::Rect(0, 0, 500, 500));

    child1->SetPosition(gfx::PointF(100.f, 100.f));
    child1->SetAnchorPoint(gfx::PointF());
    child1->SetBounds(gfx::Size(30, 30));
    child1->SetContentBounds(gfx::Size(30, 30));
    // With a child that draws_content, opacity will cause the layer to create
    // its own RenderSurface. This layer does not draw, but is intended to
    // create its own RenderSurface. TODO: setting opacity and
    // ForceRenderSurface may be redundant here.
    child1->SetOpacity(0.5f);
    child1->SetDrawsContent(false);
    child1->SetForceRenderSurface(true);

    child2->SetPosition(gfx::PointF(11.f, 11.f));
    child2->SetAnchorPoint(gfx::PointF());
    child2->SetBounds(gfx::Size(18, 18));
    child2->SetContentBounds(gfx::Size(18, 18));
    child2->SetDrawsContent(true);

    grand_child1->SetPosition(gfx::PointF(200.f, 200.f));
    grand_child1->SetAnchorPoint(gfx::PointF());
    grand_child1->SetBounds(gfx::Size(6, 8));
    grand_child1->SetContentBounds(gfx::Size(6, 8));
    grand_child1->SetDrawsContent(true);

    grand_child2->SetPosition(gfx::PointF(190.f, 190.f));
    grand_child2->SetAnchorPoint(gfx::PointF());
    grand_child2->SetBounds(gfx::Size(6, 8));
    grand_child2->SetContentBounds(gfx::Size(6, 8));
    grand_child2->SetDrawsContent(true);

    child1->AddChild(grand_child1.Pass());
    child1->AddChild(grand_child2.Pass());
    root->AddChild(child1.Pass());
    root->AddChild(child2.Pass());

    return root.Pass();
  }

  scoped_ptr<LayerImpl> CreateAndSetUpTestTreeWithOneSurface() {
    scoped_ptr<LayerImpl> root = CreateTestTreeWithOneSurface();

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    EmulateDrawingOneFrame(root.get());

    return root.Pass();
  }

  scoped_ptr<LayerImpl> CreateAndSetUpTestTreeWithTwoSurfaces() {
    scoped_ptr<LayerImpl> root = CreateTestTreeWithTwoSurfaces();

    // Setup includes going past the first frame which always damages
    // everything, so that we can actually perform specific tests.
    EmulateDrawingOneFrame(root.get());

    return root.Pass();
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(DamageTrackerTest, SanityCheckTestTreeWithOneSurface) {
  // Sanity check that the simple test tree will actually produce the expected
  // render surfaces and layer lists.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();

  EXPECT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(1, root->render_surface()->layer_list()[0]->id());
  EXPECT_EQ(2, root->render_surface()->layer_list()[1]->id());

  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 500.f, 500.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, SanityCheckTestTreeWithTwoSurfaces) {
  // Sanity check that the complex test tree will actually produce the expected
  // render surfaces and layer lists.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();

  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  gfx::RectF child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  ASSERT_TRUE(child1->render_surface());
  EXPECT_FALSE(child2->render_surface());
  EXPECT_EQ(3u, root->render_surface()->layer_list().size());
  EXPECT_EQ(2u, child1->render_surface()->layer_list().size());

  // The render surface for child1 only has a content_rect that encloses
  // grand_child1 and grand_child2, because child1 does not draw content.
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(190.f, 190.f, 16.f, 18.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 500.f, 500.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForUpdateRects) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  // CASE 1: Setting the update rect should cause the corresponding damage to
  //         the surface.
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(10.f, 11.f, 12.f, 13.f));
  EmulateDrawingOneFrame(root.get());

  // Damage position on the surface should be: position of update_rect (10, 11)
  // relative to the child (100, 100).
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(110.f, 111.f, 12.f, 13.f), root_damage_rect);

  // CASE 2: The same update rect twice in a row still produces the same
  //         damage.
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(10.f, 11.f, 12.f, 13.f));
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(110.f, 111.f, 12.f, 13.f), root_damage_rect);

  // CASE 3: Setting a different update rect should cause damage on the new
  //         update region, but no additional exposed old region.
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(20.f, 25.f, 1.f, 2.f));
  EmulateDrawingOneFrame(root.get());

  // Damage position on the surface should be: position of update_rect (20, 25)
  // relative to the child (100, 100).
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(120.f, 125.f, 1.f, 2.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForPropertyChanges) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  // CASE 1: The layer's property changed flag takes priority over update rect.
  //
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(10.f, 11.f, 12.f, 13.f));
  child->SetOpacity(0.5f);
  EmulateDrawingOneFrame(root.get());

  // Sanity check - we should not have accidentally created a separate render
  // surface for the translucent layer.
  ASSERT_FALSE(child->render_surface());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());

  // Damage should be the entire child layer in target_surface space.
  gfx::RectF expected_rect = gfx::RectF(100.f, 100.f, 30.f, 30.f);
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(expected_rect, root_damage_rect);

  // CASE 2: If a layer moves due to property change, it damages both the new
  //         location and the old (exposed) location. The old location is the
  //         entire old layer, not just the update_rect.

  // Cycle one frame of no change, just to sanity check that the next rect is
  // not because of the old damage state.
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then, test the actual layer movement.
  ClearDamageForAllSurfaces(root.get());
  child->SetPosition(gfx::PointF(200.f, 230.f));
  EmulateDrawingOneFrame(root.get());

  // Expect damage to be the combination of the previous one and the new one.
  expected_rect.Union(gfx::RectF(200, 230, 30, 30));
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(expected_rect, root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForTransformedLayer) {
  // If a layer is transformed, the damage rect should still enclose the entire
  // transformed layer.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  gfx::Transform rotation;
  rotation.Rotate(45.0);

  ClearDamageForAllSurfaces(root.get());
  child->SetAnchorPoint(gfx::PointF(0.5f, 0.5f));
  child->SetPosition(gfx::PointF(85.f, 85.f));
  EmulateDrawingOneFrame(root.get());

  // Sanity check that the layer actually moved to (85, 85), damaging its old
  // location and new location.
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(85.f, 85.f, 45.f, 45.f), root_damage_rect);

  // With the anchor on the layer's center, now we can test the rotation more
  // intuitively, since it applies about the layer's anchor.
  ClearDamageForAllSurfaces(root.get());
  child->SetTransform(rotation);
  EmulateDrawingOneFrame(root.get());

  // Since the child layer is square, rotation by 45 degrees about the center
  // should increase the size of the expected rect by sqrt(2), centered around
  // (100, 100). The old exposed region should be fully contained in the new
  // region.
  float expected_width = 30.f * sqrt(2.f);
  float expected_position = 100.f - 0.5f * expected_width;
  gfx::RectF expected_rect(
      expected_position, expected_position, expected_width, expected_width);
  root_damage_rect =
      root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(expected_rect, root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForPerspectiveClippedLayer) {
  // If a layer has a perspective transform that causes w < 0, then not
  // clipping the layer can cause an invalid damage rect. This test checks that
  // the w < 0 case is tracked properly.
  //
  // The transform is constructed so that if w < 0 clipping is not performed,
  // the incorrect rect will be very small, specifically: position (500.972504,
  // 498.544617) and size 0.056610 x 2.910767.  Instead, the correctly
  // transformed rect should actually be very huge (i.e. in theory, -infinity
  // on the left), and positioned so that the right-most bound rect will be
  // approximately 501 units in root surface space.
  //

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  gfx::Transform transform;
  transform.Translate3d(500.0, 500.0, 0.0);
  transform.ApplyPerspectiveDepth(1.0);
  transform.RotateAboutYAxis(45.0);
  transform.Translate3d(-50.0, -50.0, 0.0);

  // Set up the child
  child->SetPosition(gfx::PointF(0.f, 0.f));
  child->SetBounds(gfx::Size(100, 100));
  child->SetContentBounds(gfx::Size(100, 100));
  child->SetTransform(transform);
  EmulateDrawingOneFrame(root.get());

  // Sanity check that the child layer's bounds would actually get clipped by
  // w < 0, otherwise this test is not actually testing the intended scenario.
  gfx::QuadF test_quad(gfx::RectF(gfx::PointF(), gfx::SizeF(100.f, 100.f)));
  bool clipped = false;
  MathUtil::MapQuad(transform, test_quad, &clipped);
  EXPECT_TRUE(clipped);

  // Damage the child without moving it.
  ClearDamageForAllSurfaces(root.get());
  child->SetOpacity(0.5f);
  EmulateDrawingOneFrame(root.get());

  // The expected damage should cover the entire root surface (500x500), but we
  // don't care whether the damage rect was clamped or is larger than the
  // surface for this test.
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  gfx::RectF damage_we_care_about =
          gfx::RectF(gfx::PointF(), gfx::SizeF(500.f, 500.f));
  EXPECT_TRUE(root_damage_rect.Contains(damage_we_care_about));
}

TEST_F(DamageTrackerTest, VerifyDamageForBlurredSurface) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* surface = root->children()[0];
  LayerImpl* child = surface->children()[0];

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  int outset_top, outset_right, outset_bottom, outset_left;
  filters.GetOutsets(&outset_top, &outset_right, &outset_bottom, &outset_left);

  // Setting the filter will damage the whole surface.
  ClearDamageForAllSurfaces(root.get());
  surface->SetFilters(filters);
  EmulateDrawingOneFrame(root.get());

  // Setting the update rect should cause the corresponding damage to the
  // surface, blurred based on the size of the blur filter.
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(1.f, 2.f, 3.f, 4.f));
  EmulateDrawingOneFrame(root.get());

  // Damage position on the surface should be: position of update_rect (1, 2)
  // relative to the child (300, 300), but expanded by the blur outsets.
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  gfx::RectF expected_damage_rect =
          gfx::RectF(301.f, 302.f, 3.f, 4.f);

  expected_damage_rect.Inset(-outset_left,
                             -outset_top,
                             -outset_right,
                             -outset_bottom);
  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForImageFilter) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];
  gfx::RectF root_damage_rect, child_damage_rect;

  // Allow us to set damage on child too.
  child->SetDrawsContent(true);

  skia::RefPtr<SkImageFilter> filter =
          skia::AdoptRef(new SkBlurImageFilter(SkIntToScalar(2),
                                               SkIntToScalar(2)));

  // Setting the filter will damage the whole surface.
  ClearDamageForAllSurfaces(root.get());
  child->SetFilter(filter);
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(100.f, 100.f, 30.f, 30.f), root_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 30.f, 30.f), child_damage_rect);

  // CASE 1: Setting the update rect should damage the whole surface (for now)
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(0.f, 0.f, 1.f, 1.f));
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(100.f, 100.f, 30.f, 30.f), root_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 30.f, 30.f), child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForBackgroundBlurredChild) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];

  // Allow us to set damage on child1 too.
  child1->SetDrawsContent(true);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  int outset_top, outset_right, outset_bottom, outset_left;
  filters.GetOutsets(&outset_top, &outset_right, &outset_bottom, &outset_left);

  // Setting the filter will damage the whole surface.
  ClearDamageForAllSurfaces(root.get());
  child1->SetBackgroundFilters(filters);
  EmulateDrawingOneFrame(root.get());

  // CASE 1: Setting the update rect should cause the corresponding damage to
  //         the surface, blurred based on the size of the child's background
  //         blur filter.
  ClearDamageForAllSurfaces(root.get());
  root->set_update_rect(gfx::RectF(297.f, 297.f, 2.f, 2.f));
  EmulateDrawingOneFrame(root.get());

  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  // Damage position on the surface should be a composition of the damage on
  // the root and on child2.  Damage on the root should be: position of
  // update_rect (297, 297), but expanded by the blur outsets.
  gfx::RectF expected_damage_rect =
          gfx::RectF(297.f, 297.f, 2.f, 2.f);

  expected_damage_rect.Inset(-outset_left,
                             -outset_top,
                             -outset_right,
                             -outset_bottom);
  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);

  // CASE 2: Setting the update rect should cause the corresponding damage to
  //         the surface, blurred based on the size of the child's background
  //         blur filter. Since the damage extends to the right/bottom outside
  //         of the blurred layer, only the left/top should end up expanded.
  ClearDamageForAllSurfaces(root.get());
  root->set_update_rect(gfx::RectF(297.f, 297.f, 30.f, 30.f));
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  // Damage position on the surface should be a composition of the damage on
  // the root and on child2.  Damage on the root should be: position of
  // update_rect (297, 297), but expanded on the left/top by the blur outsets.
  expected_damage_rect =
          gfx::RectF(297.f, 297.f, 30.f, 30.f);

  expected_damage_rect.Inset(-outset_left,
                             -outset_top,
                             0,
                             0);
  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);

  // CASE 3: Setting this update rect outside the blurred content_bounds of the
  //         blurred child1 will not cause it to be expanded.
  ClearDamageForAllSurfaces(root.get());
  root->set_update_rect(gfx::RectF(30.f, 30.f, 2.f, 2.f));
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  // Damage on the root should be: position of update_rect (30, 30), not
  // expanded.
  expected_damage_rect =
          gfx::RectF(30.f, 30.f, 2.f, 2.f);

  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);

  // CASE 4: Setting this update rect inside the blurred content_bounds but
  //         outside the original content_bounds of the blurred child1 will
  //         cause it to be expanded.
  ClearDamageForAllSurfaces(root.get());
  root->set_update_rect(gfx::RectF(99.f, 99.f, 1.f, 1.f));
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  // Damage on the root should be: position of update_rect (99, 99), expanded by
  // the blurring on child1, but since it is 1 pixel outside the layer, the
  // expanding should be reduced by 1.
  expected_damage_rect =
          gfx::RectF(99.f, 99.f, 1.f, 1.f);

  expected_damage_rect.Inset(-outset_left + 1,
                             -outset_top + 1,
                             -outset_right,
                             -outset_bottom);
  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);

  // CASE 5: Setting the update rect on child2, which is above child1, will
  // not get blurred by child1, so it does not need to get expanded.
  ClearDamageForAllSurfaces(root.get());
  child2->set_update_rect(gfx::RectF(0.f, 0.f, 1.f, 1.f));
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  // Damage on child2 should be: position of update_rect offset by the child's
  // position (11, 11), and not expanded by anything.
  expected_damage_rect =
          gfx::RectF(11.f, 11.f, 1.f, 1.f);

  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);

  // CASE 6: Setting the update rect on child1 will also blur the damage, so
  //         that any pixels needed for the blur are redrawn in the current
  //         frame.
  ClearDamageForAllSurfaces(root.get());
  child1->set_update_rect(gfx::RectF(0.f, 0.f, 1.f, 1.f));
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  // Damage on child1 should be: position of update_rect offset by the child's
  // position (100, 100), and expanded by the damage.
  expected_damage_rect =
          gfx::RectF(100.f, 100.f, 1.f, 1.f);

  expected_damage_rect.Inset(-outset_left,
                             -outset_top,
                             -outset_right,
                             -outset_bottom);
  EXPECT_FLOAT_RECT_EQ(expected_damage_rect, root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForAddingAndRemovingLayer) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child1 = root->children()[0];

  // CASE 1: Adding a new layer should cause the appropriate damage.
  //
  ClearDamageForAllSurfaces(root.get());
  {
    scoped_ptr<LayerImpl> child2 =
            LayerImpl::Create(host_impl_.active_tree(), 3);
    child2->SetPosition(gfx::PointF(400.f, 380.f));
    child2->SetAnchorPoint(gfx::PointF());
    child2->SetBounds(gfx::Size(6, 8));
    child2->SetContentBounds(gfx::Size(6, 8));
    child2->SetDrawsContent(true);
    root->AddChild(child2.Pass());
  }
  EmulateDrawingOneFrame(root.get());

  // Sanity check - all 3 layers should be on the same render surface; render
  // surfaces are tested elsewhere.
  ASSERT_EQ(3u, root->render_surface()->layer_list().size());

  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(400.f, 380.f, 6.f, 8.f), root_damage_rect);

  // CASE 2: If the layer is removed, its entire old layer becomes exposed, not
  //         just the last update rect.

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then, test removing child1.
  root->RemoveChild(child1);
  child1 = NULL;
  EmulateDrawingOneFrame(root.get());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(100.f, 100.f, 30.f, 30.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForNewUnchangedLayer) {
  // If child2 is added to the layer tree, but it doesn't have any explicit
  // damage of its own, it should still indeed damage the target surface.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();

  ClearDamageForAllSurfaces(root.get());
  {
    scoped_ptr<LayerImpl> child2 =
            LayerImpl::Create(host_impl_.active_tree(), 3);
    child2->SetPosition(gfx::PointF(400.f, 380.f));
    child2->SetAnchorPoint(gfx::PointF());
    child2->SetBounds(gfx::Size(6, 8));
    child2->SetContentBounds(gfx::Size(6, 8));
    child2->SetDrawsContent(true);
    child2->ResetAllChangeTrackingForSubtree();
    // Sanity check the initial conditions of the test, if these asserts
    // trigger, it means the test no longer actually covers the intended
    // scenario.
    ASSERT_FALSE(child2->LayerPropertyChanged());
    ASSERT_TRUE(child2->update_rect().IsEmpty());
    root->AddChild(child2.Pass());
  }
  EmulateDrawingOneFrame(root.get());

  // Sanity check - all 3 layers should be on the same render surface; render
  // surfaces are tested elsewhere.
  ASSERT_EQ(3u, root->render_surface()->layer_list().size());

  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(400.f, 380.f, 6.f, 8.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForMultipleLayers) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child1 = root->children()[0];

  // In this test we don't want the above tree manipulation to be considered
  // part of the same frame.
  ClearDamageForAllSurfaces(root.get());
  {
    scoped_ptr<LayerImpl> child2 =
            LayerImpl::Create(host_impl_.active_tree(), 3);
    child2->SetPosition(gfx::PointF(400.f, 380.f));
    child2->SetAnchorPoint(gfx::PointF());
    child2->SetBounds(gfx::Size(6, 8));
    child2->SetContentBounds(gfx::Size(6, 8));
    child2->SetDrawsContent(true);
    root->AddChild(child2.Pass());
  }
  LayerImpl* child2 = root->children()[1];
  EmulateDrawingOneFrame(root.get());

  // Damaging two layers simultaneously should cause combined damage.
  // - child1 update rect in surface space: gfx::RectF(100.f, 100.f, 1.f, 2.f);
  // - child2 update rect in surface space: gfx::RectF(400.f, 380.f, 3.f, 4.f);
  ClearDamageForAllSurfaces(root.get());
  child1->set_update_rect(gfx::RectF(0.f, 0.f, 1.f, 2.f));
  child2->set_update_rect(gfx::RectF(0.f, 0.f, 3.f, 4.f));
  EmulateDrawingOneFrame(root.get());
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(
      gfx::RectF(100.f, 100.f, 303.f, 284.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForNestedSurfaces) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  LayerImpl* grand_child1 = root->children()[0]->children()[0];
  gfx::RectF child_damage_rect;
  gfx::RectF root_damage_rect;

  // CASE 1: Damage to a descendant surface should propagate properly to
  //         ancestor surface.
  ClearDamageForAllSurfaces(root.get());
  grand_child1->SetOpacity(0.5f);
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(200.f, 200.f, 6.f, 8.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(300.f, 300.f, 6.f, 8.f), root_damage_rect);

  // CASE 2: Same as previous case, but with additional damage elsewhere that
  //         should be properly unioned.
  // - child1 surface damage in root surface space:
  //   gfx::RectF(300.f, 300.f, 6.f, 8.f);
  // - child2 damage in root surface space:
  //   gfx::RectF(11.f, 11.f, 18.f, 18.f);
  ClearDamageForAllSurfaces(root.get());
  grand_child1->SetOpacity(0.7f);
  child2->SetOpacity(0.7f);
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(200.f, 200.f, 6.f, 8.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(11.f, 11.f, 295.f, 297.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForSurfaceChangeFromDescendantLayer) {
  // If descendant layer changes and affects the content bounds of the render
  // surface, then the entire descendant surface should be damaged, and it
  // should damage its ancestor surface with the old and new surface regions.

  // This is a tricky case, since only the first grand_child changes, but the
  // entire surface should be marked dirty.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  LayerImpl* grand_child1 = root->children()[0]->children()[0];
  gfx::RectF child_damage_rect;
  gfx::RectF root_damage_rect;

  ClearDamageForAllSurfaces(root.get());
  grand_child1->SetPosition(gfx::PointF(195.f, 205.f));
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  // The new surface bounds should be damaged entirely, even though only one of
  // the layers changed.
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(190.f, 190.f, 11.f, 23.f), child_damage_rect);

  // Damage to the root surface should be the union of child1's *entire* render
  // surface (in target space), and its old exposed area (also in target
  // space).
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(290.f, 290.f, 16.f, 23.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForSurfaceChangeFromAncestorLayer) {
  // An ancestor/owning layer changes that affects the position/transform of
  // the render surface. Note that in this case, the layer_property_changed flag
  // already propagates to the subtree (tested in LayerImpltest), which damages
  // the entire child1 surface, but the damage tracker still needs the correct
  // logic to compute the exposed region on the root surface.

  // TODO(shawnsingh): the expectations of this test case should change when we
  // add support for a unique scissor_rect per RenderSurface. In that case, the
  // child1 surface should be completely unchanged, since we are only
  // transforming it, while the root surface would be damaged appropriately.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  gfx::RectF child_damage_rect;
  gfx::RectF root_damage_rect;

  ClearDamageForAllSurfaces(root.get());
  child1->SetPosition(gfx::PointF(50.f, 50.f));
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  // The new surface bounds should be damaged entirely.
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(190.f, 190.f, 16.f, 18.f), child_damage_rect);

  // The entire child1 surface and the old exposed child1 surface should damage
  // the root surface.
  //  - old child1 surface in target space: gfx::RectF(290.f, 290.f, 16.f, 18.f)
  //  - new child1 surface in target space: gfx::RectF(240.f, 240.f, 16.f, 18.f)
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(240.f, 240.f, 66.f, 68.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForAddingAndRemovingRenderSurfaces) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  gfx::RectF child_damage_rect;
  gfx::RectF root_damage_rect;

  // CASE 1: If a descendant surface disappears, its entire old area becomes
  //         exposed.
  ClearDamageForAllSurfaces(root.get());
  child1->SetOpacity(1.f);
  child1->SetForceRenderSurface(false);
  EmulateDrawingOneFrame(root.get());

  // Sanity check that there is only one surface now.
  ASSERT_FALSE(child1->render_surface());
  ASSERT_EQ(4u, root->render_surface()->layer_list().size());

  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(290.f, 290.f, 16.f, 18.f), root_damage_rect);

  // CASE 2: If a descendant surface appears, its entire old area becomes
  //         exposed.

  // Cycle one frame of no change, just to sanity check that the next rect is
  // not because of the old damage state.
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Then change the tree so that the render surface is added back.
  ClearDamageForAllSurfaces(root.get());
  child1->SetOpacity(0.5f);
  child1->SetForceRenderSurface(true);
  EmulateDrawingOneFrame(root.get());

  // Sanity check that there is a new surface now.
  ASSERT_TRUE(child1->render_surface());
  EXPECT_EQ(3u, root->render_surface()->layer_list().size());
  EXPECT_EQ(2u, child1->render_surface()->layer_list().size());

  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(190.f, 190.f, 16.f, 18.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(290.f, 290.f, 16.f, 18.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyNoDamageWhenNothingChanged) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  gfx::RectF child_damage_rect;
  gfx::RectF root_damage_rect;

  // CASE 1: If nothing changes, the damage rect should be empty.
  //
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(child_damage_rect.IsEmpty());
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // CASE 2: If nothing changes twice in a row, the damage rect should still be
  //         empty.
  //
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(child_damage_rect.IsEmpty());
  EXPECT_TRUE(root_damage_rect.IsEmpty());
}

TEST_F(DamageTrackerTest, VerifyNoDamageForUpdateRectThatDoesNotDrawContent) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  gfx::RectF child_damage_rect;
  gfx::RectF root_damage_rect;

  // In our specific tree, the update rect of child1 should not cause any
  // damage to any surface because it does not actually draw content.
  ClearDamageForAllSurfaces(root.get());
  child1->set_update_rect(gfx::RectF(0.f, 0.f, 1.f, 2.f));
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(child_damage_rect.IsEmpty());
  EXPECT_TRUE(root_damage_rect.IsEmpty());
}

TEST_F(DamageTrackerTest, VerifyDamageForReplica) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  LayerImpl* grand_child1 = child1->children()[0];
  LayerImpl* grand_child2 = child1->children()[1];

  // Damage on a surface that has a reflection should cause the target surface
  // to receive the surface's damage and the surface's reflected damage.

  // For this test case, we modify grand_child2, and add grand_child3 to extend
  // the bounds of child1's surface. This way, we can test reflection changes
  // without changing content_bounds of the surface.
  grand_child2->SetPosition(gfx::PointF(180.f, 180.f));
  {
    scoped_ptr<LayerImpl> grand_child3 =
            LayerImpl::Create(host_impl_.active_tree(), 6);
    grand_child3->SetPosition(gfx::PointF(240.f, 240.f));
    grand_child3->SetAnchorPoint(gfx::PointF());
    grand_child3->SetBounds(gfx::Size(10, 10));
    grand_child3->SetContentBounds(gfx::Size(10, 10));
    grand_child3->SetDrawsContent(true);
    child1->AddChild(grand_child3.Pass());
  }
  child1->SetOpacity(0.5f);
  EmulateDrawingOneFrame(root.get());

  // CASE 1: adding a reflection about the left edge of grand_child1.
  //
  ClearDamageForAllSurfaces(root.get());
  {
    scoped_ptr<LayerImpl> grand_child1_replica =
            LayerImpl::Create(host_impl_.active_tree(), 7);
    grand_child1_replica->SetPosition(gfx::PointF());
    grand_child1_replica->SetAnchorPoint(gfx::PointF());
    gfx::Transform reflection;
    reflection.Scale3d(-1.0, 1.0, 1.0);
    grand_child1_replica->SetTransform(reflection);
    grand_child1->SetReplicaLayer(grand_child1_replica.Pass());
  }
  EmulateDrawingOneFrame(root.get());

  gfx::RectF grand_child_damage_rect =
          grand_child1->render_surface()->damage_tracker()->
              current_damage_rect();
  gfx::RectF child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  // The grand_child surface damage should not include its own replica. The
  // child surface damage should include the normal and replica surfaces.
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 8.f), grand_child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(194.f, 200.f, 12.f, 8.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(294.f, 300.f, 12.f, 8.f), root_damage_rect);

  // CASE 2: moving the descendant surface should cause both the original and
  //         reflected areas to be damaged on the target.
  ClearDamageForAllSurfaces(root.get());
  gfx::Rect old_content_rect = child1->render_surface()->content_rect();
  grand_child1->SetPosition(gfx::PointF(195.f, 205.f));
  EmulateDrawingOneFrame(root.get());
  ASSERT_EQ(old_content_rect.width(),
            child1->render_surface()->content_rect().width());
  ASSERT_EQ(old_content_rect.height(),
            child1->render_surface()->content_rect().height());

  grand_child_damage_rect =
          grand_child1->render_surface()->
              damage_tracker()->current_damage_rect();
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  // The child surface damage should include normal and replica surfaces for
  // both old and new locations.
  //  - old location in target space: gfx::RectF(194.f, 200.f, 12.f, 8.f)
  //  - new location in target space: gfx::RectF(189.f, 205.f, 12.f, 8.f)
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 6.f, 8.f), grand_child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(189.f, 200.f, 17.f, 13.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(289.f, 300.f, 17.f, 13.f), root_damage_rect);

  // CASE 3: removing the reflection should cause the entire region including
  //         reflection to damage the target surface.
  ClearDamageForAllSurfaces(root.get());
  grand_child1->SetReplicaLayer(scoped_ptr<LayerImpl>());
  EmulateDrawingOneFrame(root.get());
  ASSERT_EQ(old_content_rect.width(),
            child1->render_surface()->content_rect().width());
  ASSERT_EQ(old_content_rect.height(),
            child1->render_surface()->content_rect().height());

  EXPECT_FALSE(grand_child1->render_surface());
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();

  EXPECT_FLOAT_RECT_EQ(gfx::RectF(189.f, 205.f, 12.f, 8.f), child_damage_rect);
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(289.f, 305.f, 12.f, 8.f), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForMask) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  // In the current implementation of the damage tracker, changes to mask
  // layers should damage the entire corresponding surface.

  ClearDamageForAllSurfaces(root.get());

  // Set up the mask layer.
  {
    scoped_ptr<LayerImpl> mask_layer =
            LayerImpl::Create(host_impl_.active_tree(), 3);
    mask_layer->SetPosition(child->position());
    mask_layer->SetAnchorPoint(gfx::PointF());
    mask_layer->SetBounds(child->bounds());
    mask_layer->SetContentBounds(child->bounds());
    child->SetMaskLayer(mask_layer.Pass());
  }
  LayerImpl* mask_layer = child->mask_layer();

  // Add opacity and a grand_child so that the render surface persists even
  // after we remove the mask.
  child->SetOpacity(0.5f);
  {
    scoped_ptr<LayerImpl> grand_child =
            LayerImpl::Create(host_impl_.active_tree(), 4);
    grand_child->SetPosition(gfx::PointF(2.f, 2.f));
    grand_child->SetAnchorPoint(gfx::PointF());
    grand_child->SetBounds(gfx::Size(2, 2));
    grand_child->SetContentBounds(gfx::Size(2, 2));
    grand_child->SetDrawsContent(true);
    child->AddChild(grand_child.Pass());
  }
  EmulateDrawingOneFrame(root.get());

  // Sanity check that a new surface was created for the child.
  ASSERT_TRUE(child->render_surface());

  // CASE 1: the update_rect on a mask layer should damage the entire target
  //         surface.
  ClearDamageForAllSurfaces(root.get());
  mask_layer->set_update_rect(gfx::RectF(1.f, 2.f, 3.f, 4.f));
  EmulateDrawingOneFrame(root.get());
  gfx::RectF child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 30.f, 30.f), child_damage_rect);

  // CASE 2: a property change on the mask layer should damage the entire
  //         target surface.

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(child_damage_rect.IsEmpty());

  // Then test the property change.
  ClearDamageForAllSurfaces(root.get());
  mask_layer->SetStackingOrderChanged(true);

  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 30.f, 30.f), child_damage_rect);

  // CASE 3: removing the mask also damages the entire target surface.
  //

  // Advance one frame without damage so that we know the damage rect is not
  // leftover from the previous case.
  ClearDamageForAllSurfaces(root.get());
  EmulateDrawingOneFrame(root.get());
  child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(child_damage_rect.IsEmpty());

  // Then test mask removal.
  ClearDamageForAllSurfaces(root.get());
  child->SetMaskLayer(scoped_ptr<LayerImpl>());
  ASSERT_TRUE(child->LayerPropertyChanged());
  EmulateDrawingOneFrame(root.get());

  // Sanity check that a render surface still exists.
  ASSERT_TRUE(child->render_surface());

  child_damage_rect =
          child->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(0.f, 0.f, 30.f, 30.f), child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForReplicaMask) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  LayerImpl* grand_child1 = child1->children()[0];

  // Changes to a replica's mask should not damage the original surface,
  // because it is not masked. But it does damage the ancestor target surface.

  ClearDamageForAllSurfaces(root.get());

  // Create a reflection about the left edge of grand_child1.
  {
    scoped_ptr<LayerImpl> grand_child1_replica =
            LayerImpl::Create(host_impl_.active_tree(), 6);
    grand_child1_replica->SetPosition(gfx::PointF());
    grand_child1_replica->SetAnchorPoint(gfx::PointF());
    gfx::Transform reflection;
    reflection.Scale3d(-1.0, 1.0, 1.0);
    grand_child1_replica->SetTransform(reflection);
    grand_child1->SetReplicaLayer(grand_child1_replica.Pass());
  }
  LayerImpl* grand_child1_replica = grand_child1->replica_layer();

  // Set up the mask layer on the replica layer
  {
    scoped_ptr<LayerImpl> replica_mask_layer =
            LayerImpl::Create(host_impl_.active_tree(), 7);
    replica_mask_layer->SetPosition(gfx::PointF());
    replica_mask_layer->SetAnchorPoint(gfx::PointF());
    replica_mask_layer->SetBounds(grand_child1->bounds());
    replica_mask_layer->SetContentBounds(grand_child1->bounds());
    grand_child1_replica->SetMaskLayer(replica_mask_layer.Pass());
  }
  LayerImpl* replica_mask_layer = grand_child1_replica->mask_layer();

  EmulateDrawingOneFrame(root.get());

  // Sanity check that the appropriate render surfaces were created
  ASSERT_TRUE(grand_child1->render_surface());

  // CASE 1: a property change on the mask should damage only the reflected
  //         region on the target surface.
  ClearDamageForAllSurfaces(root.get());
  replica_mask_layer->SetStackingOrderChanged(true);
  EmulateDrawingOneFrame(root.get());

  gfx::RectF grand_child_damage_rect =
          grand_child1->render_surface()->damage_tracker()->
              current_damage_rect();
  gfx::RectF child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();

  EXPECT_TRUE(grand_child_damage_rect.IsEmpty());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(194.f, 200.f, 6.f, 8.f), child_damage_rect);

  // CASE 2: removing the replica mask damages only the reflected region on the
  //         target surface.
  //
  ClearDamageForAllSurfaces(root.get());
  grand_child1_replica->SetMaskLayer(scoped_ptr<LayerImpl>());
  EmulateDrawingOneFrame(root.get());

  grand_child_damage_rect =
          grand_child1->render_surface()->damage_tracker()->
              current_damage_rect();
  child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();

  EXPECT_TRUE(grand_child_damage_rect.IsEmpty());
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(194.f, 200.f, 6.f, 8.f), child_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForReplicaMaskWithAnchor) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithTwoSurfaces();
  LayerImpl* child1 = root->children()[0];
  LayerImpl* grand_child1 = child1->children()[0];

  // Verify that the correct replica_origin_transform is used for the
  // replica_mask.
  ClearDamageForAllSurfaces(root.get());

  // This is not actually the anchor point being tested, but by convention its
  // expected to be the same as the replica's anchor point.
  grand_child1->SetAnchorPoint(gfx::PointF(1.f, 0.f));

  {
    scoped_ptr<LayerImpl> grand_child1_replica =
            LayerImpl::Create(host_impl_.active_tree(), 6);
    grand_child1_replica->SetPosition(gfx::PointF());

    // This is the anchor being tested.
    grand_child1_replica->SetAnchorPoint(gfx::PointF(1.f, 0.f));
    gfx::Transform reflection;
    reflection.Scale3d(-1.0, 1.0, 1.0);
    grand_child1_replica->SetTransform(reflection);
    grand_child1->SetReplicaLayer(grand_child1_replica.Pass());
  }
  LayerImpl* grand_child1_replica = grand_child1->replica_layer();

  // Set up the mask layer on the replica layer
  {
    scoped_ptr<LayerImpl> replica_mask_layer =
            LayerImpl::Create(host_impl_.active_tree(), 7);
    replica_mask_layer->SetPosition(gfx::PointF());
    // Note: this is not the anchor being tested.
    replica_mask_layer->SetAnchorPoint(gfx::PointF());
    replica_mask_layer->SetBounds(grand_child1->bounds());
    replica_mask_layer->SetContentBounds(grand_child1->bounds());
    grand_child1_replica->SetMaskLayer(replica_mask_layer.Pass());
  }
  LayerImpl* replica_mask_layer = grand_child1_replica->mask_layer();

  EmulateDrawingOneFrame(root.get());

  // Sanity check that the appropriate render surfaces were created
  ASSERT_TRUE(grand_child1->render_surface());

  // A property change on the replica_mask should damage the reflected region on
  // the target surface.
  ClearDamageForAllSurfaces(root.get());
  replica_mask_layer->SetStackingOrderChanged(true);

  EmulateDrawingOneFrame(root.get());

  gfx::RectF child_damage_rect =
          child1->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(206.f, 200.f, 6.f, 8.f), child_damage_rect);
}

TEST_F(DamageTrackerTest, DamageWhenAddedExternally) {
  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  // Case 1: This test ensures that when the tracker is given damage, that
  //         it is included with any other partial damage.
  //
  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(10, 11, 12, 13));
  root->render_surface()->damage_tracker()->AddDamageNextUpdate(
      gfx::RectF(15, 16, 32, 33));
  EmulateDrawingOneFrame(root.get());
  gfx::RectF root_damage_rect =
      root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(
      gfx::UnionRects(gfx::RectF(15, 16, 32, 33),
                      gfx::RectF(100+10, 100+11, 12, 13)),
      root_damage_rect);

  // Case 2: An additional sanity check that adding damage works even when
  //         nothing on the layer tree changed.
  //
  ClearDamageForAllSurfaces(root.get());
  root->render_surface()->damage_tracker()->AddDamageNextUpdate(
      gfx::RectF(30, 31, 14, 15));
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
      root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(30, 31, 14, 15), root_damage_rect);
}

TEST_F(DamageTrackerTest, VerifyDamageForEmptyLayerList) {
  // Though it should never happen, its a good idea to verify that the damage
  // tracker does not crash when it receives an empty layer_list.

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl_.active_tree(), 1);
  root->CreateRenderSurface();

  ASSERT_TRUE(root == root->render_target());
  RenderSurfaceImpl* target_surface = root->render_surface();

  LayerImplList empty_list;
  target_surface->damage_tracker()->UpdateDamageTrackingState(
      empty_list,
      target_surface->OwningLayerId(),
      false,
      gfx::Rect(),
      NULL,
      FilterOperations(),
      NULL);

  gfx::RectF damage_rect =
          target_surface->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(damage_rect.IsEmpty());
}

TEST_F(DamageTrackerTest, VerifyDamageAccumulatesUntilReset) {
  // If damage is not cleared, it should accumulate.

  scoped_ptr<LayerImpl> root = CreateAndSetUpTestTreeWithOneSurface();
  LayerImpl* child = root->children()[0];

  ClearDamageForAllSurfaces(root.get());
  child->set_update_rect(gfx::RectF(10.f, 11.f, 1.f, 2.f));
  EmulateDrawingOneFrame(root.get());

  // Sanity check damage after the first frame; this isnt the actual test yet.
  gfx::RectF root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(110.f, 111.f, 1.f, 2.f), root_damage_rect);

  // New damage, without having cleared the previous damage, should be unioned
  // to the previous one.
  child->set_update_rect(gfx::RectF(20.f, 25.f, 1.f, 2.f));
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_FLOAT_RECT_EQ(gfx::RectF(110.f, 111.f, 11.f, 16.f), root_damage_rect);

  // If we notify the damage tracker that we drew the damaged area, then damage
  // should be emptied.
  root->render_surface()->damage_tracker()->DidDrawDamagedArea();
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(root_damage_rect.IsEmpty());

  // Damage should remain empty even after one frame, since there's yet no new
  // damage.
  EmulateDrawingOneFrame(root.get());
  root_damage_rect =
          root->render_surface()->damage_tracker()->current_damage_rect();
  EXPECT_TRUE(root_damage_rect.IsEmpty());
}

}  // namespace
}  // namespace cc
