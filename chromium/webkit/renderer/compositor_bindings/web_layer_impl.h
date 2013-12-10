// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_LAYER_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer_client.h"
#include "third_party/WebKit/public/platform/WebAnimation.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebCompositingReasons.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc { class Layer; }

// TODO(senorblanco):  Remove this once WebKit changes have landed.
class SkImageFilter;

namespace WebKit {
class WebFilterOperations;
class WebLayerClient;
struct WebFloatRect;
}

namespace webkit {

class WebToCCAnimationDelegateAdapter;

class WebLayerImpl : public WebKit::WebLayer, public cc::LayerClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebLayerImpl();
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebLayerImpl(
      scoped_refptr<cc::Layer>);
  virtual ~WebLayerImpl();

  WEBKIT_COMPOSITOR_BINDINGS_EXPORT cc::Layer* layer() const;

  // WebLayer implementation.
  virtual int id() const;
  virtual void invalidateRect(const WebKit::WebFloatRect&);
  virtual void invalidate();
  virtual void addChild(WebKit::WebLayer* child);
  virtual void insertChild(WebKit::WebLayer* child, size_t index);
  virtual void replaceChild(WebKit::WebLayer* reference,
                            WebKit::WebLayer* new_layer);
  virtual void removeFromParent();
  virtual void removeAllChildren();
  virtual void setAnchorPoint(const WebKit::WebFloatPoint&);
  virtual WebKit::WebFloatPoint anchorPoint() const;
  virtual void setAnchorPointZ(float anchor_point_z);
  virtual float anchorPointZ() const;
  virtual void setBounds(const WebKit::WebSize& bounds);
  virtual WebKit::WebSize bounds() const;
  virtual void setMasksToBounds(bool masks_to_bounds);
  virtual bool masksToBounds() const;
  virtual void setMaskLayer(WebKit::WebLayer* mask);
  virtual void setReplicaLayer(WebKit::WebLayer* replica);
  virtual void setOpacity(float opacity);
  virtual float opacity() const;
  virtual void setOpaque(bool opaque);
  virtual bool opaque() const;
  virtual void setPosition(const WebKit::WebFloatPoint& position);
  virtual WebKit::WebFloatPoint position() const;
  virtual void setSublayerTransform(const SkMatrix44&);
  virtual SkMatrix44 sublayerTransform() const;
  virtual void setTransform(const SkMatrix44& transform);
  virtual SkMatrix44 transform() const;
  virtual void setDrawsContent(bool draws_content);
  virtual bool drawsContent() const;
  virtual void setPreserves3D(bool preserves_3d);
  virtual void setUseParentBackfaceVisibility(bool visible);
  virtual void setBackgroundColor(WebKit::WebColor color);
  virtual WebKit::WebColor backgroundColor() const;
  virtual void setFilter(SkImageFilter* filter);
  virtual void setFilters(const WebKit::WebFilterOperations& filters);
  virtual void setBackgroundFilters(const WebKit::WebFilterOperations& filters);
  virtual void setCompositingReasons(WebKit::WebCompositingReasons);
  virtual void setAnimationDelegate(WebKit::WebAnimationDelegate* delegate);
  virtual bool addAnimation(WebKit::WebAnimation* animation);
  virtual void removeAnimation(int animation_id);
  virtual void removeAnimation(int animation_id,
                               WebKit::WebAnimation::TargetProperty);
  virtual void pauseAnimation(int animation_id, double time_offset);
  virtual void suspendAnimations(double monotonic_time);
  virtual void resumeAnimations(double monotonic_time);
  virtual bool hasActiveAnimation();
  virtual void setForceRenderSurface(bool force);
  virtual void setScrollPosition(WebKit::WebPoint position);
  virtual WebKit::WebPoint scrollPosition() const;
  virtual void setMaxScrollPosition(WebKit::WebSize max_position);
  virtual WebKit::WebSize maxScrollPosition() const;
  virtual void setScrollable(bool scrollable);
  virtual bool scrollable() const;
  virtual void setHaveWheelEventHandlers(bool have_wheel_event_handlers);
  virtual bool haveWheelEventHandlers() const;
  virtual void setShouldScrollOnMainThread(bool scroll_on_main);
  virtual bool shouldScrollOnMainThread() const;
  virtual void setNonFastScrollableRegion(
      const WebKit::WebVector<WebKit::WebRect>& region);
  virtual WebKit::WebVector<WebKit::WebRect> nonFastScrollableRegion() const;
  virtual void setTouchEventHandlerRegion(
      const WebKit::WebVector<WebKit::WebRect>& region);
  virtual WebKit::WebVector<WebKit::WebRect> touchEventHandlerRegion() const;
  virtual void setIsContainerForFixedPositionLayers(bool is_container);
  virtual bool isContainerForFixedPositionLayers() const;
  virtual void setPositionConstraint(
      const WebKit::WebLayerPositionConstraint& constraint);
  virtual WebKit::WebLayerPositionConstraint positionConstraint() const;
  virtual void setScrollClient(WebKit::WebLayerScrollClient* client);
  virtual bool isOrphan() const;
  virtual void setWebLayerClient(WebKit::WebLayerClient* client);

  // LayerClient implementation.
  virtual std::string DebugName() OVERRIDE;

  virtual void setScrollParent(WebKit::WebLayer* parent);
  virtual void setClipParent(WebKit::WebLayer* parent);

 protected:
  scoped_refptr<cc::Layer> layer_;
  WebKit::WebLayerClient* web_layer_client_;

 private:
  scoped_ptr<WebToCCAnimationDelegateAdapter> animation_delegate_adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_LAYER_IMPL_H_
