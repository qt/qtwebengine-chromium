// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

#include "base/bind.h"
#include "base/debug/trace_event_impl.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "cc/animation/animation.h"
#include "cc/base/region.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_position_constraint.h"
#include "third_party/WebKit/public/platform/WebCompositingReasons.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebGraphicsLayerDebugInfo.h"
#include "third_party/WebKit/public/platform/WebLayerClient.h"
#include "third_party/WebKit/public/platform/WebLayerPositionConstraint.h"
#include "third_party/WebKit/public/platform/WebLayerScrollClient.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "webkit/renderer/compositor_bindings/web_animation_impl.h"
#include "webkit/renderer/compositor_bindings/web_blend_mode.h"
#include "webkit/renderer/compositor_bindings/web_filter_operations_impl.h"
#include "webkit/renderer/compositor_bindings/web_to_cc_animation_delegate_adapter.h"

using cc::Animation;
using cc::Layer;
using blink::WebLayer;
using blink::WebFloatPoint;
using blink::WebVector;
using blink::WebRect;
using blink::WebSize;
using blink::WebColor;
using blink::WebFilterOperations;

namespace webkit {

WebLayerImpl::WebLayerImpl() : layer_(Layer::Create()) {
  web_layer_client_ = NULL;
  layer_->SetLayerClient(this);
}

WebLayerImpl::WebLayerImpl(scoped_refptr<Layer> layer) : layer_(layer) {
  web_layer_client_ = NULL;
  layer_->SetLayerClient(this);
}

WebLayerImpl::~WebLayerImpl() {
  layer_->ClearRenderSurface();
  layer_->set_layer_animation_delegate(NULL);
  web_layer_client_ = NULL;
}

int WebLayerImpl::id() const { return layer_->id(); }

void WebLayerImpl::invalidateRect(const blink::WebFloatRect& rect) {
  layer_->SetNeedsDisplayRect(rect);
}

void WebLayerImpl::invalidate() { layer_->SetNeedsDisplay(); }

void WebLayerImpl::addChild(WebLayer* child) {
  layer_->AddChild(static_cast<WebLayerImpl*>(child)->layer());
}

void WebLayerImpl::insertChild(WebLayer* child, size_t index) {
  layer_->InsertChild(static_cast<WebLayerImpl*>(child)->layer(), index);
}

void WebLayerImpl::replaceChild(WebLayer* reference, WebLayer* new_layer) {
  layer_->ReplaceChild(static_cast<WebLayerImpl*>(reference)->layer(),
                       static_cast<WebLayerImpl*>(new_layer)->layer());
}

void WebLayerImpl::removeFromParent() { layer_->RemoveFromParent(); }

void WebLayerImpl::removeAllChildren() { layer_->RemoveAllChildren(); }

void WebLayerImpl::setAnchorPoint(const WebFloatPoint& anchor_point) {
  layer_->SetAnchorPoint(anchor_point);
}

WebFloatPoint WebLayerImpl::anchorPoint() const {
  return layer_->anchor_point();
}

void WebLayerImpl::setAnchorPointZ(float anchor_point_z) {
  layer_->SetAnchorPointZ(anchor_point_z);
}

float WebLayerImpl::anchorPointZ() const { return layer_->anchor_point_z(); }

void WebLayerImpl::setBounds(const WebSize& size) { layer_->SetBounds(size); }

WebSize WebLayerImpl::bounds() const { return layer_->bounds(); }

void WebLayerImpl::setMasksToBounds(bool masks_to_bounds) {
  layer_->SetMasksToBounds(masks_to_bounds);
}

bool WebLayerImpl::masksToBounds() const { return layer_->masks_to_bounds(); }

void WebLayerImpl::setMaskLayer(WebLayer* maskLayer) {
  layer_->SetMaskLayer(
      maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : 0);
}

void WebLayerImpl::setReplicaLayer(WebLayer* replica_layer) {
  layer_->SetReplicaLayer(
      replica_layer ? static_cast<WebLayerImpl*>(replica_layer)->layer() : 0);
}

void WebLayerImpl::setOpacity(float opacity) { layer_->SetOpacity(opacity); }

float WebLayerImpl::opacity() const { return layer_->opacity(); }

void WebLayerImpl::setBlendMode(blink::WebBlendMode blend_mode) {
  layer_->SetBlendMode(BlendModeToSkia(blend_mode));
}

blink::WebBlendMode WebLayerImpl::blendMode() const {
  return BlendModeFromSkia(layer_->blend_mode());
}

void WebLayerImpl::setIsRootForIsolatedGroup(bool isolate) {
  layer_->SetIsRootForIsolatedGroup(isolate);
}

bool WebLayerImpl::isRootForIsolatedGroup() {
  return layer_->is_root_for_isolated_group();
}

void WebLayerImpl::setOpaque(bool opaque) { layer_->SetContentsOpaque(opaque); }

bool WebLayerImpl::opaque() const { return layer_->contents_opaque(); }

void WebLayerImpl::setPosition(const WebFloatPoint& position) {
  layer_->SetPosition(position);
}

WebFloatPoint WebLayerImpl::position() const { return layer_->position(); }

void WebLayerImpl::setSublayerTransform(const SkMatrix44& matrix) {
  gfx::Transform sub_layer_transform;
  sub_layer_transform.matrix() = matrix;
  layer_->SetSublayerTransform(sub_layer_transform);
}

SkMatrix44 WebLayerImpl::sublayerTransform() const {
  return layer_->sublayer_transform().matrix();
}

void WebLayerImpl::setTransform(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  layer_->SetTransform(transform);
}

SkMatrix44 WebLayerImpl::transform() const {
  return layer_->transform().matrix();
}

void WebLayerImpl::setDrawsContent(bool draws_content) {
  layer_->SetIsDrawable(draws_content);
}

bool WebLayerImpl::drawsContent() const { return layer_->DrawsContent(); }

void WebLayerImpl::setPreserves3D(bool preserve3D) {
  layer_->SetPreserves3d(preserve3D);
}

void WebLayerImpl::setUseParentBackfaceVisibility(
    bool use_parent_backface_visibility) {
  layer_->set_use_parent_backface_visibility(use_parent_backface_visibility);
}

void WebLayerImpl::setBackgroundColor(WebColor color) {
  layer_->SetBackgroundColor(color);
}

WebColor WebLayerImpl::backgroundColor() const {
  return layer_->background_color();
}

void WebLayerImpl::setFilters(const WebFilterOperations& filters) {
  const WebFilterOperationsImpl& filters_impl =
      static_cast<const WebFilterOperationsImpl&>(filters);
  layer_->SetFilters(filters_impl.AsFilterOperations());
}

void WebLayerImpl::setBackgroundFilters(const WebFilterOperations& filters) {
  const WebFilterOperationsImpl& filters_impl =
      static_cast<const WebFilterOperationsImpl&>(filters);
  layer_->SetBackgroundFilters(filters_impl.AsFilterOperations());
}

void WebLayerImpl::setCompositingReasons(
    blink::WebCompositingReasons reasons) {
  layer_->SetCompositingReasons(reasons);
}

void WebLayerImpl::setAnimationDelegate(
      blink::WebAnimationDelegate* delegate) {
  animation_delegate_adapter_.reset(
      new WebToCCAnimationDelegateAdapter(delegate));
  layer_->set_layer_animation_delegate(animation_delegate_adapter_.get());
}

bool WebLayerImpl::addAnimation(blink::WebAnimation* animation) {
  bool result = layer_->AddAnimation(
      static_cast<WebAnimationImpl*>(animation)->PassAnimation());
  delete animation;
  return result;
}

void WebLayerImpl::removeAnimation(int animation_id) {
  layer_->RemoveAnimation(animation_id);
}

void WebLayerImpl::removeAnimation(
    int animation_id,
    blink::WebAnimation::TargetProperty target_property) {
  layer_->layer_animation_controller()->RemoveAnimation(
      animation_id,
      static_cast<Animation::TargetProperty>(target_property));
}

void WebLayerImpl::pauseAnimation(int animation_id, double time_offset) {
  layer_->PauseAnimation(animation_id, time_offset);
}

bool WebLayerImpl::hasActiveAnimation() { return layer_->HasActiveAnimation(); }

void WebLayerImpl::setForceRenderSurface(bool force_render_surface) {
  layer_->SetForceRenderSurface(force_render_surface);
}

void WebLayerImpl::setScrollPosition(blink::WebPoint position) {
  layer_->SetScrollOffset(gfx::Point(position).OffsetFromOrigin());
}

blink::WebPoint WebLayerImpl::scrollPosition() const {
  return gfx::PointAtOffsetFromOrigin(layer_->scroll_offset());
}

void WebLayerImpl::setMaxScrollPosition(WebSize max_scroll_position) {
  layer_->SetMaxScrollOffset(max_scroll_position);
}

WebSize WebLayerImpl::maxScrollPosition() const {
  return layer_->max_scroll_offset();
}

void WebLayerImpl::setScrollable(bool scrollable) {
  layer_->SetScrollable(scrollable);
}

bool WebLayerImpl::scrollable() const { return layer_->scrollable(); }

void WebLayerImpl::setUserScrollable(bool horizontal, bool vertical) {
  layer_->SetUserScrollable(horizontal, vertical);
}

bool WebLayerImpl::userScrollableHorizontal() const {
  return layer_->user_scrollable_horizontal();
}

bool WebLayerImpl::userScrollableVertical() const {
  return layer_->user_scrollable_vertical();
}

void WebLayerImpl::setHaveWheelEventHandlers(bool have_wheel_event_handlers) {
  layer_->SetHaveWheelEventHandlers(have_wheel_event_handlers);
}

bool WebLayerImpl::haveWheelEventHandlers() const {
  return layer_->have_wheel_event_handlers();
}

void WebLayerImpl::setShouldScrollOnMainThread(
    bool should_scroll_on_main_thread) {
  layer_->SetShouldScrollOnMainThread(should_scroll_on_main_thread);
}

bool WebLayerImpl::shouldScrollOnMainThread() const {
  return layer_->should_scroll_on_main_thread();
}

void WebLayerImpl::setNonFastScrollableRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->SetNonFastScrollableRegion(region);
}

