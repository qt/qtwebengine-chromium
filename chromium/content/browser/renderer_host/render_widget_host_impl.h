// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/common/browser_rendering_stats.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/view_message_enums.h"
#include "content/port/browser/event_with_latency_info.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/page_zoom.h"
#include "ipc/ipc_listener.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"

class WebCursor;
struct AcceleratedSurfaceMsg_BufferPresented_Params;
struct ViewHostMsg_CompositorSurfaceBuffersSwapped_Params;
struct ViewHostMsg_UpdateRect_Params;
struct ViewHostMsg_TextInputState_Params;
struct ViewHostMsg_BeginSmoothScroll_Params;

namespace base {
class TimeTicks;
}

namespace cc {
class CompositorFrame;
class CompositorFrameAck;
}

namespace gfx {
class Range;
}

namespace ui {
class KeyEvent;
}

namespace blink {
class WebInputEvent;
class WebMouseEvent;
struct WebCompositionUnderline;
struct WebScreenInfo;
}

#if defined(OS_ANDROID)
namespace blink {
class WebLayer;
}
#endif

namespace content {
class BackingStore;
class InputRouter;
class MockRenderWidgetHost;
class OverscrollController;
class RenderWidgetHostDelegate;
class RenderWidgetHostViewPort;
class SyntheticGestureController;
class TimeoutMonitor;
struct EditCommand;

// This implements the RenderWidgetHost interface that is exposed to
// embedders of content, and adds things only visible to content.
class CONTENT_EXPORT RenderWidgetHostImpl : virtual public RenderWidgetHost,
                                            public InputRouterClient,
                                            public InputAckHandler,
                                            public IPC::Listener {
 public:
  // routing_id can be MSG_ROUTING_NONE, in which case the next available
  // routing id is taken from the RenderProcessHost.
  // If this object outlives |delegate|, DetachDelegate() must be called when
  // |delegate| goes away.
  RenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int routing_id,
                       bool hidden);
  virtual ~RenderWidgetHostImpl();

  // Similar to RenderWidgetHost::FromID, but returning the Impl object.
  static RenderWidgetHostImpl* FromID(int32 process_id, int32 routing_id);

  // Returns all RenderWidgetHosts including swapped out ones for
  // internal use. The public interface
  // RendgerWidgetHost::GetRenderWidgetHosts only returns active ones.
  static scoped_ptr<RenderWidgetHostIterator> GetAllRenderWidgetHosts();

  // Use RenderWidgetHostImpl::From(rwh) to downcast a
  // RenderWidgetHost to a RenderWidgetHostImpl.  Internally, this
  // uses RenderWidgetHost::AsRenderWidgetHostImpl().
  static RenderWidgetHostImpl* From(RenderWidgetHost* rwh);

  void set_hung_renderer_delay_ms(const base::TimeDelta& timeout) {
    hung_renderer_delay_ms_ = timeout.InMilliseconds();
  }

