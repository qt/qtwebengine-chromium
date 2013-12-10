// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/trees/layer_tree_settings.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/aura/compositor_resize_lock.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/backing_store_aura.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/touch_smooth_scroll_gesture_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_frame_subscriber.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebScreenInfo.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/win/dpi.h"
#endif

using gfx::RectToSkIRect;
using gfx::SkIRectToRect;

using WebKit::WebScreenInfo;
using WebKit::WebTouchEvent;

namespace content {

void ReleaseMailbox(scoped_refptr<MemoryHolder> holder,
                    unsigned sync_point,
                    bool lost_resource) {}

class MemoryHolder : public base::RefCounted<MemoryHolder> {
 public:
  MemoryHolder(scoped_ptr<base::SharedMemory> shared_memory,
               gfx::Size frame_size,
               base::Callback<void()> callback)
      : shared_memory_(shared_memory.Pass()),
        frame_size_(frame_size),
        callback_(callback) {}

  void GetMailbox(cc::TextureMailbox* mailbox,
                  scoped_ptr<cc::SingleReleaseCallback>* release_callback) {
    *mailbox = cc::TextureMailbox(shared_memory_.get(), frame_size_);
    *release_callback = cc::SingleReleaseCallback::Create(
        base::Bind(ReleaseMailbox, make_scoped_refptr(this)));
  }

 private:
  friend class base::RefCounted<MemoryHolder>;
  ~MemoryHolder() { callback_.Run(); }

  scoped_ptr<base::SharedMemory> shared_memory_;
  gfx::Size frame_size_;
  base::Callback<void()> callback_;
};

namespace {

void MailboxReleaseCallback(scoped_ptr<base::SharedMemory> shared_memory,
                            unsigned sync_point, bool lost_resource) {
  // NOTE: shared_memory will get released when we go out of scope.
}

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful mouse move messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

// When accelerated compositing is enabled and a widget resize is pending,
// we delay further resizes of the UI. The following constant is the maximum
// length of time that we should delay further UI resizes while waiting for a
// resized frame from a renderer.
const int kResizeLockTimeoutMs = 67;

#if defined(OS_WIN)
// Used to associate a plugin HWND with its RenderWidgetHostViewAura instance.
const wchar_t kWidgetOwnerProperty[] = L"RenderWidgetHostViewAuraOwner";

BOOL CALLBACK WindowDestroyingCallback(HWND window, LPARAM param) {
  RenderWidgetHostViewAura* widget =
      reinterpret_cast<RenderWidgetHostViewAura*>(param);
  if (GetProp(window, kWidgetOwnerProperty) == widget) {
    // Properties set on HWNDs must be removed to avoid leaks.
    RemoveProp(window, kWidgetOwnerProperty);
    RenderWidgetHostViewBase::DetachPluginWindowsCallback(window);
  }
  return TRUE;
}

BOOL CALLBACK HideWindowsCallback(HWND window, LPARAM param) {
  RenderWidgetHostViewAura* widget =
      reinterpret_cast<RenderWidgetHostViewAura*>(param);
  if (GetProp(window, kWidgetOwnerProperty) == widget)
    SetParent(window, ui::GetHiddenWindow());
  return TRUE;
}

BOOL CALLBACK ShowWindowsCallback(HWND window, LPARAM param) {
  RenderWidgetHostViewAura* widget =
      reinterpret_cast<RenderWidgetHostViewAura*>(param);

  if (GetProp(window, kWidgetOwnerProperty) == widget) {
    HWND parent =
        widget->GetNativeView()->GetRootWindow()->GetAcceleratedWidget();
    SetParent(window, parent);
  }
  return TRUE;
}

struct CutoutRectsParams {
  RenderWidgetHostViewAura* widget;
  std::vector<gfx::Rect> cutout_rects;
  std::map<HWND, WebPluginGeometry>* geometry;
};

// Used to update the region for the windowed plugin to draw in. We start with
// the clip rect from the renderer, then remove the cutout rects from the
// renderer, and then remove the transient windows from the root window and the
// constrained windows from the parent window.
BOOL CALLBACK SetCutoutRectsCallback(HWND window, LPARAM param) {
  CutoutRectsParams* params = reinterpret_cast<CutoutRectsParams*>(param);

  if (GetProp(window, kWidgetOwnerProperty) == params->widget) {
    // First calculate the offset of this plugin from the root window, since
    // the cutouts are relative to the root window.
    HWND parent = params->widget->GetNativeView()->GetRootWindow()->
        GetAcceleratedWidget();
    POINT offset;
    offset.x = offset.y = 0;
    MapWindowPoints(window, parent, &offset, 1);

    // Now get the cached clip rect and cutouts for this plugin window that came
    // from the renderer.
    std::map<HWND, WebPluginGeometry>::iterator i = params->geometry->begin();
    while (i != params->geometry->end() &&
           i->second.window != window &&
           GetParent(i->second.window) != window) {
      ++i;
    }

    if (i == params->geometry->end()) {
      NOTREACHED();
      return TRUE;
    }

    HRGN hrgn = CreateRectRgn(i->second.clip_rect.x(),
                              i->second.clip_rect.y(),
                              i->second.clip_rect.right(),
                              i->second.clip_rect.bottom());
    // We start with the cutout rects that came from the renderer, then add the
    // ones that came from transient and constrained windows.
    std::vector<gfx::Rect> cutout_rects = i->second.cutout_rects;
    for (size_t i = 0; i < params->cutout_rects.size(); ++i) {
      gfx::Rect offset_cutout = params->cutout_rects[i];
      offset_cutout.Offset(-offset.x, -offset.y);
      cutout_rects.push_back(offset_cutout);
    }
    gfx::SubtractRectanglesFromRegion(hrgn, cutout_rects);
    SetWindowRgn(window, hrgn, TRUE);
  }
  return TRUE;
}

// A callback function for EnumThreadWindows to enumerate and dismiss
// any owned popup windows.
BOOL CALLBACK DismissOwnedPopups(HWND window, LPARAM arg) {
  const HWND toplevel_hwnd = reinterpret_cast<HWND>(arg);

  if (::IsWindowVisible(window)) {
    const HWND owner = ::GetWindow(window, GW_OWNER);
    if (toplevel_hwnd == owner) {
      ::PostMessage(window, WM_CANCELMODE, 0, 0);
    }
  }

  return TRUE;
}
#endif

void UpdateWebTouchEventAfterDispatch(WebKit::WebTouchEvent* event,
                                      WebKit::WebTouchPoint* point) {
  if (point->state != WebKit::WebTouchPoint::StateReleased &&
      point->state != WebKit::WebTouchPoint::StateCancelled)
    return;
  --event->touchesLength;
  for (unsigned i = point - event->touches;
       i < event->touchesLength;
       ++i) {
    event->touches[i] = event->touches[i + 1];
  }
}

bool CanRendererHandleEvent(const ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return false;

#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON or NC events.
  switch (event->native_event().message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_NCMOUSELEAVE:
    case WM_NCMOUSEMOVE:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_NCXBUTTONDBLCLK:
      return false;
    default:
      break;
  }
#endif
  return true;
}

// We don't mark these as handled so that they're sent back to the
// DefWindowProc so it can generate WM_APPCOMMAND as necessary.
bool IsXButtonUpEvent(const ui::MouseEvent* event) {
#if defined(OS_WIN)
  switch (event->native_event().message) {
    case WM_XBUTTONUP:
    case WM_NCXBUTTONUP:
      return true;
  }
#endif
  return false;
}

void GetScreenInfoForWindow(WebScreenInfo* results, aura::Window* window) {
  const gfx::Display display = window ?
      gfx::Screen::GetScreenFor(window)->GetDisplayNearestWindow(window) :
      gfx::Screen::GetScreenFor(window)->GetPrimaryDisplay();
  results->rect = display.bounds();
  results->availableRect = display.work_area();
  // TODO(derat|oshima): Don't hardcode this. Get this from display object.
  results->depth = 24;
  results->depthPerComponent = 8;
  results->deviceScaleFactor = display.device_scale_factor();
}

bool ShouldSendPinchGesture() {
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return true;
#endif
  static bool pinch_allowed =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableViewport) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePinch);
  return pinch_allowed;
}

bool PointerEventActivates(const ui::Event& event) {
  if (event.type() == ui::ET_MOUSE_PRESSED)
    return true;

  if (event.type() == ui::ET_GESTURE_BEGIN) {
    const ui::GestureEvent& gesture =
        static_cast<const ui::GestureEvent&>(event);
    return gesture.details().touch_points() == 1;
  }

  return false;
}

// Swap ack for the renderer when kCompositeToMailbox is enabled.
void SendCompositorFrameAck(
    int32 route_id,
    uint32 output_surface_id,
    int renderer_host_id,
    const gpu::Mailbox& received_mailbox,
    const gfx::Size& received_size,
    bool skip_frame,
    const scoped_refptr<ui::Texture>& texture_to_produce) {
  cc::CompositorFrameAck ack;
  ack.gl_frame_data.reset(new cc::GLFrameData());
  DCHECK(!texture_to_produce.get() || !skip_frame);
  if (texture_to_produce.get()) {
    std::string mailbox_name = texture_to_produce->Produce();
    std::copy(mailbox_name.data(),
              mailbox_name.data() + mailbox_name.length(),
              reinterpret_cast<char*>(ack.gl_frame_data->mailbox.name));
    ack.gl_frame_data->size = texture_to_produce->size();
    ack.gl_frame_data->sync_point =
        content::ImageTransportFactory::GetInstance()->InsertSyncPoint();
  } else if (skip_frame) {
    // Skip the frame, i.e. tell the producer to reuse the same buffer that
    // we just received.
    ack.gl_frame_data->size = received_size;
    ack.gl_frame_data->mailbox = received_mailbox;
  }

  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      route_id, output_surface_id, renderer_host_id, ack);
}

void AcknowledgeBufferForGpu(
    int32 route_id,
    int gpu_host_id,
    const std::string& received_mailbox,
    bool skip_frame,
    const scoped_refptr<ui::Texture>& texture_to_produce) {
  AcceleratedSurfaceMsg_BufferPresented_Params ack;
  uint32 sync_point = 0;
  DCHECK(!texture_to_produce.get() || !skip_frame);
  if (texture_to_produce.get()) {
    ack.mailbox_name = texture_to_produce->Produce();
    sync_point =
        content::ImageTransportFactory::GetInstance()->InsertSyncPoint();
  } else if (skip_frame) {
    ack.mailbox_name = received_mailbox;
    ack.sync_point = 0;
  }

  ack.sync_point = sync_point;
  RenderWidgetHostImpl::AcknowledgeBufferPresent(
      route_id, gpu_host_id, ack);
}

}  // namespace

// We need to watch for mouse events outside a Web Popup or its parent
// and dismiss the popup for certain events.
class RenderWidgetHostViewAura::EventFilterForPopupExit :
    public ui::EventHandler {
 public:
  explicit EventFilterForPopupExit(RenderWidgetHostViewAura* rwhva)
      : rwhva_(rwhva) {
    DCHECK(rwhva_);
    aura::RootWindow* root_window = rwhva_->window_->GetRootWindow();
    DCHECK(root_window);
    root_window->AddPreTargetHandler(this);
  }

  virtual ~EventFilterForPopupExit() {
    aura::RootWindow* root_window = rwhva_->window_->GetRootWindow();
    DCHECK(root_window);
    root_window->RemovePreTargetHandler(this);
  }

  // Overridden from ui::EventHandler
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    rwhva_->ApplyEventFilterForPopupExit(event);
  }

 private:
  RenderWidgetHostViewAura* rwhva_;

  DISALLOW_COPY_AND_ASSIGN(EventFilterForPopupExit);
};

void RenderWidgetHostViewAura::ApplyEventFilterForPopupExit(
    ui::MouseEvent* event) {
  if (in_shutdown_ || is_fullscreen_)
    return;

  if (event->type() != ui::ET_MOUSE_PRESSED || !event->target())
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target != window_ &&
      (!popup_parent_host_view_ ||
       target != popup_parent_host_view_->window_)) {
    // Note: popup_parent_host_view_ may be NULL when there are multiple
    // popup children per view. See: RenderWidgetHostViewAura::InitAsPopup().
    in_shutdown_ = true;
    host_->Shutdown();
  }
}

// We have to implement the WindowObserver interface on a separate object
// because clang doesn't like implementing multiple interfaces that have
// methods with the same name. This object is owned by the
// RenderWidgetHostViewAura.
class RenderWidgetHostViewAura::WindowObserver : public aura::WindowObserver {
 public:
  explicit WindowObserver(RenderWidgetHostViewAura* view)
      : view_(view) {
    view_->window_->AddObserver(this);
  }

