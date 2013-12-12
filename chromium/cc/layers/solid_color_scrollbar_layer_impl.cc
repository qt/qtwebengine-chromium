// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/quad_sink.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

scoped_ptr<SolidColorScrollbarLayerImpl> SolidColorScrollbarLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar) {
  return make_scoped_ptr(new SolidColorScrollbarLayerImpl(
      tree_impl, id, orientation, thumb_thickness,
      is_left_side_vertical_scrollbar));
}

SolidColorScrollbarLayerImpl::~SolidColorScrollbarLayerImpl() {}

scoped_ptr<LayerImpl> SolidColorScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), thumb_thickness_,
      is_left_side_vertical_scrollbar()).PassAs<LayerImpl>();
}

SolidColorScrollbarLayerImpl::SolidColorScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar)
    : ScrollbarLayerImplBase(tree_impl, id, orientation,
          is_left_side_vertical_scrollbar),
      thumb_thickness_(thumb_thickness),
      color_(tree_impl->settings().solid_color_scrollbar_color) {}

void SolidColorScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerImplBase::PushPropertiesTo(layer);
}

int SolidColorScrollbarLayerImpl::ThumbThickness() const {
  if (thumb_thickness_ != -1)
    return thumb_thickness_;

  if (orientation() == HORIZONTAL)
    return bounds().height();
  else
    return bounds().width();
}

int SolidColorScrollbarLayerImpl::ThumbLength() const {
  return std::max(
      static_cast<int>(visible_to_total_length_ratio() * TrackLength()),
      ThumbThickness());
}

float SolidColorScrollbarLayerImpl::TrackLength() const {
  if (orientation() == HORIZONTAL)
    return bounds().width();
  else
    return bounds().height() + vertical_adjust();
}

int SolidColorScrollbarLayerImpl::TrackStart() const {
  return 0;
}

void SolidColorScrollbarLayerImpl::AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) {
  gfx::Rect thumb_quad_rect = ComputeThumbQuadRect();

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
  quad->SetNew(shared_quad_state, thumb_quad_rect, color_, false);
  quad_sink->Append(quad.PassAs<DrawQuad>(), append_quads_data);
}

}  // namespace cc
