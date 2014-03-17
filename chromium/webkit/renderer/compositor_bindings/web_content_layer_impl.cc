// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_content_layer_impl.h"

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/WebKit/public/platform/WebContentLayerClient.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

using cc::ContentLayer;
using cc::PictureLayer;

namespace webkit {

static bool usingPictureLayer() {
  return cc::switches::IsImplSidePaintingEnabled();
}

WebContentLayerImpl::WebContentLayerImpl(blink::WebContentLayerClient* client)
    : client_(client),
      ignore_lcd_text_change_(false) {
  if (usingPictureLayer())
    layer_ = make_scoped_ptr(new WebLayerImpl(PictureLayer::Create(this)));
  else
    layer_ = make_scoped_ptr(new WebLayerImpl(ContentLayer::Create(this)));
  layer_->layer()->SetIsDrawable(true);
  can_use_lcd_text_ = layer_->layer()->can_use_lcd_text();
}

WebContentLayerImpl::~WebContentLayerImpl() {
  if (usingPictureLayer())
    static_cast<PictureLayer*>(layer_->layer())->ClearClient();
  else
    static_cast<ContentLayer*>(layer_->layer())->ClearClient();
}

blink::WebLayer* WebContentLayerImpl::layer() { return layer_.get(); }

void WebContentLayerImpl::setDoubleSided(bool double_sided) {
  layer_->layer()->SetDoubleSided(double_sided);
}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable) {
  layer_->layer()->SetDrawCheckerboardForMissingTiles(enable);
}

void WebContentLayerImpl::PaintContents(SkCanvas* canvas,
                                        gfx::Rect clip,
                                        gfx::RectF* opaque) {
  if (!client_)
    return;

  blink::WebFloatRect web_opaque;
  // For picture layers, always record with LCD text.  PictureLayerImpl
  // will turn this off later during rasterization.
  bool use_lcd_text = usingPictureLayer() || can_use_lcd_text_;
  client_->paintContents(canvas, clip, use_lcd_text, web_opaque);
  *opaque = web_opaque;
}

void WebContentLayerImpl::DidChangeLayerCanUseLCDText() {
  // It is important to make this comparison because the LCD text status
  // here can get out of sync with that in the layer.
  if (can_use_lcd_text_ == layer_->layer()->can_use_lcd_text())
    return;

  // LCD text cannot be enabled once disabled.
  if (layer_->layer()->can_use_lcd_text() && ignore_lcd_text_change_)
    return;

  can_use_lcd_text_ = layer_->layer()->can_use_lcd_text();
  ignore_lcd_text_change_ = true;
  layer_->invalidate();
}

}  // namespace webkit