  virtual ~WindowObserver() {
    view_->window_->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowAddedToRootWindow(aura::Window* window) OVERRIDE {
    if (window == view_->window_)
      view_->AddedToRootWindow();
  }

  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE {
    if (window == view_->window_)
      view_->RemovingFromRootWindow();
  }

 private:
  RenderWidgetHostViewAura* view_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

#if defined(OS_WIN)
// On Windows, we need to watch the top level window for changes to transient
// windows because they can cover the view and we need to ensure that they're
// rendered on top of windowed NPAPI plugins.
class RenderWidgetHostViewAura::TransientWindowObserver
    : public aura::WindowObserver {
 public:
  explicit TransientWindowObserver(RenderWidgetHostViewAura* view)
      : view_(view), top_level_(NULL) {
    view_->window_->AddObserver(this);
  }

  virtual ~TransientWindowObserver() {
    view_->window_->RemoveObserver(this);
    StopObserving();
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) OVERRIDE {
    aura::Window* top_level = GetToplevelWindow();
    if (top_level == top_level_)
      return;

    StopObserving();
    top_level_ = top_level;
    if (top_level_ && top_level_ != view_->window_)
      top_level_->AddObserver(this);
  }

  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    if (window == top_level_)
      StopObserving();
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    if (window->transient_parent())
      SendPluginCutoutRects();
  }

  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE {
    if (window->transient_parent())
      SendPluginCutoutRects();
  }

  virtual void OnAddTransientChild(aura::Window* window,
                                   aura::Window* transient) OVERRIDE {
    transient->AddObserver(this);
    // Just wait for the OnWindowBoundsChanged of the transient, since the size
    // is not known now.
  }

  virtual void OnRemoveTransientChild(aura::Window* window,
                                      aura::Window* transient) OVERRIDE {
    transient->RemoveObserver(this);
    SendPluginCutoutRects();
  }

  aura::Window* GetToplevelWindow() {
    aura::RootWindow* root = view_->window_->GetRootWindow();
    if (!root)
      return NULL;
    aura::client::ActivationClient* activation_client =
        aura::client::GetActivationClient(root);
    if (!activation_client)
      return NULL;
    return activation_client->GetToplevelWindow(view_->window_);
  }

  void StopObserving() {
    if (!top_level_)
      return;

    const aura::Window::Windows& transients = top_level_->transient_children();
    for (size_t i = 0; i < transients.size(); ++i)
      transients[i]->RemoveObserver(this);

    if (top_level_ != view_->window_)
      top_level_->RemoveObserver(this);
    top_level_ = NULL;
  }

  void SendPluginCutoutRects() {
    std::vector<gfx::Rect> cutouts;
    if (top_level_) {
      const aura::Window::Windows& transients =
          top_level_->transient_children();
      for (size_t i = 0; i < transients.size(); ++i) {
        if (transients[i]->IsVisible())
          cutouts.push_back(transients[i]->GetBoundsInRootWindow());
      }
    }

    view_->UpdateTransientRects(cutouts);
  }
 private:
  RenderWidgetHostViewAura* view_;
  aura::Window* top_level_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowObserver);
};

#endif

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(RenderWidgetHostImpl::From(host)),
      window_(new aura::Window(this)),
      in_shutdown_(false),
      is_fullscreen_(false),
      popup_parent_host_view_(NULL),
      popup_child_host_view_(NULL),
      is_loading_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      can_compose_inline_(true),
      has_composition_text_(false),
      last_output_surface_id_(0),
      skipped_frames_(false),
      last_swapped_surface_scale_factor_(1.f),
      paint_canvas_(NULL),
      synthetic_move_sent_(false),
      accelerated_compositing_state_changed_(false),
      can_lock_compositor_(YES),
      cursor_visibility_state_in_renderer_(UNKNOWN),
      paint_observer_(NULL),
      touch_editing_client_(NULL) {
  host_->SetView(this);
  window_observer_.reset(new WindowObserver(this));
  aura::client::SetTooltipText(window_, &tooltip_);
  aura::client::SetActivationDelegate(window_, this);
  aura::client::SetActivationChangeObserver(window_, this);
  aura::client::SetFocusChangeObserver(window_, this);
  gfx::Screen::GetScreenFor(window_)->AddObserver(this);
#if defined(OS_WIN)
  transient_observer_.reset(new TransientWindowObserver(this));
#endif
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsChild(
    gfx::NativeView parent_view) {
  window_->Init(ui::LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");
}

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds_in_screen) {
  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);

  RenderWidgetHostViewAura* old_child =
      popup_parent_host_view_->popup_child_host_view_;
  if (old_child) {
    // TODO(jhorwich): Allow multiple popup_child_host_view_ per view, or
    // similar mechanism to ensure a second popup doesn't cause the first one
    // to never get a chance to filter events. See crbug.com/160589.
    DCHECK(old_child->popup_parent_host_view_ == popup_parent_host_view_);
    old_child->popup_parent_host_view_ = NULL;
  }
  popup_parent_host_view_->popup_child_host_view_ = this;
  window_->SetType(aura::client::WINDOW_TYPE_MENU);
  window_->Init(ui::LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");

  aura::RootWindow* root = popup_parent_host_view_->window_->GetRootWindow();
  window_->SetDefaultParentByRootWindow(root, bounds_in_screen);

  // TODO(erg): While I could make sure details of the StackingClient are
  // hidden behind aura, hiding the details of the ScreenPositionClient will
  // take another effort.
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root);
  gfx::Point origin_in_parent(bounds_in_screen.origin());
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(
        window_->parent(), &origin_in_parent);
  }
  SetBounds(gfx::Rect(origin_in_parent, bounds_in_screen.size()));
  Show();
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  is_fullscreen_ = true;
  window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window_->Init(ui::LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);

  aura::RootWindow* parent = NULL;
  gfx::Rect bounds;
  if (reference_host_view) {
    aura::Window* reference_window =
        static_cast<RenderWidgetHostViewAura*>(reference_host_view)->window_;
    if (reference_window) {
      host_tracker_.reset(new aura::WindowTracker);
      host_tracker_->Add(reference_window);
    }
    gfx::Display display = gfx::Screen::GetScreenFor(window_)->
        GetDisplayNearestWindow(reference_window);
    parent = reference_window->GetRootWindow();
    bounds = display.bounds();
  }
  window_->SetDefaultParentByRootWindow(parent, bounds);
  Show();
  Focus();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::WasShown() {
  DCHECK(host_);
  if (!host_->is_hidden())
    return;
  host_->WasShown();
  if (framebuffer_holder_)
    FrameMemoryManager::GetInstance()->SetFrameVisibility(this, true);

  aura::RootWindow* root = window_->GetRootWindow();
  if (root) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root);
    if (cursor_client)
      NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }

  if (!current_surface_.get() && host_->is_accelerated_compositing_active() &&
      !released_front_lock_.get()) {
    ui::Compositor* compositor = GetCompositor();
    if (compositor)
      released_front_lock_ = compositor->GetCompositorLock();
  }

#if defined(OS_WIN)
  LPARAM lparam = reinterpret_cast<LPARAM>(this);
  EnumChildWindows(ui::GetHiddenWindow(), ShowWindowsCallback, lparam);
  transient_observer_->SendPluginCutoutRects();
#endif
}

void RenderWidgetHostViewAura::WasHidden() {
  if (!host_ || host_->is_hidden())
    return;
  host_->WasHidden();
  if (framebuffer_holder_)
    FrameMemoryManager::GetInstance()->SetFrameVisibility(this, false);

  released_front_lock_ = NULL;

#if defined(OS_WIN)
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (root_window) {
    HWND parent = root_window->GetAcceleratedWidget();
    LPARAM lparam = reinterpret_cast<LPARAM>(this);

    EnumChildWindows(parent, HideWindowsCallback, lparam);
  }
#endif
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  if (HasDisplayPropertyChanged(window_))
    host_->InvalidateScreenInfo();

  window_->SetBounds(rect);
  host_->WasResized();
  MaybeCreateResizeLock();
  if (touch_editing_client_) {
    touch_editing_client_->OnSelectionOrCursorChanged(selection_anchor_rect_,
        selection_focus_rect_);
  }
}

void RenderWidgetHostViewAura::MaybeCreateResizeLock() {
  if (!ShouldCreateResizeLock())
    return;
  DCHECK(window_->GetRootWindow());
  DCHECK(window_->GetRootWindow()->compositor());

  // Listen to changes in the compositor lock state.
  ui::Compositor* compositor = window_->GetRootWindow()->compositor();
  if (!compositor->HasObserver(this))
    compositor->AddObserver(this);

  bool defer_compositor_lock =
      can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT;

  if (can_lock_compositor_ == YES)
    can_lock_compositor_ = YES_DID_LOCK;

  resize_lock_ = CreateResizeLock(defer_compositor_lock);
}

bool RenderWidgetHostViewAura::ShouldCreateResizeLock() {
  // On Windows while resizing, the the resize locks makes us mis-paint a white
  // vertical strip (including the non-client area) if the content composition
  // is lagging the UI composition. So here we disable the throttling so that
  // the UI bits can draw ahead of the content thereby reducing the amount of
  // whiteout. Because this causes the content to be drawn at wrong sizes while
  // resizing we compensate by blocking the UI thread in Compositor::Draw() by
  // issuing a FinishAllRendering() if we are resizing.
#if defined (OS_WIN)
  return false;
#endif

  if (resize_lock_)
    return false;

  if (host_->should_auto_resize())
    return false;
  if (!host_->is_accelerated_compositing_active())
    return false;

  gfx::Size desired_size = window_->bounds().size();
  if (desired_size == current_frame_size_)
    return false;

  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  ui::Compositor* compositor = root_window->compositor();
  if (!compositor)
    return false;

  return true;
}

scoped_ptr<ResizeLock> RenderWidgetHostViewAura::CreateResizeLock(
    bool defer_compositor_lock) {
  gfx::Size desired_size = window_->bounds().size();
  return scoped_ptr<ResizeLock>(new CompositorResizeLock(
      window_->GetRootWindow(),
      desired_size,
      defer_compositor_lock,
      base::TimeDelta::FromMilliseconds(kResizeLockTimeoutMs)));
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
#if defined(OS_WIN)
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (root_window) {
    HWND window = root_window->GetAcceleratedWidget();
    return reinterpret_cast<gfx::NativeViewId>(window);
  }
#endif
  return static_cast<gfx::NativeViewId>(NULL);
}

gfx::NativeViewAccessible RenderWidgetHostViewAura::GetNativeViewAccessible() {
#if defined(OS_WIN)
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return static_cast<gfx::NativeViewAccessible>(NULL);
  HWND hwnd = root_window->GetAcceleratedWidget();

  BrowserAccessibilityManager* manager =
      GetOrCreateBrowserAccessibilityManager();
  if (manager)
    return manager->GetRoot()->ToBrowserAccessibilityWin();
#endif

  NOTIMPLEMENTED();
  return static_cast<gfx::NativeViewAccessible>(NULL);
}

BrowserAccessibilityManager*
RenderWidgetHostViewAura::GetOrCreateBrowserAccessibilityManager() {
  BrowserAccessibilityManager* manager = GetBrowserAccessibilityManager();
  if (manager)
    return manager;

#if defined(OS_WIN)
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return NULL;
  HWND hwnd = root_window->GetAcceleratedWidget();

  // The accessible_parent may be NULL at this point. The WebContents will pass
  // it down to this instance (by way of the RenderViewHost and
  // RenderWidgetHost) when it is known. This instance will then set it on its
  // BrowserAccessibilityManager.
  gfx::NativeViewAccessible accessible_parent =
      host_->GetParentNativeViewAccessible();

  manager = new BrowserAccessibilityManagerWin(
      hwnd, accessible_parent,
      BrowserAccessibilityManagerWin::GetEmptyDocument(), this);
#else
  manager = BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), this);
#endif

  SetBrowserAccessibilityManager(manager);
  return manager;
}

