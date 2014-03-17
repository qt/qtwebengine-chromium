// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <set>

#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class LayerTreeHostCommonTestBase {
 protected:
  template <typename LayerType>
  void SetLayerPropertiesForTestingInternal(
      LayerType* layer,
      const gfx::Transform& transform,
      const gfx::Transform& sublayer_transform,
      gfx::PointF anchor,
      gfx::PointF position,
      gfx::Size bounds,
      bool preserves3d) {
    layer->SetTransform(transform);
    layer->SetSublayerTransform(sublayer_transform);
    layer->SetAnchorPoint(anchor);
    layer->SetPosition(position);
    layer->SetBounds(bounds);
    layer->SetPreserves3d(preserves3d);
  }

  void SetLayerPropertiesForTesting(Layer* layer,
                                    const gfx::Transform& transform,
                                    const gfx::Transform& sublayer_transform,
                                    gfx::PointF anchor,
                                    gfx::PointF position,
                                    gfx::Size bounds,
                                    bool preserves3d) {
    SetLayerPropertiesForTestingInternal<Layer>(layer,
                                                transform,
                                                sublayer_transform,
                                                anchor,
                                                position,
                                                bounds,
                                                preserves3d);
  }

  void SetLayerPropertiesForTesting(LayerImpl* layer,
                                    const gfx::Transform& transform,
                                    const gfx::Transform& sublayer_transform,
                                    gfx::PointF anchor,
                                    gfx::PointF position,
                                    gfx::Size bounds,
                                    bool preserves3d) {
    SetLayerPropertiesForTestingInternal<LayerImpl>(layer,
                                                    transform,
                                                    sublayer_transform,
                                                    anchor,
                                                    position,
                                                    bounds,
                                                    preserves3d);
    layer->SetContentBounds(bounds);
  }

  void ExecuteCalculateDrawProperties(Layer* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      Layer* page_scale_application_layer,
                                      bool can_use_lcd_text) {
    EXPECT_TRUE(page_scale_application_layer || (page_scale_factor == 1.f));
    gfx::Transform identity_matrix;
    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width() * device_scale_factor,
                  root_layer->bounds().height() * device_scale_factor);

    render_surface_layer_list_.reset(new RenderSurfaceLayerList);

    // We are probably not testing what is intended if the root_layer bounds are
    // empty.
    DCHECK(!root_layer->bounds().IsEmpty());
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root_layer, device_viewport_size, render_surface_layer_list_.get());
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = page_scale_application_layer;
    inputs.can_use_lcd_text = can_use_lcd_text;
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  void ExecuteCalculateDrawProperties(LayerImpl* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      LayerImpl* page_scale_application_layer,
                                      bool can_use_lcd_text) {
    gfx::Transform identity_matrix;
    LayerImplList dummy_render_surface_layer_list;
    gfx::Size device_viewport_size =
        gfx::Size(root_layer->bounds().width() * device_scale_factor,
                  root_layer->bounds().height() * device_scale_factor);

    // We are probably not testing what is intended if the root_layer bounds are
    // empty.
    DCHECK(!root_layer->bounds().IsEmpty());
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root_layer, device_viewport_size, &dummy_render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = page_scale_application_layer;
    inputs.can_use_lcd_text = can_use_lcd_text;
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer) {
    LayerType* page_scale_application_layer = NULL;
    ExecuteCalculateDrawProperties(
        root_layer, 1.f, 1.f, page_scale_application_layer, false);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer,
                                      float device_scale_factor) {
    LayerType* page_scale_application_layer = NULL;
    ExecuteCalculateDrawProperties(root_layer,
                                   device_scale_factor,
                                   1.f,
                                   page_scale_application_layer,
                                   false);
  }

  template <class LayerType>
  void ExecuteCalculateDrawProperties(LayerType* root_layer,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      LayerType* page_scale_application_layer) {
    ExecuteCalculateDrawProperties(root_layer,
                                   device_scale_factor,
                                   page_scale_factor,
                                   page_scale_application_layer,
                                   false);
  }

  RenderSurfaceLayerList* render_surface_layer_list() const {
    return render_surface_layer_list_.get();
  }

 private:
  scoped_ptr<RenderSurfaceLayerList> render_surface_layer_list_;
};

class LayerTreeHostCommonTest : public LayerTreeHostCommonTestBase,
                                public testing::Test {
};

class LayerWithForcedDrawsContent : public Layer {
 public:
  LayerWithForcedDrawsContent() : Layer() {}

  virtual bool DrawsContent() const OVERRIDE;

 private:
  virtual ~LayerWithForcedDrawsContent() {}
};

bool LayerWithForcedDrawsContent::DrawsContent() const { return true; }

class MockContentLayerClient : public ContentLayerClient {
 public:
  MockContentLayerClient() {}
  virtual ~MockContentLayerClient() {}
  virtual void PaintContents(SkCanvas* canvas,
                             gfx::Rect clip,
                             gfx::RectF* opaque) OVERRIDE {}
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
};

scoped_refptr<ContentLayer> CreateDrawableContentLayer(
    ContentLayerClient* delegate) {
  scoped_refptr<ContentLayer> to_return = ContentLayer::Create(delegate);
  to_return->SetIsDrawable(true);
  return to_return;
}

#define EXPECT_CONTENTS_SCALE_EQ(expected, layer)         \
  do {                                                    \
    EXPECT_FLOAT_EQ(expected, layer->contents_scale_x()); \
    EXPECT_FLOAT_EQ(expected, layer->contents_scale_y()); \
  } while (false)

TEST_F(LayerTreeHostCommonTest, TransformsForNoOpLayer) {
  // Sanity check: For layers positioned at zero, with zero size,
  // and with identity transforms, then the draw transform,
  // screen space transform, and the hierarchy passed on to children
  // layers should also be identity transforms.

  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  parent->AddChild(child);
  child->AddChild(grand_child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(),
                               false);

  ExecuteCalculateDrawProperties(parent.get());

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSingleLayer) {
  gfx::Transform identity_matrix;
  scoped_refptr<Layer> layer = Layer::Create();

  scoped_refptr<Layer> root = Layer::Create();
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               false);
  root->AddChild(layer);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // Case 1: setting the sublayer transform should not affect this layer's draw
  // transform or screen-space transform.
  gfx::Transform arbitrary_translation;
  arbitrary_translation.Translate(10.0, 20.0);
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               arbitrary_translation,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  gfx::Transform expected_draw_transform = identity_matrix;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_draw_transform,
                                  layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  layer->screen_space_transform());

  // Case 2: Setting the bounds of the layer should not affect either the draw
  // transform or the screenspace transform.
  gfx::Transform translation_to_center;
  translation_to_center.Translate(5.0, 6.0);
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  layer->screen_space_transform());

  // Case 3: The anchor point by itself (without a layer transform) should have
  // no effect on the transforms.
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  layer->screen_space_transform());

  // Case 4: A change in actual position affects both the draw transform and
  // screen space transform.
  gfx::Transform position_transform;
  position_transform.Translate(0.f, 1.2f);
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(0.f, 1.2f),
                               gfx::Size(10, 12),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(position_transform, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(position_transform,
                                  layer->screen_space_transform());

  // Case 5: In the correct sequence of transforms, the layer transform should
  // pre-multiply the translation_to_center. This is easily tested by using a
  // scale transform, because scale and translation are not commutative.
  gfx::Transform layer_transform;
  layer_transform.Scale3d(2.0, 2.0, 1.0);
  SetLayerPropertiesForTesting(layer.get(),
                               layer_transform,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(layer_transform, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(layer_transform,
                                  layer->screen_space_transform());

  // Case 6: The layer transform should occur with respect to the anchor point.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(5.0, 0.0);
  gfx::Transform expected_result =
      translation_to_anchor * layer_transform * Inverse(translation_to_anchor);
  SetLayerPropertiesForTesting(layer.get(),
                               layer_transform,
                               identity_matrix,
                               gfx::PointF(0.5f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result,
                                  layer->screen_space_transform());

  // Case 7: Verify that position pre-multiplies the layer transform.  The
  // current implementation of CalculateDrawProperties does this implicitly, but
  // it is still worth testing to detect accidental regressions.
  expected_result = position_transform * translation_to_anchor *
                    layer_transform * Inverse(translation_to_anchor);
  SetLayerPropertiesForTesting(layer.get(),
                               layer_transform,
                               identity_matrix,
                               gfx::PointF(0.5f, 0.f),
                               gfx::PointF(0.f, 1.2f),
                               gfx::Size(10, 12),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result,
                                  layer->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsAboutScrollOffset) {
  const gfx::Vector2d kScrollOffset(50, 100);
  const gfx::Vector2dF kScrollDelta(2.34f, 5.67f);
  const gfx::Vector2d kMaxScrollOffset(200, 200);
  const gfx::PointF kScrollLayerPosition(-kScrollOffset.x(),
                                         -kScrollOffset.y());
  const float kPageScale = 0.888f;
  const float kDeviceScale = 1.666f;

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);

  gfx::Transform identity_matrix;
  scoped_ptr<LayerImpl> sublayer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 1));
  LayerImpl* sublayer = sublayer_scoped_ptr.get();
  sublayer->SetContentsScale(kPageScale * kDeviceScale,
                             kPageScale * kDeviceScale);
  SetLayerPropertiesForTesting(sublayer,
                               identity_matrix,
                               identity_matrix,
                               gfx::Point(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               false);

  scoped_ptr<LayerImpl> scroll_layerScopedPtr(
      LayerImpl::Create(host_impl.active_tree(), 2));
  LayerImpl* scroll_layer = scroll_layerScopedPtr.get();
  SetLayerPropertiesForTesting(scroll_layer,
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 20),
                               false);
  scroll_layer->SetScrollable(true);
  scroll_layer->SetMaxScrollOffset(kMaxScrollOffset);
  scroll_layer->SetScrollOffset(kScrollOffset);
  scroll_layer->SetScrollDelta(kScrollDelta);
  gfx::Transform impl_transform;
  scroll_layer->AddChild(sublayer_scoped_ptr.Pass());

  scoped_ptr<LayerImpl> root(LayerImpl::Create(host_impl.active_tree(), 3));
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               false);
  root->AddChild(scroll_layerScopedPtr.Pass());

  ExecuteCalculateDrawProperties(
      root.get(), kDeviceScale, kPageScale, scroll_layer->parent());
  gfx::Transform expected_transform = identity_matrix;
  gfx::PointF sub_layer_screen_position = kScrollLayerPosition - kScrollDelta;
  sub_layer_screen_position.Scale(kPageScale * kDeviceScale);
  expected_transform.Translate(MathUtil::Round(sub_layer_screen_position.x()),
                               MathUtil::Round(sub_layer_screen_position.y()));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->screen_space_transform());

  gfx::Transform arbitrary_translate;
  const float kTranslateX = 10.6f;
  const float kTranslateY = 20.6f;
  arbitrary_translate.Translate(kTranslateX, kTranslateY);
  SetLayerPropertiesForTesting(scroll_layer,
                               arbitrary_translate,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 20),
                               false);
  ExecuteCalculateDrawProperties(
      root.get(), kDeviceScale, kPageScale, scroll_layer->parent());
  expected_transform.MakeIdentity();
  expected_transform.Translate(
      MathUtil::Round(kTranslateX * kPageScale * kDeviceScale +
                      sub_layer_screen_position.x()),
      MathUtil::Round(kTranslateY * kPageScale * kDeviceScale +
                      sub_layer_screen_position.y()));
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->draw_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSimpleHierarchy) {
  gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // One-time setup of root layer
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               false);

  // Case 1: parent's anchor point should not affect child or grand_child.
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->screen_space_transform());

  // Case 2: parent's position affects child and grand_child.
  gfx::Transform parent_position_transform;
  parent_position_transform.Translate(0.f, 1.2f);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(0.f, 1.2f),
                               gfx::Size(10, 12),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  grand_child->screen_space_transform());

  // Case 3: parent's local transform affects child and grandchild
  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(2.0, 2.0, 1.0);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(2.5, 3.0);
  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               identity_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  grand_child->screen_space_transform());

  // Case 4: parent's sublayer matrix affects child and grandchild scaling is
  // used here again so that the correct sequence of transforms is properly
  // tested.  Note that preserves3d is false, but the sublayer matrix should
  // retain its 3D properties when given to child.  But then, the child also
  // does not preserve3D. When it gives its hierarchy to the grand_child, it
  // should be flattened to 2D.
  gfx::Transform parent_sublayer_matrix;
  parent_sublayer_matrix.Scale3d(10.f, 10.f, 3.3f);
  // Sublayer matrix is applied to the anchor point of the parent layer.
  parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor) * parent_translation_to_anchor *
      parent_sublayer_matrix * Inverse(parent_translation_to_anchor);
  gfx::Transform flattened_composite_transform = parent_composite_transform;
  flattened_composite_transform.FlattenTo2d();
  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               parent_sublayer_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_composite_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_composite_transform,
                                  grand_child->screen_space_transform());

  // Case 5: same as Case 4, except that child does preserve 3D, so the
  // grand_child should receive the non-flattened composite transform.
  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               parent_sublayer_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               true);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSingleRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // One-time setup of root layer
  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               false);

  // Child is set up so that a new render surface should be created.
  child->SetOpacity(0.5f);
  child->SetForceRenderSurface(true);

  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(1.f, 0.9f, 1.f);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(25.0, 30.0);
  gfx::Transform parent_sublayer_matrix;
  parent_sublayer_matrix.Scale3d(0.9f, 1.f, 3.3f);

  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor) * parent_translation_to_anchor *
      parent_sublayer_matrix * Inverse(parent_translation_to_anchor);
  gfx::Vector2dF parent_composite_scale =
      MathUtil::ComputeTransform2dScaleComponents(parent_composite_transform,
                                                  1.f);
  gfx::Transform surface_sublayer_transform;
  surface_sublayer_transform.Scale(parent_composite_scale.x(),
                                   parent_composite_scale.y());
  gfx::Transform surface_sublayer_composite_transform =
      parent_composite_transform * Inverse(surface_sublayer_transform);

  // Child's render surface should not exist yet.
  ASSERT_FALSE(child->render_surface());

  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               parent_sublayer_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(100, 120),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(8, 10),
                               false);
  ExecuteCalculateDrawProperties(root.get());

  // Render surface should have been created now.
  ASSERT_TRUE(child->render_surface());
  ASSERT_EQ(child, child->render_target());

  // The child layer's draw transform should refer to its new render surface.
  // The screen-space transform, however, should still refer to the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(surface_sublayer_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->screen_space_transform());

  // Because the grand_child is the only drawable content, the child's render
  // surface will tighten its bounds to the grand_child.  The scale at which the
  // surface's subtree is drawn must be removed from the composite transform.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      surface_sublayer_composite_transform,
      child->render_target()->render_surface()->draw_transform());

  // The screen space is the same as the target since the child surface draws
  // into the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      surface_sublayer_composite_transform,
      child->render_target()->render_surface()->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, SublayerTransformWithAnchorPoint) {
  // crbug.com/157961 - we were always applying the sublayer transform about
  // the center of the layer, rather than the anchor point.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(parent);
  parent->AddChild(child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform parent_sublayer_matrix;
  parent_sublayer_matrix.ApplyPerspectiveDepth(2.0);
  gfx::PointF parent_anchor_point(0.2f, 0.8f);

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               false);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               parent_sublayer_matrix,
                               parent_anchor_point,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  ExecuteCalculateDrawProperties(root.get());

  gfx::Transform expected_child_draw_transform;
  expected_child_draw_transform.Translate(20.0, 80.0);
  expected_child_draw_transform.ApplyPerspectiveDepth(2.0);
  expected_child_draw_transform.Translate(-20.0, -80.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_draw_transform,
                                  child->draw_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForReplica) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> child_replica = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);
  child->SetReplicaLayer(child_replica.get());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // One-time setup of root layer
  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               false);

  // Child is set up so that a new render surface should be created.
  child->SetOpacity(0.5f);

  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(2.0, 2.0, 1.0);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(2.5, 3.0);
  gfx::Transform parent_sublayer_matrix;
  parent_sublayer_matrix.Scale3d(10.f, 10.f, 3.3f);
  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor) * parent_translation_to_anchor *
      parent_sublayer_matrix * Inverse(parent_translation_to_anchor);
  gfx::Transform replica_layer_transform;
  replica_layer_transform.Scale3d(3.0, 3.0, 1.0);
  gfx::Vector2dF parent_composite_scale =
      MathUtil::ComputeTransform2dScaleComponents(parent_composite_transform,
                                                  1.f);
  gfx::Transform surface_sublayer_transform;
  surface_sublayer_transform.Scale(parent_composite_scale.x(),
                                   parent_composite_scale.y());
  gfx::Transform replica_composite_transform =
      parent_composite_transform * replica_layer_transform *
      Inverse(surface_sublayer_transform);

  // Child's render surface should not exist yet.
  ASSERT_FALSE(child->render_surface());

  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               parent_sublayer_matrix,
                               gfx::PointF(0.25f, 0.25f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(-0.5f, -0.5f),
                               gfx::Size(1, 1),
                               false);
  SetLayerPropertiesForTesting(child_replica.get(),
                               replica_layer_transform,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(),
                               false);
  ExecuteCalculateDrawProperties(root.get());

  // Render surface should have been created now.
  ASSERT_TRUE(child->render_surface());
  ASSERT_EQ(child, child->render_target());

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      replica_composite_transform,
      child->render_target()->render_surface()->replica_draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(replica_composite_transform,
                                  child->render_target()->render_surface()
                                      ->replica_screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForRenderSurfaceHierarchy) {
  // This test creates a more complex tree and verifies it all at once. This
  // covers the following cases:
  //   - layers that are described w.r.t. a render surface: should have draw
  //   transforms described w.r.t. that surface
  //   - A render surface described w.r.t. an ancestor render surface: should
  //   have a draw transform described w.r.t. that ancestor surface
  //   - Replicas of a render surface are described w.r.t. the replica's
  //   transform around its anchor, along with the surface itself.
  //   - Sanity check on recursion: verify transforms of layers described w.r.t.
  //   a render surface that is described w.r.t. an ancestor render surface.
  //   - verifying that each layer has a reference to the correct render surface
  //   and render target values.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<Layer> render_surface2 = Layer::Create();
  scoped_refptr<Layer> child_of_root = Layer::Create();
  scoped_refptr<Layer> child_of_rs1 = Layer::Create();
  scoped_refptr<Layer> child_of_rs2 = Layer::Create();
  scoped_refptr<Layer> replica_of_rs1 = Layer::Create();
  scoped_refptr<Layer> replica_of_rs2 = Layer::Create();
  scoped_refptr<Layer> grand_child_of_root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child_of_rs1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child_of_rs2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(parent);
  parent->AddChild(render_surface1);
  parent->AddChild(child_of_root);
  render_surface1->AddChild(child_of_rs1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child_of_rs2);
  child_of_root->AddChild(grand_child_of_root);
  child_of_rs1->AddChild(grand_child_of_rs1);
  child_of_rs2->AddChild(grand_child_of_rs2);
  render_surface1->SetReplicaLayer(replica_of_rs1.get());
  render_surface2->SetReplicaLayer(replica_of_rs2.get());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // In combination with descendant draws content, opacity != 1 forces the layer
  // to have a new render surface.
  render_surface1->SetOpacity(0.5f);
  render_surface2->SetOpacity(0.33f);

  // One-time setup of root layer
  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               false);

  // All layers in the tree are initialized with an anchor at .25 and a size of
  // (10,10).  matrix "A" is the composite layer transform used in all layers,
  // centered about the anchor point.  matrix "B" is the sublayer transform used
  // in all layers, centered about the center position of the layer.  matrix "R"
  // is the composite replica transform used in all replica layers.
  //
  // x component tests that layer_transform and sublayer_transform are done in
  // the right order (translation and scale are noncommutative).  y component
  // has a translation by 1 for every ancestor, which indicates the "depth" of
  // the layer in the hierarchy.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(2.5, 0.0);
  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);
  gfx::Transform sublayer_transform;
  sublayer_transform.Scale3d(10.0, 1.0, 1.0);
  gfx::Transform replica_layer_transform;
  replica_layer_transform.Scale3d(-2.0, 5.0, 1.0);

  gfx::Transform A =
      translation_to_anchor * layer_transform * Inverse(translation_to_anchor);
  gfx::Transform B = translation_to_anchor * sublayer_transform *
                     Inverse(translation_to_anchor);
  gfx::Transform R = A * translation_to_anchor * replica_layer_transform *
                     Inverse(translation_to_anchor);

  gfx::Vector2dF surface1_parent_transform_scale =
      MathUtil::ComputeTransform2dScaleComponents(A * B, 1.f);
  gfx::Transform surface1_sublayer_transform;
  surface1_sublayer_transform.Scale(surface1_parent_transform_scale.x(),
                                    surface1_parent_transform_scale.y());

  // SS1 = transform given to the subtree of render_surface1
  gfx::Transform SS1 = surface1_sublayer_transform;
  // S1 = transform to move from render_surface1 pixels to the layer space of
  // the owning layer
  gfx::Transform S1 = Inverse(surface1_sublayer_transform);

  gfx::Vector2dF surface2_parent_transform_scale =
      MathUtil::ComputeTransform2dScaleComponents(SS1 * A * B, 1.f);
  gfx::Transform surface2_sublayer_transform;
  surface2_sublayer_transform.Scale(surface2_parent_transform_scale.x(),
                                    surface2_parent_transform_scale.y());

  // SS2 = transform given to the subtree of render_surface2
  gfx::Transform SS2 = surface2_sublayer_transform;
  // S2 = transform to move from render_surface2 pixels to the layer space of
  // the owning layer
  gfx::Transform S2 = Inverse(surface2_sublayer_transform);

  SetLayerPropertiesForTesting(parent.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child_of_root.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child_of_rs1.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child_of_rs2.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child_of_root.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child_of_rs1.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child_of_rs2.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(replica_of_rs1.get(),
                               replica_layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(),
                               false);
  SetLayerPropertiesForTesting(replica_of_rs2.get(),
                               replica_layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(),
                               gfx::Size(),
                               false);

  ExecuteCalculateDrawProperties(root.get());

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(root->render_surface());
  ASSERT_FALSE(child_of_root->render_surface());
  ASSERT_FALSE(grand_child_of_root->render_surface());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_FALSE(child_of_rs1->render_surface());
  ASSERT_FALSE(grand_child_of_rs1->render_surface());

  ASSERT_TRUE(render_surface2->render_surface());
  ASSERT_FALSE(child_of_rs2->render_surface());
  ASSERT_FALSE(grand_child_of_rs2->render_surface());

  // Verify all render target accessors
  EXPECT_EQ(root, parent->render_target());
  EXPECT_EQ(root, child_of_root->render_target());
  EXPECT_EQ(root, grand_child_of_root->render_target());

  EXPECT_EQ(render_surface1, render_surface1->render_target());
  EXPECT_EQ(render_surface1, child_of_rs1->render_target());
  EXPECT_EQ(render_surface1, grand_child_of_rs1->render_target());

  EXPECT_EQ(render_surface2, render_surface2->render_target());
  EXPECT_EQ(render_surface2, child_of_rs2->render_target());
  EXPECT_EQ(render_surface2, grand_child_of_rs2->render_target());

  // Verify layer draw transforms note that draw transforms are described with
  // respect to the nearest ancestor render surface but screen space transforms
  // are described with respect to the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A, child_of_root->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A,
                                  grand_child_of_root->draw_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1, render_surface1->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1 * B * A, child_of_rs1->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1 * B * A * B * A,
                                  grand_child_of_rs1->draw_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2, render_surface2->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2 * B * A, child_of_rs2->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2 * B * A * B * A,
                                  grand_child_of_rs2->draw_transform());

  // Verify layer screen-space transforms
  //
  EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A,
                                  child_of_root->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * A * B * A, grand_child_of_root->screen_space_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A,
                                  render_surface1->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A,
                                  child_of_rs1->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * B * A,
                                  grand_child_of_rs1->screen_space_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A,
                                  render_surface2->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * B * A,
                                  child_of_rs2->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * B * A * B * A * B * A * B * A,
                                  grand_child_of_rs2->screen_space_transform());

  // Verify render surface transforms.
  //
  // Draw transform of render surface 1 is described with respect to root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * A * S1, render_surface1->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * R * S1,
      render_surface1->render_surface()->replica_draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * A * S1,
      render_surface1->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * R * S1,
      render_surface1->render_surface()->replica_screen_space_transform());
  // Draw transform of render surface 2 is described with respect to render
  // surface 1.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      SS1 * B * A * S2, render_surface2->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      SS1 * B * R * S2,
      render_surface2->render_surface()->replica_draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * A * B * A * S2,
      render_surface2->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * B * A * B * R * S2,
      render_surface2->render_surface()->replica_screen_space_transform());

  // Sanity check. If these fail there is probably a bug in the test itself.  It
  // is expected that we correctly set up transforms so that the y-component of
  // the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, parent->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(2.0,
                  child_of_root->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      3.0, grand_child_of_root->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(2.0,
                  render_surface1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(3.0,
                  child_of_rs1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      4.0, grand_child_of_rs1->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(3.0,
                  render_surface2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(4.0,
                  child_of_rs2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      5.0, grand_child_of_rs2->screen_space_transform().matrix().get(1, 3));
}

TEST_F(LayerTreeHostCommonTest, TransformsForFlatteningLayer) {
  // For layers that flatten their subtree, there should be an orthographic
  // projection (for x and y values) in the middle of the transform sequence.
  // Note that the way the code is currently implemented, it is not expected to
  // use a canonical orthographic projection.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  gfx::Transform rotation_about_y_axis;
  rotation_about_y_axis.RotateAboutYAxis(30.0);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               rotation_about_y_axis,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               rotation_about_y_axis,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  root->AddChild(child);
  child->AddChild(grand_child);
  child->SetForceRenderSurface(true);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // No layers in this test should preserve 3d.
  ASSERT_FALSE(root->preserves_3d());
  ASSERT_FALSE(child->preserves_3d());
  ASSERT_FALSE(grand_child->preserves_3d());

  gfx::Transform expected_child_draw_transform = rotation_about_y_axis;
  gfx::Transform expected_child_screen_space_transform = rotation_about_y_axis;
  gfx::Transform expected_grand_child_draw_transform =
      rotation_about_y_axis;  // draws onto child's render surface
  gfx::Transform flattened_rotation_about_y = rotation_about_y_axis;
  flattened_rotation_about_y.FlattenTo2d();
  gfx::Transform expected_grand_child_screen_space_transform =
      flattened_rotation_about_y * rotation_about_y_axis;

  ExecuteCalculateDrawProperties(root.get());

  // The child's draw transform should have been taken by its surface.
  ASSERT_TRUE(child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_draw_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_child_screen_space_transform,
      child->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_screen_space_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_draw_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_screen_space_transform,
                                  grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForDegenerateIntermediateLayer) {
  // A layer that is empty in one axis, but not the other, was accidentally
  // skipping a necessary translation.  Without that translation, the coordinate
  // space of the layer's draw transform is incorrect.
  //
  // Normally this isn't a problem, because the layer wouldn't be drawn anyway,
  // but if that layer becomes a render surface, then its draw transform is
  // implicitly inherited by the rest of the subtree, which then is positioned
  // incorrectly as a result.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  // The child height is zero, but has non-zero width that should be accounted
  // for while computing draw transforms.
  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 0),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  root->AddChild(child);
  child->AddChild(grand_child);
  child->SetForceRenderSurface(true);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(child->render_surface());
  // This is the real test, the rest are sanity checks.
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformAboveRootLayer) {
  // Transformations applied at the root of the tree should be forwarded
  // to child layers instead of applied to the root RenderSurface.
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  child->SetScrollable(true);
  root->AddChild(child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);

  gfx::Transform translate;
  translate.Translate(50, 50);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), translate, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(translate, root->draw_properties().target_space_transform);
    EXPECT_EQ(translate, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  gfx::Transform scale;
  scale.Scale(2, 2);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), scale, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(scale, root->draw_properties().target_space_transform);
    EXPECT_EQ(scale, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  gfx::Transform rotate;
  rotate.Rotate(2);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), rotate, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(rotate, root->draw_properties().target_space_transform);
    EXPECT_EQ(rotate, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  gfx::Transform composite;
  composite.ConcatTransform(translate);
  composite.ConcatTransform(scale);
  composite.ConcatTransform(rotate);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), composite, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(composite, root->draw_properties().target_space_transform);
    EXPECT_EQ(composite, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  // Verify it composes correctly with device scale.
  float device_scale_factor = 1.5f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), translate, &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    gfx::Transform device_scaled_translate = translate;
    device_scaled_translate.Scale(device_scale_factor, device_scale_factor);
    EXPECT_EQ(device_scaled_translate,
              root->draw_properties().target_space_transform);
    EXPECT_EQ(device_scaled_translate,
              child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  // Verify it composes correctly with page scale.
  float page_scale_factor = 2.f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), translate, &render_surface_layer_list);
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    gfx::Transform page_scaled_translate = translate;
    page_scaled_translate.Scale(page_scale_factor, page_scale_factor);
    EXPECT_EQ(translate, root->draw_properties().target_space_transform);
    EXPECT_EQ(page_scaled_translate,
              child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  // Verify that it composes correctly with transforms directly on root layer.
  root->SetTransform(composite);
  root->SetSublayerTransform(composite);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), composite, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    gfx::Transform compositeSquared = composite;
    compositeSquared.ConcatTransform(composite);
    gfx::Transform compositeCubed = compositeSquared;
    compositeCubed.ConcatTransform(composite);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        compositeSquared, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        compositeCubed, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceListForRenderSurfaceWithClippedLayer) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);


  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(30.f, 30.f),
                               gfx::Size(10, 10),
                               false);

  parent->AddChild(render_surface1);
  parent->SetMasksToBounds(true);
  render_surface1->AddChild(child);
  render_surface1->SetForceRenderSurface(true);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(),
      parent->bounds(),
      gfx::Transform(),
      &render_surface_layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // The child layer's content is entirely outside the parent's clip rect, so
  // the intermediate render surface should not be listed here, even if it was
  // forced to be created. Render surfaces without children or visible content
  // are unexpected at draw time (e.g. we might try to create a content texture
  // of size 0).
  ASSERT_TRUE(parent->render_surface());
  ASSERT_FALSE(render_surface1->render_surface());
  EXPECT_EQ(1U, render_surface_layer_list.size());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceListForTransparentChild) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  parent->AddChild(render_surface1);
  render_surface1->AddChild(child);
  render_surface1->SetForceRenderSurface(true);
  render_surface1->SetOpacity(0.f);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Since the layer is transparent, render_surface1->render_surface() should
  // not have gotten added anywhere.  Also, the drawable content rect should not
  // have been extended by the children.
  ASSERT_TRUE(parent->render_surface());
  EXPECT_EQ(0U, parent->render_surface()->layer_list().size());
  EXPECT_EQ(1U, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(gfx::Rect(), parent->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest, ForceRenderSurface) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  render_surface1->SetForceRenderSurface(true);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  parent->AddChild(render_surface1);
  render_surface1->AddChild(child);

  // Sanity check before the actual test
  EXPECT_FALSE(parent->render_surface());
  EXPECT_FALSE(render_surface1->render_surface());

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    // The root layer always creates a render surface
    EXPECT_TRUE(parent->render_surface());
    EXPECT_TRUE(render_surface1->render_surface());
    EXPECT_EQ(2U, render_surface_layer_list.size());
  }

  {
    RenderSurfaceLayerList render_surface_layer_list;
    render_surface1->SetForceRenderSurface(false);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_TRUE(parent->render_surface());
    EXPECT_FALSE(render_surface1->render_surface());
    EXPECT_EQ(1U, render_surface_layer_list.size());
  }
}