  // RenderWidgetHost implementation.
  virtual void Undo() OVERRIDE;
  virtual void Redo() OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void CopyToFindPboard() OVERRIDE;
  virtual void Paste() OVERRIDE;
  virtual void PasteAndMatchStyle() OVERRIDE;
  virtual void Delete() OVERRIDE;
  virtual void SelectAll() OVERRIDE;
  virtual void Unselect() OVERRIDE;
  virtual void UpdateTextDirection(blink::WebTextDirection direction) OVERRIDE;
  virtual void NotifyTextDirection() OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void SetActive(bool active) OVERRIDE;
  virtual void CopyFromBackingStore(
      const gfx::Rect& src_rect,
      const gfx::Size& accelerated_dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;
#if defined(TOOLKIT_GTK)
  virtual bool CopyFromBackingStoreToGtkWindow(const gfx::Rect& dest_rect,
                                               GdkWindow* target) OVERRIDE;
#elif defined(OS_MACOSX)
  virtual gfx::Size GetBackingStoreSize() OVERRIDE;
  virtual bool CopyFromBackingStoreToCGContext(const CGRect& dest_rect,
                                               CGContextRef target) OVERRIDE;
#endif
  virtual void EnableFullAccessibilityMode() OVERRIDE;
  virtual void ForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) OVERRIDE;
  virtual void ForwardWheelEvent(
      const blink::WebMouseWheelEvent& wheel_event) OVERRIDE;
  virtual void ForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) OVERRIDE;
  virtual const gfx::Vector2d& GetLastScrollOffset() const OVERRIDE;
  virtual RenderProcessHost* GetProcess() const OVERRIDE;
  virtual int GetRoutingID() const OVERRIDE;
  virtual RenderWidgetHostView* GetView() const OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;
  virtual bool IsRenderView() const OVERRIDE;
  virtual void PaintAtSize(TransportDIB::Handle dib_handle,
                           int tag,
                           const gfx::Size& page_size,
                           const gfx::Size& desired_size) OVERRIDE;
  virtual void Replace(const base::string16& word) OVERRIDE;
  virtual void ReplaceMisspelling(const base::string16& word) OVERRIDE;
  virtual void ResizeRectChanged(const gfx::Rect& new_rect) OVERRIDE;
  virtual void RestartHangMonitorTimeout() OVERRIDE;
  virtual void SetIgnoreInputEvents(bool ignore_input_events) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void WasResized() OVERRIDE;
  virtual void AddKeyPressEventCallback(
      const KeyPressEventCallback& callback) OVERRIDE;
  virtual void RemoveKeyPressEventCallback(
      const KeyPressEventCallback& callback) OVERRIDE;
  virtual void AddMouseEventCallback(
      const MouseEventCallback& callback) OVERRIDE;
  virtual void RemoveMouseEventCallback(
      const MouseEventCallback& callback) OVERRIDE;
  virtual void GetWebScreenInfo(blink::WebScreenInfo* result) OVERRIDE;
  virtual void GetSnapshotFromRenderer(
      const gfx::Rect& src_subrect,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;

  const NativeWebKeyboardEvent* GetLastKeyboardEvent() const;

  // Notification that the screen info has changed.
  void NotifyScreenInfoChanged();

  // Invalidates the cached screen info so that next resize request
  // will carry the up to date screen info. Unlike
  // |NotifyScreenInfoChanged|, this doesn't send a message to the renderer.
  void InvalidateScreenInfo();

  // Sets the View of this RenderWidgetHost.
  void SetView(RenderWidgetHostView* view);

  int surface_id() const { return surface_id_; }

  bool empty() const { return current_size_.IsEmpty(); }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  virtual void Init();

  // Tells the renderer to die and then calls Destroy().
  virtual void Shutdown();

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Sends a message to the corresponding object in the renderer.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Called to notify the RenderWidget that it has been hidden or restored from
  // having been hidden.
  void WasHidden();
  void WasShown();

  // Returns true if the RenderWidget is hidden.
  bool is_hidden() const { return is_hidden_; }

  // Called to notify the RenderWidget that its associated native window
  // got/lost focused.
  virtual void GotFocus();
  virtual void LostCapture();

  // Called to notify the RenderWidget that it has lost the mouse lock.
  virtual void LostMouseLock();

  // Noifies the RenderWidget of the current mouse cursor visibility state.
  void SendCursorVisibilityState(bool is_visible);

  // Tells us whether the page is rendered directly via the GPU process.
  bool is_accelerated_compositing_active() {
    return is_accelerated_compositing_active_;
  }

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

  // Indicates if the page has finished loading.
  void SetIsLoading(bool is_loading);

  // Check for the existance of a BackingStore of the given |desired_size| and
  // return it if it exists. If the BackingStore is GPU, true is returned and
  // |*backing_store| is set to NULL.
  bool TryGetBackingStore(const gfx::Size& desired_size,
                          BackingStore** backing_store);

  // Get access to the widget's backing store matching the size of the widget's
  // view. If you pass |force_create| as true, then GetBackingStore may block
  // for the renderer to send a new frame. Otherwise, NULL will be returned if
  // the backing store doesn't already exist. It will also return NULL if the
  // backing store could not be created.
  //
  // Mac only: NULL may also be returned if the last frame was GPU accelerated.
  // Call GetView()->HasAcceleratedSurface to determine if the last frame was
  // accelerated.
  BackingStore* GetBackingStore(bool force_create);

  // Allocate a new backing store of the given size. Returns NULL on failure
  // (for example, if we don't currently have a RenderWidgetHostView.)
  BackingStore* AllocBackingStore(const gfx::Size& size);

  // When a backing store does asynchronous painting, it will call this function
  // when it is done with the DIB. We will then forward a message to the
  // renderer to send another paint.
  void DonePaintingToBackingStore();

  // GPU accelerated version of GetBackingStore function. This will
  // trigger a re-composite to the view. It may fail if a resize is pending, or
  // if a composite has already been requested and not acked yet.
  bool ScheduleComposite();

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartHangMonitorTimeout(base::TimeDelta delay);

  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopHangMonitorTimeout();

  // Forwards the given message to the renderer. These are called by the view
  // when it has received a message.
  void ForwardGestureEvent(const blink::WebGestureEvent& gesture_event);
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& ui_latency);
  void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency);
  void ForwardMouseEventWithLatencyInfo(
      const MouseEventWithLatencyInfo& mouse_event);
  void ForwardWheelEventWithLatencyInfo(
      const MouseWheelEventWithLatencyInfo& wheel_event);

  void CancelUpdateTextDirection();

  // Called when a mouse click/gesture tap activates the renderer.
  virtual void OnPointerEventActivate();

  // Notifies the renderer whether or not the input method attached to this
  // process is activated.
  // When the input method is activated, a renderer process sends IPC messages
  // to notify the status of its composition node. (This message is mainly used
  // for notifying the position of the input cursor so that the browser can
  // display input method windows under the cursor.)
  void SetInputMethodActive(bool activate);

  // Notifies the renderer changes of IME candidate window state.
  void CandidateWindowShown();
  void CandidateWindowUpdated();
  void CandidateWindowHidden();

  // Update the composition node of the renderer (or WebKit).
  // WebKit has a special node (a composition node) for input method to change
  // its text without affecting any other DOM nodes. When the input method
  // (attached to the browser) updates its text, the browser sends IPC messages
  // to update the composition node of the renderer.
  // (Read the comments of each function for its detail.)

  // Sets the text of the composition node.
  // This function can also update the cursor position and mark the specified
  // range in the composition node.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_COMPSTR flag
  //   (on Windows);
  // * when it receives a "preedit_changed" signal of GtkIMContext (on Linux);
  // * when markedText of NSTextInput is called (on Mac).
  void ImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);

  // Finishes an ongoing composition with the specified text.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR flag
  //   (on Windows);
  // * when it receives a "commit" signal of GtkIMContext (on Linux);
  // * when insertText of NSTextInput is called (on Mac).
  void ImeConfirmComposition(const base::string16& text,
                             const gfx::Range& replacement_range,
                             bool keep_selection);

  // Cancels an ongoing composition.
  void ImeCancelComposition();

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  void ExtendSelectionAndDelete(size_t before, size_t after);

  // This is for derived classes to give us access to the resizer rect.
  // And to also expose it to the RenderWidgetHostView.
  virtual gfx::Rect GetRootWindowResizerRect() const;

  bool ignore_input_events() const {
    return ignore_input_events_;
  }

  bool input_method_active() const {
    return input_method_active_;
  }

  // Whether forwarded WebInputEvents should be ignored.  True if either
  // |ignore_input_events_| or |process_->IgnoreInputEvents()| is true.
  bool IgnoreInputEvents() const;

  // Event queries delegated to the |input_router_|.
  bool ShouldForwardTouchEvent() const;

  bool has_touch_handler() const { return has_touch_handler_; }

  // Notification that the user has made some kind of input that could
  // perform an action. See OnUserGesture for more details.
  void StartUserGesture();

  // Set the RenderView background.
  void SetBackground(const SkBitmap& background);

  // Notifies the renderer that the next key event is bound to one or more
  // pre-defined edit commands
  void SetEditCommandsForNextKeyEvent(
      const std::vector<EditCommand>& commands);

  // Gets the accessibility mode.
  AccessibilityMode accessibility_mode() const {
    return accessibility_mode_;
  }

  // Send a message to the renderer process to change the accessibility mode.
  void SetAccessibilityMode(AccessibilityMode mode);

  // Relay a request from assistive technology to perform the default action
  // on a given node.
  void AccessibilityDoDefaultAction(int object_id);

  // Relay a request from assistive technology to set focus to a given node.
  void AccessibilitySetFocus(int object_id);

  // Relay a request from assistive technology to make a given object
  // visible by scrolling as many scrollable containers as necessary.
  // In addition, if it's not possible to make the entire object visible,
  // scroll so that the |subfocus| rect is visible at least. The subfocus
  // rect is in local coordinates of the object itself.
  void AccessibilityScrollToMakeVisible(
      int acc_obj_id, gfx::Rect subfocus);

  // Relay a request from assistive technology to move a given object
  // to a specific location, in the WebContents area coordinate space, i.e.
  // (0, 0) is the top-left corner of the WebContents.
  void AccessibilityScrollToPoint(int acc_obj_id, gfx::Point point);

  // Relay a request from assistive technology to set text selection.
  void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset);

  // Kill the renderer because we got a fatal accessibility error.
  void FatalAccessibilityTreeError();