void RenderWidgetHostViewAura::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
#if defined(OS_WIN)
  // We need to clip the rectangle to the tab's viewport, otherwise we will draw
  // over the browser UI.
  if (!window_->GetRootWindow()) {
    DCHECK(plugin_window_moves.empty());
    return;
  }
  HWND parent = window_->GetRootWindow()->GetAcceleratedWidget();
  gfx::Rect view_bounds = window_->GetBoundsInRootWindow();
  std::vector<WebPluginGeometry> moves = plugin_window_moves;

  gfx::Rect view_port(scroll_offset.x(), scroll_offset.y(), view_bounds.width(),
                      view_bounds.height());

  for (size_t i = 0; i < moves.size(); ++i) {
    gfx::Rect clip(moves[i].clip_rect);
    gfx::Vector2d view_port_offset(
        moves[i].window_rect.OffsetFromOrigin() + scroll_offset);
    clip.Offset(view_port_offset);
    clip.Intersect(view_port);
    clip.Offset(-view_port_offset);
    moves[i].clip_rect = clip;

    moves[i].window_rect.Offset(view_bounds.OffsetFromOrigin());

    plugin_window_moves_[moves[i].window] = moves[i];

    // transient_rects_ and constrained_rects_ are relative to the root window.
    // We want to convert them to be relative to the plugin window.
    std::vector<gfx::Rect> cutout_rects;
    cutout_rects.assign(transient_rects_.begin(), transient_rects_.end());
    cutout_rects.insert(cutout_rects.end(), constrained_rects_.begin(),
                        constrained_rects_.end());
    for (size_t j = 0; j < cutout_rects.size(); ++j) {
      gfx::Rect offset_cutout = cutout_rects[j];
      offset_cutout -= moves[i].window_rect.OffsetFromOrigin();
      moves[i].cutout_rects.push_back(offset_cutout);
    }
  }

  MovePluginWindowsHelper(parent, moves);

  // Make sure each plugin window (or its wrapper if it exists) has a pointer to
  // |this|.
  for (size_t i = 0; i < moves.size(); ++i) {
    HWND window = moves[i].window;
    if (GetParent(window) != parent) {
      window = GetParent(window);
      DCHECK(GetParent(window) == parent);
    }
    if (!GetProp(window, kWidgetOwnerProperty))
      SetProp(window, kWidgetOwnerProperty, this);
  }
#endif  // defined(OS_WIN)
}

void RenderWidgetHostViewAura::Focus() {
  // Make sure we have a FocusClient before attempting to Focus(). In some
  // situations we may not yet be in a valid Window hierarchy (such as reloading
  // after out of memory discarded the tab).
  aura::client::FocusClient* client = aura::client::GetFocusClient(window_);
  if (client)
    window_->Focus();
}

void RenderWidgetHostViewAura::Blur() {
  window_->Blur();
}

bool RenderWidgetHostViewAura::HasFocus() const {
  return window_->HasFocus();
}

bool RenderWidgetHostViewAura::IsSurfaceAvailableForCopy() const {
  return CanCopyToBitmap() || !!host_->GetBackingStore(false);
}

void RenderWidgetHostViewAura::Show() {
  window_->Show();
  WasShown();
}

void RenderWidgetHostViewAura::Hide() {
  window_->Hide();
  WasHidden();
}

bool RenderWidgetHostViewAura::IsShowing() {
  return window_->IsVisible();
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() const {
  // This is the size that we want the renderer to produce. While we're waiting
  // for the correct frame (i.e. during a resize), don't change the size so that
  // we don't pipeline more resizes than we can handle.
  gfx::Rect bounds(window_->GetBoundsInScreen());
  if (resize_lock_.get())
    return gfx::Rect(bounds.origin(), resize_lock_->expected_size());
  else
    return bounds;
}

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  host_->SetBackground(background);
  window_->layer()->SetFillsBoundsOpaquely(background.isOpaque());
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  const gfx::Display display = gfx::Screen::GetScreenFor(window_)->
      GetDisplayNearestWindow(window_);
  current_cursor_.SetDisplayInfo(display);
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  if (is_loading_ && !is_loading && paint_observer_)
    paint_observer_->OnPageLoadComplete();
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline) {
  if (text_input_type_ != type ||
      text_input_mode_ != input_mode ||
      can_compose_inline_ != can_compose_inline) {
    text_input_type_ = type;
    text_input_mode_ = input_mode;
    can_compose_inline_ = can_compose_inline;
    if (GetInputMethod())
      GetInputMethod()->OnTextInputTypeChanged(this);
    if (touch_editing_client_)
      touch_editing_client_->OnTextInputTypeChanged(text_input_type_);
  }
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  if (GetInputMethod())
    GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  composition_character_bounds_ = character_bounds;
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects,
    const ui::LatencyInfo& latency_info) {
  if (accelerated_compositing_state_changed_)
    UpdateExternalTexture();

  software_latency_info_.MergeWith(latency_info);

  // Use the state of the RenderWidgetHost and not the window as the two may
  // differ. In particular if the window is hidden but the renderer isn't and we
  // ignore the update and the window is made visible again the layer isn't
  // marked as dirty and we show the wrong thing.
  // We do this after UpdateExternalTexture() so that when we become visible
  // we're not drawing a stale texture.
  if (host_->is_hidden())
    return;

  gfx::Rect clip_rect;
  if (paint_canvas_) {
    SkRect sk_clip_rect;
    if (paint_canvas_->sk_canvas()->getClipBounds(&sk_clip_rect))
      clip_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(sk_clip_rect));
  }

  if (!scroll_rect.IsEmpty())
    SchedulePaintIfNotInClip(scroll_rect, clip_rect);

#if defined(OS_WIN)
  aura::RootWindow* root_window = window_->GetRootWindow();
#endif
  for (size_t i = 0; i < copy_rects.size(); ++i) {
    gfx::Rect rect = gfx::SubtractRects(copy_rects[i], scroll_rect);
    if (rect.IsEmpty())
      continue;

    SchedulePaintIfNotInClip(rect, clip_rect);

#if defined(OS_WIN)
    if (root_window) {
      // Send the invalid rect in screen coordinates.
      gfx::Rect screen_rect = GetViewBounds();
      gfx::Rect invalid_screen_rect(rect);
      invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());
      HWND hwnd = root_window->GetAcceleratedWidget();
      PaintPluginWindowsHelper(hwnd, invalid_screen_rect);
    }
#endif  // defined(OS_WIN)
  }
}

void RenderWidgetHostViewAura::RenderProcessGone(base::TerminationStatus status,
                                                 int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::Destroy() {
  // Beware, this function is not called on all destruction paths. It will
  // implicitly end up calling ~RenderWidgetHostViewAura though, so all
  // destruction/cleanup code should happen there, not here.
  in_shutdown_ = true;
  delete window_;
}

void RenderWidgetHostViewAura::SetTooltipText(const string16& tooltip_text) {
  tooltip_ = tooltip_text;
  aura::RootWindow* root_window = window_->GetRootWindow();
  aura::client::TooltipClient* tooltip_client =
      aura::client::GetTooltipClient(root_window);
  if (tooltip_client) {
    tooltip_client->UpdateTooltip(window_);
    // Content tooltips should be visible indefinitely.
    tooltip_client->SetTooltipShownTimeout(window_, 0);
  }
}

void RenderWidgetHostViewAura::SelectionChanged(const string16& text,
                                                size_t offset,
                                                const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

#if defined(USE_X11) && !defined(OS_CHROMEOS)
  if (text.empty() || range.is_empty())
    return;

  // Set the BUFFER_SELECTION to the ui::Clipboard.
  ui::ScopedClipboardWriter clipboard_writer(
      ui::Clipboard::GetForCurrentThread(),
      ui::Clipboard::BUFFER_SELECTION);
  clipboard_writer.WriteText(text);
#endif  // defined(USE_X11) && !defined(OS_CHROMEOS)
}

void RenderWidgetHostViewAura::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (selection_anchor_rect_ == params.anchor_rect &&
      selection_focus_rect_ == params.focus_rect)
    return;

  selection_anchor_rect_ = params.anchor_rect;
  selection_focus_rect_ = params.focus_rect;

  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);

  if (touch_editing_client_) {
    touch_editing_client_->OnSelectionOrCursorChanged(selection_anchor_rect_,
        selection_focus_rect_);
  }
}

void RenderWidgetHostViewAura::ScrollOffsetChanged() {
  aura::RootWindow* root = window_->GetRootWindow();
  if (!root)
    return;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root);
  if (cursor_client && !cursor_client->IsCursorVisible())
    cursor_client->DisableMouseEvents();
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreAura(host_, size);
}

void RenderWidgetHostViewAura::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  if (!CanCopyToBitmap()) {
    callback.Run(false, SkBitmap());
    return;
  }

  const gfx::Size& dst_size_in_pixel = ConvertViewSizeToPixel(this, dst_size);
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &RenderWidgetHostViewAura::CopyFromCompositingSurfaceHasResult,
          dst_size_in_pixel,
          callback));
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(current_device_scale_factor_, src_subrect);
  request->set_area(src_subrect_in_pixel);
  window_->layer()->RequestCopyOfOutput(request.Pass());
}

void RenderWidgetHostViewAura::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  if (!CanCopyToVideoFrame()) {
    callback.Run(false);
    return;
  }

  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &RenderWidgetHostViewAura::
              CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(),  // For caching the ReadbackYUVInterface on this class.
          target,
          callback));
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(current_device_scale_factor_, src_subrect);
  request->set_area(src_subrect_in_pixel);
  window_->layer()->RequestCopyOfOutput(request.Pass());
}

bool RenderWidgetHostViewAura::CanCopyToBitmap() const {
  return GetCompositor() && window_->layer()->has_external_content();
}

bool RenderWidgetHostViewAura::CanCopyToVideoFrame() const {
  return GetCompositor() &&
         window_->layer()->has_external_content() &&
         host_->is_accelerated_compositing_active();
}

bool RenderWidgetHostViewAura::CanSubscribeFrame() const {
  return true;
}

void RenderWidgetHostViewAura::BeginFrameSubscription(
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscriber_ = subscriber.Pass();
}

void RenderWidgetHostViewAura::EndFrameSubscription() {
  frame_subscriber_.reset();
}


void RenderWidgetHostViewAura::OnAcceleratedCompositingStateChange() {
  // Delay processing the state change until we either get a software frame if
  // switching to software mode or receive a buffers swapped notification
  // if switching to accelerated mode.
  // Sometimes (e.g. on a page load) the renderer will spuriously disable then
  // re-enable accelerated compositing, causing us to flash.
  // TODO(piman): factor the enable/disable accelerated compositing message into
  // the UpdateRect/AcceleratedSurfaceBuffersSwapped messages so that we have
  // fewer inconsistent temporary states.
  accelerated_compositing_state_changed_ = true;
}

bool RenderWidgetHostViewAura::ShouldSkipFrame(gfx::Size size_in_dip) const {
  if (can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT ||
      !resize_lock_.get())
    return false;

  return size_in_dip != resize_lock_->expected_size();
}

void RenderWidgetHostViewAura::CheckResizeLock() {
  if (!resize_lock_ || resize_lock_->expected_size() != current_frame_size_)
    return;

  // Since we got the size we were looking for, unlock the compositor. But delay
  // the release of the lock until we've kicked a frame with the new texture, to
  // avoid resizing the UI before we have a chance to draw a "good" frame.
  resize_lock_->UnlockCompositor();
  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
}

void RenderWidgetHostViewAura::UpdateExternalTexture() {
  // Delay processing accelerated compositing state change till here where we
  // act upon the state change. (Clear the external texture if switching to
  // software mode or set the external texture if going to accelerated mode).
  if (accelerated_compositing_state_changed_)
    accelerated_compositing_state_changed_ = false;

  bool is_compositing_active = host_->is_accelerated_compositing_active();
  if (is_compositing_active && current_surface_.get()) {
    window_->layer()->SetExternalTexture(current_surface_.get());
    current_frame_size_ = ConvertSizeToDIP(
        current_surface_->device_scale_factor(), current_surface_->size());
    CheckResizeLock();
    framebuffer_holder_ = NULL;
    FrameMemoryManager::GetInstance()->RemoveFrame(this);
  } else if (is_compositing_active && framebuffer_holder_) {
    cc::TextureMailbox mailbox;
    scoped_ptr<cc::SingleReleaseCallback> callback;
    framebuffer_holder_->GetMailbox(&mailbox, &callback);
    window_->layer()->SetTextureMailbox(mailbox,
                                        callback.Pass(),
                                        last_swapped_surface_scale_factor_);
    current_frame_size_ = ConvertSizeToDIP(last_swapped_surface_scale_factor_,
                                           mailbox.shared_memory_size());
    CheckResizeLock();
  } else {
    window_->layer()->SetExternalTexture(NULL);
    resize_lock_.reset();
    host_->WasResized();
    framebuffer_holder_ = NULL;
    FrameMemoryManager::GetInstance()->RemoveFrame(this);
  }
}