TEST_F(LayerTreeHostCommonTest, ClipRectCullsRenderSurfaces) {
  // The entire subtree of layers that are outside the clip rect should be
  // culled away, and should not affect the render_surface_layer_list.
  //
  // The test tree is set up as follows:
  //  - all layers except the leaf_nodes are forced to be a new render surface
  //  that have something to draw.
  //  - parent is a large container layer.
  //  - child has masksToBounds=true to cause clipping.
  //  - grand_child is positioned outside of the child's bounds
  //  - great_grand_child is also kept outside child's bounds.
  //
  // In this configuration, grand_child and great_grand_child are completely
  // outside the clip rect, and they should never get scheduled on the list of
  // render surfaces.
  //

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<Layer> great_grand_child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  parent->AddChild(child);
  child->AddChild(grand_child);
  grand_child->AddChild(great_grand_child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // leaf_node1 ensures that parent and child are kept on the
  // render_surface_layer_list, even though grand_child and great_grand_child
  // should be clipped.
  child->AddChild(leaf_node1);
  great_grand_child->AddChild(leaf_node2);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(45.f, 45.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(great_grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(leaf_node1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               false);
  SetLayerPropertiesForTesting(leaf_node2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);

  child->SetMasksToBounds(true);
  child->SetOpacity(0.4f);
  child->SetForceRenderSurface(true);
  grand_child->SetOpacity(0.5f);
  great_grand_child->SetOpacity(0.4f);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  ASSERT_EQ(2U, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(child->id(), render_surface_layer_list.at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, ClipRectCullsSurfaceWithoutVisibleContent) {
  // When a render surface has a clip rect, it is used to clip the content rect
  // of the surface. When the render surface is animating its transforms, then
  // the content rect's position in the clip rect is not defined on the main
  // thread, and its content rect should not be clipped.

  // The test tree is set up as follows:
  //  - parent is a container layer that masksToBounds=true to cause clipping.
  //  - child is a render surface, which has a clip rect set to the bounds of
  //  the parent.
  //  - grand_child is a render surface, and the only visible content in child.
  //  It is positioned outside of the clip rect from parent.

  // In this configuration, grand_child should be outside the clipped
  // content rect of the child, making grand_child not appear in the
  // render_surface_layer_list. However, when we place an animation on the
  // child, this clipping should be avoided and we should keep the grand_child
  // in the render_surface_layer_list.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  parent->AddChild(child);
  child->AddChild(grand_child);
  grand_child->AddChild(leaf_node);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(200.f, 200.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(leaf_node.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  parent->SetMasksToBounds(true);
  child->SetOpacity(0.4f);
  child->SetForceRenderSurface(true);
  grand_child->SetOpacity(0.4f);
  grand_child->SetForceRenderSurface(true);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    // Without an animation, we should cull child and grand_child from the
    // render_surface_layer_list.
    ASSERT_EQ(1U, render_surface_layer_list.size());
    EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  }

  // Now put an animating transform on child.
  AddAnimatedTransformToController(
      child->layer_animation_controller(), 10.0, 30, 0);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    // With an animating transform, we should keep child and grand_child in the
    // render_surface_layer_list.
    ASSERT_EQ(3U, render_surface_layer_list.size());
    EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
    EXPECT_EQ(child->id(), render_surface_layer_list.at(1)->id());
    EXPECT_EQ(grand_child->id(), render_surface_layer_list.at(2)->id());
  }
}

TEST_F(LayerTreeHostCommonTest, IsClippedIsSetCorrectly) {
  // Layer's IsClipped() property is set to true when:
  //  - the layer clips its subtree, e.g. masks to bounds,
  //  - the layer is clipped by an ancestor that contributes to the same
  //    render target,
  //  - a surface is clipped by an ancestor that contributes to the same
  //    render target.
  //
  // In particular, for a layer that owns a render surface:
  //  - the render surface inherits any clip from ancestors, and does NOT
  //    pass that clipped status to the layer itself.
  //  - but if the layer itself masks to bounds, it is considered clipped
  //    and propagates the clip to the subtree.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child1 = Layer::Create();
  scoped_refptr<Layer> child2 = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(parent);
  parent->AddChild(child1);
  parent->AddChild(child2);
  child1->AddChild(grand_child);
  child2->AddChild(leaf_node2);
  grand_child->AddChild(leaf_node1);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  child2->SetForceRenderSurface(true);

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(leaf_node1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(leaf_node2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);

  // Case 1: nothing is clipped except the root render surface.
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    ASSERT_TRUE(root->render_surface());
    ASSERT_TRUE(child2->render_surface());

    EXPECT_FALSE(root->is_clipped());
    EXPECT_TRUE(root->render_surface()->is_clipped());
    EXPECT_FALSE(parent->is_clipped());
    EXPECT_FALSE(child1->is_clipped());
    EXPECT_FALSE(child2->is_clipped());
    EXPECT_FALSE(child2->render_surface()->is_clipped());
    EXPECT_FALSE(grand_child->is_clipped());
    EXPECT_FALSE(leaf_node1->is_clipped());
    EXPECT_FALSE(leaf_node2->is_clipped());
  }

  // Case 2: parent masksToBounds, so the parent, child1, and child2's
  // surface are clipped. But layers that contribute to child2's surface are
  // not clipped explicitly because child2's surface already accounts for
  // that clip.
  {
    RenderSurfaceLayerList render_surface_layer_list;
    parent->SetMasksToBounds(true);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    ASSERT_TRUE(root->render_surface());
    ASSERT_TRUE(child2->render_surface());

    EXPECT_FALSE(root->is_clipped());
    EXPECT_TRUE(root->render_surface()->is_clipped());
    EXPECT_TRUE(parent->is_clipped());
    EXPECT_TRUE(child1->is_clipped());
    EXPECT_FALSE(child2->is_clipped());
    EXPECT_TRUE(child2->render_surface()->is_clipped());
    EXPECT_TRUE(grand_child->is_clipped());
    EXPECT_TRUE(leaf_node1->is_clipped());
    EXPECT_FALSE(leaf_node2->is_clipped());
  }

  // Case 3: child2 masksToBounds. The layer and subtree are clipped, and
  // child2's render surface is not clipped.
  {
    RenderSurfaceLayerList render_surface_layer_list;
    parent->SetMasksToBounds(false);
    child2->SetMasksToBounds(true);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    ASSERT_TRUE(root->render_surface());
    ASSERT_TRUE(child2->render_surface());

    EXPECT_FALSE(root->is_clipped());
    EXPECT_TRUE(root->render_surface()->is_clipped());
    EXPECT_FALSE(parent->is_clipped());
    EXPECT_FALSE(child1->is_clipped());
    EXPECT_TRUE(child2->is_clipped());
    EXPECT_FALSE(child2->render_surface()->is_clipped());
    EXPECT_FALSE(grand_child->is_clipped());
    EXPECT_FALSE(leaf_node1->is_clipped());
    EXPECT_TRUE(leaf_node2->is_clipped());
  }
}

TEST_F(LayerTreeHostCommonTest, DrawableContentRectForLayers) {
  // Verify that layers get the appropriate DrawableContentRect when their
  // parent masksToBounds is true.
  //
  //   grand_child1 - completely inside the region; DrawableContentRect should
  //   be the layer rect expressed in target space.
  //   grand_child2 - partially clipped but NOT masksToBounds; the clip rect
  //   will be the intersection of layer bounds and the mask region.
  //   grand_child3 - partially clipped and masksToBounds; the
  //   DrawableContentRect will still be the intersection of layer bounds and
  //   the mask region.
  //   grand_child4 - outside parent's clip rect; the DrawableContentRect should
  //   be empty.
  //

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child1 = Layer::Create();
  scoped_refptr<Layer> grand_child2 = Layer::Create();
  scoped_refptr<Layer> grand_child3 = Layer::Create();
  scoped_refptr<Layer> grand_child4 = Layer::Create();

  parent->AddChild(child);
  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  child->AddChild(grand_child3);
  child->AddChild(grand_child4);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(grand_child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child4.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(45.f, 45.f),
                               gfx::Size(10, 10),
                               false);

  child->SetMasksToBounds(true);
  grand_child3->SetMasksToBounds(true);

  // Force everyone to be a render surface.
  child->SetOpacity(0.4f);
  grand_child1->SetOpacity(0.5f);
  grand_child2->SetOpacity(0.5f);
  grand_child3->SetOpacity(0.5f);
  grand_child4->SetOpacity(0.5f);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_RECT_EQ(gfx::Rect(5, 5, 10, 10),
                 grand_child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(15, 15, 5, 5),
                 grand_child3->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(15, 15, 5, 5),
                 grand_child3->drawable_content_rect());
  EXPECT_TRUE(grand_child4->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest, ClipRectIsPropagatedCorrectlyToSurfaces) {
  // Verify that render surfaces (and their layers) get the appropriate
  // clip rects when their parent masksToBounds is true.
  //
  // Layers that own render surfaces (at least for now) do not inherit any
  // clipping; instead the surface will enforce the clip for the entire subtree.
  // They may still have a clip rect of their own layer bounds, however, if
  // masksToBounds was true.
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child1 = Layer::Create();
  scoped_refptr<Layer> grand_child2 = Layer::Create();
  scoped_refptr<Layer> grand_child3 = Layer::Create();
  scoped_refptr<Layer> grand_child4 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node4 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  parent->AddChild(child);
  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  child->AddChild(grand_child3);
  child->AddChild(grand_child4);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // the leaf nodes ensure that these grand_children become render surfaces for
  // this test.
  grand_child1->AddChild(leaf_node1);
  grand_child2->AddChild(leaf_node2);
  grand_child3->AddChild(leaf_node3);
  grand_child4->AddChild(leaf_node4);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(grand_child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child4.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(45.f, 45.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(leaf_node1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(leaf_node2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(leaf_node3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(leaf_node4.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  child->SetMasksToBounds(true);
  grand_child3->SetMasksToBounds(true);
  grand_child4->SetMasksToBounds(true);

  // Force everyone to be a render surface.
  child->SetOpacity(0.4f);
  child->SetForceRenderSurface(true);
  grand_child1->SetOpacity(0.5f);
  grand_child1->SetForceRenderSurface(true);
  grand_child2->SetOpacity(0.5f);
  grand_child2->SetForceRenderSurface(true);
  grand_child3->SetOpacity(0.5f);
  grand_child3->SetForceRenderSurface(true);
  grand_child4->SetOpacity(0.5f);
  grand_child4->SetForceRenderSurface(true);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  ASSERT_TRUE(grand_child1->render_surface());
  ASSERT_TRUE(grand_child2->render_surface());
  ASSERT_TRUE(grand_child3->render_surface());
  // Because grand_child4 is entirely clipped, it is expected to not have a
  // render surface.
  EXPECT_FALSE(grand_child4->render_surface());

  // Surfaces are clipped by their parent, but un-affected by the owning layer's
  // masksToBounds.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 20, 20),
                 grand_child1->render_surface()->clip_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 20, 20),
                 grand_child2->render_surface()->clip_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 20, 20),
                 grand_child3->render_surface()->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, AnimationsForRenderSurfaceHierarchy) {
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<Layer> render_surface2 = Layer::Create();
  scoped_refptr<Layer> child_of_root = Layer::Create();
  scoped_refptr<Layer> child_of_rs1 = Layer::Create();
  scoped_refptr<Layer> child_of_rs2 = Layer::Create();
  scoped_refptr<Layer> grand_child_of_root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child_of_rs1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child_of_rs2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  parent->AddChild(render_surface1);
  parent->AddChild(child_of_root);
  render_surface1->AddChild(child_of_rs1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child_of_rs2);
  child_of_root->AddChild(grand_child_of_root);
  child_of_rs1->AddChild(grand_child_of_rs1);
  child_of_rs2->AddChild(grand_child_of_rs2);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // Make our render surfaces.
  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);
  gfx::Transform sublayer_transform;
  sublayer_transform.Scale3d(10.0, 1.0, 1.0);

  SetLayerPropertiesForTesting(parent.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child_of_root.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child_of_rs1.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(child_of_rs2.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child_of_root.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child_of_rs1.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child_of_rs2.get(),
                               layer_transform,
                               sublayer_transform,
                               gfx::PointF(0.25f, 0.f),
                               gfx::PointF(2.5f, 0.f),
                               gfx::Size(10, 10),
                               false);

  // Put an animated opacity on the render surface.
  AddOpacityTransitionToController(
      render_surface1->layer_animation_controller(), 10.0, 1.f, 0.f, false);

  // Also put an animated opacity on a layer without descendants.
  AddOpacityTransitionToController(
      grand_child_of_root->layer_animation_controller(), 10.0, 1.f, 0.f, false);

  // Put a transform animation on the render surface.
  AddAnimatedTransformToController(
      render_surface2->layer_animation_controller(), 10.0, 30, 0);

  // Also put transform animations on grand_child_of_root, and
  // grand_child_of_rs2
  AddAnimatedTransformToController(
      grand_child_of_root->layer_animation_controller(), 10.0, 30, 0);
  AddAnimatedTransformToController(
      grand_child_of_rs2->layer_animation_controller(), 10.0, 30, 0);

  ExecuteCalculateDrawProperties(parent.get());

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(parent->render_surface());
  ASSERT_FALSE(child_of_root->render_surface());
  ASSERT_FALSE(grand_child_of_root->render_surface());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_FALSE(child_of_rs1->render_surface());
  ASSERT_FALSE(grand_child_of_rs1->render_surface());

  ASSERT_TRUE(render_surface2->render_surface());
  ASSERT_FALSE(child_of_rs2->render_surface());
  ASSERT_FALSE(grand_child_of_rs2->render_surface());

  // Verify all render target accessors
  EXPECT_EQ(parent, parent->render_target());
  EXPECT_EQ(parent, child_of_root->render_target());
  EXPECT_EQ(parent, grand_child_of_root->render_target());

  EXPECT_EQ(render_surface1, render_surface1->render_target());
  EXPECT_EQ(render_surface1, child_of_rs1->render_target());
  EXPECT_EQ(render_surface1, grand_child_of_rs1->render_target());

  EXPECT_EQ(render_surface2, render_surface2->render_target());
  EXPECT_EQ(render_surface2, child_of_rs2->render_target());
  EXPECT_EQ(render_surface2, grand_child_of_rs2->render_target());

  // Verify draw_opacity_is_animating values
  EXPECT_FALSE(parent->draw_opacity_is_animating());
  EXPECT_FALSE(child_of_root->draw_opacity_is_animating());
  EXPECT_TRUE(grand_child_of_root->draw_opacity_is_animating());
  EXPECT_FALSE(render_surface1->draw_opacity_is_animating());
  EXPECT_TRUE(render_surface1->render_surface()->draw_opacity_is_animating());
  EXPECT_FALSE(child_of_rs1->draw_opacity_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->draw_opacity_is_animating());
  EXPECT_FALSE(render_surface2->draw_opacity_is_animating());
  EXPECT_FALSE(render_surface2->render_surface()->draw_opacity_is_animating());
  EXPECT_FALSE(child_of_rs2->draw_opacity_is_animating());
  EXPECT_FALSE(grand_child_of_rs2->draw_opacity_is_animating());

  // Verify draw_transform_is_animating values
  EXPECT_FALSE(parent->draw_transform_is_animating());
  EXPECT_FALSE(child_of_root->draw_transform_is_animating());
  EXPECT_TRUE(grand_child_of_root->draw_transform_is_animating());
  EXPECT_FALSE(render_surface1->draw_transform_is_animating());
  EXPECT_FALSE(render_surface1->render_surface()
                   ->target_surface_transforms_are_animating());
  EXPECT_FALSE(child_of_rs1->draw_transform_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->draw_transform_is_animating());
  EXPECT_FALSE(render_surface2->draw_transform_is_animating());
  EXPECT_TRUE(render_surface2->render_surface()
                  ->target_surface_transforms_are_animating());
  EXPECT_FALSE(child_of_rs2->draw_transform_is_animating());
  EXPECT_TRUE(grand_child_of_rs2->draw_transform_is_animating());

  // Verify screen_space_transform_is_animating values
  EXPECT_FALSE(parent->screen_space_transform_is_animating());
  EXPECT_FALSE(child_of_root->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_root->screen_space_transform_is_animating());
  EXPECT_FALSE(render_surface1->screen_space_transform_is_animating());
  EXPECT_FALSE(render_surface1->render_surface()
                   ->screen_space_transforms_are_animating());
  EXPECT_FALSE(child_of_rs1->screen_space_transform_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->screen_space_transform_is_animating());
  EXPECT_TRUE(render_surface2->screen_space_transform_is_animating());
  EXPECT_TRUE(render_surface2->render_surface()
                  ->screen_space_transforms_are_animating());
  EXPECT_TRUE(child_of_rs2->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_rs2->screen_space_transform_is_animating());

  // Sanity check. If these fail there is probably a bug in the test itself.
  // It is expected that we correctly set up transforms so that the y-component
  // of the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, parent->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(2.0,
                  child_of_root->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      3.0, grand_child_of_root->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(2.0,
                  render_surface1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(3.0,
                  child_of_rs1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      4.0, grand_child_of_rs1->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(3.0,
                  render_surface2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(4.0,
                  child_of_rs2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      5.0, grand_child_of_rs2->screen_space_transform().matrix().get(1, 3));
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForIdentityTransform) {
  // Test the calculateVisibleRect() function works correctly for identity
  // transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  gfx::Rect layer_content_rect = gfx::Rect(10, 10, 30, 30);
  gfx::Rect expected = gfx::Rect(10, 10, 30, 30);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);

  // Case 2: Layer is outside the surface rect.
  layer_content_rect = gfx::Rect(120, 120, 30, 30);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_TRUE(actual.IsEmpty());

  // Case 3: Layer is partially overlapping the surface rect.
  layer_content_rect = gfx::Rect(80, 80, 30, 30);
  expected = gfx::Rect(80, 80, 20, 20);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForTranslations) {
  // Test the calculateVisibleRect() function works correctly for scaling
  // transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 30, 30);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(10.0, 10.0);
  gfx::Rect expected = gfx::Rect(0, 0, 30, 30);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);

  // Case 2: Layer is outside the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(120.0, 120.0);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_TRUE(actual.IsEmpty());

  // Case 3: Layer is partially overlapping the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(80.0, 80.0);
  expected = gfx::Rect(0, 0, 20, 20);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor2DRotations) {
  // Test the calculateVisibleRect() function works correctly for rotations
  // about z-axis (i.e. 2D rotations).  Remember that calculateVisibleRect()
  // should return the g in the layer's space.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 30, 30);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.Rotate(45.0);
  gfx::Rect expected = gfx::Rect(0, 0, 30, 30);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);

  // Case 2: Layer is outside the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-50.0, 0.0);
  layer_to_surface_transform.Rotate(45.0);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_TRUE(actual.IsEmpty());

  // Case 3: The layer is rotated about its top-left corner. In surface space,
  // the layer is oriented diagonally, with the left half outside of the render
  // surface. In this case, the g should still be the entire layer
  // (remember the g is computed in layer space); both the top-left
  // and bottom-right corners of the layer are still visible.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Rotate(45.0);
  expected = gfx::Rect(0, 0, 30, 30);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);

  // Case 4: The layer is rotated about its top-left corner, and translated
  // upwards. In surface space, the layer is oriented diagonally, with only the
  // top corner of the surface overlapping the layer. In layer space, the render
  // surface overlaps the right side of the layer. The g should be
  // the layer's right half.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(0.0, -sqrt(2.0) * 15.0);
  layer_to_surface_transform.Rotate(45.0);
  expected = gfx::Rect(15, 0, 15, 30);  // Right half of layer bounds.
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor3dOrthographicTransform) {
  // Test that the calculateVisibleRect() function works correctly for 3d
  // transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, should be fully contained in the render surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  gfx::Rect expected = gfx::Rect(0, 0, 100, 100);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);

  // Case 2: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, but shifted to the side so only the right-half the layer would be
  // visible on the surface.
  // 100 is the un-rotated layer width; divided by sqrt(2) is the rotated width.
  SkMScalar half_width_of_rotated_layer =
      SkDoubleToMScalar((100.0 / sqrt(2.0)) * 0.5);
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-half_width_of_rotated_layer, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);  // Rotates about the left
                                                      // edge of the layer.
  expected = gfx::Rect(50, 0, 50, 100);  // Tight half of the layer.
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor3dPerspectiveTransform) {
  // Test the calculateVisibleRect() function works correctly when the layer has
  // a perspective projection onto the target surface.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-50, -50, 200, 200);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Even though the layer is twice as large as the surface, due to
  // perspective foreshortening, the layer will fit fully in the surface when
  // its translated more than the perspective amount.
  layer_to_surface_transform.MakeIdentity();

  // The following sequence of transforms applies the perspective about the
  // center of the surface.
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.ApplyPerspectiveDepth(9.0);
  layer_to_surface_transform.Translate(-50.0, -50.0);

  // This translate places the layer in front of the surface's projection plane.
  layer_to_surface_transform.Translate3d(0.0, 0.0, -27.0);

  gfx::Rect expected = gfx::Rect(-50, -50, 200, 200);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);

  // Case 2: same projection as before, except that the layer is also translated
  // to the side, so that only the right half of the layer should be visible.
  //
  // Explanation of expected result: The perspective ratio is (z distance
  // between layer and camera origin) / (z distance between projection plane and
  // camera origin) == ((-27 - 9) / 9) Then, by similar triangles, if we want to
  // move a layer by translating -50 units in projected surface units (so that
  // only half of it is visible), then we would need to translate by (-36 / 9) *
  // -50 == -200 in the layer's units.
  layer_to_surface_transform.Translate3d(-200.0, 0.0, 0.0);
  expected = gfx::Rect(gfx::Point(50, -50),
                       gfx::Size(100, 200));  // The right half of the layer's
                                              // bounding rect.
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest,
       VisibleRectFor3dOrthographicIsNotClippedBehindSurface) {
  // There is currently no explicit concept of an orthographic projection plane
  // in our code (nor in the CSS spec to my knowledge). Therefore, layers that
  // are technically behind the surface in an orthographic world should not be
  // clipped when they are flattened to the surface.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // This sequence of transforms effectively rotates the layer about the y-axis
  // at the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.Translate(-50.0, 0.0);

  gfx::Rect expected = gfx::Rect(0, 0, 100, 100);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor3dPerspectiveWhenClippedByW) {
  // Test the calculateVisibleRect() function works correctly when projecting a
  // surface onto a layer, but the layer is partially behind the camera (not
  // just behind the projection plane). In this case, the cartesian coordinates
  // may seem to be valid, but actually they are not. The visible rect needs to
  // be properly clipped by the w = 0 plane in homogeneous coordinates before
  // converting to cartesian coordinates.

  gfx::Rect target_surface_rect = gfx::Rect(-50, -50, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-10, -1, 20, 2);
  gfx::Transform layer_to_surface_transform;

  // The layer is positioned so that the right half of the layer should be in
  // front of the camera, while the other half is behind the surface's
  // projection plane. The following sequence of transforms applies the
  // perspective and rotation about the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(-2.0, 0.0, 1.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);

  // Sanity check that this transform does indeed cause w < 0 when applying the
  // transform, otherwise this code is not testing the intended scenario.
  bool clipped;
  MathUtil::MapQuad(layer_to_surface_transform,
                    gfx::QuadF(gfx::RectF(layer_content_rect)),
                    &clipped);
  ASSERT_TRUE(clipped);

  int expected_x_position = 0;
  int expected_width = 10;
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected_x_position, actual.x());
  EXPECT_EQ(expected_width, actual.width());
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForPerspectiveUnprojection) {
  // To determine visible rect in layer space, there needs to be an
  // un-projection from surface space to layer space. When the original
  // transform was a perspective projection that was clipped, it returns a rect
  // that encloses the clipped bounds.  Un-projecting this new rect may require
  // clipping again.

  // This sequence of transforms causes one corner of the layer to protrude
  // across the w = 0 plane, and should be clipped.
  gfx::Rect target_surface_rect = gfx::Rect(-50, -50, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-10, -10, 20, 20);
  gfx::Transform layer_to_surface_transform;
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(0.0, 0.0, -5.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.RotateAboutXAxis(80.0);

  // Sanity check that un-projection does indeed cause w < 0, otherwise this
  // code is not testing the intended scenario.
  bool clipped;
  gfx::RectF clipped_rect =
      MathUtil::MapClippedRect(layer_to_surface_transform, layer_content_rect);
  MathUtil::ProjectQuad(
      Inverse(layer_to_surface_transform), gfx::QuadF(clipped_rect), &clipped);
  ASSERT_TRUE(clipped);

  // Only the corner of the layer is not visible on the surface because of being
  // clipped. But, the net result of rounding visible region to an axis-aligned
  // rect is that the entire layer should still be considered visible.
  gfx::Rect expected = gfx::Rect(-10, -10, 20, 20);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_RECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, DrawableAndVisibleContentRectsForSimpleLayers) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(child1);
  root->AddChild(child2);
  root->AddChild(child3);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible_content_rects.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());

  // layer visible_content_rects are clipped by their target surface.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_content_rect());
  EXPECT_TRUE(child3->visible_content_rect().IsEmpty());

  // layer drawable_content_rects are not clipped.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersClippedByLayer) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(child);
  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  child->AddChild(grand_child3);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(grand_child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(grand_child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(grand_child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               false);

  child->SetMasksToBounds(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_FALSE(child->render_surface());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), child->visible_content_rect());

  // All grandchild visible content rects should be clipped by child.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), grand_child1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 25, 25), grand_child2->visible_content_rect());
  EXPECT_TRUE(grand_child3->visible_content_rect().IsEmpty());

  // All grandchild DrawableContentRects should also be clipped by child.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 50, 50),
                 grand_child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(75, 75, 25, 25),
                 grand_child2->drawable_content_rect());
  EXPECT_TRUE(grand_child3->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersInUnclippedRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);
  render_surface1->AddChild(child2);
  render_surface1->AddChild(child3);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               false);

  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0),
                 render_surface1->visible_content_rect());

  // An unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 170, 170),
                 render_surface1->render_surface()->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child2->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child3->visible_content_rect());

  EXPECT_RECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersWithUninvertibleTransform) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // Case 1: a truly degenerate matrix
  gfx::Transform identity_matrix;
  gfx::Transform uninvertible_matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               uninvertible_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(child->visible_content_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());

  // Case 2: a matrix with flattened z, technically uninvertible but still
  // drawable and visible. In this case, we must assume that the entire layer
  // bounds are visible since there is no way to inverse-project the surface
  // bounds to intersect.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.matrix().set(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(child.get(),
                               uninvertible_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 50, 50), child->drawable_content_rect());

  // Case 3: a matrix with flattened z, technically uninvertible but still
  // drawable, but not visible. In this case, we don't need to conservatively
  // assume that the whole layer is visible.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.Translate(500.0, 0.0);
  uninvertible_matrix.matrix().set(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(child.get(),
                               uninvertible_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(child->visible_content_rect().IsEmpty());
  EXPECT_RECT_EQ(gfx::Rect(505, 5, 50, 50), child->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersInClippedRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);
  render_surface1->AddChild(child2);
  render_surface1->AddChild(child3);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               false);

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0),
                 render_surface1->visible_content_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 95, 95),
                 render_surface1->render_surface()->DrawableContentRect());

  // All layers that draw content into the surface have their visible content
  // rect clipped by the surface clip rect.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_content_rect());
  EXPECT_TRUE(child3->visible_content_rect().IsEmpty());

  // But the DrawableContentRects are unclipped.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForSurfaceHierarchy) {
  // Check that clipping does not propagate down surfaces.
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<Layer> render_surface2 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(render_surface1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child1);
  render_surface2->AddChild(child2);
  render_surface2->AddChild(child3);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(7, 13),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               false);

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_TRUE(render_surface2->render_surface());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0),
                 render_surface1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0),
                 render_surface2->visible_content_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 95, 95),
                 render_surface1->render_surface()->DrawableContentRect());

  // render_surface1 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1's content rect. So,
  // render_surface2 grows to enclose all drawable content of its subtree.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 170, 170),
                 render_surface2->render_surface()->DrawableContentRect());

  // All layers that draw content into render_surface2 think they are unclipped.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child2->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child3->visible_content_rect());

  // DrawableContentRects are also unclipped.
  EXPECT_RECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsWithTransformOnUnclippedSurface) {
  // Layers that have non-axis aligned bounds (due to transforms) have an
  // expanded, axis-aligned DrawableContentRect and visible content rect.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               child_rotation,
                               identity_matrix,
                               gfx::PointF(0.5f, 0.5f),
                               gfx::PointF(25.f, 25.f),
                               gfx::Size(50, 50),
                               false);

  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0),
                 render_surface1->visible_content_rect());

  // The unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect expected_surface_drawable_content =
      gfx::Rect(50 - diagonal_radius,
                50 - diagonal_radius,
                diagonal_radius * 2,
                diagonal_radius * 2);
  EXPECT_RECT_EQ(expected_surface_drawable_content,
                 render_surface1->render_surface()->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_content_rect());
  EXPECT_RECT_EQ(expected_surface_drawable_content,
                 child1->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsWithTransformOnClippedSurface) {
  // Layers that have non-axis aligned bounds (due to transforms) have an
  // expanded, axis-aligned DrawableContentRect and visible content rect.

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               child_rotation,
                               identity_matrix,
                               gfx::PointF(0.5f, 0.5f),
                               gfx::PointF(25.f, 25.f),
                               gfx::Size(50, 50),
                               false);

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  // The clipped surface clamps the DrawableContentRect that encloses the
  // rotated layer.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect unclipped_surface_content = gfx::Rect(50 - diagonal_radius,
                                                  50 - diagonal_radius,
                                                  diagonal_radius * 2,
                                                  diagonal_radius * 2);
  gfx::Rect expected_surface_drawable_content =
      gfx::IntersectRects(unclipped_surface_content, gfx::Rect(0, 0, 50, 50));
  EXPECT_RECT_EQ(expected_surface_drawable_content,
                 render_surface1->render_surface()->DrawableContentRect());

  // On the clipped surface, only a quarter  of the child1 is visible, but when
  // rotating it back to  child1's content space, the actual enclosing rect ends
  // up covering the full left half of child1.
  //
  // Given the floating point math, this number is a little bit fuzzy.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 26, 50), child1->visible_content_rect());

  // The child's DrawableContentRect is unclipped.
  EXPECT_RECT_EQ(unclipped_surface_content, child1->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest, DrawableAndVisibleContentRectsInHighDPI) {
  MockContentLayerClient client;

  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<ContentLayer> render_surface1 =
      CreateDrawableContentLayer(&client);
  scoped_refptr<ContentLayer> render_surface2 =
      CreateDrawableContentLayer(&client);
  scoped_refptr<ContentLayer> child1 = CreateDrawableContentLayer(&client);
  scoped_refptr<ContentLayer> child2 = CreateDrawableContentLayer(&client);
  scoped_refptr<ContentLayer> child3 = CreateDrawableContentLayer(&client);
  root->AddChild(render_surface1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child1);
  render_surface2->AddChild(child2);
  render_surface2->AddChild(child3);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(3, 4),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(7, 13),
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               false);

  float device_scale_factor = 2.f;

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get(), device_scale_factor);

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_TRUE(render_surface2->render_surface());

  // drawable_content_rects for all layers and surfaces are scaled by
  // device_scale_factor.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 200, 200),
                 root->render_surface()->DrawableContentRect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 200, 200), root->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(10, 10, 190, 190),
                 render_surface1->render_surface()->DrawableContentRect());

  // render_surface2 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1.
  EXPECT_RECT_EQ(gfx::Rect(10, 10, 350, 350),
                 render_surface2->render_surface()->DrawableContentRect());

  EXPECT_RECT_EQ(gfx::Rect(10, 10, 100, 100), child1->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(150, 150, 100, 100),
                 child2->drawable_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(250, 250, 100, 100),
                 child3->drawable_content_rect());

  // The root layer does not actually draw content of its own.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_content_rect());

  // All layer visible content rects are expressed in content space of each
  // layer, so they are also scaled by the device_scale_factor.
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 6, 8),
                 render_surface1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 14, 26),
                 render_surface2->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), child1->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), child2->visible_content_rect());
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), child3->visible_content_rect());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithoutPreserves3d) {
  // Verify the behavior of back-face culling when there are no preserve-3d
  // layers. Note that 3d transforms still apply in this case, but they are
  // "flattened" to each parent layer according to current W3C spec.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
      front_facing_child_of_front_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
      back_facing_child_of_front_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
      front_facing_child_of_back_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
      back_facing_child_of_back_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent());

  parent->AddChild(front_facing_child);
  parent->AddChild(back_facing_child);
  parent->AddChild(front_facing_surface);
  parent->AddChild(back_facing_surface);
  front_facing_surface->AddChild(front_facing_child_of_front_facing_surface);
  front_facing_surface->AddChild(back_facing_child_of_front_facing_surface);
  back_facing_surface->AddChild(front_facing_child_of_back_facing_surface);
  back_facing_surface->AddChild(back_facing_child_of_back_facing_surface);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // Nothing is double-sided
  front_facing_child->SetDoubleSided(false);
  back_facing_child->SetDoubleSided(false);
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);
  front_facing_child_of_front_facing_surface->SetDoubleSided(false);
  back_facing_child_of_front_facing_surface->SetDoubleSided(false);
  front_facing_child_of_back_facing_surface->SetDoubleSided(false);
  back_facing_child_of_back_facing_surface->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  // Having a descendant and opacity will force these to have render surfaces.
  front_facing_surface->SetOpacity(0.5f);
  back_facing_surface->SetOpacity(0.5f);

  // Nothing preserves 3d. According to current W3C CSS gfx::Transforms spec,
  // these layers should blindly use their own local transforms to determine
  // back-face culling.
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(front_facing_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_child.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(front_facing_surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(front_facing_child_of_front_facing_surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_child_of_front_facing_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(front_facing_child_of_back_facing_surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_child_of_back_facing_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify which render surfaces were created.
  EXPECT_FALSE(front_facing_child->render_surface());
  EXPECT_FALSE(back_facing_child->render_surface());
  EXPECT_TRUE(front_facing_surface->render_surface());
  EXPECT_TRUE(back_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_back_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_back_facing_surface->render_surface());

  // Verify the render_surface_layer_list.
  ASSERT_EQ(3u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(front_facing_surface->id(), render_surface_layer_list.at(1)->id());
  // Even though the back facing surface LAYER gets culled, the other
  // descendants should still be added, so the SURFACE should not be culled.
  EXPECT_EQ(back_facing_surface->id(), render_surface_layer_list.at(2)->id());

  // Verify root surface's layer list.
  ASSERT_EQ(
      3u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_child->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(1)->id());
  EXPECT_EQ(back_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(2)->id());

  // Verify front_facing_surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(front_facing_child_of_front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());

  // Verify back_facing_surface's layer list; its own layer should be culled
  // from the surface list.
  ASSERT_EQ(
      1u,
      render_surface_layer_list.at(2)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_child_of_back_facing_surface->id(),
            render_surface_layer_list.at(2)
                ->render_surface()->layer_list().at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithPreserves3d) {
  // Verify the behavior of back-face culling when preserves-3d transform style
  // is used.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
  front_facing_child_of_front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
  back_facing_child_of_front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
  front_facing_child_of_back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent>
  back_facing_child_of_back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> dummy_replica_layer1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> dummy_replica_layer2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  parent->AddChild(front_facing_child);
  parent->AddChild(back_facing_child);
  parent->AddChild(front_facing_surface);
  parent->AddChild(back_facing_surface);
  front_facing_surface->AddChild(front_facing_child_of_front_facing_surface);
  front_facing_surface->AddChild(back_facing_child_of_front_facing_surface);
  back_facing_surface->AddChild(front_facing_child_of_back_facing_surface);
  back_facing_surface->AddChild(back_facing_child_of_back_facing_surface);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // Nothing is double-sided
  front_facing_child->SetDoubleSided(false);
  back_facing_child->SetDoubleSided(false);
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);
  front_facing_child_of_front_facing_surface->SetDoubleSided(false);
  back_facing_child_of_front_facing_surface->SetDoubleSided(false);
  front_facing_child_of_back_facing_surface->SetDoubleSided(false);
  back_facing_child_of_back_facing_surface->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  // Opacity will not force creation of render surfaces in this case because of
  // the preserve-3d transform style. Instead, an example of when a surface
  // would be created with preserve-3d is when there is a replica layer.
  front_facing_surface->SetReplicaLayer(dummy_replica_layer1.get());
  back_facing_surface->SetReplicaLayer(dummy_replica_layer2.get());

  // Each surface creates its own new 3d rendering context (as defined by W3C
  // spec).  According to current W3C CSS gfx::Transforms spec, layers in a 3d
  // rendering context should use the transform with respect to that context.
  // This 3d rendering context occurs when (a) parent's transform style is flat
  // and (b) the layer's transform style is preserve-3d.
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);  // parent transform style is flat.
  SetLayerPropertiesForTesting(front_facing_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_child.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(
      front_facing_surface.get(),
      identity_matrix,
      identity_matrix,
      gfx::PointF(),
      gfx::PointF(),
      gfx::Size(100, 100),
      true);  // surface transform style is preserve-3d.
  SetLayerPropertiesForTesting(
      back_facing_surface.get(),
      backface_matrix,
      identity_matrix,
      gfx::PointF(),
      gfx::PointF(),
      gfx::Size(100, 100),
      true);  // surface transform style is preserve-3d.
  SetLayerPropertiesForTesting(front_facing_child_of_front_facing_surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_child_of_front_facing_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(front_facing_child_of_back_facing_surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(back_facing_child_of_back_facing_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify which render surfaces were created.
  EXPECT_FALSE(front_facing_child->render_surface());
  EXPECT_FALSE(back_facing_child->render_surface());
  EXPECT_TRUE(front_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_back_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_back_facing_surface->render_surface());

  // Verify the render_surface_layer_list. The back-facing surface should be
  // culled.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(front_facing_surface->id(), render_surface_layer_list.at(1)->id());

  // Verify root surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_child->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(1)->id());

  // Verify front_facing_surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(front_facing_child_of_front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithAnimatingTransforms) {
  // Verify that layers are appropriately culled when their back face is showing
  // and they are not double sided, while animations are going on.
  //
  // Layers that are animating do not get culled on the main thread, as their
  // transforms should be treated as "unknown" so we can not be sure that their
  // back face is really showing.
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> animating_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child_of_animating_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> animating_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  parent->AddChild(child);
  parent->AddChild(animating_surface);
  animating_surface->AddChild(child_of_animating_surface);
  parent->AddChild(animating_child);
  parent->AddChild(child2);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // Nothing is double-sided
  child->SetDoubleSided(false);
  child2->SetDoubleSided(false);
  animating_surface->SetDoubleSided(false);
  child_of_animating_surface->SetDoubleSided(false);
  animating_child->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  // Make our render surface.
  animating_surface->SetForceRenderSurface(true);

  // Animate the transform on the render surface.
  AddAnimatedTransformToController(
      animating_surface->layer_animation_controller(), 10.0, 30, 0);
  // This is just an animating layer, not a surface.
  AddAnimatedTransformToController(
      animating_child->layer_animation_controller(), 10.0, 30, 0);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(animating_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child_of_animating_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(animating_child.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_FALSE(child->render_surface());
  EXPECT_TRUE(animating_surface->render_surface());
  EXPECT_FALSE(child_of_animating_surface->render_surface());
  EXPECT_FALSE(animating_child->render_surface());
  EXPECT_FALSE(child2->render_surface());

  // Verify that the animating_child and child_of_animating_surface were not
  // culled, but that child was.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(animating_surface->id(), render_surface_layer_list.at(1)->id());

  // The non-animating child be culled from the layer list for the parent render
  // surface.
  ASSERT_EQ(
      3u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(animating_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(animating_child->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(1)->id());
  EXPECT_EQ(child2->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(2)->id());

  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(animating_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(child_of_animating_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());

  EXPECT_FALSE(child2->visible_content_rect().IsEmpty());

  // The animating layers should have a visible content rect that represents the
  // area of the front face that is within the viewport.
  EXPECT_EQ(animating_child->visible_content_rect(),
            gfx::Rect(animating_child->content_bounds()));
  EXPECT_EQ(animating_surface->visible_content_rect(),
            gfx::Rect(animating_surface->content_bounds()));
  // And layers in the subtree of the animating layer should have valid visible
  // content rects also.
  EXPECT_EQ(child_of_animating_surface->visible_content_rect(),
            gfx::Rect(child_of_animating_surface->content_bounds()));
}

TEST_F(LayerTreeHostCommonTest,
     BackFaceCullingWithPreserves3dForFlatteningSurface) {
  // Verify the behavior of back-face culling for a render surface that is
  // created when it flattens its subtree, and its parent has preserves-3d.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  parent->AddChild(front_facing_surface);
  parent->AddChild(back_facing_surface);
  front_facing_surface->AddChild(child1);
  back_facing_surface->AddChild(child2);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  // RenderSurfaces are not double-sided
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);   // parent transform style is preserve3d.
  SetLayerPropertiesForTesting(front_facing_surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);  // surface transform style is flat.
  SetLayerPropertiesForTesting(back_facing_surface.get(),
                               backface_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);  // surface transform style is flat.
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify which render surfaces were created.
  EXPECT_TRUE(front_facing_surface->render_surface());
  EXPECT_FALSE(
      back_facing_surface->render_surface());  // because it should be culled
  EXPECT_FALSE(child1->render_surface());
  EXPECT_FALSE(child2->render_surface());

  // Verify the render_surface_layer_list. The back-facing surface should be
  // culled.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(front_facing_surface->id(), render_surface_layer_list.at(1)->id());

  // Verify root surface's layer list.
  ASSERT_EQ(
      1u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());

  // Verify front_facing_surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(child1->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());
}


TEST_F(LayerTreeHostCommonTest, HitTestingForEmptyLayerList) {
  // Hit testing on an empty render_surface_layer_list should return a null
  // pointer.
  LayerImplList render_surface_layer_list;

  gfx::Point test_point(0, 0);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 20);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSingleLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::Point test_point(101, 101);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(1, 1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSingleLayerAndHud) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);
  scoped_ptr<HeadsUpDisplayLayerImpl> hud =
      HeadsUpDisplayLayerImpl::Create(host_impl.active_tree(), 11111);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  // Create hud and add it as a child of root.
  gfx::Size hud_bounds(200, 200);
  SetLayerPropertiesForTesting(hud.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               hud_bounds,
                               false);
  hud->SetDrawsContent(true);

  host_impl.active_tree()->set_hud_layer(hud.get());
  root->AddChild(hud.PassAs<LayerImpl>());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), hud_bounds, &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());

  // Hit testing for a point inside HUD, but outside root should return null
  gfx::Point test_point(101, 101);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer, never the HUD
  // layer.
  test_point = gfx::Point(1, 1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForUninvertibleTransform) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform uninvertible_transform;
  uninvertible_transform.matrix().set(0, 0, 0.0);
  uninvertible_transform.matrix().set(1, 1, 0.0);
  uninvertible_transform.matrix().set(2, 2, 0.0);
  uninvertible_transform.matrix().set(3, 3, 0.0);
  ASSERT_FALSE(uninvertible_transform.IsInvertible());

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               uninvertible_transform,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  ASSERT_FALSE(root->screen_space_transform().IsInvertible());

  // Hit testing any point should not hit the layer. If the invertible matrix is
  // accidentally ignored and treated like an identity, then the hit testing
  // will incorrectly hit the layer when it shouldn't.
  gfx::Point test_point(1, 1);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 10);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 30);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(50, 50);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(67, 48);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(99, 99);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSinglePositionedLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  gfx::PointF position(50.f, 50.f);
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::Point test_point(49, 49);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the root render surface would clamp it.
  test_point = gfx::Point(101, 101);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(51, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSingleRotatedLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform identity_matrix;
  gfx::Transform rotation45_degrees_about_center;
  rotation45_degrees_about_center.Translate(50.0, 50.0);
  rotation45_degrees_about_center.RotateAboutZAxis(45.0);
  rotation45_degrees_about_center.Translate(-50.0, -50.0);
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               rotation45_degrees_about_center,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::Point test_point(99, 99);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(1, 1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(1, 50);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  // Hit testing the corners that would overlap the unclipped layer, but are
  // outside the clipped region.
  test_point = gfx::Point(50, -1);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_FALSE(result_layer);

  test_point = gfx::Point(-1, 50);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_FALSE(result_layer);
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSinglePerspectiveLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform identity_matrix;

  // perspective_projection_about_center * translation_by_z is designed so that
  // the 100 x 100 layer becomes 50 x 50, and remains centered at (50, 50).
  gfx::Transform perspective_projection_about_center;
  perspective_projection_about_center.Translate(50.0, 50.0);
  perspective_projection_about_center.ApplyPerspectiveDepth(1.0);
  perspective_projection_about_center.Translate(-50.0, -50.0);
  gfx::Transform translation_by_z;
  translation_by_z.Translate3d(0.0, 0.0, -1.0);

  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(
      root.get(),
      perspective_projection_about_center * translation_by_z,
      identity_matrix,
      anchor,
      position,
      bounds,
      false);
  root->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit testing for points outside the layer.
  // These corners would have been inside the un-transformed layer, but they
  // should not hit the correctly transformed layer.
  gfx::Point test_point(24, 24);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(76, 76);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the root layer.
  test_point = gfx::Point(26, 26);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(74, 74);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSingleLayerWithScaledContents) {
  // A layer's visible content rect is actually in the layer's content space.
  // The screen space transform converts from the layer's origin space to screen
  // space. This test makes sure that hit testing works correctly accounts for
  // the contents scale.  A contents scale that is not 1 effectively forces a
  // non-identity transform between layer's content space and layer's origin
  // space. The hit testing code must take this into account.
  //
  // To test this, the layer is positioned at (25, 25), and is size (50, 50). If
  // contents scale is ignored, then hit testing will mis-interpret the visible
  // content rect as being larger than the actual bounds of the layer.
  //
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl.active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    // override content bounds and contents scale
    test_layer->SetContentBounds(gfx::Size(100, 100));
    test_layer->SetContentsScale(2, 2);

    test_layer->SetDrawsContent(true);
    root->AddChild(test_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer = root->children()[0];
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                 test_layer->visible_content_rect());
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit testing for a point outside the layer should return a null pointer (the
  // root layer does not draw content, so it will not be hit tested either).
  gfx::Point test_point(101, 101);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(24, 24);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(76, 76);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the test layer.
  test_point = gfx::Point(26, 26);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(74, 74);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForSimpleClippedLayer) {
  // Test that hit-testing will only work for the visible portion of a layer,
  // and not the entire layer bounds. Here we just test the simple axis-aligned
  // case.
  gfx::Transform identity_matrix;
  gfx::PointF anchor;

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    scoped_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl.active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    clipping_layer->SetMasksToBounds(true);

    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl.active_tree(), 456);
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child->SetDrawsContent(true);
    clipping_layer->AddChild(child.Pass());
    root->AddChild(clipping_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  ASSERT_EQ(456, root->render_surface()->layer_list().at(0)->id());

  // Hit testing for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::Point test_point(24, 24);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Even though the layer exists at (101, 101), it should not be visible there
  // since the clipping_layer would clamp it.
  test_point = gfx::Point(76, 76);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::Point(26, 26);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::Point(74, 74);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForMultiClippedRotatedLayer) {
  // This test checks whether hit testing correctly avoids hit testing with
  // multiple ancestors that clip in non axis-aligned ways. To pass this test,
  // the hit testing algorithm needs to recognize that multiple parent layers
  // may clip the layer, and should not actually hit those clipped areas.
  //
  // The child and grand_child layers are both initialized to clip the
  // rotated_leaf. The child layer is rotated about the top-left corner, so that
  // the root + child clips combined create a triangle. The rotated_leaf will
  // only be visible where it overlaps this triangle.
  //
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 123);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetMasksToBounds(true);
  {
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl.active_tree(), 456);
    scoped_ptr<LayerImpl> grand_child =
        LayerImpl::Create(host_impl.active_tree(), 789);
    scoped_ptr<LayerImpl> rotated_leaf =
        LayerImpl::Create(host_impl.active_tree(), 2468);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(80, 80);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child->SetMasksToBounds(true);

    gfx::Transform rotation45_degrees_about_corner;
    rotation45_degrees_about_corner.RotateAboutZAxis(45.0);

    // remember, positioned with respect to its parent which is already at 10,
    // 10
    position = gfx::PointF();
    bounds =
        gfx::Size(200, 200);  // to ensure it covers at least sqrt(2) * 100.
    SetLayerPropertiesForTesting(grand_child.get(),
                                 rotation45_degrees_about_corner,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    grand_child->SetMasksToBounds(true);

    // Rotates about the center of the layer
    gfx::Transform rotated_leaf_transform;
    rotated_leaf_transform.Translate(
        -10.0, -10.0);  // cancel out the grand_parent's position
    rotated_leaf_transform.RotateAboutZAxis(
        -45.0);  // cancel out the corner 45-degree rotation of the parent.
    rotated_leaf_transform.Translate(50.0, 50.0);
    rotated_leaf_transform.RotateAboutZAxis(45.0);
    rotated_leaf_transform.Translate(-50.0, -50.0);
    position = gfx::PointF();
    bounds = gfx::Size(100, 100);
    SetLayerPropertiesForTesting(rotated_leaf.get(),
                                 rotated_leaf_transform,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    rotated_leaf->SetDrawsContent(true);

    grand_child->AddChild(rotated_leaf.Pass());
    child->AddChild(grand_child.Pass());
    root->AddChild(child.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  // The grand_child is expected to create a render surface because it
  // MasksToBounds and is not axis aligned.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  ASSERT_EQ(
      1u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  ASSERT_EQ(789,
            render_surface_layer_list.at(0)->render_surface()->layer_list().at(
                0)->id());  // grand_child's surface.
  ASSERT_EQ(
      1u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  ASSERT_EQ(
      2468,
      render_surface_layer_list[1]->render_surface()->layer_list().at(0)->id());

  // (11, 89) is close to the the bottom left corner within the clip, but it is
  // not inside the layer.
  gfx::Point test_point(11, 89);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Closer inwards from the bottom left will overlap the layer.
  test_point = gfx::Point(25, 75);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());

  // (4, 50) is inside the unclipped layer, but that corner of the layer should
  // be clipped away by the grandparent and should not get hit. If hit testing
  // blindly uses visible content rect without considering how parent may clip
  // the layer, then hit testing would accidentally think that the point
  // successfully hits the layer.
  test_point = gfx::Point(4, 50);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // (11, 50) is inside the layer and within the clipped area.
  test_point = gfx::Point(11, 50);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());

  // Around the middle, just to the right and up, would have hit the layer
  // except that that area should be clipped away by the parent.
  test_point = gfx::Point(51, 49);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Around the middle, just to the left and down, should successfully hit the
  // layer.
  test_point = gfx::Point(49, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2468, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForNonClippingIntermediateLayer) {
  // This test checks that hit testing code does not accidentally clip to layer
  // bounds for a layer that actually does not clip.
  gfx::Transform identity_matrix;
  gfx::PointF anchor;

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    scoped_ptr<LayerImpl> intermediate_layer =
        LayerImpl::Create(host_impl.active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(10.f, 10.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(intermediate_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    // Sanity check the intermediate layer should not clip.
    ASSERT_FALSE(intermediate_layer->masks_to_bounds());
    ASSERT_FALSE(intermediate_layer->mask_layer());

    // The child of the intermediate_layer is translated so that it does not
    // overlap intermediate_layer at all.  If child is incorrectly clipped, we
    // would not be able to hit it successfully.
    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl.active_tree(), 456);
    position = gfx::PointF(60.f, 60.f);  // 70, 70 in screen space
    bounds = gfx::Size(20, 20);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child->SetDrawsContent(true);
    intermediate_layer->AddChild(child.Pass());
    root->AddChild(intermediate_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  ASSERT_EQ(456, root->render_surface()->layer_list().at(0)->id());

  // Hit testing for a point outside the layer should return a null pointer.
  gfx::Point test_point(69, 69);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(91, 91);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit testing for a point inside should return the child layer.
  test_point = gfx::Point(71, 71);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::Point(89, 89);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForMultipleLayers) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    scoped_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl.active_tree(), 2);
    scoped_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl.active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl.active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child1->SetDrawsContent(true);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child2.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child2->SetDrawsContent(true);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    grand_child1->SetDrawsContent(true);

    child1->AddChild(grand_child1.Pass());
    root->AddChild(child1.Pass());
    root->AddChild(child2.Pass());
  }

  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  LayerImpl* grand_child1 = child1->children()[0];

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_EQ(1u, render_surface_layer_list.size());

  RenderSurfaceImpl* root_render_surface = root->render_surface();
  ASSERT_EQ(4u, root_render_surface->layer_list().size());
  ASSERT_EQ(1, root_render_surface->layer_list().at(0)->id());  // root layer
  ASSERT_EQ(2, root_render_surface->layer_list().at(1)->id());  // child1
  ASSERT_EQ(4, root_render_surface->layer_list().at(2)->id());  // grand_child1
  ASSERT_EQ(3, root_render_surface->layer_list().at(3)->id());  // child2

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::Point test_point = gfx::Point(1, 1);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::Point(15, 15);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::Point(51, 20);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::Point(80, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::Point(51, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::Point(20, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForMultipleLayerLists) {
  //
  // The geometry is set up similarly to the previous case, but
  // all layers are forced to be render surfaces now.
  //
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);
  {
    // child 1 and child2 are initialized to overlap between x=50 and x=60.
    // grand_child is set to overlap both child1 and child2 between y=50 and
    // y=60.  The expected stacking order is: (front) child2, (second)
    // grand_child, (third) child1, and (back) the root layer behind all other
    // layers.

    scoped_ptr<LayerImpl> child1 =
        LayerImpl::Create(host_impl.active_tree(), 2);
    scoped_ptr<LayerImpl> child2 =
        LayerImpl::Create(host_impl.active_tree(), 3);
    scoped_ptr<LayerImpl> grand_child1 =
        LayerImpl::Create(host_impl.active_tree(), 4);

    position = gfx::PointF(10.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child1.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child1->SetDrawsContent(true);
    child1->SetForceRenderSurface(true);

    position = gfx::PointF(50.f, 10.f);
    bounds = gfx::Size(50, 50);
    SetLayerPropertiesForTesting(child2.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child2->SetDrawsContent(true);
    child2->SetForceRenderSurface(true);

    // Remember that grand_child is positioned with respect to its parent (i.e.
    // child1).  In screen space, the intended position is (10, 50), with size
    // 100 x 50.
    position = gfx::PointF(0.f, 40.f);
    bounds = gfx::Size(100, 50);
    SetLayerPropertiesForTesting(grand_child1.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    grand_child1->SetDrawsContent(true);
    grand_child1->SetForceRenderSurface(true);

    child1->AddChild(grand_child1.Pass());
    root->AddChild(child1.Pass());
    root->AddChild(child2.Pass());
  }

  LayerImpl* child1 = root->children()[0];
  LayerImpl* child2 = root->children()[1];
  LayerImpl* grand_child1 = child1->children()[0];

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  ASSERT_TRUE(grand_child1);
  ASSERT_TRUE(child1->render_surface());
  ASSERT_TRUE(child2->render_surface());
  ASSERT_TRUE(grand_child1->render_surface());
  ASSERT_EQ(4u, render_surface_layer_list.size());
  // The root surface has the root layer, and child1's and child2's render
  // surfaces.
  ASSERT_EQ(3u, root->render_surface()->layer_list().size());
  // The child1 surface has the child1 layer and grand_child1's render surface.
  ASSERT_EQ(2u, child1->render_surface()->layer_list().size());
  ASSERT_EQ(1u, child2->render_surface()->layer_list().size());
  ASSERT_EQ(1u, grand_child1->render_surface()->layer_list().size());
  ASSERT_EQ(1, render_surface_layer_list.at(0)->id());  // root layer
  ASSERT_EQ(2, render_surface_layer_list[1]->id());  // child1
  ASSERT_EQ(4, render_surface_layer_list.at(2)->id());  // grand_child1
  ASSERT_EQ(3, render_surface_layer_list[3]->id());  // child2

  // Nothing overlaps the root_layer at (1, 1), so hit testing there should find
  // the root layer.
  gfx::Point test_point = gfx::Point(1, 1);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // At (15, 15), child1 and root are the only layers. child1 is expected to be
  // on top.
  test_point = gfx::Point(15, 15);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(2, result_layer->id());

  // At (51, 20), child1 and child2 overlap. child2 is expected to be on top.
  test_point = gfx::Point(51, 20);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (80, 51), child2 and grand_child1 overlap. child2 is expected to be on
  // top.
  test_point = gfx::Point(80, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (51, 51), all layers overlap each other. child2 is expected to be on top
  // of all other layers.
  test_point = gfx::Point(51, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // At (20, 51), child1 and grand_child1 overlap. grand_child1 is expected to
  // be on top.
  test_point = gfx::Point(20, 51);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest, HitTestingForEmptyLayers) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);

  // Layer 1 - root
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 1);
  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  {
    // Layer 2 - empty: drawsContent=false
    gfx::PointF position(10.f, 10.f);
    gfx::Size bounds(30, 30);
    scoped_ptr<LayerImpl> empty_layer =
        LayerImpl::Create(host_impl.active_tree(), 2);
    SetLayerPropertiesForTesting(empty_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    empty_layer->SetDrawsContent(false);
    root->AddChild(empty_layer.Pass());
  }

  {
    // Layer 3 - empty, but has touch handler
    gfx::PointF position(10.f, 60.f);
    gfx::Size bounds(30, 30);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl.active_tree(), 3);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    test_layer->SetDrawsContent(false);
    Region touch_handler_region(gfx::Rect(10, 10, 10, 10));
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->AddChild(test_layer.Pass());
  }

  {
    // Layer 4 - empty, but has mousewheel handler
    gfx::PointF position(60.f, 60.f);
    gfx::Size bounds(30, 30);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl.active_tree(), 4);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    test_layer->SetDrawsContent(false);
    test_layer->SetHaveWheelEventHandlers(true);
    root->AddChild(test_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify that the root layer and empty layers with touch/wheel handlers
  // (but not the empty layer without a touch handler) are in the RSSL.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  EXPECT_EQ(1, render_surface_layer_list[0]->id());
  ASSERT_EQ(3u, root->render_surface()->layer_list().size());
  EXPECT_EQ(1, root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(3, root->render_surface()->layer_list().at(1)->id());
  EXPECT_EQ(4, root->render_surface()->layer_list().at(2)->id());

  // Hit testing for a point inside the empty no-handlers layer should return
  // the root layer.
  gfx::Point test_point = gfx::Point(15, 15);
  LayerImpl* result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(1, result_layer->id());

  // Hit testing for a point inside the touch handler layer should return it.
  test_point = gfx::Point(15, 75);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(3, result_layer->id());

  // Hit testing for a point inside the mousewheel layer should return it.
  test_point = gfx::Point(75, 75);
  result_layer = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(4, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest,
       HitCheckingTouchHandlerRegionsForEmptyLayerList) {
  // Hit checking on an empty render_surface_layer_list should return a null
  // pointer.
  LayerImplList render_surface_layer_list;

  gfx::Point test_point(0, 0);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 20);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeHostCommonTest, HitCheckingTouchHandlerRegionsForSingleLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit checking for any point should return a null pointer for a layer without
  // any touch event handler regions.
  gfx::Point test_point(11, 11);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  root->SetTouchEventHandlerRegion(touch_handler_region);
  // Hit checking for a point outside the layer should return a null pointer.
  test_point = gfx::Point(101, 101);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(1, 1);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(99, 99);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(11, 11);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(59, 59);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest,
     HitCheckingTouchHandlerRegionsForUninvertibleTransform) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform uninvertible_transform;
  uninvertible_transform.matrix().set(0, 0, 0.0);
  uninvertible_transform.matrix().set(1, 1, 0.0);
  uninvertible_transform.matrix().set(2, 2, 0.0);
  uninvertible_transform.matrix().set(3, 3, 0.0);
  ASSERT_FALSE(uninvertible_transform.IsInvertible());

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               uninvertible_transform,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);
  root->SetTouchEventHandlerRegion(touch_handler_region);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  ASSERT_FALSE(root->screen_space_transform().IsInvertible());

  // Hit checking any point should not hit the touch handler region on the
  // layer. If the invertible matrix is accidentally ignored and treated like an
  // identity, then the hit testing will incorrectly hit the layer when it
  // shouldn't.
  gfx::Point test_point(1, 1);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 10);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(10, 30);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(50, 50);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(67, 48);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(99, 99);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(-1, -1);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);
}

TEST_F(LayerTreeHostCommonTest,
     HitCheckingTouchHandlerRegionsForSinglePositionedLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);

  gfx::Transform identity_matrix;
  Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
  gfx::PointF anchor;
  // this layer is positioned, and hit testing should correctly know where the
  // layer is located.
  gfx::PointF position(50.f, 50.f);
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);
  root->SetTouchEventHandlerRegion(touch_handler_region);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit checking for a point outside the layer should return a null pointer.
  gfx::Point test_point(49, 49);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Even though the layer has a touch handler region containing (101, 101), it
  // should not be visible there since the root render surface would clamp it.
  test_point = gfx::Point(101, 101);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(51, 51);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(61, 61);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(99, 99);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest,
     HitCheckingTouchHandlerRegionsForSingleLayerWithScaledContents) {
  // A layer's visible content rect is actually in the layer's content space.
  // The screen space transform converts from the layer's origin space to screen
  // space. This test makes sure that hit testing works correctly accounts for
  // the contents scale.  A contents scale that is not 1 effectively forces a
  // non-identity transform between layer's content space and layer's origin
  // space. The hit testing code must take this into account.
  //
  // To test this, the layer is positioned at (25, 25), and is size (50, 50). If
  // contents scale is ignored, then hit checking will mis-interpret the visible
  // content rect as being larger than the actual bounds of the layer.
  //
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    Region touch_handler_region(gfx::Rect(10, 10, 30, 30));
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl.active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    // override content bounds and contents scale
    test_layer->SetContentBounds(gfx::Size(100, 100));
    test_layer->SetContentsScale(2, 2);

    test_layer->SetDrawsContent(true);
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->AddChild(test_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer = root->children()[0];
  EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), test_layer->visible_content_rect());
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Hit checking for a point outside the layer should return a null pointer
  // (the root layer does not draw content, so it will not be tested either).
  gfx::Point test_point(76, 76);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(26, 26);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(34, 34);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(65, 65);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(74, 74);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(35, 35);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(64, 64);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest,
     HitCheckingTouchHandlerRegionsForSingleLayerWithDeviceScale) {
  // The layer's device_scale_factor and page_scale_factor should scale the
  // content rect and we should be able to hit the touch handler region by
  // scaling the points accordingly.
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  // Set the bounds of the root layer big enough to fit the child when scaled.
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    Region touch_handler_region(gfx::Rect(10, 10, 30, 30));
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    scoped_ptr<LayerImpl> test_layer =
        LayerImpl::Create(host_impl.active_tree(), 12345);
    SetLayerPropertiesForTesting(test_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);

    test_layer->SetDrawsContent(true);
    test_layer->SetTouchEventHandlerRegion(touch_handler_region);
    root->AddChild(test_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  float device_scale_factor = 3.f;
  float page_scale_factor = 5.f;
  gfx::Size scaled_bounds_for_root = gfx::ToCeiledSize(
      gfx::ScaleSize(root->bounds(), device_scale_factor * page_scale_factor));

  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), scaled_bounds_for_root, &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = root.get();
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  // The visible content rect for test_layer is actually 100x100, even though
  // its layout size is 50x50, positioned at 25x25.
  LayerImpl* test_layer = root->children()[0];
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());

  // Check whether the child layer fits into the root after scaled.
  EXPECT_RECT_EQ(gfx::Rect(test_layer->content_bounds()),
                 test_layer->visible_content_rect());

  // Hit checking for a point outside the layer should return a null pointer
  // (the root layer does not draw content, so it will not be tested either).
  gfx::PointF test_point(76.f, 76.f);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(26, 26);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(34, 34);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(65, 65);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(74, 74);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(35, 35);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());

  test_point = gfx::Point(64, 64);
  test_point =
      gfx::ScalePoint(test_point, device_scale_factor * page_scale_factor);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(12345, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest,
     HitCheckingTouchHandlerRegionsForSimpleClippedLayer) {
  // Test that hit-checking will only work for the visible portion of a layer,
  // and not the entire layer bounds. Here we just test the simple axis-aligned
  // case.
  gfx::Transform identity_matrix;
  gfx::PointF anchor;

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    scoped_ptr<LayerImpl> clipping_layer =
        LayerImpl::Create(host_impl.active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(25.f, 25.f);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(clipping_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    clipping_layer->SetMasksToBounds(true);

    scoped_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl.active_tree(), 456);
    Region touch_handler_region(gfx::Rect(10, 10, 50, 50));
    position = gfx::PointF(-50.f, -50.f);
    bounds = gfx::Size(300, 300);
    SetLayerPropertiesForTesting(child.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    child->SetDrawsContent(true);
    child->SetTouchEventHandlerRegion(touch_handler_region);
    clipping_layer->AddChild(child.Pass());
    root->AddChild(clipping_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  ASSERT_EQ(456, root->render_surface()->layer_list().at(0)->id());

  // Hit checking for a point outside the layer should return a null pointer.
  // Despite the child layer being very large, it should be clipped to the root
  // layer's bounds.
  gfx::Point test_point(24, 24);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the layer, but outside the touch handler
  // region should return a null pointer.
  test_point = gfx::Point(35, 35);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(74, 74);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  // Hit checking for a point inside the touch event handler region should
  // return the root layer.
  test_point = gfx::Point(25, 25);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());

  test_point = gfx::Point(34, 34);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(456, result_layer->id());
}

TEST_F(LayerTreeHostCommonTest,
       HitCheckingTouchHandlerOverlappingRegions) {
  gfx::Transform identity_matrix;
  gfx::PointF anchor;

  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  {
    scoped_ptr<LayerImpl> touch_layer =
        LayerImpl::Create(host_impl.active_tree(), 123);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position;
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(touch_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    touch_layer->SetDrawsContent(true);
    touch_layer->SetTouchEventHandlerRegion(gfx::Rect(0, 0, 50, 50));
    root->AddChild(touch_layer.Pass());
  }

  {
    scoped_ptr<LayerImpl> notouch_layer =
        LayerImpl::Create(host_impl.active_tree(), 1234);
    // this layer is positioned, and hit testing should correctly know where the
    // layer is located.
    gfx::PointF position(0, 25);
    gfx::Size bounds(50, 50);
    SetLayerPropertiesForTesting(notouch_layer.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 anchor,
                                 position,
                                 bounds,
                                 false);
    notouch_layer->SetDrawsContent(true);
    root->AddChild(notouch_layer.Pass());
  }

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Sanity check the scenario we just created.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  ASSERT_EQ(123, root->render_surface()->layer_list().at(0)->id());
  ASSERT_EQ(1234, root->render_surface()->layer_list().at(1)->id());

  gfx::Point test_point(35, 35);
  LayerImpl* result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);

  test_point = gfx::Point(35, 15);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  ASSERT_TRUE(result_layer);
  EXPECT_EQ(123, result_layer->id());

  test_point = gfx::Point(35, 65);
  result_layer =
      LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
          test_point, render_surface_layer_list);
  EXPECT_FALSE(result_layer);
}

class NoScaleContentLayer : public ContentLayer {
 public:
  static scoped_refptr<NoScaleContentLayer> Create(ContentLayerClient* client) {
    return make_scoped_refptr(new NoScaleContentLayer(client));
  }

  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE {
    // Skip over the ContentLayer to the base Layer class.
    Layer::CalculateContentsScale(ideal_contents_scale,
                                  device_scale_factor,
                                  page_scale_factor,
                                  animating_transform_to_screen,
                                  contents_scale_x,
                                  contents_scale_y,
                                  content_bounds);
  }

 protected:
  explicit NoScaleContentLayer(ContentLayerClient* client)
      : ContentLayer(client) {}
  virtual ~NoScaleContentLayer() {}
};

scoped_refptr<NoScaleContentLayer> CreateNoScaleDrawableContentLayer(
    ContentLayerClient* delegate) {
  scoped_refptr<NoScaleContentLayer> to_return =
      NoScaleContentLayer::Create(delegate);
  to_return->SetIsDrawable(true);
  return to_return;
}

TEST_F(LayerTreeHostCommonTest, LayerTransformsInHighDPI) {
  // Verify draw and screen space transforms of layers not in a surface.
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> child = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> child_empty =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_empty.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(),
                               true);

  scoped_refptr<NoScaleContentLayer> child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_no_scale.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  parent->AddChild(child);
  parent->AddChild(child_empty);
  parent->AddChild(child_no_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 1.f;

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor, parent);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor, child);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           child_empty);
  EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);

  EXPECT_EQ(1u, render_surface_layer_list.size());

  // Verify parent transforms
  gfx::Transform expected_parent_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->draw_transform());

  // Verify results of transformed parent rects
  gfx::RectF parent_content_bounds(parent->content_bounds());

  gfx::RectF parent_draw_rect =
      MathUtil::MapClippedRect(parent->draw_transform(), parent_content_bounds);
  gfx::RectF parent_screen_space_rect = MathUtil::MapClippedRect(
      parent->screen_space_transform(), parent_content_bounds);

  gfx::RectF expected_parent_draw_rect(parent->bounds());
  expected_parent_draw_rect.Scale(device_scale_factor);
  EXPECT_FLOAT_RECT_EQ(expected_parent_draw_rect, parent_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_parent_draw_rect, parent_screen_space_rect);

  // Verify child and child_empty transforms. They should match.
  gfx::Transform expected_child_transform;
  expected_child_transform.Translate(
      device_scale_factor * child->position().x(),
      device_scale_factor * child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_empty->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_empty->screen_space_transform());

  // Verify results of transformed child and child_empty rects. They should
  // match.
  gfx::RectF child_content_bounds(child->content_bounds());

  gfx::RectF child_draw_rect =
      MathUtil::MapClippedRect(child->draw_transform(), child_content_bounds);
  gfx::RectF child_screen_space_rect = MathUtil::MapClippedRect(
      child->screen_space_transform(), child_content_bounds);

  gfx::RectF child_empty_draw_rect = MathUtil::MapClippedRect(
      child_empty->draw_transform(), child_content_bounds);
  gfx::RectF child_empty_screen_space_rect = MathUtil::MapClippedRect(
      child_empty->screen_space_transform(), child_content_bounds);

  gfx::RectF expected_child_draw_rect(child->position(), child->bounds());
  expected_child_draw_rect.Scale(device_scale_factor);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_screen_space_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_empty_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_empty_screen_space_rect);

  // Verify child_no_scale transforms
  gfx::Transform expected_child_no_scale_transform = child->draw_transform();
  // All transforms operate on content rects. The child's content rect
  // incorporates device scale, but the child_no_scale does not; add it here.
  expected_child_no_scale_transform.Scale(device_scale_factor,
                                          device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_no_scale_transform,
                                  child_no_scale->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_no_scale_transform,
                                  child_no_scale->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, SurfaceLayerTransformsInHighDPI) {
  // Verify draw and screen space transforms of layers in a surface.
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.0);

  gfx::Transform scale_small_matrix;
  scale_small_matrix.Scale(SK_MScalar1 / 10.f, SK_MScalar1 / 12.f);

  scoped_refptr<Layer> root = Layer::Create();

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> perspective_surface =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(perspective_surface.get(),
                               perspective_matrix * scale_small_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> scale_surface =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(scale_surface.get(),
                               scale_small_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  perspective_surface->SetForceRenderSurface(true);
  scale_surface->SetForceRenderSurface(true);

  parent->AddChild(perspective_surface);
  parent->AddChild(scale_surface);
  root->AddChild(parent);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 3.f;

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = root;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor, parent);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           perspective_surface);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           scale_surface);

  EXPECT_EQ(3u, render_surface_layer_list.size());

  gfx::Transform expected_parent_draw_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_draw_transform,
                                  parent->draw_transform());

  // The scaled surface is rendered at its appropriate scale, and drawn 1:1
  // into its target.
  gfx::Transform expected_scale_surface_draw_transform;
  expected_scale_surface_draw_transform.Translate(
      device_scale_factor * page_scale_factor * scale_surface->position().x(),
      device_scale_factor * page_scale_factor * scale_surface->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_scale_surface_draw_transform,
      scale_surface->render_surface()->draw_transform());
  gfx::Transform expected_scale_surface_layer_draw_transform =
      scale_small_matrix;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_scale_surface_layer_draw_transform,
                                  scale_surface->draw_transform());

  // The scale for the perspective surface is not known, so it is rendered 1:1
  // with the screen, and then scaled during drawing.
  gfx::Transform expected_perspective_surface_draw_transform;
  expected_perspective_surface_draw_transform.Translate(
      device_scale_factor * page_scale_factor *
      perspective_surface->position().x(),
      device_scale_factor * page_scale_factor *
      perspective_surface->position().y());
  expected_perspective_surface_draw_transform.PreconcatTransform(
      perspective_matrix);
  expected_perspective_surface_draw_transform.PreconcatTransform(
      scale_small_matrix);
  gfx::Transform expected_perspective_surface_layer_draw_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_perspective_surface_draw_transform,
      perspective_surface->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_perspective_surface_layer_draw_transform,
      perspective_surface->draw_transform());
}

