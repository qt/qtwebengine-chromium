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

namespace blink {
class WebGraphicsContext3D;
}

namespace webkit {

class WebCompositorSupportImpl : public blink::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  virtual ~WebCompositorSupportImpl();

  virtual blink::WebLayer* createLayer();
  virtual blink::WebContentLayer* createContentLayer(
      blink::WebContentLayerClient* client);
  virtual blink::WebExternalTextureLayer* createExternalTextureLayer(
      blink::WebExternalTextureLayerClient* client);
  virtual blink::WebImageLayer* createImageLayer();
  virtual blink::WebNinePatchLayer* createNinePatchLayer();
  virtual blink::WebSolidColorLayer* createSolidColorLayer();
  virtual blink::WebScrollbarLayer* createScrollbarLayer(
      blink::WebScrollbar* scrollbar,
      blink::WebScrollbarThemePainter painter,
      blink::WebScrollbarThemeGeometry*);
  virtual blink::WebScrollbarLayer* createSolidColorScrollbarLayer(
      blink::WebScrollbar::Orientation orientation, int thumb_thickness,
      bool is_left_side_vertical_scrollbar);
  virtual blink::WebAnimation* createAnimation(
      const blink::WebAnimationCurve& curve,
      blink::WebAnimation::TargetProperty target,
      int animation_id);
  virtual blink::WebFilterAnimationCurve* createFilterAnimationCurve();
  virtual blink::WebFloatAnimationCurve* createFloatAnimationCurve();
  virtual blink::WebTransformAnimationCurve* createTransformAnimationCurve();
  virtual blink::WebTransformOperations* createTransformOperations();
  virtual blink::WebFilterOperations* createFilterOperations();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCompositorSupportImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
