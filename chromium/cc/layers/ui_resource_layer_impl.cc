// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/ui_resource_layer_impl.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/rect_f.h"

namespace cc {

UIResourceLayerImpl::UIResourceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      ui_resource_id_(0),
      uv_top_left_(0.f, 0.f),
      uv_bottom_right_(1.f, 1.f) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

UIResourceLayerImpl::~UIResourceLayerImpl() {}

scoped_ptr<LayerImpl> UIResourceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return UIResourceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void UIResourceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  UIResourceLayerImpl* layer_impl = static_cast<UIResourceLayerImpl*>(layer);

  layer_impl->SetUIResourceId(ui_resource_id_);
  layer_impl->SetImageBounds(image_bounds_);
  layer_impl->SetUV(uv_top_left_, uv_bottom_right_);
  layer_impl->SetVertexOpacity(vertex_opacity_);
}

void UIResourceLayerImpl::SetUIResourceId(UIResourceId uid) {
  if (uid == ui_resource_id_)
    return;
  ui_resource_id_ = uid;
  NoteLayerPropertyChanged();
}

void UIResourceLayerImpl::SetImageBounds(gfx::Size image_bounds) {
  // This check imposes an ordering on the call sequence.  An UIResource must
  // exist before SetImageBounds can be called.
  DCHECK(ui_resource_id_);

  if (image_bounds_ == image_bounds)
    return;

  image_bounds_ = image_bounds;

  NoteLayerPropertyChanged();
}

void UIResourceLayerImpl::SetUV(gfx::PointF top_left,
                                gfx::PointF bottom_right) {
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  NoteLayerPropertyChanged();
}

void UIResourceLayerImpl::SetVertexOpacity(const float vertex_opacity[4]) {
  if (vertex_opacity_[0] == vertex_opacity[0] &&
      vertex_opacity_[1] == vertex_opacity[1] &&
      vertex_opacity_[2] == vertex_opacity[2] &&
      vertex_opacity_[3] == vertex_opacity[3])
    return;
  vertex_opacity_[0] = vertex_opacity[0];
  vertex_opacity_[1] = vertex_opacity[1];
  vertex_opacity_[2] = vertex_opacity[2];
  vertex_opacity_[3] = vertex_opacity[3];
  NoteLayerPropertyChanged();
}

bool UIResourceLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  if (!ui_resource_id_ || draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void UIResourceLayerImpl::AppendQuads(QuadSink* quad_sink,
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

  gfx::Rect quad_rect(bounds());

  bool opaque = layer_tree_impl()->IsUIResourceOpaque(ui_resource_id_) ||
                contents_opaque();
  gfx::Rect opaque_rect(opaque ? quad_rect : gfx::Rect());
  scoped_ptr<TextureDrawQuad> quad;

  quad = TextureDrawQuad::Create();
  quad->SetNew(shared_quad_state,
               quad_rect,
               opaque_rect,
               resource,
               premultiplied_alpha,
               uv_top_left_,
               uv_bottom_right_,
               SK_ColorTRANSPARENT,
               vertex_opacity_,
               flipped);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

const char* UIResourceLayerImpl::LayerTypeAsString() const {
  return "cc::UIResourceLayerImpl";
}

base::DictionaryValue* UIResourceLayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = LayerImpl::LayerTreeAsJson();

  result->Set("ImageBounds", MathUtil::AsValue(image_bounds_).release());

  base::ListValue* list = new base::ListValue;
  list->AppendDouble(vertex_opacity_[0]);
  list->AppendDouble(vertex_opacity_[1]);
  list->AppendDouble(vertex_opacity_[2]);
  list->AppendDouble(vertex_opacity_[3]);
  result->Set("VertexOpacity", list);

  result->Set("UVTopLeft", MathUtil::AsValue(uv_top_left_).release());
  result->Set("UVBottomRight", MathUtil::AsValue(uv_bottom_right_).release());

  return result;
}

}  // namespace cc