TEST_F(LayerTreeHostCommonTest,
     LayerTransformsInHighDPIAccurateScaleZeroChildPosition) {
  // Verify draw and screen space transforms of layers not in a surface.
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(133, 133),
                               true);

  scoped_refptr<ContentLayer> child = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(13, 13),
                               true);

  scoped_refptr<NoScaleContentLayer> child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_no_scale.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(13, 13),
                               true);

  parent->AddChild(child);
  parent->AddChild(child_no_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  float device_scale_factor = 1.7f;
  float page_scale_factor = 1.f;

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = parent.get();
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor, parent);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor, child);
  EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);

  EXPECT_EQ(1u, render_surface_layer_list.size());

  // Verify parent transforms
  gfx::Transform expected_parent_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->draw_transform());

  // Verify results of transformed parent rects
  gfx::RectF parent_content_bounds(parent->content_bounds());

  gfx::RectF parent_draw_rect =
      MathUtil::MapClippedRect(parent->draw_transform(), parent_content_bounds);
  gfx::RectF parent_screen_space_rect = MathUtil::MapClippedRect(
      parent->screen_space_transform(), parent_content_bounds);

  gfx::RectF expected_parent_draw_rect(parent->bounds());
  expected_parent_draw_rect.Scale(device_scale_factor);
  expected_parent_draw_rect.set_width(ceil(expected_parent_draw_rect.width()));
  expected_parent_draw_rect.set_height(
      ceil(expected_parent_draw_rect.height()));
  EXPECT_FLOAT_RECT_EQ(expected_parent_draw_rect, parent_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_parent_draw_rect, parent_screen_space_rect);

  // Verify child transforms
  gfx::Transform expected_child_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->screen_space_transform());

  // Verify results of transformed child rects
  gfx::RectF child_content_bounds(child->content_bounds());

  gfx::RectF child_draw_rect =
      MathUtil::MapClippedRect(child->draw_transform(), child_content_bounds);
  gfx::RectF child_screen_space_rect = MathUtil::MapClippedRect(
      child->screen_space_transform(), child_content_bounds);

  gfx::RectF expected_child_draw_rect(child->bounds());
  expected_child_draw_rect.Scale(device_scale_factor);
  expected_child_draw_rect.set_width(ceil(expected_child_draw_rect.width()));
  expected_child_draw_rect.set_height(ceil(expected_child_draw_rect.height()));
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_screen_space_rect);

  // Verify child_no_scale transforms
  gfx::Transform expected_child_no_scale_transform = child->draw_transform();
  // All transforms operate on content rects. The child's content rect
  // incorporates device scale, but the child_no_scale does not; add it here.
  expected_child_no_scale_transform.Scale(device_scale_factor,
                                          device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_no_scale_transform,
                                  child_no_scale->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_no_scale_transform,
                                  child_no_scale->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, ContentsScale) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 1.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(100, 100));

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> child_empty =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_empty.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(),
                               true);

  scoped_refptr<NoScaleContentLayer> child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(12.f, 12.f),
                               gfx::Size(10, 10),
                               true);

  root->AddChild(parent);

  parent->AddChild(child_scale);
  parent->AddChild(child_empty);
  parent->AddChild(child_no_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 1.f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale, parent);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale * initial_child_scale,
                             child_scale);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale * initial_child_scale,
                             child_empty);
    EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);

    // The parent is scaled up and shouldn't need to scale during draw. The
    // child that can scale its contents should also not need to scale during
    // draw. This shouldn't change if the child has empty bounds. The other
    // children should.
    EXPECT_FLOAT_EQ(1.0, parent->draw_transform().matrix().get(0, 0));
    EXPECT_FLOAT_EQ(1.0, parent->draw_transform().matrix().get(1, 1));
    EXPECT_FLOAT_EQ(1.0, child_scale->draw_transform().matrix().get(0, 0));
    EXPECT_FLOAT_EQ(1.0, child_scale->draw_transform().matrix().get(1, 1));
    EXPECT_FLOAT_EQ(1.0, child_empty->draw_transform().matrix().get(0, 0));
    EXPECT_FLOAT_EQ(1.0, child_empty->draw_transform().matrix().get(1, 1));
    EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor *
                        initial_parent_scale * initial_child_scale,
                    child_no_scale->draw_transform().matrix().get(0, 0));
    EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor *
                        initial_parent_scale * initial_child_scale,
                    child_no_scale->draw_transform().matrix().get(1, 1));
  }

  // If the device_scale_factor or page_scale_factor changes, then it should be
  // updated using the initial transform as the raster scale.
  device_scale_factor = 2.25f;
  page_scale_factor = 1.25f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(
        device_scale_factor * page_scale_factor * initial_parent_scale, parent);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                                 initial_parent_scale * initial_child_scale,
                             child_scale);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale * initial_child_scale,
                             child_empty);
    EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);
  }

  // If the transform changes, we expect the raster scale to be reset to 1.0.
  SkMScalar second_child_scale = 1.75;
  child_scale_matrix.Scale(second_child_scale / initial_child_scale,
                           second_child_scale / initial_child_scale);
  child_scale->SetTransform(child_scale_matrix);
  child_empty->SetTransform(child_scale_matrix);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale,
                             parent);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                             child_scale);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                             child_empty);
    EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);
  }

  // If the device_scale_factor or page_scale_factor changes, then it should be
  // updated, but still using 1.0 as the raster scale.
  device_scale_factor = 2.75f;
  page_scale_factor = 1.75f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale,
                             parent);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                             child_scale);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                             child_empty);
    EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);
  }
}