bool RenderWidgetHostViewAura::SwapBuffersPrepare(
    const gfx::Rect& surface_rect,
    float surface_scale_factor,
    const gfx::Rect& damage_rect,
    const std::string& mailbox_name,
    const BufferPresentedCallback& ack_callback) {
  if (last_swapped_surface_size_ != surface_rect.size()) {
    // The surface could have shrunk since we skipped an update, in which
    // case we can expect a full update.
    DLOG_IF(ERROR, damage_rect != surface_rect) << "Expected full damage rect";
    skipped_damage_.setEmpty();
    last_swapped_surface_size_ = surface_rect.size();
    last_swapped_surface_scale_factor_ = surface_scale_factor;
  }

  if (ShouldSkipFrame(ConvertSizeToDIP(surface_scale_factor,
                                       surface_rect.size())) ||
      mailbox_name.empty()) {
    skipped_damage_.op(RectToSkIRect(damage_rect), SkRegion::kUnion_Op);
    ack_callback.Run(true, scoped_refptr<ui::Texture>());
    return false;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  current_surface_ =
      factory->CreateTransportClient(surface_scale_factor);
  if (!current_surface_.get()) {
    LOG(ERROR) << "Failed to create ImageTransport texture";
    ack_callback.Run(true, scoped_refptr<ui::Texture>());
    return false;
  }

  current_surface_->Consume(mailbox_name, surface_rect.size());
  released_front_lock_ = NULL;
  UpdateExternalTexture();

  return true;
}

void RenderWidgetHostViewAura::SwapBuffersCompleted(
    const BufferPresentedCallback& ack_callback,
    const scoped_refptr<ui::Texture>& texture_to_return) {
  ui::Compositor* compositor = GetCompositor();
  if (!compositor) {
    ack_callback.Run(false, texture_to_return);
  } else {
    AddOnCommitCallbackAndDisableLocks(
        base::Bind(ack_callback, false, texture_to_return));
  }

  DidReceiveFrameFromRenderer();
}

void RenderWidgetHostViewAura::DidReceiveFrameFromRenderer() {
  if (frame_subscriber() && CanCopyToVideoFrame()) {
    const base::Time present_time = base::Time::Now();
    scoped_refptr<media::VideoFrame> frame;
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
    if (frame_subscriber()->ShouldCaptureFrame(present_time,
                                               &frame, &callback)) {
      CopyFromCompositingSurfaceToVideoFrame(
          gfx::Rect(current_frame_size_),
          frame,
          base::Bind(callback, present_time));
    }
  }
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::UpdateTransientRects(
    const std::vector<gfx::Rect>& rects) {
  transient_rects_ = rects;
  UpdateCutoutRects();
}

void RenderWidgetHostViewAura::UpdateConstrainedWindowRects(
    const std::vector<gfx::Rect>& rects) {
  constrained_rects_ = rects;
  UpdateCutoutRects();
}

void RenderWidgetHostViewAura::UpdateCutoutRects() {
  if (!window_->GetRootWindow())
    return;
  HWND parent = window_->GetRootWindow()->GetAcceleratedWidget();
  CutoutRectsParams params;
  params.widget = this;
  params.cutout_rects.assign(transient_rects_.begin(), transient_rects_.end());
  params.cutout_rects.insert(params.cutout_rects.end(),
                             constrained_rects_.begin(),
                             constrained_rects_.end());
  params.geometry = &plugin_window_moves_;
  LPARAM lparam = reinterpret_cast<LPARAM>(&params);
  EnumChildWindows(parent, SetCutoutRectsCallback, lparam);
}
#endif

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
    int gpu_host_id) {
  BufferPresentedCallback ack_callback = base::Bind(
      &AcknowledgeBufferForGpu,
      params_in_pixel.route_id,
      gpu_host_id,
      params_in_pixel.mailbox_name);
  BuffersSwapped(params_in_pixel.size,
                 gfx::Rect(params_in_pixel.size),
                 params_in_pixel.scale_factor,
                 params_in_pixel.mailbox_name,
                 params_in_pixel.latency_info,
                 ack_callback);
}

void RenderWidgetHostViewAura::SwapDelegatedFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::DelegatedFrameData> frame_data,
    float frame_device_scale_factor,
    const ui::LatencyInfo& latency_info) {
  gfx::Size frame_size;
  gfx::Size frame_size_in_dip;
  gfx::Rect damage_rect;
  gfx::Rect damage_rect_in_dip;

  if (!frame_data->render_pass_list.empty()) {
    cc::RenderPass* root_pass = frame_data->render_pass_list.back();

    frame_size = root_pass->output_rect.size();
    frame_size_in_dip = ConvertSizeToDIP(frame_device_scale_factor, frame_size);

    damage_rect = gfx::ToEnclosingRect(root_pass->damage_rect);
    damage_rect.Intersect(gfx::Rect(frame_size));
    damage_rect_in_dip = ConvertRectToDIP(frame_device_scale_factor,
                                          damage_rect);
  }

  framebuffer_holder_ = NULL;
  FrameMemoryManager::GetInstance()->RemoveFrame(this);

  if (ShouldSkipFrame(frame_size_in_dip)) {
    cc::CompositorFrameAck ack;
    cc::TransferableResource::ReturnResources(frame_data->resource_list,
                                              &ack.resources);
    RenderWidgetHostImpl::SendSwapCompositorFrameAck(
        host_->GetRoutingID(), output_surface_id,
        host_->GetProcess()->GetID(), ack);
    skipped_frames_ = true;
    return;
  }

  if (skipped_frames_) {
    skipped_frames_ = false;
    damage_rect = gfx::Rect(frame_size);
    damage_rect_in_dip = gfx::Rect(frame_size_in_dip);

    // Give the same damage rect to the compositor.
    cc::RenderPass* root_pass = frame_data->render_pass_list.back();
    root_pass->damage_rect = damage_rect;
  }

  if (output_surface_id != last_output_surface_id_) {
    // Resource ids are scoped by the output surface.
    // If the originating output surface doesn't match the last one, it
    // indicates the renderer's output surface may have been recreated, in which
    // case we should recreate the DelegatedRendererLayer, to avoid matching
    // resources from the old one with resources from the new one which would
    // have the same id.
    window_->layer()->SetDelegatedFrame(scoped_ptr<cc::DelegatedFrameData>(),
                                        frame_size_in_dip);
    last_output_surface_id_ = output_surface_id;
  }
  window_->layer()->SetDelegatedFrame(frame_data.Pass(), frame_size_in_dip);
  released_front_lock_ = NULL;
  current_frame_size_ = frame_size_in_dip;
  CheckResizeLock();

  if (paint_observer_)
    paint_observer_->OnUpdateCompositorContent();
  window_->SchedulePaintInRect(damage_rect_in_dip);

  ui::Compositor* compositor = GetCompositor();
  if (!compositor) {
    SendDelegatedFrameAck(output_surface_id);
  } else {
    compositor->SetLatencyInfo(latency_info);
    AddOnCommitCallbackAndDisableLocks(
        base::Bind(&RenderWidgetHostViewAura::SendDelegatedFrameAck,
                   AsWeakPtr(),
                   output_surface_id));
  }
  DidReceiveFrameFromRenderer();
}

void RenderWidgetHostViewAura::SendDelegatedFrameAck(uint32 output_surface_id) {
  cc::CompositorFrameAck ack;
  window_->layer()->TakeUnusedResourcesForChildCompositor(&ack.resources);
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      host_->GetRoutingID(), output_surface_id,
      host_->GetProcess()->GetID(), ack);
}

void RenderWidgetHostViewAura::SwapSoftwareFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::SoftwareFrameData> frame_data,
    float frame_device_scale_factor,
    const ui::LatencyInfo& latency_info) {
  const gfx::Size& frame_size = frame_data->size;
  const gfx::Rect& damage_rect = frame_data->damage_rect;
  gfx::Size frame_size_in_dip =
      ConvertSizeToDIP(frame_device_scale_factor, frame_size);
  if (ShouldSkipFrame(frame_size_in_dip)) {
    ReleaseSoftwareFrame(output_surface_id, frame_data->id);
    SendSoftwareFrameAck(output_surface_id);
    return;
  }

  const size_t size_in_bytes = 4 * frame_size.GetArea();
#ifdef OS_WIN
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(frame_data->handle, true,
                             host_->GetProcess()->GetHandle()));
#else
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(frame_data->handle, true));
#endif

  if (!shared_memory->Map(size_in_bytes)) {
    host_->GetProcess()->ReceivedBadMessage();
    return;
  }

  if (last_swapped_surface_size_ != frame_size) {
    DLOG_IF(ERROR, damage_rect != gfx::Rect(frame_size))
        << "Expected full damage rect";
  }
  last_swapped_surface_size_ = frame_size;
  last_swapped_surface_scale_factor_ = frame_device_scale_factor;

  scoped_refptr<MemoryHolder> holder(new MemoryHolder(
      shared_memory.Pass(),
      frame_size,
      base::Bind(&RenderWidgetHostViewAura::ReleaseSoftwareFrame,
                 AsWeakPtr(),
                 output_surface_id,
                 frame_data->id)));
  framebuffer_holder_.swap(holder);
  cc::TextureMailbox mailbox;
  scoped_ptr<cc::SingleReleaseCallback> callback;
  framebuffer_holder_->GetMailbox(&mailbox, &callback);
  DCHECK(mailbox.IsSharedMemory());
  current_frame_size_ = frame_size_in_dip;

  released_front_lock_ = NULL;
  CheckResizeLock();
  window_->layer()->SetTextureMailbox(mailbox,
                                      callback.Pass(),
                                      frame_device_scale_factor);
  window_->SchedulePaintInRect(
      ConvertRectToDIP(frame_device_scale_factor, damage_rect));

  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    compositor->SetLatencyInfo(latency_info);
    AddOnCommitCallbackAndDisableLocks(
        base::Bind(&RenderWidgetHostViewAura::SendSoftwareFrameAck,
                   AsWeakPtr(),
                   output_surface_id));
  }
  if (paint_observer_)
    paint_observer_->OnUpdateCompositorContent();
  DidReceiveFrameFromRenderer();
  FrameMemoryManager::GetInstance()->AddFrame(this, !host_->is_hidden());
}

void RenderWidgetHostViewAura::SendSoftwareFrameAck(uint32 output_surface_id) {
  unsigned software_frame_id = 0;
  if (!released_software_frames_.empty()) {
    unsigned released_output_surface_id =
        released_software_frames_.back().output_surface_id;
    if (released_output_surface_id == output_surface_id) {
      software_frame_id = released_software_frames_.back().frame_id;
      released_software_frames_.pop_back();
    }
  }

  cc::CompositorFrameAck ack;
  ack.last_software_frame_id = software_frame_id;
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(
      host_->GetRoutingID(), output_surface_id,
      host_->GetProcess()->GetID(), ack);
  SendReclaimSoftwareFrames();
}

void RenderWidgetHostViewAura::SendReclaimSoftwareFrames() {
  while (!released_software_frames_.empty()) {
    cc::CompositorFrameAck ack;
    ack.last_software_frame_id = released_software_frames_.back().frame_id;
    RenderWidgetHostImpl::SendReclaimCompositorResources(
        host_->GetRoutingID(),
        released_software_frames_.back().output_surface_id,
        host_->GetProcess()->GetID(),
        ack);
    released_software_frames_.pop_back();
  }
}

void RenderWidgetHostViewAura::ReleaseSoftwareFrame(
    uint32 output_surface_id,
    unsigned software_frame_id) {
  SendReclaimSoftwareFrames();
  released_software_frames_.push_back(
      ReleasedFrameInfo(output_surface_id, software_frame_id));
}

void RenderWidgetHostViewAura::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  TRACE_EVENT0("content", "RenderWidgetHostViewAura::OnSwapCompositorFrame");
  if (frame->delegated_frame_data) {
    SwapDelegatedFrame(output_surface_id,
                       frame->delegated_frame_data.Pass(),
                       frame->metadata.device_scale_factor,
                       frame->metadata.latency_info);
    return;
  }

  if (frame->software_frame_data) {
    SwapSoftwareFrame(output_surface_id,
                      frame->software_frame_data.Pass(),
                      frame->metadata.device_scale_factor,
                      frame->metadata.latency_info);
    return;
  }

  if (!frame->gl_frame_data || frame->gl_frame_data->mailbox.IsZero())
    return;

  BufferPresentedCallback ack_callback = base::Bind(
      &SendCompositorFrameAck,
      host_->GetRoutingID(), output_surface_id, host_->GetProcess()->GetID(),
      frame->gl_frame_data->mailbox, frame->gl_frame_data->size);

  if (!frame->gl_frame_data->sync_point) {
    LOG(ERROR) << "CompositorFrame without sync point. Skipping frame...";
    ack_callback.Run(true, scoped_refptr<ui::Texture>());
    return;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->WaitSyncPoint(frame->gl_frame_data->sync_point);

  std::string mailbox_name(
      reinterpret_cast<const char*>(frame->gl_frame_data->mailbox.name),
      sizeof(frame->gl_frame_data->mailbox.name));
  BuffersSwapped(frame->gl_frame_data->size,
                 frame->gl_frame_data->sub_buffer_rect,
                 frame->metadata.device_scale_factor,
                 mailbox_name,
                 frame->metadata.latency_info,
                 ack_callback);
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
  if (GetBrowserAccessibilityManager()) {
    GetBrowserAccessibilityManager()->ToBrowserAccessibilityManagerWin()
        ->set_parent_iaccessible(accessible_parent);
  }
}
#endif