#if defined(OS_WIN) && defined(USE_AURA)
  void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent);
  gfx::NativeViewAccessible GetParentNativeViewAccessible() const;
#endif

  // Executes the edit command on the RenderView.
  void ExecuteEditCommand(const std::string& command,
                          const std::string& value);

  // Tells the renderer to scroll the currently focused node into rect only if
  // the currently focused node is a Text node (textfield, text area or content
  // editable divs).
  void ScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);

  // Requests the renderer to select the region between two points.
  void SelectRange(const gfx::Point& start, const gfx::Point& end);

  // Requests the renderer to move the caret selection towards the point.
  void MoveCaret(const gfx::Point& point);

  // Called when the reponse to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  bool GotResponseToLockMouseRequest(bool allowed);

  // Tells the RenderWidget about the latest vsync parameters.
  // Note: Make sure the timebase was obtained using
  // base::TimeTicks::HighResNow. Using the non-high res timer will result in
  // incorrect synchronization across processes.
  virtual void UpdateVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval);

  // Called by the view in response to AcceleratedSurfaceBuffersSwapped or
  // AcceleratedSurfacePostSubBuffer.
  static void AcknowledgeBufferPresent(
      int32 route_id,
      int gpu_host_id,
      const AcceleratedSurfaceMsg_BufferPresented_Params& params);

  // Called by the view in response to OnSwapCompositorFrame.
  static void SendSwapCompositorFrameAck(
      int32 route_id,
      uint32 output_surface_id,
      int renderer_host_id,
      const cc::CompositorFrameAck& ack);

  // Called by the view to return resources to the compositor.
  static void SendReclaimCompositorResources(int32 route_id,
                                             uint32 output_surface_id,
                                             int renderer_host_id,
                                             const cc::CompositorFrameAck& ack);

  // Called by the view in response to AcceleratedSurfaceBuffersSwapped for
  // platforms that support deferred GPU process descheduling. This does
  // nothing if the compositor thread is enabled.
  // TODO(jbates) Once the compositor thread is always on, this can be removed.
  void AcknowledgeSwapBuffersToRenderer();

  bool is_threaded_compositing_enabled() const {
    return is_threaded_compositing_enabled_;
  }