TEST_F(LayerTreeHostCommonTest,
     ContentsScale_LayerTransformsDontAffectContentsScale) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 1.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(100, 100));

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> child_empty =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_empty.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(),
                               true);

  scoped_refptr<NoScaleContentLayer> child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(12.f, 12.f),
                               gfx::Size(10, 10),
                               true);

  root->AddChild(parent);

  parent->AddChild(child_scale);
  parent->AddChild(child_empty);
  parent->AddChild(child_no_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;

  float device_scale_factor = 2.5f;
  float page_scale_factor = 1.f;

  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = root.get(),
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor, parent);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           child_scale);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           child_empty);
  EXPECT_CONTENTS_SCALE_EQ(1, child_no_scale);

  // Since the transform scale does not affect contents scale, it should affect
  // the draw transform instead.
  EXPECT_FLOAT_EQ(initial_parent_scale,
                  parent->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(initial_parent_scale,
                  parent->draw_transform().matrix().get(1, 1));
  EXPECT_FLOAT_EQ(initial_parent_scale * initial_child_scale,
                  child_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(initial_parent_scale * initial_child_scale,
                  child_scale->draw_transform().matrix().get(1, 1));
  EXPECT_FLOAT_EQ(initial_parent_scale * initial_child_scale,
                  child_empty->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(initial_parent_scale * initial_child_scale,
                  child_empty->draw_transform().matrix().get(1, 1));
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor *
                      initial_parent_scale * initial_child_scale,
                  child_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor *
                      initial_parent_scale * initial_child_scale,
                  child_no_scale->draw_transform().matrix().get(1, 1));
}

TEST_F(LayerTreeHostCommonTest, SmallContentsScale) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 0.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(100, 100));

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  root->AddChild(parent);

  parent->AddChild(child_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 0.01f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale,
                             parent);
    // The child's scale is < 1, so we should not save and use that scale
    // factor.
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor * 1,
                             child_scale);
  }

  // When chilld's total scale becomes >= 1, we should save and use that scale
  // factor.
  child_scale_matrix.MakeIdentity();
  SkMScalar final_child_scale = 0.75;
  child_scale_matrix.Scale(final_child_scale, final_child_scale);
  child_scale->SetTransform(child_scale_matrix);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_application_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale,
                             parent);
    EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                             initial_parent_scale * final_child_scale,
                             child_scale);
  }
}

