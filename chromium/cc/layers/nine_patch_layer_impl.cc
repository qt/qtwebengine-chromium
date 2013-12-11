// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/rect_f.h"

namespace cc {

NinePatchLayerImpl::NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      fill_center_(false),
      ui_resource_id_(0) {}

NinePatchLayerImpl::~NinePatchLayerImpl() {}

ResourceProvider::ResourceId NinePatchLayerImpl::ContentsResourceId() const {
  return 0;
}

scoped_ptr<LayerImpl> NinePatchLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return NinePatchLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void NinePatchLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  layer_impl->SetUIResourceId(ui_resource_id_);
  layer_impl->SetLayout(image_bounds_, image_aperture_, border_, fill_center_);
}

static gfx::RectF NormalizedRect(float x,
                                 float y,
                                 float width,
                                 float height,
                                 float total_width,
                                 float total_height) {
  return gfx::RectF(x / total_width,
                    y / total_height,
                    width / total_width,
                    height / total_height);
}

void NinePatchLayerImpl::SetUIResourceId(UIResourceId uid) {
  if (uid == ui_resource_id_)
    return;
  ui_resource_id_ = uid;
  NoteLayerPropertyChanged();
}

void NinePatchLayerImpl::SetLayout(gfx::Size image_bounds,
                                   gfx::Rect aperture,
                                   gfx::Rect border,
                                   bool fill_center) {
  // This check imposes an ordering on the call sequence.  An UIResource must
  // exist before SetLayout can be called.
  DCHECK(ui_resource_id_);

  // TODO(ccameron): the following "greater than or equal to" (GE) checks should
  // be greater than (GT) to avoid degenerate nine-patches.  The relaxed
  // condition "equal to" is a workaround for the overhang shadow use case and
  // should be investigated further.

  // |border| is in layer space.  It cannot exceed the bounds of the layer.
  DCHECK(!border.size().IsEmpty());
  DCHECK_GE(bounds().width(), border.width());
  DCHECK_GE(bounds().height(), border.height());

  // Sanity Check on |border|
  DCHECK_LT(border.x(), border.width());
  DCHECK_LT(border.y(), border.height());
  DCHECK_GE(border.x(), 0);
  DCHECK_GE(border.y(), 0);

  // |aperture| is in image space.  It cannot exceed the bounds of the bitmap.
  DCHECK(!aperture.size().IsEmpty());
  DCHECK(gfx::Rect(image_bounds.width(), image_bounds.height())
             .Contains(aperture));

  // Avoid the degenerate cases where the aperture touches the edge of the
  // image.
  DCHECK_LT(aperture.width(), image_bounds.width() - 1);
  DCHECK_LT(aperture.height(), image_bounds.height() - 1);
  DCHECK_GT(aperture.x(), 0);
  DCHECK_GT(aperture.y(), 0);

  if (image_bounds_ == image_bounds && image_aperture_ == aperture &&
      border_ == border && fill_center_ == fill_center)
    return;

  image_bounds_ = image_bounds;
  image_aperture_ = aperture;
  border_ = border;
  fill_center_ = fill_center;

  NoteLayerPropertyChanged();
}