#if defined(USE_AURA)
  // Called by the view when the parent changes. If a parent isn't available,
  // NULL is used.
  void ParentChanged(gfx::NativeViewId new_parent);
#endif

  // Signals that the compositing surface was updated, e.g. after a lost context
  // event.
  void CompositingSurfaceUpdated();

  void set_allow_privileged_mouse_lock(bool allow) {
    allow_privileged_mouse_lock_ = allow;
  }

  // Resets state variables related to tracking pending size and painting.
  //
  // We need to reset these flags when we want to repaint the contents of
  // browser plugin in this RWH. Resetting these flags will ensure we ignore
  // any previous pending acks that are not relevant upon repaint.
  void ResetSizeAndRepaintPendingFlags();

  void DetachDelegate();

  // Update the renderer's cache of the screen rect of the view and window.
  void SendScreenRects();

  OverscrollController* overscroll_controller() const {
    return overscroll_controller_.get();
  }

  // Sets whether the overscroll controller should be enabled for this page.
  void SetOverscrollControllerEnabled(bool enabled);

  // Suppreses future char events until a keydown. See
  // suppress_next_char_events_.
  void SuppressNextCharEvents();

  // Called by RenderWidgetHostView in response to OnSetNeedsFlushInput.
  void FlushInput();

  // InputRouterClient
  virtual void SetNeedsFlush() OVERRIDE;

  // Indicates whether the renderer drives the RenderWidgetHosts's size or the
  // other way around.
  bool should_auto_resize() { return should_auto_resize_; }

  void ComputeTouchLatency(const ui::LatencyInfo& latency_info);
  void FrameSwapped(const ui::LatencyInfo& latency_info);
  void DidReceiveRendererFrame();

  // Returns the ID that uniquely describes this component to the latency
  // subsystem.
  int64 GetLatencyComponentId();

  static void CompositorFrameDrawn(const ui::LatencyInfo& latency_info);

  // Don't check whether we expected a resize ack during layout tests.
  static void DisableResizeAckCheckForTesting();

 protected:
  virtual RenderWidgetHostImpl* AsRenderWidgetHostImpl() OVERRIDE;

  // Create a LatencyInfo struct with INPUT_EVENT_LATENCY_RWH_COMPONENT
  // component if it is not already in |original|. And if |original| is
  // not NULL, it is also merged into the resulting LatencyInfo.
  ui::LatencyInfo CreateRWHLatencyInfoIfNotExist(
      const ui::LatencyInfo* original, blink::WebInputEvent::Type type);

  // Called when we receive a notification indicating that the renderer
  // process has gone. This will reset our state so that our state will be
  // consistent if a new renderer is created.
  void RendererExited(base::TerminationStatus status, int exit_code);

  // Retrieves an id the renderer can use to refer to its view.
  // This is used for various IPC messages, including plugins.
  gfx::NativeViewId GetNativeViewId() const;

  // Retrieves an id for the surface that the renderer can draw to
  // when accelerated compositing is enabled.
  gfx::GLSurfaceHandle GetCompositingSurface();

  // ---------------------------------------------------------------------------
  // The following methods are overridden by RenderViewHost to send upwards to
  // its delegate.

  // Called when a mousewheel event was not processed by the renderer.
  virtual void UnhandledWheelEvent(const blink::WebMouseWheelEvent& event) {}

  // Notification that the user has made some kind of input that could
  // perform an action. The gestures that count are 1) any mouse down
  // event and 2) enter or space key presses.
  virtual void OnUserGesture() {}

  // Callbacks for notification when the renderer becomes unresponsive to user
  // input events, and subsequently responsive again.
  virtual void NotifyRendererUnresponsive() {}
  virtual void NotifyRendererResponsive() {}

  // Called when auto-resize resulted in the renderer size changing.
  virtual void OnRenderAutoResized(const gfx::Size& new_size) {}

  // ---------------------------------------------------------------------------

  // RenderViewHost overrides this method to impose further restrictions on when
  // to allow mouse lock.
  // Once the request is approved or rejected, GotResponseToLockMouseRequest()
  // will be called.
  virtual void RequestToLockMouse(bool user_gesture,
                                  bool last_unlocked_by_target);

  void RejectMouseLockOrUnlockIfNecessary();
  bool IsMouseLocked() const;

  // RenderViewHost overrides this method to report when in fullscreen mode.
  virtual bool IsFullscreen() const;

  // Indicates if the render widget host should track the render widget's size
  // as opposed to visa versa.
  void SetShouldAutoResize(bool enable);

  // Expose increment/decrement of the in-flight event count, so
  // RenderViewHostImpl can account for in-flight beforeunload/unload events.
  int increment_in_flight_event_count() { return ++in_flight_event_count_; }
  int decrement_in_flight_event_count() { return --in_flight_event_count_; }

  // Returns whether an overscroll gesture is in progress.
  bool IsInOverscrollGesture() const;

  // The View associated with the RenderViewHost. The lifetime of this object
  // is associated with the lifetime of the Render process. If the Renderer
  // crashes, its View is destroyed and this pointer becomes NULL, even though
  // render_view_host_ lives on to load another URL (creating a new View while
  // doing so).
  RenderWidgetHostViewPort* view_;

  // true if a renderer has once been valid. We use this flag to display a sad
  // tab only when we lose our renderer and not if a paint occurs during
  // initialization.
  bool renderer_initialized_;

  // This value indicates how long to wait before we consider a renderer hung.
  int hung_renderer_delay_ms_;

 private:
  friend class MockRenderWidgetHost;

  // Tell this object to destroy itself.
  void Destroy();

  // Called by |hang_timeout_monitor_| on delayed response from the renderer.
  void RendererIsUnresponsive();

  // Called if we know the renderer is responsive. When we currently think the
  // renderer is unresponsive, this will clear that state and call
  // NotifyRendererResponsive.
  void RendererIsResponsive();

  // IPC message handlers
  void OnRenderViewReady();
  void OnRenderProcessGone(int status, int error_code);
  void OnClose();
  void OnUpdateScreenRectsAck();
  void OnRequestMove(const gfx::Rect& pos);
  void OnSetTooltipText(const base::string16& tooltip_text,
                        blink::WebTextDirection text_direction_hint);
  void OnPaintAtSizeAck(int tag, const gfx::Size& size);