TEST_F(LayerTreeHostCommonTest, ContentsScaleForSurfaces) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 2.0;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 3.0;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(100, 100));

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> surface_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> surface_scale_child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_scale_child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<NoScaleContentLayer> surface_scale_child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_scale_child_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<NoScaleContentLayer> surface_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(12.f, 12.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> surface_no_scale_child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_no_scale_child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<NoScaleContentLayer> surface_no_scale_child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_no_scale_child_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  root->AddChild(parent);

  parent->AddChild(surface_scale);
  parent->AddChild(surface_no_scale);

  surface_scale->SetForceRenderSurface(true);
  surface_scale->AddChild(surface_scale_child_scale);
  surface_scale->AddChild(surface_scale_child_no_scale);

  surface_no_scale->SetForceRenderSurface(true);
  surface_no_scale->AddChild(surface_no_scale_child_scale);
  surface_no_scale->AddChild(surface_no_scale_child_no_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  SkMScalar device_scale_factor = 5;
  SkMScalar page_scale_factor = 7;

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = root.get();
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_CONTENTS_SCALE_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale, parent);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor *
                               initial_parent_scale * initial_child_scale,
                           surface_scale);
  EXPECT_CONTENTS_SCALE_EQ(1, surface_no_scale);
  EXPECT_CONTENTS_SCALE_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale *
      initial_child_scale * initial_child_scale,
      surface_scale_child_scale);
  EXPECT_CONTENTS_SCALE_EQ(1, surface_scale_child_no_scale);
  EXPECT_CONTENTS_SCALE_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale *
      initial_child_scale * initial_child_scale,
      surface_no_scale_child_scale);
  EXPECT_CONTENTS_SCALE_EQ(1, surface_no_scale_child_no_scale);

  // The parent is scaled up and shouldn't need to scale during draw.
  EXPECT_FLOAT_EQ(1.0, parent->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(1.0, parent->draw_transform().matrix().get(1, 1));

  // RenderSurfaces should always be 1:1 with their target.
  EXPECT_FLOAT_EQ(
      1.0,
      surface_scale->render_surface()->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      1.0,
      surface_scale->render_surface()->draw_transform().matrix().get(1, 1));

  // The surface_scale can apply contents scale so the layer shouldn't need to
  // scale during draw.
  EXPECT_FLOAT_EQ(1.0, surface_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(1.0, surface_scale->draw_transform().matrix().get(1, 1));

  // The surface_scale_child_scale can apply contents scale so it shouldn't need
  // to scale during draw.
  EXPECT_FLOAT_EQ(
      1.0, surface_scale_child_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      1.0, surface_scale_child_scale->draw_transform().matrix().get(1, 1));

  // The surface_scale_child_no_scale can not apply contents scale, so it needs
  // to be scaled during draw.
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale *
          initial_child_scale * initial_child_scale,
      surface_scale_child_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale *
          initial_child_scale * initial_child_scale,
      surface_scale_child_no_scale->draw_transform().matrix().get(1, 1));

  // RenderSurfaces should always be 1:1 with their target.
  EXPECT_FLOAT_EQ(
      1.0,
      surface_no_scale->render_surface()->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      1.0,
      surface_no_scale->render_surface()->draw_transform().matrix().get(1, 1));

  // The surface_no_scale layer can not apply contents scale, so it needs to be
  // scaled during draw.
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor *
                      initial_parent_scale * initial_child_scale,
                  surface_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor *
                      initial_parent_scale * initial_child_scale,
                  surface_no_scale->draw_transform().matrix().get(1, 1));

  // The surface_scale_child_scale can apply contents scale so it shouldn't need
  // to scale during draw.
  EXPECT_FLOAT_EQ(
      1.0, surface_no_scale_child_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      1.0, surface_no_scale_child_scale->draw_transform().matrix().get(1, 1));

  // The surface_scale_child_no_scale can not apply contents scale, so it needs
  // to be scaled during draw.
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale *
          initial_child_scale * initial_child_scale,
      surface_no_scale_child_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_parent_scale *
          initial_child_scale * initial_child_scale,
      surface_no_scale_child_no_scale->draw_transform().matrix().get(1, 1));
}