void RenderWidgetHostViewAura::BuffersSwapped(
    const gfx::Size& surface_size,
    const gfx::Rect& damage_rect,
    float surface_scale_factor,
    const std::string& mailbox_name,
    const ui::LatencyInfo& latency_info,
    const BufferPresentedCallback& ack_callback) {
  scoped_refptr<ui::Texture> previous_texture(current_surface_);
  const gfx::Rect surface_rect = gfx::Rect(surface_size);
  framebuffer_holder_ = NULL;
  FrameMemoryManager::GetInstance()->RemoveFrame(this);

  if (!SwapBuffersPrepare(surface_rect,
                          surface_scale_factor,
                          damage_rect,
                          mailbox_name,
                          ack_callback)) {
    return;
  }

  SkRegion damage(RectToSkIRect(damage_rect));
  if (!skipped_damage_.isEmpty()) {
    damage.op(skipped_damage_, SkRegion::kUnion_Op);
    skipped_damage_.setEmpty();
  }

  DCHECK(surface_rect.Contains(SkIRectToRect(damage.getBounds())));
  ui::Texture* current_texture = current_surface_.get();

  const gfx::Size surface_size_in_pixel = surface_size;
  DLOG_IF(ERROR, previous_texture.get() &&
      previous_texture->size() != current_texture->size() &&
      SkIRectToRect(damage.getBounds()) != surface_rect) <<
      "Expected full damage rect after size change";
  if (previous_texture.get() && !previous_damage_.isEmpty() &&
      previous_texture->size() == current_texture->size()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    GLHelper* gl_helper = factory->GetGLHelper();
    gl_helper->CopySubBufferDamage(
        current_texture->PrepareTexture(),
        previous_texture->PrepareTexture(),
        damage,
        previous_damage_);
  }
  previous_damage_ = damage;

  ui::Compositor* compositor = GetCompositor();
  if (compositor) {
    // Co-ordinates come in OpenGL co-ordinate space.
    // We need to convert to layer space.
    gfx::Rect rect_to_paint =
        ConvertRectToDIP(surface_scale_factor,
                         gfx::Rect(damage_rect.x(),
                                   surface_size_in_pixel.height() -
                                       damage_rect.y() - damage_rect.height(),
                                   damage_rect.width(),
                                   damage_rect.height()));

    // Damage may not have been DIP aligned, so inflate damage to compensate
    // for any round-off error.
    rect_to_paint.Inset(-1, -1);
    rect_to_paint.Intersect(window_->bounds());

    if (paint_observer_)
      paint_observer_->OnUpdateCompositorContent();
    window_->SchedulePaintInRect(rect_to_paint);
    compositor->SetLatencyInfo(latency_info);
  }

  SwapBuffersCompleted(ack_callback, previous_texture);
}

void RenderWidgetHostViewAura::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
    int gpu_host_id) {
  gfx::Rect damage_rect(params_in_pixel.x,
                        params_in_pixel.y,
                        params_in_pixel.width,
                        params_in_pixel.height);
  BufferPresentedCallback ack_callback =
      base::Bind(&AcknowledgeBufferForGpu,
                 params_in_pixel.route_id,
                 gpu_host_id,
                 params_in_pixel.mailbox_name);
  BuffersSwapped(params_in_pixel.surface_size,
                 damage_rect,
                 params_in_pixel.surface_scale_factor,
                 params_in_pixel.mailbox_name,
                 params_in_pixel.latency_info,
                 ack_callback);
}

void RenderWidgetHostViewAura::AcceleratedSurfaceSuspend() {
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease() {
  // This really tells us to release the frontbuffer.
  if (current_surface_.get()) {
    ui::Compositor* compositor = GetCompositor();
    if (compositor) {
      // We need to wait for a commit to clear to guarantee that all we
      // will not issue any more GL referencing the previous surface.
      AddOnCommitCallbackAndDisableLocks(
          base::Bind(&RenderWidgetHostViewAura::
                     SetSurfaceNotInUseByCompositor,
                     AsWeakPtr(),
                     current_surface_));  // Hold a ref so the texture will not
                                          // get deleted until after commit.
    }
    current_surface_ = NULL;
    UpdateExternalTexture();
  }
}

bool RenderWidgetHostViewAura::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  // Aura doesn't use GetBackingStore for accelerated pages, so it doesn't
  // matter what is returned here as GetBackingStore is the only caller of this
  // method. TODO(jbates) implement this if other Aura code needs it.
  return false;
}

void RenderWidgetHostViewAura::SetSurfaceNotInUseByCompositor(
    scoped_refptr<ui::Texture>) {
}

// static
void RenderWidgetHostViewAura::CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty()) {
    callback.Run(false, SkBitmap());
    return;
  }

  if (result->HasTexture()) {
    PrepareTextureCopyOutputResult(dst_size_in_pixel, callback, result.Pass());
    return;
  }

  DCHECK(result->HasBitmap());
  PrepareBitmapCopyOutputResult(dst_size_in_pixel, callback, result.Pass());
}

static void CopyFromCompositingSurfaceFinished(
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  bitmap_pixels_lock.reset();
  release_callback->Run(0, false);
  callback.Run(result, *bitmap);
}

// static
void RenderWidgetHostViewAura::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));

  DCHECK(result->HasTexture());
  if (!result->HasTexture())
    return;

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    dst_size_in_pixel.width(), dst_size_in_pixel.height());
  if (!bitmap->allocPixels())
    return;
  bitmap->setIsOpaque(true);

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8* pixels = static_cast<uint8*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.name(),
      texture_mailbox.sync_point(),
      result->size(),
      gfx::Rect(result->size()),
      dst_size_in_pixel,
      pixels,
      base::Bind(&CopyFromCompositingSurfaceFinished,
                 callback,
                 base::Passed(&release_callback),
                 base::Passed(&bitmap),
                 base::Passed(&bitmap_pixels_lock)));
}

// static
void RenderWidgetHostViewAura::PrepareBitmapCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK(result->HasBitmap());

  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));
  if (!result->HasBitmap())
    return;

  scoped_ptr<SkBitmap> source = result->TakeBitmap();
  DCHECK(source);
  if (!source)
    return;

  ignore_result(scoped_callback_runner.Release());

  SkBitmap bitmap = skia::ImageOperations::Resize(
      *source,
      skia::ImageOperations::RESIZE_BEST,
      dst_size_in_pixel.width(),
      dst_size_in_pixel.height());
  callback.Run(true, bitmap);
}

static void CopyFromCompositingSurfaceFinishedForVideo(
    const base::Callback<void(bool)>& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  release_callback->Run(0, false);
  callback.Run(result);
}

// static
void RenderWidgetHostViewAura::CopyFromCompositingSurfaceHasResultForVideo(
    base::WeakPtr<RenderWidgetHostViewAura> rwhva,
    scoped_refptr<media::VideoFrame> video_frame,
    const base::Callback<void(bool)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(base::Bind(callback, false));

  if (!rwhva)
    return;

  if (result->IsEmpty())
    return;
  if (result->size().IsEmpty())
    return;

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's coded_size() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame =
      media::ComputeLetterboxRegion(gfx::Rect(video_frame->coded_size()),
                                    result->size());
  region_in_frame = gfx::Rect(region_in_frame.x() & ~1,
                              region_in_frame.y() & ~1,
                              region_in_frame.width() & ~1,
                              region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return;

  // We only handle texture readbacks for now. If the compositor is in software
  // mode, we could produce a software-backed VideoFrame here as well.
  if (!result->HasTexture()) {
    DCHECK(result->HasBitmap());
    scoped_ptr<SkBitmap> bitmap = result->TakeBitmap();
    // Scale the bitmap to the required size, if necessary.
    SkBitmap scaled_bitmap;
    if (result->size().width() != region_in_frame.width() ||
        result->size().height() != region_in_frame.height()) {
      skia::ImageOperations::ResizeMethod method =
          skia::ImageOperations::RESIZE_GOOD;
      scaled_bitmap = skia::ImageOperations::Resize(*bitmap.get(), method,
                                                    region_in_frame.width(),
                                                    region_in_frame.height());
    } else {
      scaled_bitmap = *bitmap.get();
    }

    {
      SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

      media::CopyRGBToVideoFrame(
          reinterpret_cast<uint8*>(scaled_bitmap.getPixels()),
          scaled_bitmap.rowBytes(),
          region_in_frame,
          video_frame.get());
    }
    ignore_result(scoped_callback_runner.Release());
    callback.Run(true);
    return;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  gfx::Rect result_rect(result->size());

  content::ReadbackYUVInterface* yuv_readback_pipeline =
      rwhva->yuv_readback_pipeline_.get();
  if (yuv_readback_pipeline == NULL ||
      yuv_readback_pipeline->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline->scaler()->DstSize() != region_in_frame.size()) {
    GLHelper::ScalerQuality quality = GLHelper::SCALER_QUALITY_FAST;
    std::string quality_switch = switches::kTabCaptureDownscaleQuality;
    // If we're scaling up, we can use the "best" quality.
    if (result_rect.size().width() < region_in_frame.size().width() &&
        result_rect.size().height() < region_in_frame.size().height())
      quality_switch = switches::kTabCaptureUpscaleQuality;

    std::string switch_value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(quality_switch);
    if (switch_value == "fast")
      quality = GLHelper::SCALER_QUALITY_FAST;
    else if (switch_value == "good")
      quality = GLHelper::SCALER_QUALITY_GOOD;
    else if (switch_value == "best")
      quality = GLHelper::SCALER_QUALITY_BEST;

    rwhva->yuv_readback_pipeline_.reset(
        gl_helper->CreateReadbackPipelineYUV(quality,
                                             result_rect.size(),
                                             result_rect,
                                             video_frame->coded_size(),
                                             region_in_frame,
                                             true,
                                             false));
    yuv_readback_pipeline = rwhva->yuv_readback_pipeline_.get();
  }

  ignore_result(scoped_callback_runner.Release());
  base::Callback<void(bool result)> finished_callback = base::Bind(
      &CopyFromCompositingSurfaceFinishedForVideo,
      callback,
      base::Passed(&release_callback));
  yuv_readback_pipeline->ReadbackYUV(
      texture_mailbox.name(),
      texture_mailbox.sync_point(),
      video_frame.get(),
      finished_callback);
}

void RenderWidgetHostViewAura::GetScreenInfo(WebScreenInfo* results) {
  GetScreenInfoForWindow(results, window_->GetRootWindow() ? window_ : NULL);
}

gfx::Rect RenderWidgetHostViewAura::GetBoundsInRootWindow() {
#if defined(OS_WIN)
  // aura::Window::GetBoundsInScreen doesn't take non-client area into
  // account.
  RECT window_rect = {0};

  aura::Window* top_level = window_->GetToplevelWindow();
  aura::RootWindow* root_window = top_level->GetRootWindow();
  if (!root_window)
    return top_level->GetBoundsInScreen();
  HWND hwnd = root_window->GetAcceleratedWidget();
  ::GetWindowRect(hwnd, &window_rect);
  gfx::Rect rect(window_rect);

  // Maximized windows are outdented from the work area by the frame thickness
  // even though this "frame" is not painted.  This confuses code (and people)
  // that think of a maximized window as corresponding exactly to the work area.
  // Correct for this by subtracting the frame thickness back off.
  if (::IsZoomed(hwnd)) {
    rect.Inset(GetSystemMetrics(SM_CXSIZEFRAME),
               GetSystemMetrics(SM_CYSIZEFRAME));
  }

  return gfx::win::ScreenToDIPRect(rect);
#else
  return window_->GetToplevelWindow()->GetBoundsInScreen();
#endif
}

void RenderWidgetHostViewAura::GestureEventAck(int gesture_event_type,
                                               InputEventAckState ack_result) {
  if (touch_editing_client_)
    touch_editing_client_->GestureEventAck(gesture_event_type);
}

void RenderWidgetHostViewAura::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  ScopedVector<ui::TouchEvent> events;
  if (!MakeUITouchEventsFromWebTouchEvents(touch, &events,
                                           SCREEN_COORDINATES))
    return;

  aura::RootWindow* root = window_->GetRootWindow();
  // |root| is NULL during tests.
  if (!root)
    return;

  ui::EventResult result = (ack_result ==
      INPUT_EVENT_ACK_STATE_CONSUMED) ? ui::ER_HANDLED : ui::ER_UNHANDLED;
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter) {
    root->ProcessedTouchEvent((*iter), window_, result);
  }
}