#if defined(OS_MACOSX)
  void OnCompositorSurfaceBuffersSwapped(
      const ViewHostMsg_CompositorSurfaceBuffersSwapped_Params& params);
#endif
  bool OnSwapCompositorFrame(const IPC::Message& message);
  void OnOverscrolled(gfx::Vector2dF accumulated_overscroll,
                      gfx::Vector2dF current_fling_velocity);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);
  void OnUpdateIsDelayed();
  void OnQueueSyntheticGesture(const SyntheticGesturePacket& gesture_packet);
  virtual void OnFocus();
  virtual void OnBlur();
  void OnSetCursor(const WebCursor& cursor);
  void OnTextInputTypeChanged(ui::TextInputType type,
                              ui::TextInputMode input_mode,
                              bool can_compose_inline);
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds);
#endif
  void OnImeCancelComposition();
  void OnDidActivateAcceleratedCompositing(bool activated);
  void OnLockMouse(bool user_gesture,
                   bool last_unlocked_by_target,
                   bool privileged);
  void OnUnlockMouse();
  void OnShowDisambiguationPopup(const gfx::Rect& rect,
                                 const gfx::Size& size,
                                 const TransportDIB::Id& id);
#if defined(OS_WIN)
  void OnWindowlessPluginDummyWindowCreated(
      gfx::NativeViewId dummy_activation_window);
  void OnWindowlessPluginDummyWindowDestroyed(
      gfx::NativeViewId dummy_activation_window);
