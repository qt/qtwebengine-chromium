// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/output/begin_frame_args.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/base/android/window_android_observer.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"

struct ViewHostMsg_TextInputState_Params;

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;

namespace cc {
class CopyOutputResult;
class DelegatedFrameProvider;
class DelegatedRendererLayer;
class Layer;
class SingleReleaseCallback;
class TextureLayer;
}

namespace blink {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class OverscrollGlow;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public BrowserAccessibilityDelegate,
      public cc::DelegatedFrameResourceCollectionClient,
      public ImageTransportFactoryAndroidObserver,
      public ui::WindowAndroidObserver {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  virtual ~RenderWidgetHostViewAndroid();

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual gfx::Size GetPhysicalBackingSize() const OVERRIDE;
  virtual float GetOverdrawBottomHeight() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputTypeChanged(ui::TextInputType type,
                                    ui::TextInputMode input_mode,
                                    bool can_compose_inline) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& copy_rects,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const base::string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceInitialized(int host_id,
                                             int route_id) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const blink::WebMouseWheelEvent& event) OVERRIDE;
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) OVERRIDE;
  virtual void OnSetNeedsFlushInput() OVERRIDE;
  virtual void GestureEventAck(int gesture_event_type,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>&
          params) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void HasTouchEventHandlers(bool need_touch_events) OVERRIDE;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) OVERRIDE;
  virtual void OnOverscrolled(gfx::Vector2dF accumulated_overscroll,
                              gfx::Vector2dF current_fling_velocity) OVERRIDE;
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) OVERRIDE;
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      OVERRIDE;

  // Implementation of BrowserAccessibilityDelegate:
  virtual void SetAccessibilityFocus(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, gfx::Rect subfocus) OVERRIDE;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, gfx::Point point) OVERRIDE;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) OVERRIDE;
  virtual gfx::Point GetLastTouchEventLocation() const OVERRIDE;
  virtual void FatalAccessibilityTreeError() OVERRIDE;

  // cc::DelegatedFrameResourceCollectionClient implementation.
  virtual void UnusedResourcesAreAvailable() OVERRIDE;

  // ui::WindowAndroidObserver implementation.
  virtual void OnCompositingDidCommit() OVERRIDE;
  virtual void OnAttachCompositor() OVERRIDE {}
  virtual void OnDetachCompositor() OVERRIDE;

  // ImageTransportFactoryAndroidObserver implementation.
  virtual void OnLostResources() OVERRIDE;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendTouchEvent(const blink::WebTouchEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);
  void SendBeginFrame(const cc::BeginFrameArgs& args);

  void OnTextInputStateChanged(const ViewHostMsg_TextInputState_Params& params);
  void OnDidChangeBodyBackgroundColor(SkColor color);
  void OnStartContentIntent(const GURL& content_url);
  void OnSetNeedsBeginFrame(bool enabled);
  void OnSmartClipDataExtracted(const string16& result);

  int GetNativeImeAdapter();

  void WasResized();

  blink::WebGLId GetScaledContentTexture(float scale, gfx::Size* out_size);
  bool PopulateBitmapWithContents(jobject jbitmap);

  bool HasValidFrame() const;

  // Select all text between the given coordinates.
  void SelectRange(const gfx::Point& start, const gfx::Point& end);

  void MoveCaret(const gfx::Point& point);

  void RequestContentClipping(const gfx::Rect& clipping,
                              const gfx::Size& content_size);

  // Returns true when animation ticks are still needed. This avoids a separate
  // round-trip for requesting follow-up animation.
  bool Animate(base::TimeTicks frame_time);

  void SynchronousFrameMetadata(
      const cc::CompositorFrameMetadata& frame_metadata);

 private:
  void BuffersSwapped(const gpu::Mailbox& mailbox,
                      uint32_t output_surface_id,
                      const base::Closure& ack_callback);

  void RunAckCallbacks();

  void DestroyDelegatedContent();
  void SwapDelegatedFrame(uint32 output_surface_id,
                          scoped_ptr<cc::DelegatedFrameData> frame_data);
  void SendDelegatedFrameAck(uint32 output_surface_id);

  void UpdateContentViewCoreFrameMetadata(
      const cc::CompositorFrameMetadata& frame_metadata);
  void ComputeContentsSize(const cc::CompositorFrameMetadata& frame_metadata);
  void ResetClipping();
  void ClipContents(const gfx::Rect& clipping, const gfx::Size& content_size);

  void AttachLayers();
  void RemoveLayers();

  void UpdateAnimationSize(const cc::CompositorFrameMetadata& frame_metadata);

  // Called after async screenshot task completes. Scales and crops the result
  // of the copy.
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareBitmapCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(
      const gfx::Rect& src_subrect_in_pixel,
      const gfx::Size& dst_size_in_pixel,
      const base::Callback<void(bool, const SkBitmap&)>& callback);

  // The model object.
  RenderWidgetHostImpl* host_;

  // Used to track whether this render widget needs a BeginFrame.
  bool needs_begin_frame_;

  // Whether or not this widget is potentially attached to the view hierarchy.
  // This view may not actually be attached if this is true, but it should be
  // treated as such, because as soon as a ContentViewCore is set the layer
  // will be attached automatically.
  bool are_layers_attached_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  // The texture layer for this view when using browser-side compositing.
  scoped_refptr<cc::TextureLayer> texture_layer_;

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;
  scoped_refptr<cc::DelegatedRendererLayer> delegated_renderer_layer_;

  // The layer used for rendering the contents of this view.
  // It is either owned by texture_layer_ or surface_texture_transport_
  // depending on the mode.
  scoped_refptr<cc::Layer> layer_;

  // The most recent texture id that was pushed to the texture layer.
  unsigned int texture_id_in_layer_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // The most recent content size that was pushed to the texture layer.
  gfx::Size content_size_in_layer_;

  // The mailbox of the previously received frame.
  gpu::Mailbox current_mailbox_;

  // The output surface id of the last received frame.
  uint32_t last_output_surface_id_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_;

  std::queue<base::Closure> ack_callbacks_;

  const bool overscroll_effect_enabled_;
  // Used to render overscroll overlays.
  // Note: |overscroll_effect_| will never be NULL, even if it's never enabled.
  scoped_ptr<OverscrollGlow> overscroll_effect_;

  bool flush_input_requested_;

  int accelerated_surface_route_id_;

  // Size to use if we have no backing ContentViewCore
  gfx::Size default_size_;

  const bool using_synchronous_compositor_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