WebVector<WebRect> WebLayerImpl::nonFastScrollableRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->non_fast_scrollable_region());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->non_fast_scrollable_region());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::setTouchEventHandlerRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->SetTouchEventHandlerRegion(region);
}

WebVector<WebRect> WebLayerImpl::touchEventHandlerRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->touch_event_handler_region());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->touch_event_handler_region());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::setIsContainerForFixedPositionLayers(bool enable) {
  layer_->SetIsContainerForFixedPositionLayers(enable);
}

bool WebLayerImpl::isContainerForFixedPositionLayers() const {
  return layer_->IsContainerForFixedPositionLayers();
}

static blink::WebLayerPositionConstraint ToWebLayerPositionConstraint(
    const cc::LayerPositionConstraint& constraint) {
  blink::WebLayerPositionConstraint web_constraint;
  web_constraint.isFixedPosition = constraint.is_fixed_position();
  web_constraint.isFixedToRightEdge = constraint.is_fixed_to_right_edge();
  web_constraint.isFixedToBottomEdge = constraint.is_fixed_to_bottom_edge();
  return web_constraint;
}

static cc::LayerPositionConstraint ToLayerPositionConstraint(
    const blink::WebLayerPositionConstraint& web_constraint) {
  cc::LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(web_constraint.isFixedPosition);
  constraint.set_is_fixed_to_right_edge(web_constraint.isFixedToRightEdge);
  constraint.set_is_fixed_to_bottom_edge(web_constraint.isFixedToBottomEdge);
  return constraint;
}