#endif
  void OnSnapshot(bool success, const SkBitmap& bitmap);

  // Called (either immediately or asynchronously) after we're done with our
  // BackingStore and can send an ACK to the renderer so it can paint onto it
  // again.
  void DidUpdateBackingStore(const ViewHostMsg_UpdateRect_Params& params,
                             const base::TimeTicks& paint_start);

  // Paints the given bitmap to the current backing store at the given
  // location.  Returns true if the passed callback was asynchronously
  // scheduled in the future (and thus the caller must manually synchronously
  // call the callback function).
  bool PaintBackingStoreRect(TransportDIB::Id bitmap,
                             const gfx::Rect& bitmap_rect,
                             const std::vector<gfx::Rect>& copy_rects,
                             const gfx::Size& view_size,
                             float scale_factor,
                             const base::Closure& completion_callback);

  // Scrolls the given |clip_rect| in the backing by the given dx/dy amount. The
  // |dib| and its corresponding location |bitmap_rect| in the backing store
  // is the newly painted pixels by the renderer.
  void ScrollBackingStoreRect(const gfx::Vector2d& delta,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size);

  // Give key press listeners a chance to handle this key press. This allow
  // widgets that don't have focus to still handle key presses.
  bool KeyPressListenersHandleEvent(const NativeWebKeyboardEvent& event);

  // InputRouterClient
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& event,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void IncrementInFlightEventCount() OVERRIDE;
  virtual void DecrementInFlightEventCount() OVERRIDE;
  virtual void OnHasTouchEventHandlers(bool has_handlers) OVERRIDE;
  virtual OverscrollController* GetOverscrollController() const OVERRIDE;
  virtual void DidFlush() OVERRIDE;

  // InputAckHandler
  virtual void OnKeyboardEventAck(const NativeWebKeyboardEvent& event,
                                  InputEventAckState ack_result) OVERRIDE;
  virtual void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                                 InputEventAckState ack_result) OVERRIDE;
  virtual void OnUnexpectedEventAck(UnexpectedEventAckType type) OVERRIDE;

  // Called when there is a new auto resize (using a post to avoid a stack
  // which may get in recursive loops).
  void DelayedAutoResized();

  void WindowSnapshotReachedScreen(int snapshot_id);

  // Our delegate, which wants to know mainly about keyboard events.
  // It will remain non-NULL until DetachDelegate() is called.
  RenderWidgetHostDelegate* delegate_;

  // Created during construction but initialized during Init*(). Therefore, it
  // is guaranteed never to be NULL, but its channel may be NULL if the
  // renderer crashed, so you must always check that.
  RenderProcessHost* process_;

  // The ID of the corresponding object in the Renderer Instance.
  int routing_id_;

  // The ID of the surface corresponding to this render widget.
  int surface_id_;

  // Indicates whether a page is loading or not.
  bool is_loading_;

  // Indicates whether a page is hidden or not.
  bool is_hidden_;

  // Indicates whether a page is fullscreen or not.
  bool is_fullscreen_;

  // True when a page is rendered directly via the GPU process.
  bool is_accelerated_compositing_active_;

  // True if threaded compositing is enabled on this view.
  bool is_threaded_compositing_enabled_;

  // Set if we are waiting for a repaint ack for the view.
  bool repaint_ack_pending_;

  // True when waiting for RESIZE_ACK.
  bool resize_ack_pending_;

  // Cached copy of the screen info so that it doesn't need to be updated every
  // time the window is resized.
  scoped_ptr<blink::WebScreenInfo> screen_info_;

  // Set if screen_info_ may have changed and should be recomputed and force a
  // resize message.
  bool screen_info_out_of_date_;

  // The current size of the RenderWidget.
  gfx::Size current_size_;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  gfx::Size physical_backing_size_;

  // The height of the physical backing surface that is overdrawn opaquely in
  // the browser, for example by an on-screen-keyboard (in DPI-adjusted pixels).
  float overdraw_bottom_height_;

  // The size we last sent as requested size to the renderer. |current_size_|
  // is only updated once the resize message has been ack'd. This on the other
  // hand is updated when the resize message is sent. This is very similar to
  // |resize_ack_pending_|, but the latter is not set if the new size has width
  // or height zero, which is why we need this too.
  gfx::Size last_requested_size_;

  // The next auto resize to send.
  gfx::Size new_auto_size_;

  // True if the render widget host should track the render widget's size as
  // opposed to visa versa.
  bool should_auto_resize_;

  bool waiting_for_screen_rects_ack_;
  gfx::Rect last_view_screen_rect_;
  gfx::Rect last_window_screen_rect_;

  AccessibilityMode accessibility_mode_;

  // Keyboard event listeners.
  std::vector<KeyPressEventCallback> key_press_event_callbacks_;

  // Mouse event callbacks.
  std::vector<MouseEventCallback> mouse_event_callbacks_;

  // If true, then we should repaint when restoring even if we have a
  // backingstore.  This flag is set to true if we receive a paint message
  // while is_hidden_ to true.  Even though we tell the render widget to hide
  // itself, a paint message could already be in flight at that point.
  bool needs_repainting_on_restore_;

  // This is true if the renderer is currently unresponsive.
  bool is_unresponsive_;

  // The following value indicates a time in the future when we would consider
  // the renderer hung if it does not generate an appropriate response message.
  base::Time time_when_considered_hung_;

  // This value denotes the number of input events yet to be acknowledged
  // by the renderer.
  int in_flight_event_count_;

  // This timer runs to check if time_when_considered_hung_ has past.
  base::OneShotTimer<RenderWidgetHostImpl> hung_renderer_timer_;

  // Flag to detect recursive calls to GetBackingStore().
  bool in_get_backing_store_;

  // Flag to trigger the GetBackingStore method to abort early.
  bool abort_get_backing_store_;

  // Set when we call DidPaintRect/DidScrollRect on the view.
  bool view_being_painted_;

  // Used for UMA histogram logging to measure the time for a repaint view
  // operation to finish.
  base::TimeTicks repaint_start_time_;

  // Set to true if we shouldn't send input events from the render widget.
  bool ignore_input_events_;

  // Indicates whether IME is active.
  bool input_method_active_;

  // Set when we update the text direction of the selected input element.
  bool text_direction_updated_;
  blink::WebTextDirection text_direction_;

  // Set when we cancel updating the text direction.
  // This flag also ignores succeeding update requests until we call
  // NotifyTextDirection().
  bool text_direction_canceled_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  // System may translate a RawKeyDown event into zero or more Char events,
  // usually we send them to the renderer directly in sequence. However, If a
  // RawKeyDown event was not handled by the renderer but was handled by
  // our UnhandledKeyboardEvent() method, e.g. as an accelerator key, then we
  // shall not send the following sequence of Char events, which was generated
  // by this RawKeyDown event, to the renderer. Otherwise the renderer may
  // handle the Char events and cause unexpected behavior.
  // For example, pressing alt-2 may let the browser switch to the second tab,
  // but the Char event generated by alt-2 may also activate a HTML element
  // if its accesskey happens to be "2", then the user may get confused when
  // switching back to the original tab, because the content may already be
  // changed.
  bool suppress_next_char_events_;

  // The last scroll offset of the render widget.
  gfx::Vector2d last_scroll_offset_;

  bool pending_mouse_lock_request_;
  bool allow_privileged_mouse_lock_;

  // Keeps track of whether the webpage has any touch event handler. If it does,
  // then touch events are sent to the renderer. Otherwise, the touch events are
  // not sent to the renderer.
  bool has_touch_handler_;

  base::WeakPtrFactory<RenderWidgetHostImpl> weak_factory_;

  scoped_ptr<SyntheticGestureController> synthetic_gesture_controller_;

  // Receives and handles all input events.
  scoped_ptr<InputRouter> input_router_;

  scoped_ptr<OverscrollController> overscroll_controller_;

  scoped_ptr<TimeoutMonitor> hang_monitor_timeout_;

#if defined(OS_WIN)
  std::list<HWND> dummy_windows_for_activation_;
#endif

  // List of callbacks for pending snapshot requests to the renderer.
  std::queue<base::Callback<void(bool, const SkBitmap&)> > pending_snapshots_;

  int64 last_input_number_;

  BrowserRenderingStats rendering_stats_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_
