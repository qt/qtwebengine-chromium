// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_H_
#define CC_LAYERS_LAYER_IMPL_H_

#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/input/input_handler.h"
#include "cc/layers/compositing_reasons.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer_lists.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/output/filter_operations.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/resources/resource_provider.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class LayerTreeHostImpl;
class LayerTreeImpl;
class QuadSink;
class Renderer;
class ScrollbarAnimationController;
class ScrollbarLayerImplBase;
class Layer;

struct AppendQuadsData;

enum DrawMode {
  DRAW_MODE_NONE,
  DRAW_MODE_HARDWARE,
  DRAW_MODE_SOFTWARE,
  DRAW_MODE_RESOURCELESS_SOFTWARE
};

class CC_EXPORT LayerImpl : LayerAnimationValueObserver {
 public:
  static scoped_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new LayerImpl(tree_impl, id));
  }

  virtual ~LayerImpl();

  int id() const { return layer_id_; }

  // LayerAnimationValueObserver implementation.
  virtual void OnOpacityAnimated(float opacity) OVERRIDE;
  virtual void OnTransformAnimated(const gfx::Transform& transform) OVERRIDE;
  virtual bool IsActive() const OVERRIDE;

  // Tree structure.
  LayerImpl* parent() { return parent_; }
  const LayerImpl* parent() const { return parent_; }
  const OwnedLayerImplList& children() const { return children_; }
  OwnedLayerImplList& children() { return children_; }
  LayerImpl* child_at(size_t index) const { return children_[index]; }
  void AddChild(scoped_ptr<LayerImpl> child);
  scoped_ptr<LayerImpl> RemoveChild(LayerImpl* child);
  void set_parent(LayerImpl* parent) { parent_ = parent; }
  // Warning: This does not preserve tree structure invariants.
  void ClearChildList();

  bool HasAncestor(const LayerImpl* ancestor) const;

  void SetScrollParent(LayerImpl* parent);

  LayerImpl* scroll_parent() { return scroll_parent_; }
  const LayerImpl* scroll_parent() const { return scroll_parent_; }

  void SetScrollChildren(std::set<LayerImpl*>* children);
  void RemoveScrollChild(LayerImpl* child);

  std::set<LayerImpl*>* scroll_children() { return scroll_children_.get(); }
  const std::set<LayerImpl*>* scroll_children() const {
    return scroll_children_.get();
  }

  void SetClipParent(LayerImpl* ancestor);

  LayerImpl* clip_parent() {
    return clip_parent_;
  }
  const LayerImpl* clip_parent() const {
    return clip_parent_;
  }

  void SetClipChildren(std::set<LayerImpl*>* children);
  void RemoveClipChild(LayerImpl* child);

  std::set<LayerImpl*>* clip_children() { return clip_children_.get(); }
  const std::set<LayerImpl*>* clip_children() const {
    return clip_children_.get();
  }

  void PassCopyRequests(ScopedPtrVector<CopyOutputRequest>* requests);
  void TakeCopyRequestsAndTransformToTarget(
      ScopedPtrVector<CopyOutputRequest>* request);
  bool HasCopyRequest() const { return !copy_requests_.empty(); }

  void SetMaskLayer(scoped_ptr<LayerImpl> mask_layer);
  LayerImpl* mask_layer() { return mask_layer_.get(); }
  const LayerImpl* mask_layer() const { return mask_layer_.get(); }
  scoped_ptr<LayerImpl> TakeMaskLayer();

  void SetReplicaLayer(scoped_ptr<LayerImpl> replica_layer);
  LayerImpl* replica_layer() { return replica_layer_.get(); }
  const LayerImpl* replica_layer() const { return replica_layer_.get(); }
  scoped_ptr<LayerImpl> TakeReplicaLayer();

  bool has_mask() const { return mask_layer_; }
  bool has_replica() const { return replica_layer_; }
  bool replica_has_mask() const {
    return replica_layer_ && (mask_layer_ || replica_layer_->mask_layer_);
  }

  LayerTreeImpl* layer_tree_impl() const { return layer_tree_impl_; }

  scoped_ptr<SharedQuadState> CreateSharedQuadState() const;
  // WillDraw must be called before AppendQuads. If WillDraw returns false,
  // AppendQuads and DidDraw will not be called. If WillDraw returns true,
  // DidDraw is guaranteed to be called before another WillDraw or before
  // the layer is destroyed. To enforce this, any class that overrides
  // WillDraw/DidDraw must call the base class version only if WillDraw
  // returns true.
  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider);
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) {}
  virtual void DidDraw(ResourceProvider* resource_provider);

  virtual ResourceProvider::ResourceId ContentsResourceId() const;

  virtual bool HasDelegatedContent() const;
  virtual bool HasContributingDelegatedRenderPasses() const;
  virtual RenderPass::Id FirstContributingRenderPassId() const;
  virtual RenderPass::Id NextContributingRenderPassId(RenderPass::Id id) const;

  virtual void UpdateTilePriorities() {}

  virtual ScrollbarLayerImplBase* ToScrollbarLayer();

  // Returns true if this layer has content to draw.
  void SetDrawsContent(bool draws_content);
  bool DrawsContent() const { return draws_content_; }

  void SetHideLayerAndSubtree(bool hide);
  bool hide_layer_and_subtree() const { return hide_layer_and_subtree_; }

  bool force_render_surface() const { return force_render_surface_; }
  void SetForceRenderSurface(bool force) { force_render_surface_ = force; }

  void SetAnchorPoint(gfx::PointF anchor_point);
  gfx::PointF anchor_point() const { return anchor_point_; }

  void SetAnchorPointZ(float anchor_point_z);
  float anchor_point_z() const { return anchor_point_z_; }

  void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }
  // If contents_opaque(), return an opaque color else return a
  // non-opaque color.  Tries to return background_color(), if possible.
  SkColor SafeOpaqueBackgroundColor() const;

  void SetFilters(const FilterOperations& filters);
  const FilterOperations& filters() const { return filters_; }

  void SetBackgroundFilters(const FilterOperations& filters);
  const FilterOperations& background_filters() const {
    return background_filters_;
  }

  void SetFilter(const skia::RefPtr<SkImageFilter>& filter);
  skia::RefPtr<SkImageFilter> filter() const { return filter_; }

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return masks_to_bounds_; }

  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }

  void SetOpacity(float opacity);
  float opacity() const { return opacity_; }
  bool OpacityIsAnimating() const;
  bool OpacityIsAnimatingOnImplOnly() const;

  void SetPosition(gfx::PointF position);
  gfx::PointF position() const { return position_; }

  void SetIsContainerForFixedPositionLayers(bool container) {
    is_container_for_fixed_position_layers_ = container;
  }
  // This is a non-trivial function in Layer.
  bool IsContainerForFixedPositionLayers() const {
    return is_container_for_fixed_position_layers_;
  }

  void SetFixedContainerSizeDelta(gfx::Vector2dF delta) {
    fixed_container_size_delta_ = delta;
  }
  gfx::Vector2dF fixed_container_size_delta() const {
    return fixed_container_size_delta_;
  }

  void SetPositionConstraint(const LayerPositionConstraint& constraint) {
    position_constraint_ = constraint;
  }
  const LayerPositionConstraint& position_constraint() const {
    return position_constraint_;
  }

  void SetPreserves3d(bool preserves_3d);
  bool preserves_3d() const { return preserves_3d_; }

  void SetUseParentBackfaceVisibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  void SetSublayerTransform(const gfx::Transform& sublayer_transform);
  const gfx::Transform& sublayer_transform() const {
    return sublayer_transform_;
  }

  // Debug layer name.
  void SetDebugName(const std::string& debug_name) { debug_name_ = debug_name; }
  std::string debug_name() const { return debug_name_; }

  void SetCompositingReasons(CompositingReasons reasons) {
      compositing_reasons_ = reasons;
  }

  CompositingReasons compositing_reasons() const {
      return compositing_reasons_;
  }

  bool ShowDebugBorders() const;

  // These invalidate the host's render surface layer list.  The caller
  // is responsible for calling set_needs_update_draw_properties on the tree
  // so that its list can be recreated.
  void CreateRenderSurface();
  void ClearRenderSurface();

  DrawProperties<LayerImpl, RenderSurfaceImpl>& draw_properties() {
    return draw_properties_;
  }
  const DrawProperties<LayerImpl, RenderSurfaceImpl>& draw_properties() const {
    return draw_properties_;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  const gfx::Transform& draw_transform() const {
    return draw_properties_.target_space_transform;
  }
  const gfx::Transform& screen_space_transform() const {
    return draw_properties_.screen_space_transform;
  }
  float draw_opacity() const { return draw_properties_.opacity; }
  bool draw_opacity_is_animating() const {
    return draw_properties_.opacity_is_animating;
  }
  bool draw_transform_is_animating() const {
    return draw_properties_.target_space_transform_is_animating;
  }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  bool screen_space_opacity_is_animating() const {
    return draw_properties_.screen_space_opacity_is_animating;
  }
  bool can_use_lcd_text() const { return draw_properties_.can_use_lcd_text; }
  bool is_clipped() const { return draw_properties_.is_clipped; }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect drawable_content_rect() const {
    return draw_properties_.drawable_content_rect;
  }
  gfx::Rect visible_content_rect() const {
    return draw_properties_.visible_content_rect;
  }
  LayerImpl* render_target() {
    DCHECK(!draw_properties_.render_target ||
           draw_properties_.render_target->render_surface());
    return draw_properties_.render_target;
  }
  const LayerImpl* render_target() const {
    DCHECK(!draw_properties_.render_target ||
           draw_properties_.render_target->render_surface());
    return draw_properties_.render_target;
  }
  RenderSurfaceImpl* render_surface() const {
    return draw_properties_.render_surface.get();
  }
  int num_unclipped_descendants() const {
    return draw_properties_.num_unclipped_descendants;
  }

  // The client should be responsible for setting bounds, content bounds and
  // contents scale to appropriate values. LayerImpl doesn't calculate any of
  // them from the other values.

  void SetBounds(gfx::Size bounds);
  gfx::Size bounds() const { return bounds_; }

  void SetContentBounds(gfx::Size content_bounds);
  gfx::Size content_bounds() const { return draw_properties_.content_bounds; }

  float contents_scale_x() const { return draw_properties_.contents_scale_x; }
  float contents_scale_y() const { return draw_properties_.contents_scale_y; }
  void SetContentsScale(float contents_scale_x, float contents_scale_y);

  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds);

  void SetScrollOffsetDelegate(
      LayerScrollOffsetDelegate* scroll_offset_delegate);
  void SetScrollOffset(gfx::Vector2d scroll_offset);
  void SetScrollOffsetAndDelta(gfx::Vector2d scroll_offset,
                               gfx::Vector2dF scroll_delta);
  gfx::Vector2d scroll_offset() const { return scroll_offset_; }

  void SetMaxScrollOffset(gfx::Vector2d max_scroll_offset);
  gfx::Vector2d max_scroll_offset() const { return max_scroll_offset_; }

  void SetScrollDelta(gfx::Vector2dF scroll_delta);
  gfx::Vector2dF ScrollDelta() const;

  gfx::Vector2dF TotalScrollOffset() const;

  void SetSentScrollDelta(gfx::Vector2d sent_scroll_delta);
  gfx::Vector2d sent_scroll_delta() const { return sent_scroll_delta_; }

  // Returns the delta of the scroll that was outside of the bounds of the
  // initial scroll
  gfx::Vector2dF ScrollBy(gfx::Vector2dF scroll);

  void SetScrollable(bool scrollable) { scrollable_ = scrollable; }
  bool scrollable() const { return scrollable_; }

  void ApplySentScrollDeltasFromAbortedCommit();
  void ApplyScrollDeltasSinceBeginFrame();

  void SetShouldScrollOnMainThread(bool should_scroll_on_main_thread) {
    should_scroll_on_main_thread_ = should_scroll_on_main_thread;
  }
  bool should_scroll_on_main_thread() const {
    return should_scroll_on_main_thread_;
  }

  void SetHaveWheelEventHandlers(bool have_wheel_event_handlers) {
    have_wheel_event_handlers_ = have_wheel_event_handlers;
  }
  bool have_wheel_event_handlers() const { return have_wheel_event_handlers_; }

  void SetNonFastScrollableRegion(const Region& region) {
    non_fast_scrollable_region_ = region;
  }
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchEventHandlerRegion(const Region& region) {
    touch_event_handler_region_ = region;
  }
  const Region& touch_event_handler_region() const {
    return touch_event_handler_region_;
  }

  void SetDrawCheckerboardForMissingTiles(bool checkerboard) {
    draw_checkerboard_for_missing_tiles_ = checkerboard;
  }
  bool DrawCheckerboardForMissingTiles() const;

  InputHandler::ScrollStatus TryScroll(
      gfx::PointF screen_space_point,
      InputHandler::ScrollInputType type) const;

  void SetDoubleSided(bool double_sided);
  bool double_sided() const { return double_sided_; }

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return transform_; }
  bool TransformIsAnimating() const;
  bool TransformIsAnimatingOnImplOnly() const;

  // Note this rect is in layer space (not content space).
  void set_update_rect(const gfx::RectF& update_rect) {
    update_rect_ = update_rect;
  }
  const gfx::RectF& update_rect() const { return update_rect_; }

  virtual base::DictionaryValue* LayerTreeAsJson() const;

  void SetStackingOrderChanged(bool stacking_order_changed);

  bool LayerPropertyChanged() const {
    return layer_property_changed_ || LayerIsAlwaysDamaged();
  }
  bool LayerSurfacePropertyChanged() const;

  void ResetAllChangeTrackingForSubtree();

  virtual bool LayerIsAlwaysDamaged() const;

  LayerAnimationController* layer_animation_controller() {
    return layer_animation_controller_.get();
  }

  virtual Region VisibleContentOpaqueRegion() const;

  virtual void DidBecomeActive();

  virtual void DidBeginTracing();

  // Indicates that the surface previously used to render this layer
  // was lost and that a new one has been created. Won't be called
  // until the new surface has been created successfully.
  virtual void DidLoseOutputSurface();

  ScrollbarAnimationController* scrollbar_animation_controller() const {
    return scrollbar_animation_controller_.get();
  }

  void SetHorizontalScrollbarLayer(ScrollbarLayerImplBase* scrollbar_layer);
  ScrollbarLayerImplBase* horizontal_scrollbar_layer() {
    return horizontal_scrollbar_layer_;
  }

  void SetVerticalScrollbarLayer(ScrollbarLayerImplBase* scrollbar_layer);
  ScrollbarLayerImplBase* vertical_scrollbar_layer() {
    return vertical_scrollbar_layer_;
  }

  gfx::Rect LayerRectToContentRect(const gfx::RectF& layer_rect) const;

  virtual skia::RefPtr<SkPicture> GetPicture();

  virtual bool CanClipSelf() const;

  virtual bool AreVisibleResourcesReady() const;

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);
  virtual void PushPropertiesTo(LayerImpl* layer);

  scoped_ptr<base::Value> AsValue() const;
  virtual size_t GPUMemoryUsageInBytes() const;

  // TODO(danakj): Be true only if needed. crbug.com/259511
  bool needs_push_properties() const { return true; }
  bool descendant_needs_push_properties() const { return true; }

 protected:
  LayerImpl(LayerTreeImpl* layer_impl, int id);

  // Get the color and size of the layer's debug border.
  virtual void GetDebugBorderProperties(SkColor* color, float* width) const;

  void AppendDebugBorderQuad(QuadSink* quad_sink,
                             const SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data) const;
  void AppendDebugBorderQuad(QuadSink* quad_sink,
                             const SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data,
                             SkColor color,
                             float width) const;

  virtual void AsValueInto(base::DictionaryValue* dict) const;

  void NoteLayerSurfacePropertyChanged();
  void NoteLayerPropertyChanged();
  void NoteLayerPropertyChangedForSubtree();

  // Note carefully this does not affect the current layer.
  void NoteLayerPropertyChangedForDescendants();

 private:
  void UpdateScrollbarPositions();

  virtual const char* LayerTypeAsString() const;

  // Properties internal to LayerImpl
  LayerImpl* parent_;
  OwnedLayerImplList children_;

  LayerImpl* scroll_parent_;

  // Storing a pointer to a set rather than a set since this will be rarely
  // used. If this pointer turns out to be too heavy, we could have this (and
  // the scroll parent above) be stored in a LayerImpl -> scroll_info
  // map somewhere.
  scoped_ptr<std::set<LayerImpl*> > scroll_children_;

  LayerImpl* clip_parent_;
  scoped_ptr<std::set<LayerImpl*> > clip_children_;

  // mask_layer_ can be temporarily stolen during tree sync, we need this ID to
  // confirm newly assigned layer is still the previous one
  int mask_layer_id_;
  scoped_ptr<LayerImpl> mask_layer_;
  int replica_layer_id_;  // ditto
  scoped_ptr<LayerImpl> replica_layer_;
  int layer_id_;
  LayerTreeImpl* layer_tree_impl_;

  // Properties synchronized from the associated Layer.
  gfx::PointF anchor_point_;
  float anchor_point_z_;
  gfx::Size bounds_;
  gfx::Vector2d scroll_offset_;
  LayerScrollOffsetDelegate* scroll_offset_delegate_;
  bool scrollable_;
  bool should_scroll_on_main_thread_;
  bool have_wheel_event_handlers_;
  Region non_fast_scrollable_region_;
  Region touch_event_handler_region_;
  SkColor background_color_;
  bool stacking_order_changed_;

  // Whether the "back" of this layer should draw.
  bool double_sided_;

  // Tracks if drawing-related properties have changed since last redraw.
  bool layer_property_changed_;

  // Indicates that a property has changed on this layer that would not
  // affect the pixels on its target surface, but would require redrawing
  // the target_surface onto its ancestor target_surface.
  // For layers that do not own a surface this flag acts as
  // layer_property_changed_.
  bool layer_surface_property_changed_;

  bool masks_to_bounds_;
  bool contents_opaque_;
  float opacity_;
  gfx::PointF position_;
  bool preserves_3d_;
  bool use_parent_backface_visibility_;
  bool draw_checkerboard_for_missing_tiles_;
  gfx::Transform sublayer_transform_;
  gfx::Transform transform_;

  bool draws_content_;
  bool hide_layer_and_subtree_;
  bool force_render_surface_;

  // Set for the layer that other layers are fixed to.
  bool is_container_for_fixed_position_layers_;
  // This property is effective when
  // is_container_for_fixed_position_layers_ == true,
  gfx::Vector2dF fixed_container_size_delta_;

  LayerPositionConstraint position_constraint_;

  gfx::Vector2dF scroll_delta_;
  gfx::Vector2d sent_scroll_delta_;
  gfx::Vector2d max_scroll_offset_;
  gfx::Vector2dF last_scroll_offset_;

  // The global depth value of the center of the layer. This value is used
  // to sort layers from back to front.
  float draw_depth_;

  // Debug layer name.
  std::string debug_name_;
  CompositingReasons compositing_reasons_;

  FilterOperations filters_;
  FilterOperations background_filters_;
  skia::RefPtr<SkImageFilter> filter_;

 protected:
  DrawMode current_draw_mode_;

 private:
  // Rect indicating what was repainted/updated during update.
  // Note that plugin layers bypass this and leave it empty.
  // Uses layer's content space.
  gfx::RectF update_rect_;

  // Manages animations for this layer.
  scoped_refptr<LayerAnimationController> layer_animation_controller_;

  // Manages scrollbars for this layer
  scoped_ptr<ScrollbarAnimationController> scrollbar_animation_controller_;

  // Weak pointers to this layer's scrollbars, if it has them. Updated during
  // tree synchronization.
  ScrollbarLayerImplBase* horizontal_scrollbar_layer_;
  ScrollbarLayerImplBase* vertical_scrollbar_layer_;

  ScopedPtrVector<CopyOutputRequest> copy_requests_;

  // Group of properties that need to be computed based on the layer tree
  // hierarchy before layers can be drawn.
  DrawProperties<LayerImpl, RenderSurfaceImpl> draw_properties_;

  DISALLOW_COPY_AND_ASSIGN(LayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_H_
