// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_UI_RESOURCE_LAYER_IMPL_H_
#define CC_LAYERS_UI_RESOURCE_LAYER_IMPL_H_

#include <string>

#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class CC_EXPORT UIResourceLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<UIResourceLayerImpl> Create(LayerTreeImpl* tree_impl,
                                               int id) {
    return make_scoped_ptr(new UIResourceLayerImpl(tree_impl, id));
  }
  virtual ~UIResourceLayerImpl();

  void SetUIResourceId(UIResourceId uid);

  void SetImageBounds(gfx::Size image_bounds);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(gfx::PointF top_left, gfx::PointF bottom_right);

  // Sets an opacity value per vertex. It will be multiplied by the layer
  // opacity value.
  void SetVertexOpacity(const float vertex_opacity[4]);

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  virtual base::DictionaryValue* LayerTreeAsJson() const OVERRIDE;

 protected:
  UIResourceLayerImpl(LayerTreeImpl* tree_impl, int id);

  // The size of the resource bitmap in pixels.
  gfx::Size image_bounds_;

  UIResourceId ui_resource_id_;

  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  float vertex_opacity_[4];

 private:
  virtual const char* LayerTypeAsString() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UIResourceLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_UI_RESOURCE_LAYER_IMPL_H_
