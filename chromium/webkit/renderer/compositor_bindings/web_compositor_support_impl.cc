// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_compositor_support_impl.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/animation/transform_operations.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "webkit/child/webthread_impl.h"
#include "webkit/renderer/compositor_bindings/web_animation_impl.h"
#include "webkit/renderer/compositor_bindings/web_content_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_external_texture_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_filter_animation_curve_impl.h"
#include "webkit/renderer/compositor_bindings/web_filter_operations_impl.h"
#include "webkit/renderer/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/renderer/compositor_bindings/web_image_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_nine_patch_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_scrollbar_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_solid_color_layer_impl.h"
#include "webkit/renderer/compositor_bindings/web_transform_animation_curve_impl.h"
#include "webkit/renderer/compositor_bindings/web_transform_operations_impl.h"

using blink::WebAnimation;
using blink::WebAnimationCurve;
using blink::WebContentLayer;
using blink::WebContentLayerClient;
using blink::WebExternalTextureLayer;
using blink::WebExternalTextureLayerClient;
using blink::WebFilterAnimationCurve;
using blink::WebFilterOperations;
using blink::WebFloatAnimationCurve;
using blink::WebImageLayer;
using blink::WebNinePatchLayer;
using blink::WebLayer;
using blink::WebScrollbar;
using blink::WebScrollbarLayer;
using blink::WebScrollbarThemeGeometry;
using blink::WebScrollbarThemePainter;
using blink::WebSolidColorLayer;
using blink::WebTransformAnimationCurve;
using blink::WebTransformOperations;

namespace webkit {

WebCompositorSupportImpl::WebCompositorSupportImpl() {}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {}

WebLayer* WebCompositorSupportImpl::createLayer() {
  return new WebLayerImpl();
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return new WebContentLayerImpl(client);
}

WebExternalTextureLayer* WebCompositorSupportImpl::createExternalTextureLayer(
    WebExternalTextureLayerClient* client) {
  return new WebExternalTextureLayerImpl(client);
}

blink::WebImageLayer* WebCompositorSupportImpl::createImageLayer() {
  return new WebImageLayerImpl();
}

blink::WebNinePatchLayer* WebCompositorSupportImpl::createNinePatchLayer() {
  return new WebNinePatchLayerImpl();
}

WebSolidColorLayer* WebCompositorSupportImpl::createSolidColorLayer() {
  return new WebSolidColorLayerImpl();
}

WebScrollbarLayer* WebCompositorSupportImpl::createScrollbarLayer(
    WebScrollbar* scrollbar,
    WebScrollbarThemePainter painter,
    WebScrollbarThemeGeometry* geometry) {
  return new WebScrollbarLayerImpl(scrollbar, painter, geometry);
}

WebScrollbarLayer* WebCompositorSupportImpl::createSolidColorScrollbarLayer(
      WebScrollbar::Orientation orientation, int thumb_thickness,
      bool is_left_side_vertical_scrollbar) {
  return new WebScrollbarLayerImpl(orientation, thumb_thickness,
      is_left_side_vertical_scrollbar);
}

WebAnimation* WebCompositorSupportImpl::createAnimation(
    const blink::WebAnimationCurve& curve,
    blink::WebAnimation::TargetProperty target,
    int animation_id) {
  return new WebAnimationImpl(curve, target, animation_id, 0);
}

WebFilterAnimationCurve*
WebCompositorSupportImpl::createFilterAnimationCurve() {
  return new WebFilterAnimationCurveImpl();
}

WebFloatAnimationCurve* WebCompositorSupportImpl::createFloatAnimationCurve() {
  return new WebFloatAnimationCurveImpl();
}

WebTransformAnimationCurve*
WebCompositorSupportImpl::createTransformAnimationCurve() {
  return new WebTransformAnimationCurveImpl();
}

WebTransformOperations* WebCompositorSupportImpl::createTransformOperations() {
  return new WebTransformOperationsImpl();
}

WebFilterOperations* WebCompositorSupportImpl::createFilterOperations() {
  return new WebFilterOperationsImpl();
}

}  // namespace webkit