SyntheticGesture* RenderWidgetHostViewAura::CreateSmoothScrollGesture(
    bool scroll_down,
    int pixels_to_scroll,
    int mouse_event_x,
    int mouse_event_y) {
  return new TouchSmoothScrollGestureAura(scroll_down,
                                          pixels_to_scroll,
                                          mouse_event_x,
                                          mouse_event_y,
                                          window_);
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::OnAccessibilityEvents(
    const std::vector<AccessibilityHostMsg_EventParams>& params) {
  BrowserAccessibilityManager* manager =
      GetOrCreateBrowserAccessibilityManager();
  if (manager)
    manager->OnAccessibilityEvents(params);
}

gfx::GLSurfaceHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  if (shared_surface_handle_.is_null()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    shared_surface_handle_ = factory->CreateSharedSurfaceHandle();
    if (!shared_surface_handle_.is_null())
      factory->AddObserver(this);
  }
  return shared_surface_handle_;
}

bool RenderWidgetHostViewAura::LockMouse() {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  if (mouse_locked_)
    return true;

  mouse_locked_ = true;
  window_->SetCapture();
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->HideCursor();
    cursor_client->LockCursor();
  }

  if (ShouldMoveToCenter()) {
    synthetic_move_sent_ = true;
    window_->MoveCursorTo(gfx::Rect(window_->bounds().size()).CenterPoint());
  }
  if (aura::client::GetTooltipClient(root_window))
    aura::client::GetTooltipClient(root_window)->SetTooltipsEnabled(false);
  return true;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!mouse_locked_ || !root_window)
    return;

  mouse_locked_ = false;

  window_->ReleaseCapture();
  window_->MoveCursorTo(unlocked_mouse_position_);
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->UnlockCursor();
    cursor_client->ShowCursor();
  }

  if (aura::client::GetTooltipClient(root_window))
    aura::client::GetTooltipClient(root_window)->SetTooltipsEnabled(true);

  host_->LostMouseLock();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::TextInputClient implementation:
void RenderWidgetHostViewAura::SetCompositionText(
    const ui::CompositionText& composition) {
  if (!host_)
    return;

  // ui::CompositionUnderline should be identical to
  // WebKit::WebCompositionUnderline, so that we can do reinterpret_cast safely.
  COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
                 sizeof(WebKit::WebCompositionUnderline),
                 ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);

  // TODO(suzhe): convert both renderer_host and renderer to use
  // ui::CompositionText.
  const std::vector<WebKit::WebCompositionUnderline>& underlines =
      reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
          composition.underlines);

  // TODO(suzhe): due to a bug of webkit, we can't use selection range with
  // composition string. See: https://bugs.webkit.org/show_bug.cgi?id=37788
  host_->ImeSetComposition(composition.text, underlines,
                           composition.selection.end(),
                           composition.selection.end());

  has_composition_text_ = !composition.text.empty();
}

void RenderWidgetHostViewAura::ConfirmCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeConfirmComposition(string16(), gfx::Range::InvalidRange(), false);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ClearCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeCancelComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertText(const string16& text) {
  DCHECK(text_input_type_ != ui::TEXT_INPUT_TYPE_NONE);
  if (host_)
    host_->ImeConfirmComposition(text, gfx::Range::InvalidRange(), false);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertChar(char16 ch, int flags) {
  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->InsertChar(ch, flags);
    return;
  }

  if (host_) {
    double now = ui::EventTimeForNow().InSecondsF();
    // Send a WebKit::WebInputEvent::Char event to |host_|.
    NativeWebKeyboardEvent webkit_event(ui::ET_KEY_PRESSED,
                                        true /* is_char */,
                                        ch,
                                        flags,
                                        now);
    host_->ForwardKeyboardEvent(webkit_event);
  }
}

gfx::NativeWindow RenderWidgetHostViewAura::GetAttachedWindow() const {
  return window_;
}

ui::TextInputType RenderWidgetHostViewAura::GetTextInputType() const {
  return text_input_type_;
}

ui::TextInputMode RenderWidgetHostViewAura::GetTextInputMode() const {
  return text_input_mode_;
}

bool RenderWidgetHostViewAura::CanComposeInline() const {
  return can_compose_inline_;
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectToScreen(const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return rect;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (!screen_position_client)
    return rect;
  screen_position_client->ConvertPointToScreen(window_, &origin);
  screen_position_client->ConvertPointToScreen(window_, &end);
  return gfx::Rect(origin.x(),
                   origin.y(),
                   end.x() - origin.x(),
                   end.y() - origin.y());
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectFromScreen(
    const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::RootWindow* root_window = window_->GetRootWindow();
  if (root_window) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    screen_position_client->ConvertPointFromScreen(window_, &origin);
    screen_position_client->ConvertPointFromScreen(window_, &end);
    return gfx::Rect(origin.x(),
                     origin.y(),
                     end.x() - origin.x(),
                     end.y() - origin.y());
  }

  return rect;
}

gfx::Rect RenderWidgetHostViewAura::GetCaretBounds() {
  const gfx::Rect rect =
      gfx::UnionRects(selection_anchor_rect_, selection_focus_rect_);
  return ConvertRectToScreen(rect);
}

bool RenderWidgetHostViewAura::GetCompositionCharacterBounds(uint32 index,
                                                             gfx::Rect* rect) {
  DCHECK(rect);
  if (index >= composition_character_bounds_.size())
    return false;
  *rect = ConvertRectToScreen(composition_character_bounds_[index]);
  return true;
}

bool RenderWidgetHostViewAura::HasCompositionText() {
  return has_composition_text_;
}

bool RenderWidgetHostViewAura::GetTextRange(gfx::Range* range) {
  range->set_start(selection_text_offset_);
  range->set_end(selection_text_offset_ + selection_text_.length());
  return true;
}

bool RenderWidgetHostViewAura::GetCompositionTextRange(gfx::Range* range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetSelectionRange(gfx::Range* range) {
  range->set_start(selection_range_.start());
  range->set_end(selection_range_.end());
  return true;
}

bool RenderWidgetHostViewAura::SetSelectionRange(const gfx::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::DeleteRange(const gfx::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetTextFromRange(
    const gfx::Range& range,
    string16* text) {
  gfx::Range selection_text_range(selection_text_offset_,
      selection_text_offset_ + selection_text_.length());

  if (!selection_text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (selection_text_range.EqualsIgnoringDirection(range)) {
    // Avoid calling substr whose performance is low.
    *text = selection_text_;
  } else {
    *text = selection_text_.substr(
        range.GetMin() - selection_text_offset_,
        range.length());
  }
  return true;
}

void RenderWidgetHostViewAura::OnInputMethodChanged() {
  if (!host_)
    return;

  if (GetInputMethod())
    host_->SetInputMethodActive(GetInputMethod()->IsActive());

  // TODO(suzhe): implement the newly added “locale” property of HTML DOM
  // TextEvent.
}

bool RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) {
  if (!host_)
    return false;
  host_->UpdateTextDirection(
      direction == base::i18n::RIGHT_TO_LEFT ?
      WebKit::WebTextDirectionRightToLeft :
      WebKit::WebTextDirectionLeftToRight);
  host_->NotifyTextDirection();
  return true;
}

void RenderWidgetHostViewAura::ExtendSelectionAndDelete(
    size_t before, size_t after) {
  if (host_)
    host_->ExtendSelectionAndDelete(before, after);
}

void RenderWidgetHostViewAura::EnsureCaretInRect(const gfx::Rect& rect) {
  gfx::Rect intersected_rect(
      gfx::IntersectRects(rect, window_->GetBoundsInScreen()));

  if (intersected_rect.IsEmpty())
    return;

  host_->ScrollFocusedEditableNodeIntoRect(
      ConvertRectFromScreen(intersected_rect));
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, gfx::DisplayObserver implementation:

void RenderWidgetHostViewAura::OnDisplayBoundsChanged(
    const gfx::Display& display) {
  gfx::Screen* screen = gfx::Screen::GetScreenFor(window_);
  if (display.id() == screen->GetDisplayNearestWindow(window_).id()) {
    UpdateScreenInfo(window_);
    current_cursor_.SetDisplayInfo(display);
    UpdateCursorIfOverSelf();
  }
}

void RenderWidgetHostViewAura::OnDisplayAdded(
    const gfx::Display& new_display) {
}

void RenderWidgetHostViewAura::OnDisplayRemoved(
    const gfx::Display& old_display) {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

gfx::Size RenderWidgetHostViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size RenderWidgetHostViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  // We care about this only in fullscreen mode, where there is no
  // WebContentsViewAura. We are sized via SetSize() or SetBounds() by
  // WebContentsViewAura in other cases.
  if (is_fullscreen_)
    SetSize(new_bounds.size());
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  if (mouse_locked_)
    return ui::kCursorNone;
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool RenderWidgetHostViewAura::CanFocus() {
  return popup_type_ == WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host_->LostCapture();
  if (touch_editing_client_)
    touch_editing_client_->EndTouchEditing();
}

void RenderWidgetHostViewAura::OnPaint(gfx::Canvas* canvas) {
  bool has_backing_store = !!host_->GetBackingStore(false);
  if (has_backing_store) {
    paint_canvas_ = canvas;
    BackingStoreAura* backing_store = static_cast<BackingStoreAura*>(
        host_->GetBackingStore(true));
    paint_canvas_ = NULL;
    backing_store->SkiaShowRect(gfx::Point(), canvas);

    if (paint_observer_)
      paint_observer_->OnPaintComplete();
    ui::Compositor* compositor = GetCompositor();
    if (compositor) {
      compositor->SetLatencyInfo(software_latency_info_);
      software_latency_info_.Clear();
    }
  } else {
    // For non-opaque windows, we don't draw anything, since we depend on the
    // canvas coming from the compositor to already be initialized as
    // transparent.
    if (window_->layer()->fills_bounds_opaquely())
      canvas->DrawColor(SK_ColorWHITE);
  }
}

void RenderWidgetHostViewAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  if (!host_)
    return;

  BackingStoreAura* backing_store = static_cast<BackingStoreAura*>(
      host_->GetBackingStore(false));
  if (backing_store)  // NULL in hardware path.
    backing_store->ScaleFactorChanged(device_scale_factor);

  UpdateScreenInfo(window_);

  const gfx::Display display = gfx::Screen::GetScreenFor(window_)->
      GetDisplayNearestWindow(window_);
  DCHECK_EQ(device_scale_factor, display.device_scale_factor());
  current_cursor_.SetDisplayInfo(display);
}

void RenderWidgetHostViewAura::OnWindowDestroying() {
#if defined(OS_WIN)
  HWND parent = NULL;
  // If the tab was hidden and it's closed, host_->is_hidden would have been
  // reset to false in RenderWidgetHostImpl::RendererExited.
  if (!window_->GetRootWindow() || host_->is_hidden()) {
    parent = ui::GetHiddenWindow();
  } else {
    parent = window_->GetRootWindow()->GetAcceleratedWidget();
  }
  LPARAM lparam = reinterpret_cast<LPARAM>(this);
  EnumChildWindows(parent, WindowDestroyingCallback, lparam);
#endif

  // Make sure that the input method no longer references to this object before
  // this object is removed from the root window (i.e. this object loses access
  // to the input method).
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);
}

void RenderWidgetHostViewAura::OnWindowDestroyed() {
  host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool RenderWidgetHostViewAura::HasHitTestMask() const {
  return false;
}

void RenderWidgetHostViewAura::GetHitTestMask(gfx::Path* mask) const {
}

void RenderWidgetHostViewAura::DidRecreateLayer(ui::Layer *old_layer,
                                                ui::Layer *new_layer) {
  float mailbox_scale_factor;
  cc::TextureMailbox old_mailbox =
      old_layer->GetTextureMailbox(&mailbox_scale_factor);
  scoped_refptr<ui::Texture> old_texture = old_layer->external_texture();
  // The new_layer is the one that will be used by our Window, so that's the one
  // that should keep our texture. old_layer will be returned to the
  // RecreateLayer caller, and should have a copy.
  if (old_texture.get()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    GLHelper* gl_helper = factory->GetGLHelper();
    scoped_refptr<ui::Texture> new_texture;
    if (host_->is_accelerated_compositing_active() &&
        gl_helper && current_surface_.get()) {
      WebKit::WebGLId texture_id =
          gl_helper->CopyTexture(current_surface_->PrepareTexture(),
                                 current_surface_->size());
      if (texture_id) {
        new_texture = factory->CreateOwnedTexture(
          current_surface_->size(),
          current_surface_->device_scale_factor(), texture_id);
      }
    }
    old_layer->SetExternalTexture(new_texture);
    new_layer->SetExternalTexture(old_texture);
  } else if (old_mailbox.IsSharedMemory()) {
    base::SharedMemory* old_buffer = old_mailbox.shared_memory();
    const size_t size = old_mailbox.shared_memory_size_in_bytes();

    scoped_ptr<base::SharedMemory> new_buffer(new base::SharedMemory);
    new_buffer->CreateAndMapAnonymous(size);

    if (old_buffer->memory() && new_buffer->memory()) {
      memcpy(new_buffer->memory(), old_buffer->memory(), size);
      base::SharedMemory* new_buffer_raw_ptr = new_buffer.get();
      scoped_ptr<cc::SingleReleaseCallback> callback =
          cc::SingleReleaseCallback::Create(base::Bind(MailboxReleaseCallback,
                                                       Passed(&new_buffer)));
      cc::TextureMailbox new_mailbox(new_buffer_raw_ptr,
                                     old_mailbox.shared_memory_size());
      new_layer->SetTextureMailbox(new_mailbox,
                                   callback.Pass(),
                                   mailbox_scale_factor);
    }
  }
  // TODO(piman): handle delegated frames.
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::EventHandler implementation:

void RenderWidgetHostViewAura::OnKeyEvent(ui::KeyEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnKeyEvent");
  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->OnKeyEvent(event);
    if (event->handled())
      return;
  }

  // We need to handle the Escape key for Pepper Flash.
  if (is_fullscreen_ && event->key_code() == ui::VKEY_ESCAPE) {
    // Focus the window we were created from.
    if (host_tracker_.get() && !host_tracker_->windows().empty()) {
      aura::Window* host = *(host_tracker_->windows().begin());
      aura::client::FocusClient* client = aura::client::GetFocusClient(host);
      if (client) {
        // Calling host->Focus() may delete |this|. We create a local observer
        // for that. In that case we exit without further access to any members.
        aura::WindowTracker tracker;
        aura::Window* window = window_;
        tracker.Add(window);
        host->Focus();
        if (!tracker.Contains(window)) {
          event->SetHandled();
          return;
        }
      }
    }
    if (!in_shutdown_) {
      in_shutdown_ = true;
      host_->Shutdown();
    }
  } else {
    // We don't have to communicate with an input method here.
    if (!event->HasNativeEvent()) {
      NativeWebKeyboardEvent webkit_event(
          event->type(),
          event->is_char(),
          event->is_char() ? event->GetCharacter() : event->key_code(),
          event->flags(),
          ui::EventTimeForNow().InSecondsF());
      host_->ForwardKeyboardEvent(webkit_event);
    } else {
      NativeWebKeyboardEvent webkit_event(event);
      host_->ForwardKeyboardEvent(webkit_event);
    }
  }
  event->SetHandled();
}

void RenderWidgetHostViewAura::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnMouseEvent");

  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  if (mouse_locked_) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window_->GetRootWindow());
    DCHECK(!cursor_client || !cursor_client->IsCursorVisible());

    if (event->type() == ui::ET_MOUSEWHEEL) {
      WebKit::WebMouseWheelEvent mouse_wheel_event =
          MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent*>(event));
      if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0)
        host_->ForwardWheelEvent(mouse_wheel_event);
      return;
    }

    WebKit::WebMouseEvent mouse_event = MakeWebMouseEvent(event);
    gfx::Point center(gfx::Rect(window_->bounds().size()).CenterPoint());

    bool is_move_to_center_event = (event->type() == ui::ET_MOUSE_MOVED ||
        event->type() == ui::ET_MOUSE_DRAGGED) &&
        mouse_event.x == center.x() && mouse_event.y == center.y();

    ModifyEventMovementAndCoords(&mouse_event);

    bool should_not_forward = is_move_to_center_event && synthetic_move_sent_;
    if (should_not_forward) {
      synthetic_move_sent_ = false;
    } else {
      // Check if the mouse has reached the border and needs to be centered.
      if (ShouldMoveToCenter()) {
        synthetic_move_sent_ = true;
        window_->MoveCursorTo(center);
      }
      // Forward event to renderer.
      if (CanRendererHandleEvent(event) &&
          !(event->flags() & ui::EF_FROM_TOUCH))
        host_->ForwardMouseEvent(mouse_event);
    }
    return;
  }

  // As the overscroll is handled during scroll events from the trackpad, the
  // RWHVA window is transformed by the overscroll controller. This transform
  // triggers a synthetic mouse-move event to be generated (by the aura
  // RootWindow). But this event interferes with the overscroll gesture. So,
  // ignore such synthetic mouse-move events if an overscroll gesture is in
  // progress.
  if (host_->overscroll_controller() &&
      host_->overscroll_controller()->overscroll_mode() != OVERSCROLL_NONE &&
      event->flags() & ui::EF_IS_SYNTHESIZED &&
      (event->type() == ui::ET_MOUSE_ENTERED ||
       event->type() == ui::ET_MOUSE_EXITED ||
       event->type() == ui::ET_MOUSE_MOVED)) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_MOUSEWHEEL) {
#if defined(OS_WIN)
    // We get mouse wheel/scroll messages even if we are not in the foreground.
    // So here we check if we have any owned popup windows in the foreground and
    // dismiss them.
    aura::RootWindow* root_window = window_->GetRootWindow();
    if (root_window) {
      HWND parent = root_window->GetAcceleratedWidget();
      HWND toplevel_hwnd = ::GetAncestor(parent, GA_ROOT);
      EnumThreadWindows(GetCurrentThreadId(),
                        DismissOwnedPopups,
                        reinterpret_cast<LPARAM>(toplevel_hwnd));
    }
#endif
    WebKit::WebMouseWheelEvent mouse_wheel_event =
        MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent*>(event));
    if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0)
      host_->ForwardWheelEvent(mouse_wheel_event);
  } else if (CanRendererHandleEvent(event) &&
             !(event->flags() & ui::EF_FROM_TOUCH)) {
    WebKit::WebMouseEvent mouse_event = MakeWebMouseEvent(event);
    ModifyEventMovementAndCoords(&mouse_event);
    host_->ForwardMouseEvent(mouse_event);
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      window_->SetCapture();
      // Confirm existing composition text on mouse click events, to make sure
      // the input caret won't be moved with an ongoing composition text.
      FinishImeCompositionSession();
      break;
    case ui::ET_MOUSE_RELEASED:
      window_->ReleaseCapture();
      break;
    default:
      break;
  }

  // Needed to propagate mouse event to native_tab_contents_view_aura.
  // TODO(pkotwicz): Find a better way of doing this.
  // In fullscreen mode which is typically used by flash, don't forward
  // the mouse events to the parent. The renderer and the plugin process
  // handle these events.
  if (!is_fullscreen_ && window_->parent()->delegate() &&
      !(event->flags() & ui::EF_FROM_TOUCH))
    window_->parent()->delegate()->OnMouseEvent(event);

  if (!IsXButtonUpEvent(event))
    event->SetHandled();
}