TEST_F(LayerTreeHostCommonTest,
     ContentsScaleForSurfaces_LayerTransformsDontAffectContentsScale) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 2.0;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 3.0;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(100, 100));

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> surface_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> surface_scale_child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_scale_child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<NoScaleContentLayer> surface_scale_child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_scale_child_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<NoScaleContentLayer> surface_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(12.f, 12.f),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<ContentLayer> surface_no_scale_child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_no_scale_child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  scoped_refptr<NoScaleContentLayer> surface_no_scale_child_no_scale =
      CreateNoScaleDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(surface_no_scale_child_no_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  root->AddChild(parent);

  parent->AddChild(surface_scale);
  parent->AddChild(surface_no_scale);

  surface_scale->SetForceRenderSurface(true);
  surface_scale->AddChild(surface_scale_child_scale);
  surface_scale->AddChild(surface_scale_child_no_scale);

  surface_no_scale->SetForceRenderSurface(true);
  surface_no_scale->AddChild(surface_no_scale_child_scale);
  surface_no_scale->AddChild(surface_no_scale_child_no_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;

  SkMScalar device_scale_factor = 5.0;
  SkMScalar page_scale_factor = 7.0;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.page_scale_factor = page_scale_factor;
  inputs.page_scale_application_layer = root.get();
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           parent);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           surface_scale);
  EXPECT_CONTENTS_SCALE_EQ(1.f, surface_no_scale);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           surface_scale_child_scale);
  EXPECT_CONTENTS_SCALE_EQ(1.f, surface_scale_child_no_scale);
  EXPECT_CONTENTS_SCALE_EQ(device_scale_factor * page_scale_factor,
                           surface_no_scale_child_scale);
  EXPECT_CONTENTS_SCALE_EQ(1.f, surface_no_scale_child_no_scale);

  // The parent is scaled up during draw, since its contents are not scaled by
  // the transform hierarchy.
  EXPECT_FLOAT_EQ(initial_parent_scale,
                  parent->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(initial_parent_scale,
                  parent->draw_transform().matrix().get(1, 1));

  // The child surface is scaled up during draw since its subtree is not scaled
  // by the transform hierarchy.
  EXPECT_FLOAT_EQ(
      initial_parent_scale * initial_child_scale,
      surface_scale->render_surface()->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      initial_parent_scale * initial_child_scale,
      surface_scale->render_surface()->draw_transform().matrix().get(1, 1));

  // The surface_scale's RenderSurface is scaled during draw, so the layer does
  // not need to be scaled when drawing into its surface.
  EXPECT_FLOAT_EQ(1.0, surface_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(1.0, surface_scale->draw_transform().matrix().get(1, 1));

  // The surface_scale_child_scale is scaled when drawing into its surface,
  // since its content bounds are not scaled by the transform hierarchy.
  EXPECT_FLOAT_EQ(
      initial_child_scale,
      surface_scale_child_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      initial_child_scale,
      surface_scale_child_scale->draw_transform().matrix().get(1, 1));

  // The surface_scale_child_no_scale has a fixed contents scale of 1, so it
  // needs to be scaled by the device and page scale factors, along with the
  // transform hierarchy.
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_child_scale,
      surface_scale_child_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_child_scale,
      surface_scale_child_no_scale->draw_transform().matrix().get(1, 1));

  // The child surface is scaled up during draw since its subtree is not scaled
  // by the transform hierarchy.
  EXPECT_FLOAT_EQ(
      initial_parent_scale * initial_child_scale,
      surface_no_scale->render_surface()->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      initial_parent_scale * initial_child_scale,
      surface_no_scale->render_surface()->draw_transform().matrix().get(1, 1));

  // The surface_no_scale layer has a fixed contents scale of 1, so it needs to
  // be scaled by the device and page scale factors. Its surface is already
  // scaled by the transform hierarchy so those don't need to scale the layer's
  // drawing.
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor,
                  surface_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(device_scale_factor * page_scale_factor,
                  surface_no_scale->draw_transform().matrix().get(1, 1));

  // The surface_no_scale_child_scale has its contents scaled by the page and
  // device scale factors, but needs to be scaled by the transform hierarchy
  // when drawing.
  EXPECT_FLOAT_EQ(
      initial_child_scale,
      surface_no_scale_child_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      initial_child_scale,
      surface_no_scale_child_scale->draw_transform().matrix().get(1, 1));

  // The surface_no_scale_child_no_scale has a fixed contents scale of 1, so it
  // needs to be scaled by the device and page scale factors. It also needs to
  // be scaled by any transform heirarchy below its target surface.
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_child_scale,
      surface_no_scale_child_no_scale->draw_transform().matrix().get(0, 0));
  EXPECT_FLOAT_EQ(
      device_scale_factor * page_scale_factor * initial_child_scale,
      surface_no_scale_child_no_scale->draw_transform().matrix().get(1, 1));
}