void WebLayerImpl::setPositionConstraint(
    const blink::WebLayerPositionConstraint& constraint) {
  layer_->SetPositionConstraint(ToLayerPositionConstraint(constraint));
}

blink::WebLayerPositionConstraint WebLayerImpl::positionConstraint() const {
  return ToWebLayerPositionConstraint(layer_->position_constraint());
}

void WebLayerImpl::setScrollClient(
    blink::WebLayerScrollClient* scroll_client) {
  if (scroll_client) {
    layer_->set_did_scroll_callback(
        base::Bind(&blink::WebLayerScrollClient::didScroll,
                   base::Unretained(scroll_client)));
  } else {
    layer_->set_did_scroll_callback(base::Closure());
  }
}

bool WebLayerImpl::isOrphan() const { return !layer_->layer_tree_host(); }

void WebLayerImpl::setWebLayerClient(blink::WebLayerClient* client) {
  web_layer_client_ = client;
}

// TODO(chrishtr): move DebugName into this class.
class TracedDebugInfo : public base::debug::ConvertableToTraceFormat {
 public:
  // This object takes ownership of the debug_info object.
  explicit TracedDebugInfo(blink::WebGraphicsLayerDebugInfo* debug_info) :
    debug_info_(debug_info) {}
  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    blink::WebString web_string;
    debug_info_->appendAsTraceFormat(&web_string);
    out->append(web_string.utf8());
  }
 private:
  virtual ~TracedDebugInfo() {}
  scoped_ptr<blink::WebGraphicsLayerDebugInfo> debug_info_;
  base::ThreadChecker thread_checker_;
};

scoped_refptr<base::debug::ConvertableToTraceFormat>
    WebLayerImpl::TakeDebugInfo() {
  if (!web_layer_client_)
    return NULL;
  blink::WebGraphicsLayerDebugInfo* debug_info =
      web_layer_client_->takeDebugInfo();

  if (debug_info)
    return new TracedDebugInfo(debug_info);
  else
    return NULL;
}

std::string WebLayerImpl::DebugName() {
  if (!web_layer_client_)
    return std::string();

  std::string name = web_layer_client_->debugName(this).utf8();
  DCHECK(IsStringASCII(name));
  return name;
}

void WebLayerImpl::setScrollParent(blink::WebLayer* parent) {
  cc::Layer* scroll_parent = NULL;
  if (parent)
    scroll_parent = static_cast<WebLayerImpl*>(parent)->layer();
  layer_->SetScrollParent(scroll_parent);
}

void WebLayerImpl::setClipParent(blink::WebLayer* parent) {
  cc::Layer* clip_parent = NULL;
  if (parent)
    clip_parent = static_cast<WebLayerImpl*>(parent)->layer();
  layer_->SetClipParent(clip_parent);
}

Layer* WebLayerImpl::layer() const { return layer_.get(); }

}  // namespace webkit
