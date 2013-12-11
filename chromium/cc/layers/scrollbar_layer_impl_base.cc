// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>
#include "cc/layers/layer.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

ScrollbarLayerImplBase::ScrollbarLayerImplBase(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar)
    : LayerImpl(tree_impl, id),
      scroll_layer_id_(Layer::INVALID_ID),
      is_overlay_scrollbar_(false),
      thumb_thickness_scale_factor_(1.f),
      current_pos_(0.f),
      maximum_(0),
      orientation_(orientation),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar),
      vertical_adjust_(0.f),
      visible_to_total_length_ratio_(1.f) {}

void ScrollbarLayerImplBase::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
}

ScrollbarLayerImplBase* ScrollbarLayerImplBase::ToScrollbarLayer() {
  return this;
}

gfx::Rect ScrollbarLayerImplBase::ScrollbarLayerRectToContentRect(
    gfx::RectF layer_rect) const {
  // Don't intersect with the bounds as in LayerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::RectF content_rect = gfx::ScaleRect(layer_rect,
      contents_scale_x(),
      contents_scale_y());
  return gfx::ToEnclosingRect(content_rect);
}

void ScrollbarLayerImplBase::SetCurrentPos(float current_pos) {
  if (current_pos_ == current_pos)
    return;
  current_pos_ = current_pos;
  NoteLayerPropertyChanged();
}

void ScrollbarLayerImplBase::SetMaximum(int maximum) {
  if (maximum_ == maximum)
    return;
  maximum_ = maximum;
  NoteLayerPropertyChanged();
}

void ScrollbarLayerImplBase::SetVerticalAdjust(float vertical_adjust) {
  if (vertical_adjust_ == vertical_adjust)
    return;
  vertical_adjust_ = vertical_adjust;
  NoteLayerPropertyChanged();
}

void ScrollbarLayerImplBase::SetVisibleToTotalLengthRatio(float ratio) {
  if (visible_to_total_length_ratio_ == ratio)
    return;
  visible_to_total_length_ratio_ = ratio;
  NoteLayerPropertyChanged();
}

gfx::Rect ScrollbarLayerImplBase::ComputeThumbQuadRect() const {
  // Thumb extent is the length of the thumb in the scrolling direction, thumb
  // thickness is in the perpendicular direction. Here's an example of a
  // horizontal scrollbar - inputs are above the scrollbar, computed values
  // below:
  //
  //    |<------------------- track_length_ ------------------->|
  //
  // |--| <-- start_offset
  //
  // +--+----------------------------+------------------+-------+--+
  // |<||                            |##################|       ||>|
  // +--+----------------------------+------------------+-------+--+
  //
  //                                 |<- thumb_length ->|
  //
  // |<------- thumb_offset -------->|
  //
  // For painted, scrollbars, the length is fixed. For solid color scrollbars we
  // have to compute it. The ratio of the thumb's length to the track's length
  // is the same as that of the visible viewport to the total viewport, unless
  // that would make the thumb's length less than its thickness.
  //
  // vertical_adjust_ is used when the layer geometry from the main thread is
  // not in sync with what the user sees. For instance on Android scrolling the
  // top bar controls out of view reveals more of the page content. We want the
  // root layer scrollbars to reflect what the user sees even if we haven't
  // received new layer geometry from the main thread.  If the user has scrolled
  // down by 50px and the initial viewport size was 950px the geometry would
  // look something like this:
  //
  // vertical_adjust_ = 50, scroll position 0, visible ratios 99%
  // Layer geometry:             Desired thumb positions:
  // +--------------------+-+   +----------------------+   <-- 0px
  // |                    |v|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // |                    |t|   |                     #|
  // |                    |i|   |                     #|
  // |                    |c|   |                     #|
  // |                    |a|   |                     #|
  // |                    |l|   |                     #|
  // |                    | |   |                     #|
  // |                    |l|   |                     #|
  // |                    |a|   |                     #|
  // |                    |y|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // +--------------------+-+   |                     #|
  // | horizontal  layer  | |   |                     #|
  // +--------------------+-+   |                     #|  <-- 950px
  // |                      |   |                     #|
  // |                      |   |##################### |
  // +----------------------+   +----------------------+  <-- 1000px
  //
  // The layer geometry is set up for a 950px tall viewport, but the user can
  // actually see down to 1000px. Thus we have to move the quad for the
  // horizontal scrollbar down by the vertical_adjust_ factor and lay the
  // vertical thumb out on a track lengthed by the vertical_adjust_ factor. This
  // means the quads may extend outside the layer's bounds.

  // With the length known, we can compute the thumb's position.
  float track_length = TrackLength();
  int thumb_length = ThumbLength();
  int thumb_thickness = ThumbThickness();

  // With the length known, we can compute the thumb's position.
  float clamped_current_pos =
      std::min(std::max(current_pos_, 0.f), static_cast<float>(maximum_));
  float ratio = clamped_current_pos / maximum_;
  float max_offset = track_length - thumb_length;
  int thumb_offset = static_cast<int>(ratio * max_offset) + TrackStart();

  float thumb_thickness_adjustment =
      thumb_thickness * (1.f - thumb_thickness_scale_factor_);

  gfx::RectF thumb_rect;
  if (orientation_ == HORIZONTAL) {
    thumb_rect = gfx::RectF(thumb_offset,
                            vertical_adjust_ + thumb_thickness_adjustment,
                            thumb_length,
                            thumb_thickness - thumb_thickness_adjustment);
  } else {
    thumb_rect = gfx::RectF(
        is_left_side_vertical_scrollbar_
            ? bounds().width() - thumb_thickness
            : thumb_thickness_adjustment,
        thumb_offset,
        thumb_thickness - thumb_thickness_adjustment,
        thumb_length);
  }

  return ScrollbarLayerRectToContentRect(thumb_rect);
}

}  // namespace cc