TEST_F(LayerTreeHostCommonTest, ContentsScaleForAnimatingLayer) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 1.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  scoped_refptr<Layer> root = Layer::Create();
  root->SetBounds(gfx::Size(100, 100));

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true);

  scoped_refptr<ContentLayer> child_scale =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child_scale.get(),
                               child_scale_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  root->AddChild(parent);

  parent->AddChild(child_scale);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // Now put an animating transform on child.
  int animation_id = AddAnimatedTransformToController(
      child_scale->layer_animation_controller(), 10.0, 30, 0);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(initial_parent_scale, parent);
    // The layers with animating transforms should not compute a contents scale
    // other than 1 until they finish animating.
    EXPECT_CONTENTS_SCALE_EQ(1, child_scale);
  }

  // Remove the animation, now it can save a raster scale.
  child_scale->layer_animation_controller()->RemoveAnimation(animation_id);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_CONTENTS_SCALE_EQ(initial_parent_scale, parent);
    // The layers with animating transforms should not compute a contents scale
    // other than 1 until they finish animating.
    EXPECT_CONTENTS_SCALE_EQ(initial_parent_scale * initial_child_scale,
                             child_scale);
  }
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceTransformsInHighDPI) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true);

  scoped_refptr<ContentLayer> child = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  gfx::Transform replica_transform;
  replica_transform.Scale(1.0, -1.0);
  scoped_refptr<ContentLayer> replica = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(replica.get(),
                               replica_transform,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               true);

  // This layer should end up in the same surface as child, with the same draw
  // and screen space transforms.
  scoped_refptr<ContentLayer> duplicate_child_non_owner =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(duplicate_child_non_owner.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true);

  parent->AddChild(child);
  child->AddChild(duplicate_child_non_owner);
  child->SetReplicaLayer(replica.get());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  RenderSurfaceLayerList render_surface_layer_list;

  float device_scale_factor = 1.5f;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because it has a replica layer).
  EXPECT_EQ(2u, render_surface_layer_list.size());

  gfx::Transform expected_parent_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->draw_transform());

  gfx::Transform expected_draw_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_draw_transform,
                                  child->draw_transform());

  gfx::Transform expected_screen_space_transform;
  expected_screen_space_transform.Translate(
      device_scale_factor * child->position().x(),
      device_scale_factor * child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_screen_space_transform,
                                  child->screen_space_transform());

  gfx::Transform expected_duplicate_child_draw_transform =
      child->draw_transform();
  EXPECT_TRANSFORMATION_MATRIX_EQ(child->draw_transform(),
                                  duplicate_child_non_owner->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      child->screen_space_transform(),
      duplicate_child_non_owner->screen_space_transform());
  EXPECT_RECT_EQ(child->drawable_content_rect(),
                 duplicate_child_non_owner->drawable_content_rect());
  EXPECT_EQ(child->content_bounds(),
            duplicate_child_non_owner->content_bounds());

  gfx::Transform expected_render_surface_draw_transform;
  expected_render_surface_draw_transform.Translate(
      device_scale_factor * child->position().x(),
      device_scale_factor * child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_render_surface_draw_transform,
                                  child->render_surface()->draw_transform());

  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(device_scale_factor * 2.f,
                                            device_scale_factor * 2.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_surface_draw_transform,
                                  child->render_surface()->draw_transform());

  gfx::Transform expected_surface_screen_space_transform;
  expected_surface_screen_space_transform.Translate(device_scale_factor * 2.f,
                                                    device_scale_factor * 2.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_screen_space_transform,
      child->render_surface()->screen_space_transform());

  gfx::Transform expected_replica_draw_transform;
  expected_replica_draw_transform.matrix().set(1, 1, -1.0);
  expected_replica_draw_transform.matrix().set(0, 3, 6.0);
  expected_replica_draw_transform.matrix().set(1, 3, 6.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_draw_transform,
      child->render_surface()->replica_draw_transform());

  gfx::Transform expected_replica_screen_space_transform;
  expected_replica_screen_space_transform.matrix().set(1, 1, -1.0);
  expected_replica_screen_space_transform.matrix().set(0, 3, 6.0);
  expected_replica_screen_space_transform.matrix().set(1, 3, 6.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_screen_space_transform,
      child->render_surface()->replica_screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_screen_space_transform,
      child->render_surface()->replica_screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest,
     RenderSurfaceTransformsInHighDPIAccurateScaleZeroPosition) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  scoped_refptr<ContentLayer> parent = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(33, 31),
                               true);

  scoped_refptr<ContentLayer> child = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(13, 11),
                               true);

  gfx::Transform replica_transform;
  replica_transform.Scale(1.0, -1.0);
  scoped_refptr<ContentLayer> replica = CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(replica.get(),
                               replica_transform,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(13, 11),
                               true);

  // This layer should end up in the same surface as child, with the same draw
  // and screen space transforms.
  scoped_refptr<ContentLayer> duplicate_child_non_owner =
      CreateDrawableContentLayer(&delegate);
  SetLayerPropertiesForTesting(duplicate_child_non_owner.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(13, 11),
                               true);

  parent->AddChild(child);
  child->AddChild(duplicate_child_non_owner);
  child->SetReplicaLayer(replica.get());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(parent);

  float device_scale_factor = 1.7f;

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because it has a replica layer).
  EXPECT_EQ(2u, render_surface_layer_list.size());

  gfx::Transform identity_transform;

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform, parent->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  duplicate_child_non_owner->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      identity_transform, duplicate_child_non_owner->screen_space_transform());
  EXPECT_RECT_EQ(child->drawable_content_rect(),
                 duplicate_child_non_owner->drawable_content_rect());
  EXPECT_EQ(child->content_bounds(),
            duplicate_child_non_owner->content_bounds());

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      identity_transform, child->render_surface()->screen_space_transform());

  gfx::Transform expected_replica_draw_transform;
  expected_replica_draw_transform.matrix().set(1, 1, -1.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_draw_transform,
      child->render_surface()->replica_draw_transform());

  gfx::Transform expected_replica_screen_space_transform;
  expected_replica_screen_space_transform.matrix().set(1, 1, -1.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_screen_space_transform,
      child->render_surface()->replica_screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, SubtreeSearch) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<Layer> grand_child = Layer::Create();
  scoped_refptr<Layer> mask_layer = Layer::Create();
  scoped_refptr<Layer> replica_layer = Layer::Create();

  grand_child->SetReplicaLayer(replica_layer.get());
  child->AddChild(grand_child.get());
  child->SetMaskLayer(mask_layer.get());
  root->AddChild(child.get());

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  int nonexistent_id = -1;
  EXPECT_EQ(root,
            LayerTreeHostCommon::FindLayerInSubtree(root.get(), root->id()));
  EXPECT_EQ(child,
            LayerTreeHostCommon::FindLayerInSubtree(root.get(), child->id()));
  EXPECT_EQ(
      grand_child,
      LayerTreeHostCommon::FindLayerInSubtree(root.get(), grand_child->id()));
  EXPECT_EQ(
      mask_layer,
      LayerTreeHostCommon::FindLayerInSubtree(root.get(), mask_layer->id()));
  EXPECT_EQ(
      replica_layer,
      LayerTreeHostCommon::FindLayerInSubtree(root.get(), replica_layer->id()));
  EXPECT_EQ(
      0, LayerTreeHostCommon::FindLayerInSubtree(root.get(), nonexistent_id));
}

TEST_F(LayerTreeHostCommonTest, TransparentChildRenderSurfaceCreation) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent());

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);

  root->AddChild(child);
  child->AddChild(grand_child);
  child->SetOpacity(0.5f);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_FALSE(child->render_surface());
}

TEST_F(LayerTreeHostCommonTest, OpacityAnimatingOnPendingTree) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.pending_tree(), 1);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false);
  root->SetDrawsContent(true);

  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.pending_tree(), 2);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  child->SetDrawsContent(true);
  child->SetOpacity(0.0f);

  // Add opacity animation.
  AddOpacityTransitionToController(
      child->layer_animation_controller(), 10.0, 0.0f, 1.0f, false);

  root->AddChild(child.Pass());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and two layers. The child
  // layer should be included even though it is transparent.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
}

typedef std::tr1::tuple<bool, bool> LCDTextTestParam;
class LCDTextTest
    : public LayerTreeHostCommonTestBase,
      public testing::TestWithParam<LCDTextTestParam> {
 protected:
  virtual void SetUp() {
    can_use_lcd_text_ = std::tr1::get<0>(GetParam());

    root_ = Layer::Create();
    child_ = Layer::Create();
    grand_child_ = Layer::Create();
    child_->AddChild(grand_child_.get());
    root_->AddChild(child_.get());

    gfx::Transform identity_matrix;
    SetLayerPropertiesForTesting(root_.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 gfx::PointF(),
                                 gfx::PointF(),
                                 gfx::Size(1, 1),
                                 false);
    SetLayerPropertiesForTesting(child_.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 gfx::PointF(),
                                 gfx::PointF(),
                                 gfx::Size(1, 1),
                                 false);
    SetLayerPropertiesForTesting(grand_child_.get(),
                                 identity_matrix,
                                 identity_matrix,
                                 gfx::PointF(),
                                 gfx::PointF(),
                                 gfx::Size(1, 1),
                                 false);

    child_->SetForceRenderSurface(std::tr1::get<1>(GetParam()));

    host_ = FakeLayerTreeHost::Create();
    host_->SetRootLayer(root_);
  }

  bool can_use_lcd_text_;
  scoped_ptr<FakeLayerTreeHost> host_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> child_;
  scoped_refptr<Layer> grand_child_;
};

TEST_P(LCDTextTest, CanUseLCDText) {
  // Case 1: Identity transform.
  gfx::Transform identity_matrix;
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, child_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, grand_child_->can_use_lcd_text());

  // Case 2: Integral translation.
  gfx::Transform integral_translation;
  integral_translation.Translate(1.0, 2.0);
  child_->SetTransform(integral_translation);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, child_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, grand_child_->can_use_lcd_text());

  // Case 3: Non-integral translation.
  gfx::Transform non_integral_translation;
  non_integral_translation.Translate(1.5, 2.5);
  child_->SetTransform(non_integral_translation);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_FALSE(child_->can_use_lcd_text());
  EXPECT_FALSE(grand_child_->can_use_lcd_text());

  // Case 4: Rotation.
  gfx::Transform rotation;
  rotation.Rotate(10.0);
  child_->SetTransform(rotation);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_FALSE(child_->can_use_lcd_text());
  EXPECT_FALSE(grand_child_->can_use_lcd_text());

  // Case 5: Scale.
  gfx::Transform scale;
  scale.Scale(2.0, 2.0);
  child_->SetTransform(scale);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_FALSE(child_->can_use_lcd_text());
  EXPECT_FALSE(grand_child_->can_use_lcd_text());

  // Case 6: Skew.
  gfx::Transform skew;
  skew.SkewX(10.0);
  child_->SetTransform(skew);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_FALSE(child_->can_use_lcd_text());
  EXPECT_FALSE(grand_child_->can_use_lcd_text());

  // Case 7: Translucent.
  child_->SetTransform(identity_matrix);
  child_->SetOpacity(0.5f);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_FALSE(child_->can_use_lcd_text());
  EXPECT_FALSE(grand_child_->can_use_lcd_text());

  // Case 8: Sanity check: restore transform and opacity.
  child_->SetTransform(identity_matrix);
  child_->SetOpacity(1.f);
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, child_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, grand_child_->can_use_lcd_text());
}

TEST_P(LCDTextTest, CanUseLCDTextWithAnimation) {
  // Sanity check: Make sure can_use_lcd_text_ is set on each node.
  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, child_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, grand_child_->can_use_lcd_text());

  // Add opacity animation.
  child_->SetOpacity(0.9f);
  AddOpacityTransitionToController(
      child_->layer_animation_controller(), 10.0, 0.9f, 0.1f, false);

  ExecuteCalculateDrawProperties(
      root_.get(), 1.f, 1.f, NULL, can_use_lcd_text_);
  // Text AA should not be adjusted while animation is active.
  // Make sure LCD text AA setting remains unchanged.
  EXPECT_EQ(can_use_lcd_text_, root_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, child_->can_use_lcd_text());
  EXPECT_EQ(can_use_lcd_text_, grand_child_->can_use_lcd_text());
}

INSTANTIATE_TEST_CASE_P(LayerTreeHostCommonTest,
                        LCDTextTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_SingleLayer) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create();
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> child = Layer::Create();
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  child->SetIsDrawable(true);

  scoped_refptr<Layer> grand_child = Layer::Create();
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  grand_child->SetIsDrawable(true);
  grand_child->SetHideLayerAndSubtree(true);

  child->AddChild(grand_child);
  root->AddChild(child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and two layers. The grand child has
  // hidden itself.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(child->id(), root->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_SingleLayerImpl) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.pending_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetDrawsContent(true);

  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.pending_tree(), 2);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  child->SetDrawsContent(true);

  scoped_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  grand_child->SetDrawsContent(true);
  grand_child->SetHideLayerAndSubtree(true);

  child->AddChild(grand_child.Pass());
  root->AddChild(child.Pass());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and two layers. The grand child has
  // hidden itself.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(1, root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(2, root->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_TwoLayers) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create();
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> child = Layer::Create();
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  child->SetIsDrawable(true);
  child->SetHideLayerAndSubtree(true);

  scoped_refptr<Layer> grand_child = Layer::Create();
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  grand_child->SetIsDrawable(true);

  child->AddChild(grand_child);
  root->AddChild(child);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and one layers. The child has
  // hidden itself and the grand child.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_TwoLayersImpl) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.pending_tree(), 1);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetDrawsContent(true);

  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.pending_tree(), 2);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  child->SetDrawsContent(true);
  child->SetHideLayerAndSubtree(true);

  scoped_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  grand_child->SetDrawsContent(true);

  child->AddChild(grand_child.Pass());
  root->AddChild(child.Pass());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and one layers. The child has
  // hidden itself and the grand child.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  EXPECT_EQ(1, root->render_surface()->layer_list().at(0)->id());
}

void EmptyCopyOutputCallback(scoped_ptr<CopyOutputResult> result) {}

