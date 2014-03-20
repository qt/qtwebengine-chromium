// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_
#define CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"

namespace cc {

class LayerTreeImpl;

class CC_EXPORT ScrollbarLayerImplBase : public LayerImpl {
 public:
  int ScrollLayerId() const { return scroll_layer_id_; }
  void set_scroll_layer_id(int id) { scroll_layer_id_ = id; }

  float current_pos() const { return current_pos_; }
  void SetCurrentPos(float current_pos);
  int maximum() const { return maximum_; }
  void SetMaximum(int maximum);

  void SetVerticalAdjust(float vertical_adjust);

  bool is_overlay_scrollbar() const { return is_overlay_scrollbar_; }
  void set_is_overlay_scrollbar(bool is_overlay) {
    is_overlay_scrollbar_ = is_overlay;
  }

  ScrollbarOrientation orientation() const { return orientation_; }
  bool is_left_side_vertical_scrollbar() {
    return is_left_side_vertical_scrollbar_;
  }

  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual ScrollbarLayerImplBase* ToScrollbarLayer() OVERRIDE;

  void SetVisibleToTotalLengthRatio(float ratio);
  virtual gfx::Rect ComputeThumbQuadRect() const;

  float thumb_thickness_scale_factor() {
    return thumb_thickness_scale_factor_;
  }
  void SetThumbThicknessScaleFactor(float thumb_thickness_scale_factor);

 protected:
  ScrollbarLayerImplBase(LayerTreeImpl* tree_impl,
                         int id,
                         ScrollbarOrientation orientation,
                         bool is_left_side_vertical_scrollbar);
  virtual ~ScrollbarLayerImplBase() {}

  gfx::Rect ScrollbarLayerRectToContentRect(gfx::RectF layer_rect) const;

  float visible_to_total_length_ratio() const {
    return visible_to_total_length_ratio_;
  }
  float vertical_adjust() const { return vertical_adjust_; }

  virtual int ThumbThickness() const = 0;
  virtual int ThumbLength() const = 0;
  virtual float TrackLength() const = 0;
  virtual int TrackStart() const = 0;

 private:
  int scroll_layer_id_;
  bool is_overlay_scrollbar_;

  float thumb_thickness_scale_factor_;
  float current_pos_;
  int maximum_;
  ScrollbarOrientation orientation_;
  bool is_left_side_vertical_scrollbar_;

  // Difference between the clip layer's height and the visible viewport
  // height (which may differ in the presence of top-controls hiding).
  float vertical_adjust_;

  float visible_to_total_length_ratio_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarLayerImplBase);
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_IMPL_BASE_H_
