// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_IMPL_H_

#include "cc/base/cc_export.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

namespace cc {

class CC_EXPORT SolidColorScrollbarLayerImpl : public ScrollbarLayerImplBase {
 public:
  static scoped_ptr<SolidColorScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation,
      int thumb_thickness,
      bool is_left_side_vertical_scrollbar);
  virtual ~SolidColorScrollbarLayerImpl();

  // LayerImpl overrides.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;


 protected:
  SolidColorScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                               int id,
                               ScrollbarOrientation orientation,
                               int thumb_thickness,
                               bool is_left_side_vertical_scrollbar);

  // ScrollbarLayerImplBase implementation.
  virtual int ThumbThickness() const OVERRIDE;
  virtual int ThumbLength() const OVERRIDE;
  virtual float TrackLength() const OVERRIDE;
  virtual int TrackStart() const OVERRIDE;

 private:
  int thumb_thickness_;
  SkColor color_;
};

}  // namespace cc

#endif  // CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_IMPL_H_