TEST_F(LayerTreeHostCommonTest, SubtreeHiddenWithCopyRequest) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create();
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> copy_grand_parent = Layer::Create();
  SetLayerPropertiesForTesting(copy_grand_parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  copy_grand_parent->SetIsDrawable(true);

  scoped_refptr<Layer> copy_parent = Layer::Create();
  SetLayerPropertiesForTesting(copy_parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  copy_parent->SetIsDrawable(true);
  copy_parent->SetForceRenderSurface(true);

  scoped_refptr<Layer> copy_layer = Layer::Create();
  SetLayerPropertiesForTesting(copy_layer.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  copy_layer->SetIsDrawable(true);

  scoped_refptr<Layer> copy_child = Layer::Create();
  SetLayerPropertiesForTesting(copy_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  copy_child->SetIsDrawable(true);

  scoped_refptr<Layer> copy_grand_parent_sibling_before = Layer::Create();
  SetLayerPropertiesForTesting(copy_grand_parent_sibling_before.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  copy_grand_parent_sibling_before->SetIsDrawable(true);

  scoped_refptr<Layer> copy_grand_parent_sibling_after = Layer::Create();
  SetLayerPropertiesForTesting(copy_grand_parent_sibling_after.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  copy_grand_parent_sibling_after->SetIsDrawable(true);

  copy_layer->AddChild(copy_child);
  copy_parent->AddChild(copy_layer);
  copy_grand_parent->AddChild(copy_parent);
  root->AddChild(copy_grand_parent_sibling_before);
  root->AddChild(copy_grand_parent);
  root->AddChild(copy_grand_parent_sibling_after);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  // Hide the copy_grand_parent and its subtree. But make a copy request in that
  // hidden subtree on copy_layer.
  copy_grand_parent->SetHideLayerAndSubtree(true);
  copy_grand_parent_sibling_before->SetHideLayerAndSubtree(true);
  copy_grand_parent_sibling_after->SetHideLayerAndSubtree(true);
  copy_layer->RequestCopyOfOutput(CopyOutputRequest::CreateRequest(
      base::Bind(&EmptyCopyOutputCallback)));
  EXPECT_TRUE(copy_layer->HasCopyRequest());

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_TRUE(root->draw_properties().layer_or_descendant_has_copy_request);
  EXPECT_TRUE(copy_grand_parent->draw_properties().
              layer_or_descendant_has_copy_request);
  EXPECT_TRUE(copy_parent->draw_properties().
              layer_or_descendant_has_copy_request);
  EXPECT_TRUE(copy_layer->draw_properties().
              layer_or_descendant_has_copy_request);
  EXPECT_FALSE(copy_child->draw_properties().
               layer_or_descendant_has_copy_request);
  EXPECT_FALSE(copy_grand_parent_sibling_before->draw_properties().
               layer_or_descendant_has_copy_request);
  EXPECT_FALSE(copy_grand_parent_sibling_after->draw_properties().
               layer_or_descendant_has_copy_request);

  // We should have three render surfaces, one for the root, one for the parent
  // since it owns a surface, and one for the copy_layer.
  ASSERT_EQ(3u, render_surface_layer_list.size());
  EXPECT_EQ(root->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(copy_parent->id(), render_surface_layer_list.at(1)->id());
  EXPECT_EQ(copy_layer->id(), render_surface_layer_list.at(2)->id());

  // The root render surface should have 2 contributing layers. The
  // copy_grand_parent is hidden along with its siblings, but the copy_parent
  // will appear since something in its subtree needs to be drawn for a copy
  // request.
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(copy_parent->id(),
            root->render_surface()->layer_list().at(1)->id());

  // Nothing actually drawns into the copy parent, so only the copy_layer will
  // appear in its list, since it needs to be drawn for the copy request.
  ASSERT_EQ(1u, copy_parent->render_surface()->layer_list().size());
  EXPECT_EQ(copy_layer->id(),
            copy_parent->render_surface()->layer_list().at(0)->id());

  // The copy_layer's render surface should have two contributing layers.
  ASSERT_EQ(2u, copy_layer->render_surface()->layer_list().size());
  EXPECT_EQ(copy_layer->id(),
            copy_layer->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(copy_child->id(),
            copy_layer->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, ClippedOutCopyRequest) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create();
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> copy_parent = Layer::Create();
  SetLayerPropertiesForTesting(copy_parent.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(),
                               false);
  copy_parent->SetIsDrawable(true);
  copy_parent->SetMasksToBounds(true);

  scoped_refptr<Layer> copy_layer = Layer::Create();
  SetLayerPropertiesForTesting(copy_layer.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  copy_layer->SetIsDrawable(true);

  scoped_refptr<Layer> copy_child = Layer::Create();
  SetLayerPropertiesForTesting(copy_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  copy_child->SetIsDrawable(true);

  copy_layer->AddChild(copy_child);
  copy_parent->AddChild(copy_layer);
  root->AddChild(copy_parent);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  copy_layer->RequestCopyOfOutput(CopyOutputRequest::CreateRequest(
      base::Bind(&EmptyCopyOutputCallback)));
  EXPECT_TRUE(copy_layer->HasCopyRequest());

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface, as the others are clipped out.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  EXPECT_EQ(root->id(), render_surface_layer_list.at(0)->id());

  // The root render surface should only have 1 contributing layer, since the
  // other layers are empty/clipped away.
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectInsideSurface) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create();
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  root->SetIsDrawable(true);

  // The surface is moved slightly outside of the viewport.
  scoped_refptr<Layer> surface = Layer::Create();
  SetLayerPropertiesForTesting(surface.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(-10, -20),
                               gfx::Size(),
                               false);
  surface->SetForceRenderSurface(true);

  scoped_refptr<Layer> surface_child = Layer::Create();
  SetLayerPropertiesForTesting(surface_child.get(),
                               identity_matrix,
                               identity_matrix,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  surface_child->SetIsDrawable(true);

  surface->AddChild(surface_child);
  root->AddChild(surface);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // The visible_content_rect for the |surface_child| should not be clipped by
  // the viewport.
  EXPECT_EQ(gfx::Rect(50, 50).ToString(),
            surface_child->visible_content_rect().ToString());
}

TEST_F(LayerTreeHostCommonTest, TransformedClipParent) {
  // Ensure that a transform between the layer and its render surface is not a
  // problem. Constructs the following layer tree.
  //
  //   root (a render surface)
  //     + render_surface
  //       + clip_parent (scaled)
  //         + intervening_clipping_layer
  //           + clip_child
  //
  // The render surface should be resized correctly and the clip child should
  // inherit the right clip rect.
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface = Layer::Create();
  scoped_refptr<Layer> clip_parent = Layer::Create();
  scoped_refptr<Layer> intervening = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(render_surface);
  render_surface->AddChild(clip_parent);
  clip_parent->AddChild(intervening);
  intervening->AddChild(clip_child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  render_surface->SetForceRenderSurface(true);

  gfx::Transform scale_transform;
  scale_transform.Scale(2, 2);

  gfx::Transform identity_transform;

  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(render_surface.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               scale_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(5, 5),
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(10, 10),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(root->render_surface());
  ASSERT_TRUE(render_surface->render_surface());

  // Ensure that we've inherited our clip parent's clip and weren't affected
  // by the intervening clip layer.
  ASSERT_EQ(gfx::Rect(1, 1, 20, 20).ToString(),
            clip_parent->clip_rect().ToString());
  ASSERT_EQ(clip_parent->clip_rect().ToString(),
            clip_child->clip_rect().ToString());
  ASSERT_EQ(gfx::Rect(3, 3, 10, 10).ToString(),
            intervening->clip_rect().ToString());

  // Ensure that the render surface reports a content rect that has been grown
  // to accomodate for the clip child.
  ASSERT_EQ(gfx::Rect(5, 5, 16, 16).ToString(),
            render_surface->render_surface()->content_rect().ToString());

  // The above check implies the two below, but they nicely demonstrate that
  // we've grown, despite the intervening layer's clip.
  ASSERT_TRUE(clip_parent->clip_rect().Contains(
      render_surface->render_surface()->content_rect()));
  ASSERT_FALSE(intervening->clip_rect().Contains(
      render_surface->render_surface()->content_rect()));
}

TEST_F(LayerTreeHostCommonTest, ClipParentWithInterveningRenderSurface) {
  // Ensure that intervening render surfaces are not a problem in the basic
  // case. In the following tree, both render surfaces should be resized to
  // accomodate for the clip child, despite an intervening clip.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1 (sets opacity)
  //        + intervening (masks to bounds)
  //          + render_surface2 (also sets opacity)
  //            + clip_child
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> clip_parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<Layer> intervening = Layer::Create();
  scoped_refptr<Layer> render_surface2 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(clip_parent);
  clip_parent->AddChild(render_surface1);
  render_surface1->AddChild(intervening);
  intervening->AddChild(render_surface2);
  render_surface2->AddChild(clip_child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  gfx::Transform translation_transform;
  translation_transform.Translate(2, 2);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               translation_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(5, 5),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(-10.f, -10.f),
                               gfx::Size(60, 60),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  // Since the render surfaces could have expanded, they should not clip (their
  // bounds would no longer be reliable). We should resort to layer clipping
  // in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface1->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface2->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface2->render_surface()->is_clipped());

  // NB: clip rects are in target space.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->clip_rect().ToString());
  EXPECT_TRUE(render_surface1->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface2->clip_rect().ToString());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The content rects of both render surfaces should both have expanded to
  // contain the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->render_surface()->content_rect().ToString());
  EXPECT_EQ(gfx::Rect(-1, -1, 40, 40).ToString(),
            render_surface2->render_surface()->content_rect().ToString());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), and should have the correctly sized visible
  // content rect.
  EXPECT_EQ(gfx::Rect(-1, -1, 40, 40).ToString(),
            clip_child->clip_rect().ToString());
  EXPECT_EQ(gfx::Rect(9, 9, 40, 40).ToString(),
            clip_child->visible_content_rect().ToString());
  EXPECT_TRUE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClipParentScrolledInterveningLayer) {
  // Ensure that intervening render surfaces are not a problem, even if there
  // is a scroll involved. Note, we do _not_ have to consider any other sort
  // of transform.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1 (sets opacity)
  //        + intervening (masks to bounds AND scrolls)
  //          + render_surface2 (also sets opacity)
  //            + clip_child
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> clip_parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<Layer> intervening = Layer::Create();
  scoped_refptr<Layer> render_surface2 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(clip_parent);
  clip_parent->AddChild(render_surface1);
  render_surface1->AddChild(intervening);
  intervening->AddChild(render_surface2);
  render_surface2->AddChild(clip_child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);
  intervening->SetScrollable(true);
  intervening->SetMaxScrollOffset(gfx::Vector2d(50, 50));
  intervening->SetScrollOffset(gfx::Vector2d(3, 3));

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  gfx::Transform translation_transform;
  translation_transform.Translate(2, 2);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               translation_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(5, 5),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(-10.f, -10.f),
                               gfx::Size(60, 60),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  // Since the render surfaces could have expanded, they should not clip (their
  // bounds would no longer be reliable). We should resort to layer clipping
  // in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface1->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface2->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface2->render_surface()->is_clipped());

  // NB: clip rects are in target space.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->clip_rect().ToString());
  EXPECT_TRUE(render_surface1->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(2, 2, 3, 3).ToString(),
            render_surface2->clip_rect().ToString());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The content rects of both render surfaces should both have expanded to
  // contain the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->render_surface()->content_rect().ToString());
  EXPECT_EQ(gfx::Rect(2, 2, 40, 40).ToString(),
            render_surface2->render_surface()->content_rect().ToString());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), and should have the correctly sized visible
  // content rect.
  EXPECT_EQ(gfx::Rect(2, 2, 40, 40).ToString(),
            clip_child->clip_rect().ToString());
  EXPECT_EQ(gfx::Rect(12, 12, 40, 40).ToString(),
            clip_child->visible_content_rect().ToString());
  EXPECT_TRUE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, DescendantsOfClipChildren) {
  // Ensures that descendants of the clip child inherit the correct clip.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + intervening (masks to bounds)
  //        + clip_child
  //          + child
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> clip_parent = Layer::Create();
  scoped_refptr<Layer> intervening = Layer::Create();
  scoped_refptr<Layer> clip_child = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(clip_parent);
  clip_parent->AddChild(intervening);
  intervening->AddChild(clip_child);
  clip_child->AddChild(child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(5, 5),
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(60, 60),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(60, 60),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());

  // Neither the clip child nor its descendant should have inherited the clip
  // from |intervening|.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            clip_child->clip_rect().ToString());
  EXPECT_TRUE(clip_child->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            child->visible_content_rect().ToString());
  EXPECT_TRUE(child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest,
       SurfacesShouldBeUnaffectedByNonDescendantClipChildren) {
  // Ensures that non-descendant clip children in the tree do not affect
  // render surfaces.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1
  //        + clip_child
  //      + render_surface2
  //        + non_clip_child
  //
  // In this example render_surface2 should be unaffected by clip_child.
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> clip_parent = Layer::Create();
  scoped_refptr<Layer> render_surface1 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);
  scoped_refptr<Layer> render_surface2 = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> non_clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(clip_parent);
  clip_parent->AddChild(render_surface1);
  render_surface1->AddChild(clip_child);
  clip_parent->AddChild(render_surface2);
  render_surface2->AddChild(non_clip_child);

  clip_child->SetClipParent(clip_parent.get());

  clip_parent->SetMasksToBounds(true);
  render_surface1->SetMasksToBounds(true);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(15, 15),
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(5, 5),
                               gfx::Size(5, 5),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(5, 5),
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(-1, 1),
                               gfx::Size(10, 10),
                               false);
  SetLayerPropertiesForTesting(non_clip_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(5, 5),
                               false);

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface1->clip_rect().ToString());
  EXPECT_TRUE(render_surface1->is_clipped());

  // The render surface should not clip (it has unclipped descendants), instead
  // it should rely on layer clipping.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface1->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());

  // That said, it should have grown to accomodate the unclipped descendant.
  EXPECT_EQ(gfx::Rect(-1, 1, 6, 4).ToString(),
            render_surface1->render_surface()->content_rect().ToString());

  // This render surface should clip. It has no unclipped descendants.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface2->clip_rect().ToString());
  EXPECT_TRUE(render_surface2->render_surface()->is_clipped());

  // It also shouldn't have grown to accomodate the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface2->render_surface()->content_rect().ToString());

  // Sanity check our num_unclipped_descendants values.
  EXPECT_EQ(1, render_surface1->num_unclipped_descendants());
  EXPECT_EQ(0, render_surface2->num_unclipped_descendants());
}

TEST_F(LayerTreeHostCommonTest, CanRenderToSeparateSurface) {
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);
  scoped_ptr<LayerImpl> child1 =
      LayerImpl::Create(host_impl.active_tree(), 123456);
  scoped_ptr<LayerImpl> child2 =
      LayerImpl::Create(host_impl.active_tree(), 1234567);
  scoped_ptr<LayerImpl> child3 =
      LayerImpl::Create(host_impl.active_tree(), 12345678);

  gfx::Transform identity_matrix;
  gfx::PointF anchor;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  root->SetDrawsContent(true);

  // This layer structure normally forces render surface due to preserves3d
  // behavior.
  bool preserves3d = true;
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               preserves3d);
  child1->SetDrawsContent(true);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  child2->SetDrawsContent(true);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               identity_matrix,
                               anchor,
                               position,
                               bounds,
                               false);
  child3->SetDrawsContent(true);

  child2->AddChild(child3.Pass());
  child1->AddChild(child2.Pass());
  root->AddChild(child1.Pass());

  {
    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.can_render_to_separate_surface = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_EQ(2u, render_surface_layer_list.size());
  }

  {
    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.can_render_to_separate_surface = false;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_EQ(1u, render_surface_layer_list.size());
  }
}

TEST_F(LayerTreeHostCommonTest, DoNotIncludeBackfaceInvisibleSurfaces) {
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> render_surface = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(render_surface);
  render_surface->AddChild(child);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(render_surface.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);

  root->SetPreserves3d(true);
  render_surface->SetDoubleSided(false);
  render_surface->SetForceRenderSurface(true);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(2u, render_surface_layer_list()->size());
  EXPECT_EQ(1u,
            render_surface_layer_list()->at(0)
                ->render_surface()->layer_list().size());
  EXPECT_EQ(1u,
            render_surface_layer_list()->at(1)
                ->render_surface()->layer_list().size());

  gfx::Transform rotation_transform = identity_transform;
  rotation_transform.RotateAboutXAxis(180.0);

  render_surface->SetTransform(rotation_transform);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(1u, render_surface_layer_list()->size());
  EXPECT_EQ(0u,
            render_surface_layer_list()->at(0)
                ->render_surface()->layer_list().size());
}

TEST_F(LayerTreeHostCommonTest, ClippedByScrollParent) {
  // Checks that the simple case (being clipped by a scroll parent that would
  // have been processed before you anyhow) results in the right clips.
  //
  // + root
  //   + scroll_parent_border
  //   | + scroll_parent_clip
  //   |   + scroll_parent
  //   + scroll_child
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> scroll_parent_border = Layer::Create();
  scoped_refptr<Layer> scroll_parent_clip = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> scroll_parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent);
  scoped_refptr<LayerWithForcedDrawsContent> scroll_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(scroll_child);

  root->AddChild(scroll_parent_border);
  scroll_parent_border->AddChild(scroll_parent_clip);
  scroll_parent_clip->AddChild(scroll_parent);

  scroll_parent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent.get());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(scroll_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 30, 30).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClippedByOutOfOrderScrollParent) {
  // Checks that clipping by a scroll parent that follows you in paint order
  // still results in correct clipping.
  //
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> scroll_parent_border = Layer::Create();
  scoped_refptr<Layer> scroll_parent_clip = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> scroll_parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent);
  scoped_refptr<LayerWithForcedDrawsContent> scroll_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(scroll_parent_border);
  scroll_parent_border->AddChild(scroll_parent_clip);
  scroll_parent_clip->AddChild(scroll_parent);

  root->AddChild(scroll_child);

  scroll_parent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent.get());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(scroll_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 30, 30).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClippedByOutOfOrderScrollGrandparent) {
  // Checks that clipping by a scroll parent and scroll grandparent that follow
  // you in paint order still results in correct clipping.
  //
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //   | + scroll_parent_clip
  //   |   + scroll_parent
  //   + scroll_grandparent_border
  //     + scroll_grandparent_clip
  //       + scroll_grandparent
  //
  scoped_refptr<Layer> root = Layer::Create();
  scoped_refptr<Layer> scroll_parent_border = Layer::Create();
  scoped_refptr<Layer> scroll_parent_clip = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> scroll_parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  scoped_refptr<Layer> scroll_grandparent_border = Layer::Create();
  scoped_refptr<Layer> scroll_grandparent_clip = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> scroll_grandparent =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  scoped_refptr<LayerWithForcedDrawsContent> scroll_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(scroll_child);

  root->AddChild(scroll_parent_border);
  scroll_parent_border->AddChild(scroll_parent_clip);
  scroll_parent_clip->AddChild(scroll_parent);

  root->AddChild(scroll_grandparent_border);
  scroll_grandparent_border->AddChild(scroll_grandparent_clip);
  scroll_grandparent_clip->AddChild(scroll_grandparent);

  scroll_parent_clip->SetMasksToBounds(true);
  scroll_grandparent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent.get());
  scroll_parent_border->SetScrollParent(scroll_grandparent.get());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_grandparent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_grandparent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(scroll_grandparent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(scroll_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 20, 20).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());

  // Despite the fact that we visited the above layers out of order to get the
  // correct clip, the layer lists should be unaffected.
  EXPECT_EQ(3u, root->render_surface()->layer_list().size());
  EXPECT_EQ(scroll_child.get(),
            root->render_surface()->layer_list().at(0));
  EXPECT_EQ(scroll_parent.get(),
            root->render_surface()->layer_list().at(1));
  EXPECT_EQ(scroll_grandparent.get(),
            root->render_surface()->layer_list().at(2));
}

TEST_F(LayerTreeHostCommonTest, OutOfOrderClippingRequiresRSLLSorting) {
  // Ensures that even if we visit layers out of order, we still produce a
  // correctly ordered render surface layer list.
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //         + render_surface1
  //   + scroll_grandparent_border
  //     + scroll_grandparent_clip
  //       + scroll_grandparent
  //         + render_surface2
  //
  scoped_refptr<LayerWithForcedDrawsContent> root =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  scoped_refptr<Layer> scroll_parent_border = Layer::Create();
  scoped_refptr<Layer> scroll_parent_clip = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> scroll_parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent);
  scoped_refptr<LayerWithForcedDrawsContent> render_surface1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  scoped_refptr<Layer> scroll_grandparent_border = Layer::Create();
  scoped_refptr<Layer> scroll_grandparent_clip = Layer::Create();
  scoped_refptr<LayerWithForcedDrawsContent> scroll_grandparent =
      make_scoped_refptr(new LayerWithForcedDrawsContent);
  scoped_refptr<LayerWithForcedDrawsContent> render_surface2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  scoped_refptr<LayerWithForcedDrawsContent> scroll_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent);

  root->AddChild(scroll_child);

  root->AddChild(scroll_parent_border);
  scroll_parent_border->AddChild(scroll_parent_clip);
  scroll_parent_clip->AddChild(scroll_parent);
  scroll_parent->AddChild(render_surface2);

  root->AddChild(scroll_grandparent_border);
  scroll_grandparent_border->AddChild(scroll_grandparent_clip);
  scroll_grandparent_clip->AddChild(scroll_grandparent);
  scroll_grandparent->AddChild(render_surface1);

  scroll_parent_clip->SetMasksToBounds(true);
  scroll_grandparent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent.get());
  scroll_parent_border->SetScrollParent(scroll_grandparent.get());

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_grandparent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_grandparent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               false);
  SetLayerPropertiesForTesting(scroll_grandparent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(scroll_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(),
      root->bounds(),
      identity_transform,
      &render_surface_layer_list);

  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 20, 20).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());

  // Despite the fact that we had to process the layers out of order to get the
  // right clip, our render_surface_layer_list's order should be unaffected.
  EXPECT_EQ(3u, render_surface_layer_list.size());
  EXPECT_EQ(root.get(), render_surface_layer_list.at(0));
  EXPECT_EQ(render_surface2.get(), render_surface_layer_list.at(1));
  EXPECT_EQ(render_surface1.get(), render_surface_layer_list.at(2));
  EXPECT_TRUE(render_surface_layer_list.at(0)->render_surface());
  EXPECT_TRUE(render_surface_layer_list.at(1)->render_surface());
  EXPECT_TRUE(render_surface_layer_list.at(2)->render_surface());
}

TEST_F(LayerTreeHostCommonTest, DoNotClobberSorting) {
  // We rearrange layer list contributions if we have to visit children out of
  // order, but it should be a 'stable' rearrangement. That is, the layer list
  // additions for a single layer should not be reordered, though their position
  // wrt to the contributions due to a sibling may vary.
  //
  // + root
  //   + scroll_child
  //     + top_content
  //     + bottom_content
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> scroll_parent_border =
      LayerImpl::Create(host_impl.active_tree(), 2);
  scoped_ptr<LayerImpl> scroll_parent_clip =
      LayerImpl::Create(host_impl.active_tree(), 3);
  scoped_ptr<LayerImpl> scroll_parent =
      LayerImpl::Create(host_impl.active_tree(), 4);
  scoped_ptr<LayerImpl> scroll_child =
      LayerImpl::Create(host_impl.active_tree(), 5);
  scoped_ptr<LayerImpl> bottom_content =
      LayerImpl::Create(host_impl.active_tree(), 6);
  scoped_ptr<LayerImpl> top_content =
      LayerImpl::Create(host_impl.active_tree(), 7);

  scroll_parent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent.get());
  scoped_ptr<std::set<LayerImpl*> > scroll_children(new std::set<LayerImpl*>);
  scroll_children->insert(scroll_child.get());
  scroll_parent->SetScrollChildren(scroll_children.release());

  scroll_child->SetDrawsContent(true);
  scroll_parent->SetDrawsContent(true);
  top_content->SetDrawsContent(true);
  bottom_content->SetDrawsContent(true);

  gfx::Transform identity_transform;
  gfx::Transform top_transform;
  top_transform.Translate3d(0.0, 0.0, 5.0);
  gfx::Transform bottom_transform;
  bottom_transform.Translate3d(0.0, 0.0, 3.0);

  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_border.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroll_parent_clip.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(scroll_parent.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(scroll_child.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(top_content.get(),
                               top_transform,
                               top_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(bottom_content.get(),
                               bottom_transform,
                               bottom_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);

  scroll_child->SetPreserves3d(true);

  scroll_child->AddChild(top_content.Pass());
  scroll_child->AddChild(bottom_content.Pass());
  root->AddChild(scroll_child.Pass());

  scroll_parent_clip->AddChild(scroll_parent.Pass());
  scroll_parent_border->AddChild(scroll_parent_clip.Pass());
  root->AddChild(scroll_parent_border.Pass());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);

  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_TRUE(root->render_surface());

  // If we don't sort by depth and let the layers get added in the order they
  // would normally be visited in, then layers 6 and 7 will be out of order. In
  // other words, although we've had to shift 5, 6, and 7 to appear before 4
  // in the list (because of the scroll parent relationship), this should not
  // have an effect on the the order of 5, 6, and 7 (which had been reordered
  // due to layer sorting).
  EXPECT_EQ(4u, root->render_surface()->layer_list().size());
  EXPECT_EQ(5, root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(6, root->render_surface()->layer_list().at(1)->id());
  EXPECT_EQ(7, root->render_surface()->layer_list().at(2)->id());
  EXPECT_EQ(4, root->render_surface()->layer_list().at(3)->id());
}

TEST_F(LayerTreeHostCommonTest, ScrollCompensationWithRounding) {
  // This test verifies that a scrolling layer that gets snapped to
  // integer coordinates doesn't move a fixed position child.
  //
  // + root
  //   + container
  //     + scroller
  //       + fixed
  //
  FakeImplProxy proxy;
  FakeLayerTreeHostImpl host_impl(&proxy);
  host_impl.CreatePendingTree();
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> container =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* container_layer = container.get();
  scoped_ptr<LayerImpl> scroller =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* scroll_layer = scroller.get();
  scoped_ptr<LayerImpl> fixed = LayerImpl::Create(host_impl.active_tree(), 4);
  LayerImpl* fixed_layer = fixed.get();

  container->SetIsContainerForFixedPositionLayers(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);

  scroller->SetScrollable(true);

  gfx::Transform identity_transform;
  gfx::Transform container_transform;
  container_transform.Translate3d(10.0, 20.0, 0.0);
  gfx::Vector2dF container_offset = container_transform.To2dTranslation();

  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);
  SetLayerPropertiesForTesting(container.get(),
                               container_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               false);
  SetLayerPropertiesForTesting(scroller.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false);
  SetLayerPropertiesForTesting(fixed.get(),
                               identity_transform,
                               identity_transform,
                               gfx::PointF(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               false);

  scroller->AddChild(fixed.Pass());
  container->AddChild(scroller.Pass());
  root->AddChild(container.Pass());

  // Rounded to integers already.
  {
    gfx::Vector2dF scroll_delta(3.0, 5.0);
    scroll_layer->SetScrollDelta(scroll_delta);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);
    EXPECT_VECTOR_EQ(scroll_layer->draw_properties()
                         .screen_space_transform.To2dTranslation(),
                     container_offset - scroll_delta);
  }

  // Scroll delta requiring rounding.
  {
    gfx::Vector2dF scroll_delta(4.1f, 8.1f);
    scroll_layer->SetScrollDelta(scroll_delta);

    gfx::Vector2dF rounded_scroll_delta(4.f, 8.f);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);
    EXPECT_VECTOR_EQ(scroll_layer->draw_properties()
                         .screen_space_transform.To2dTranslation(),
                     container_offset - rounded_scroll_delta);
  }
}

}  // namespace
}  // namespace cc