void RenderWidgetHostViewAura::OnScrollEvent(ui::ScrollEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnScrollEvent");
  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  if (event->type() == ui::ET_SCROLL) {
    if (event->finger_count() != 2)
      return;
    WebKit::WebGestureEvent gesture_event =
        MakeWebGestureEventFlingCancel();
    host_->ForwardGestureEvent(gesture_event);
    WebKit::WebMouseWheelEvent mouse_wheel_event =
        MakeWebMouseWheelEvent(event);
    host_->ForwardWheelEvent(mouse_wheel_event);
    RecordAction(UserMetricsAction("TrackpadScroll"));
  } else if (event->type() == ui::ET_SCROLL_FLING_START ||
             event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    WebKit::WebGestureEvent gesture_event =
        MakeWebGestureEvent(event);
    host_->ForwardGestureEvent(gesture_event);
    if (event->type() == ui::ET_SCROLL_FLING_START)
      RecordAction(UserMetricsAction("TrackpadScrollFling"));
  }

  event->SetHandled();
}

void RenderWidgetHostViewAura::OnTouchEvent(ui::TouchEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnTouchEvent");
  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  // Update the touch event first.
  WebKit::WebTouchPoint* point = UpdateWebTouchEventFromUIEvent(*event,
                                                                &touch_event_);

  // Forward the touch event only if a touch point was updated, and there's a
  // touch-event handler in the page, and no other touch-event is in the queue.
  // It is important to always consume the event if there is a touch-event
  // handler in the page, or some touch-event is already in the queue, even if
  // no point has been updated, to make sure that this event does not get
  // processed by the gesture recognizer before the events in the queue.
  if (host_->ShouldForwardTouchEvent())
    event->StopPropagation();

  if (point) {
    if (host_->ShouldForwardTouchEvent())
      host_->ForwardTouchEventWithLatencyInfo(touch_event_, *event->latency());
    UpdateWebTouchEventAfterDispatch(&touch_event_, point);
  }
}

void RenderWidgetHostViewAura::OnGestureEvent(ui::GestureEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewAura::OnGestureEvent");
  // Pinch gestures are currently disabled by default. See crbug.com/128477.
  if ((event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      event->type() == ui::ET_GESTURE_PINCH_UPDATE ||
      event->type() == ui::ET_GESTURE_PINCH_END) && !ShouldSendPinchGesture()) {
    event->SetHandled();
    return;
  }

  if (touch_editing_client_ && touch_editing_client_->HandleInputEvent(event))
    return;

  RenderViewHostDelegate* delegate = NULL;
  if (popup_type_ == WebKit::WebPopupTypeNone && !is_fullscreen_)
    delegate = RenderViewHost::From(host_)->GetDelegate();

  if (delegate && event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1) {
    delegate->HandleGestureBegin();
  }

  WebKit::WebGestureEvent gesture = MakeWebGestureEvent(event);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    // Webkit does not stop a fling-scroll on tap-down. So explicitly send an
    // event to stop any in-progress flings.
    WebKit::WebGestureEvent fling_cancel = gesture;
    fling_cancel.type = WebKit::WebInputEvent::GestureFlingCancel;
    fling_cancel.sourceDevice = WebKit::WebGestureEvent::Touchscreen;
    host_->ForwardGestureEvent(fling_cancel);
  }

  if (gesture.type != WebKit::WebInputEvent::Undefined) {
    host_->ForwardGestureEventWithLatencyInfo(gesture, *event->latency());

    if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
        event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
        event->type() == ui::ET_GESTURE_SCROLL_END) {
      RecordAction(UserMetricsAction("TouchscreenScroll"));
    } else if (event->type() == ui::ET_SCROLL_FLING_START) {
      RecordAction(UserMetricsAction("TouchscreenScrollFling"));
    }
  }

  if (delegate && event->type() == ui::ET_GESTURE_END &&
      event->details().touch_points() == 1) {
    delegate->HandleGestureEnd();
  }

  // If a gesture is not processed by the webpage, then WebKit processes it
  // (e.g. generates synthetic mouse events).
  event->SetHandled();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::ActivationDelegate implementation:

bool RenderWidgetHostViewAura::ShouldActivate() const {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return true;
  const ui::Event* event = root_window->current_event();
  if (!event)
    return true;
  return is_fullscreen_;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura,
//     aura::client::ActivationChangeObserver implementation:

void RenderWidgetHostViewAura::OnWindowActivated(aura::Window* gained_active,
                                                 aura::Window* lost_active) {
  DCHECK(window_ == gained_active || window_ == lost_active);
  if (window_ == gained_active) {
    const ui::Event* event = window_->GetRootWindow()->current_event();
    if (event && PointerEventActivates(*event))
      host_->OnPointerEventActivate();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::CursorClientObserver implementation:

void RenderWidgetHostViewAura::OnCursorVisibilityChanged(bool is_visible) {
  NotifyRendererOfCursorVisibilityState(is_visible);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::FocusChangeObserver implementation:

void RenderWidgetHostViewAura::OnWindowFocused(aura::Window* gained_focus,
                                               aura::Window* lost_focus) {
  DCHECK(window_ == gained_focus || window_ == lost_focus);
  if (window_ == gained_focus) {
    // We need to honor input bypass if the associated tab is does not want
    // input. This gives the current focused window a chance to be the text
    // input client and handle events.
    if (host_->ignore_input_events())
      return;

    host_->GotFocus();
    host_->SetActive(true);

    ui::InputMethod* input_method = GetInputMethod();
    if (input_method) {
      // Ask the system-wide IME to send all TextInputClient messages to |this|
      // object.
      input_method->SetFocusedTextInputClient(this);
      host_->SetInputMethodActive(input_method->IsActive());

      // Often the application can set focus to the view in response to a key
      // down. However the following char event shouldn't be sent to the web
      // page.
      host_->SuppressNextCharEvents();
    } else {
      host_->SetInputMethodActive(false);
    }
  } else if (window_ == lost_focus) {
    host_->SetActive(false);
    host_->Blur();

    DetachFromInputMethod();
    host_->SetInputMethodActive(false);

    if (touch_editing_client_)
      touch_editing_client_->EndTouchEditing();

    // If we lose the focus while fullscreen, close the window; Pepper Flash
    // won't do it for us (unlike NPAPI Flash). However, we do not close the
    // window if we lose the focus to a window on another display.
    gfx::Screen* screen = gfx::Screen::GetScreenFor(window_);
    bool focusing_other_display =
        gained_focus && screen->GetNumDisplays() > 1 &&
        (screen->GetDisplayNearestWindow(window_).id() !=
         screen->GetDisplayNearestWindow(gained_focus).id());
    if (is_fullscreen_ && !in_shutdown_ && !focusing_other_display) {
      in_shutdown_ = true;
      host_->Shutdown();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::RootWindowObserver implementation:

void RenderWidgetHostViewAura::OnRootWindowHostMoved(
    const aura::RootWindow* root,
    const gfx::Point& new_origin) {
  UpdateScreenInfo(window_);
}

void RenderWidgetHostViewAura::ReleaseCurrentFrame() {
  if (framebuffer_holder_.get() && !current_surface_.get()) {
    framebuffer_holder_ = NULL;
    ui::Compositor* compositor = GetCompositor();
    if (compositor) {
      AddOnCommitCallbackAndDisableLocks(base::Bind(
          &RenderWidgetHostViewAura::SendReclaimSoftwareFrames, AsWeakPtr()));
    }
    UpdateExternalTexture();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::CompositorObserver implementation:

void RenderWidgetHostViewAura::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  if (can_lock_compositor_ == NO_PENDING_COMMIT) {
    can_lock_compositor_ = YES;
    if (resize_lock_.get() && resize_lock_->GrabDeferredLock())
      can_lock_compositor_ = YES_DID_LOCK;
  }
  RunOnCommitCallbacks();
  if (resize_lock_ && resize_lock_->expected_size() == current_frame_size_) {
    resize_lock_.reset();
    host_->WasResized();
    // We may have had a resize while we had the lock (e.g. if the lock expired,
    // or if the UI still gave us some resizes), so make sure we grab a new lock
    // if necessary.
    MaybeCreateResizeLock();
  }
}

void RenderWidgetHostViewAura::OnCompositingStarted(
    ui::Compositor* compositor, base::TimeTicks start_time) {
  last_draw_ended_ = start_time;
}

void RenderWidgetHostViewAura::OnCompositingEnded(
    ui::Compositor* compositor) {
  if (paint_observer_)
    paint_observer_->OnCompositingComplete();
}

void RenderWidgetHostViewAura::OnCompositingAborted(
    ui::Compositor* compositor) {
}

void RenderWidgetHostViewAura::OnCompositingLockStateChanged(
    ui::Compositor* compositor) {
  // A compositor lock that is part of a resize lock timed out. We
  // should display a renderer frame.
  if (!compositor->IsLocked() && can_lock_compositor_ == YES_DID_LOCK) {
    can_lock_compositor_ = NO_PENDING_RENDERER_FRAME;
  }
}

void RenderWidgetHostViewAura::OnUpdateVSyncParameters(
    ui::Compositor* compositor,
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (IsShowing()) {
    if (IsDeadlineSchedulingEnabled()) {
      // The deadline scheduler has logic to stagger the draws of the
      // Renderer and Browser built-in, so send it an accurate timebase.
      host_->UpdateVSyncParameters(timebase, interval);
    } else if (!last_draw_ended_.is_null()) {
      // For the non-deadline scheduler, we send the Renderer an offset
      // vsync timebase to avoid its draws racing the Browser's draws.
      host_->UpdateVSyncParameters(last_draw_ended_, interval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, BrowserAccessibilityDelegate implementation:

void RenderWidgetHostViewAura::SetAccessibilityFocus(int acc_obj_id) {
  if (!host_)
    return;

  host_->AccessibilitySetFocus(acc_obj_id);
}

void RenderWidgetHostViewAura::AccessibilityDoDefaultAction(int acc_obj_id) {
  if (!host_)
    return;

  host_->AccessibilityDoDefaultAction(acc_obj_id);
}

void RenderWidgetHostViewAura::AccessibilityScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  if (!host_)
    return;

  host_->AccessibilityScrollToMakeVisible(acc_obj_id, subfocus);
}

void RenderWidgetHostViewAura::AccessibilityScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  if (!host_)
    return;

  host_->AccessibilityScrollToPoint(acc_obj_id, point);
}

void RenderWidgetHostViewAura::AccessibilitySetTextSelection(
    int acc_obj_id, int start_offset, int end_offset) {
  if (!host_)
    return;

  host_->AccessibilitySetTextSelection(
      acc_obj_id, start_offset, end_offset);
}

gfx::Point RenderWidgetHostViewAura::GetLastTouchEventLocation() const {
  // Only needed for Win 8 non-aura.
  return gfx::Point();
}

void RenderWidgetHostViewAura::FatalAccessibilityTreeError() {
  host_->FatalAccessibilityTreeError();
  SetBrowserAccessibilityManager(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ImageTransportFactoryObserver implementation:

void RenderWidgetHostViewAura::OnLostResources() {
  current_surface_ = NULL;
  UpdateExternalTexture();

  // Make sure all ImageTransportClients are deleted now that the context those
  // are using is becoming invalid. This sends pending ACKs and needs to happen
  // after calling UpdateExternalTexture() which syncs with the impl thread.
  RunOnCommitCallbacks();

  DCHECK(!shared_surface_handle_.is_null());
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->DestroySharedSurfaceHandle(shared_surface_handle_);
  shared_surface_handle_ = factory->CreateSharedSurfaceHandle();
  host_->CompositingSurfaceUpdated();
  host_->ScheduleComposite();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
  if (paint_observer_)
    paint_observer_->OnViewDestroyed();
  if (touch_editing_client_)
    touch_editing_client_->OnViewDestroyed();
  if (!shared_surface_handle_.is_null()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    factory->DestroySharedSurfaceHandle(shared_surface_handle_);
    factory->RemoveObserver(this);
  }
  window_observer_.reset();
#if defined(OS_WIN)
  transient_observer_.reset();
#endif
  if (window_->GetRootWindow())
    window_->GetRootWindow()->RemoveRootWindowObserver(this);
  UnlockMouse();
  if (popup_type_ != WebKit::WebPopupTypeNone && popup_parent_host_view_) {
    DCHECK(popup_parent_host_view_->popup_child_host_view_ == NULL ||
           popup_parent_host_view_->popup_child_host_view_ == this);
    popup_parent_host_view_->popup_child_host_view_ = NULL;
  }
  if (popup_child_host_view_) {
    DCHECK(popup_child_host_view_->popup_parent_host_view_ == NULL ||
           popup_child_host_view_->popup_parent_host_view_ == this);
    popup_child_host_view_->popup_parent_host_view_ = NULL;
  }
  aura::client::SetTooltipText(window_, NULL);
  gfx::Screen::GetScreenFor(window_)->RemoveObserver(this);

  // This call is usually no-op since |this| object is already removed from the
  // Aura root window and we don't have a way to get an input method object
  // associated with the window, but just in case.
  DetachFromInputMethod();
  FrameMemoryManager::GetInstance()->RemoveFrame(this);
}

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  const gfx::Point screen_point =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return;

  gfx::Rect screen_rect = GetViewBounds();
  gfx::Point local_point = screen_point;
  local_point.Offset(-screen_rect.x(), -screen_rect.y());

  if (!root_window->HasFocus() ||
      root_window->GetEventHandlerForPoint(local_point) != window_) {
    return;
  }

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  // Do not show loading cursor when the cursor is currently hidden.
  if (is_loading_ && cursor != ui::kCursorNone)
    cursor = ui::kCursorPointer;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->SetCursor(cursor);
  }
}

ui::InputMethod* RenderWidgetHostViewAura::GetInputMethod() const {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return NULL;
  return root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
}

bool RenderWidgetHostViewAura::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

void RenderWidgetHostViewAura::FinishImeCompositionSession() {
  if (!has_composition_text_)
    return;
  if (host_)
    host_->ImeConfirmComposition(string16(), gfx::Range::InvalidRange(), false);
  ImeCancelComposition();
}

void RenderWidgetHostViewAura::ModifyEventMovementAndCoords(
    WebKit::WebMouseEvent* event) {
  // If the mouse has just entered, we must report zero movementX/Y. Hence we
  // reset any global_mouse_position set previously.
  if (event->type == WebKit::WebInputEvent::MouseEnter ||
      event->type == WebKit::WebInputEvent::MouseLeave)
    global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Movement is computed by taking the difference of the new cursor position
  // and the previous. Under mouse lock the cursor will be warped back to the
  // center so that we are not limited by clipping boundaries.
  // We do not measure movement as the delta from cursor to center because
  // we may receive more mouse movement events before our warp has taken
  // effect.
  event->movementX = event->globalX - global_mouse_position_.x();
  event->movementY = event->globalY - global_mouse_position_.y();

  global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (mouse_locked_) {
    event->x = unlocked_mouse_position_.x();
    event->y = unlocked_mouse_position_.y();
    event->windowX = unlocked_mouse_position_.x();
    event->windowY = unlocked_mouse_position_.y();
    event->globalX = unlocked_global_mouse_position_.x();
    event->globalY = unlocked_global_mouse_position_.y();
  } else {
    unlocked_mouse_position_.SetPoint(event->windowX, event->windowY);
    unlocked_global_mouse_position_.SetPoint(event->globalX, event->globalY);
  }
}

void RenderWidgetHostViewAura::NotifyRendererOfCursorVisibilityState(
    bool is_visible) {
  if (host_->is_hidden() ||
      (cursor_visibility_state_in_renderer_ == VISIBLE && is_visible) ||
      (cursor_visibility_state_in_renderer_ == NOT_VISIBLE && !is_visible))
    return;

  cursor_visibility_state_in_renderer_ = is_visible ? VISIBLE : NOT_VISIBLE;
  host_->SendCursorVisibilityState(is_visible);
}

void RenderWidgetHostViewAura::SchedulePaintIfNotInClip(
    const gfx::Rect& rect,
    const gfx::Rect& clip) {
  if (!clip.IsEmpty()) {
    gfx::Rect to_paint = gfx::SubtractRects(rect, clip);
    if (!to_paint.IsEmpty())
      window_->SchedulePaintInRect(to_paint);
  } else {
    window_->SchedulePaintInRect(rect);
  }
}

bool RenderWidgetHostViewAura::ShouldMoveToCenter() {
  gfx::Rect rect = window_->bounds();
  rect = ConvertRectToScreen(rect);
  int border_x = rect.width() * kMouseLockBorderPercentage / 100;
  int border_y = rect.height() * kMouseLockBorderPercentage / 100;

  return global_mouse_position_.x() < rect.x() + border_x ||
      global_mouse_position_.x() > rect.right() - border_x ||
      global_mouse_position_.y() < rect.y() + border_y ||
      global_mouse_position_.y() > rect.bottom() - border_y;
}

void RenderWidgetHostViewAura::RunOnCommitCallbacks() {
  for (std::vector<base::Closure>::const_iterator
      it = on_compositing_did_commit_callbacks_.begin();
      it != on_compositing_did_commit_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_did_commit_callbacks_.clear();
}

void RenderWidgetHostViewAura::AddOnCommitCallbackAndDisableLocks(
    const base::Closure& callback) {
  ui::Compositor* compositor = GetCompositor();
  DCHECK(compositor);

  if (!compositor->HasObserver(this))
    compositor->AddObserver(this);

  can_lock_compositor_ = NO_PENDING_COMMIT;
  on_compositing_did_commit_callbacks_.push_back(callback);
}

void RenderWidgetHostViewAura::AddedToRootWindow() {
  window_->GetRootWindow()->AddRootWindowObserver(this);
  host_->ParentChanged(GetNativeViewId());
  UpdateScreenInfo(window_);
  if (popup_type_ != WebKit::WebPopupTypeNone)
    event_filter_for_popup_exit_.reset(new EventFilterForPopupExit(this));

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client) {
    cursor_client->AddObserver(this);
    NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }
  UpdateExternalTexture();
}

void RenderWidgetHostViewAura::RemovingFromRootWindow() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client)
    cursor_client->RemoveObserver(this);

  event_filter_for_popup_exit_.reset();
  window_->GetRootWindow()->RemoveRootWindowObserver(this);
  host_->ParentChanged(0);
  ui::Compositor* compositor = GetCompositor();
  // We can't get notification for commits after this point, which would
  // guarantee that the compositor isn't using an old texture any more, so
  // instead we force the texture to NULL which synchronizes with the compositor
  // thread, and makes it safe to run the callback.
  window_->layer()->SetExternalTexture(NULL);
  RunOnCommitCallbacks();
  resize_lock_.reset();
  host_->WasResized();
  if (compositor && compositor->HasObserver(this))
    compositor->RemoveObserver(this);
}

ui::Compositor* RenderWidgetHostViewAura::GetCompositor() const {
  aura::RootWindow* root_window = window_->GetRootWindow();
  return root_window ? root_window->compositor() : NULL;
}

void RenderWidgetHostViewAura::DetachFromInputMethod() {
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this)
    input_method->SetFocusedTextInputClient(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewAura(widget);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(WebScreenInfo* results) {
  GetScreenInfoForWindow(results, NULL);
}

}  // namespace content