bool NinePatchLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  if (!ui_resource_id_ || draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void NinePatchLayerImpl::AppendQuads(QuadSink* quad_sink,
                                     AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  if (!ui_resource_id_)
    return;

  ResourceProvider::ResourceId resource =
      layer_tree_impl()->ResourceIdForUIResource(ui_resource_id_);

  if (!resource)
    return;

  static const bool flipped = false;
  static const bool premultiplied_alpha = true;

  DCHECK(!bounds().IsEmpty());

  // NinePatch border widths in layer space.
  int layer_left_width = border_.x();
  int layer_top_height = border_.y();
  int layer_right_width = border_.width() - layer_left_width;
  int layer_bottom_height = border_.height() - layer_top_height;

  int layer_middle_width = bounds().width() - border_.width();
  int layer_middle_height = bounds().height() - border_.height();

  // Patch positions in layer space
  gfx::Rect layer_top_left(0, 0, layer_left_width, layer_top_height);
  gfx::Rect layer_top_right(bounds().width() - layer_right_width,
                            0,
                            layer_right_width,
                            layer_top_height);
  gfx::Rect layer_bottom_left(0,
                              bounds().height() - layer_bottom_height,
                              layer_left_width,
                              layer_bottom_height);
  gfx::Rect layer_bottom_right(layer_top_right.x(),
                               layer_bottom_left.y(),
                               layer_right_width,
                               layer_bottom_height);
  gfx::Rect layer_top(
      layer_top_left.right(), 0, layer_middle_width, layer_top_height);
  gfx::Rect layer_left(
      0, layer_top_left.bottom(), layer_left_width, layer_middle_height);
  gfx::Rect layer_right(layer_top_right.x(),
                        layer_top_right.bottom(),
                        layer_right_width,
                        layer_left.height());
  gfx::Rect layer_bottom(layer_top.x(),
                         layer_bottom_left.y(),
                         layer_top.width(),
                         layer_bottom_height);
  gfx::Rect layer_center(layer_left_width,
                         layer_top_height,
                         layer_middle_width,
                         layer_middle_height);

  // Note the following values are in image (bitmap) space.
  float image_width = image_bounds_.width();
  float image_height = image_bounds_.height();

  int image_aperture_left_width = image_aperture_.x();
  int image_aperture_top_height = image_aperture_.y();
  int image_aperture_right_width = image_width - image_aperture_.right();
  int image_aperture_bottom_height = image_height - image_aperture_.bottom();
  // Patch positions in bitmap UV space (from zero to one)
  gfx::RectF uv_top_left = NormalizedRect(0,
                                          0,
                                          image_aperture_left_width,
                                          image_aperture_top_height,
                                          image_width,
                                          image_height);
  gfx::RectF uv_top_right =
      NormalizedRect(image_width - image_aperture_right_width,
                     0,
                     image_aperture_right_width,
                     image_aperture_top_height,
                     image_width,
                     image_height);
  gfx::RectF uv_bottom_left =
      NormalizedRect(0,
                     image_height - image_aperture_bottom_height,
                     image_aperture_left_width,
                     image_aperture_bottom_height,
                     image_width,
                     image_height);
  gfx::RectF uv_bottom_right =
      NormalizedRect(image_width - image_aperture_right_width,
                     image_height - image_aperture_bottom_height,
                     image_aperture_right_width,
                     image_aperture_bottom_height,
                     image_width,
                     image_height);
  gfx::RectF uv_top(
      uv_top_left.right(),
      0,
      (image_width - image_aperture_left_width - image_aperture_right_width) /
          image_width,
      (image_aperture_top_height) / image_height);
  gfx::RectF uv_left(0,
                     uv_top_left.bottom(),
                     image_aperture_left_width / image_width,
                     (image_height - image_aperture_top_height -
                      image_aperture_bottom_height) /
                         image_height);
  gfx::RectF uv_right(uv_top_right.x(),
                      uv_top_right.bottom(),
                      image_aperture_right_width / image_width,
                      uv_left.height());
  gfx::RectF uv_bottom(uv_top.x(),
                       uv_bottom_left.y(),
                       uv_top.width(),
                       image_aperture_bottom_height / image_height);
  gfx::RectF uv_center(uv_top_left.right(),
                       uv_top_left.bottom(),
                       uv_top.width(),
                       uv_left.height());

  // Nothing is opaque here.
  // TODO(danakj): Should we look at the SkBitmaps to determine opaqueness?
  gfx::Rect opaque_rect;
  const float vertex_opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
  scoped_ptr<TextureDrawQuad> quad;

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_top_left,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_top_left.origin(),
               uv_top_left.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_top_right,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_top_right.origin(),
               uv_top_right.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_bottom_left,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_bottom_left.origin(),
               uv_bottom_left.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_bottom_right,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_bottom_right.origin(),
               uv_bottom_right.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_top,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_top.origin(),
               uv_top.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_left,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_left.origin(),
               uv_left.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_right,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_right.origin(),
               uv_right.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               layer_bottom,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_bottom.origin(),
               uv_bottom.bottom_right(),
               SK_ColorTRANSPARENT,
               vertex_opacity,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);

  if (fill_center_) {
    quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 layer_center,
                 opaque_rect,
                 resource,
                 premultiplied_alpha,
                 uv_center.origin(),
                 uv_center.bottom_right(),
                 SK_ColorTRANSPARENT,
                 vertex_opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
  }
}

const char* NinePatchLayerImpl::LayerTypeAsString() const {
  return "cc::NinePatchLayerImpl";
}

base::DictionaryValue* NinePatchLayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = LayerImpl::LayerTreeAsJson();

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(image_aperture_.origin().x());
  list->AppendInteger(image_aperture_.origin().y());
  list->AppendInteger(image_aperture_.size().width());
  list->AppendInteger(image_aperture_.size().height());
  result->Set("ImageAperture", list);

  result->Set("ImageBounds", MathUtil::AsValue(image_bounds_).release());
  result->Set("Border", MathUtil::AsValue(border_).release());

  base::FundamentalValue* fill_center =
      base::Value::CreateBooleanValue(fill_center_);
  result->Set("FillCenter", fill_center);

  return result;
}

}  // namespace cc
