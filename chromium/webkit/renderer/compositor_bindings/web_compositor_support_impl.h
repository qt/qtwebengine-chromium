// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "third_party/WebKit/public/platform/WebCompositorSupport.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebTransformOperations.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace webkit {

class WebCompositorSupportImpl : public WebKit::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  virtual ~WebCompositorSupportImpl();

  virtual WebKit::WebLayer* createLayer();
  virtual WebKit::WebContentLayer* createContentLayer(
      WebKit::WebContentLayerClient* client);
  virtual WebKit::WebExternalTextureLayer* createExternalTextureLayer(
      WebKit::WebExternalTextureLayerClient* client);
  virtual WebKit::WebImageLayer* createImageLayer();
  virtual WebKit::WebNinePatchLayer* createNinePatchLayer();
  virtual WebKit::WebSolidColorLayer* createSolidColorLayer();
  virtual WebKit::WebScrollbarLayer* createScrollbarLayer(
      WebKit::WebScrollbar* scrollbar,
      WebKit::WebScrollbarThemePainter painter,
      WebKit::WebScrollbarThemeGeometry*);
  virtual WebKit::WebScrollbarLayer* createSolidColorScrollbarLayer(
      WebKit::WebScrollbar::Orientation orientation, int thumb_thickness);
  virtual WebKit::WebScrollbarLayer* createSolidColorScrollbarLayer(
      WebKit::WebScrollbar::Orientation orientation, int thumb_thickness,
      bool is_left_side_vertical_scrollbar);
  virtual WebKit::WebAnimation* createAnimation(
      const WebKit::WebAnimationCurve& curve,
      WebKit::WebAnimation::TargetProperty target,
      int animation_id);
  virtual WebKit::WebFloatAnimationCurve* createFloatAnimationCurve();
  virtual WebKit::WebTransformAnimationCurve* createTransformAnimationCurve();
  virtual WebKit::WebTransformOperations* createTransformOperations();
  virtual WebKit::WebFilterOperations* createFilterOperations();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCompositorSupportImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
