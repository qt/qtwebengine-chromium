// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_root_window_host_win.h"

#include "base/win/metro.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/base/win/shell.h"
#include "ui/compositor/compositor_constants.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/path.h"
#include "ui/gfx/path_win.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/win/dpi.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/corewm/input_method_event_filter.h"
#include "ui/views/corewm/tooltip_win.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

namespace {

gfx::Size GetExpandedWindowSize(DWORD window_style, gfx::Size size) {
  if (!(window_style & WS_EX_COMPOSITED) || !ui::win::IsAeroGlassEnabled())
    return size;

  // Some AMD drivers can't display windows that are less than 64x64 pixels,
  // so expand them to be at least that size. http://crbug.com/286609
  gfx::Size expanded(std::max(size.width(), 64), std::max(size.height(), 64));
  return expanded;
}

void InsetBottomRight(gfx::Rect* rect, gfx::Vector2d vector) {
  rect->Inset(0, 0, vector.x(), vector.y());
}

}  // namespace

DEFINE_WINDOW_PROPERTY_KEY(aura::Window*, kContentWindowForRootWindow, NULL);

// Identifies the DesktopRootWindowHostWin associated with the RootWindow.
DEFINE_WINDOW_PROPERTY_KEY(DesktopRootWindowHostWin*, kDesktopRootWindowHostKey,
                           NULL);

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, public:

DesktopRootWindowHostWin::DesktopRootWindowHostWin(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : root_window_(NULL),
      message_handler_(new HWNDMessageHandler(this)),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      content_window_(NULL),
      drag_drop_client_(NULL),
      should_animate_window_close_(false),
      pending_close_(false),
      has_non_client_view_(false),
      tooltip_(NULL),
      is_cursor_visible_(true) {
}

DesktopRootWindowHostWin::~DesktopRootWindowHostWin() {
  // WARNING: |content_window_| has been destroyed by the time we get here.
  desktop_native_widget_aura_->OnDesktopRootWindowHostDestroyed(
      root_window_);
}

// static
aura::Window* DesktopRootWindowHostWin::GetContentWindowForHWND(HWND hwnd) {
  aura::RootWindow* root = aura::RootWindow::GetForAcceleratedWidget(hwnd);
  return root ? root->window()->GetProperty(kContentWindowForRootWindow) : NULL;
}

// static
ui::NativeTheme* DesktopRootWindowHost::GetNativeTheme(aura::Window* window) {
  // Use NativeThemeWin for windows shown on the desktop, those not on the
  // desktop come from Ash and get NativeThemeAura.
  aura::WindowEventDispatcher* dispatcher =
      window ? window->GetDispatcher() : NULL;
  if (dispatcher) {
    HWND host_hwnd = dispatcher->host()->GetAcceleratedWidget();
    if (host_hwnd &&
        DesktopRootWindowHostWin::GetContentWindowForHWND(host_hwnd)) {
      return ui::NativeThemeWin::instance();
    }
  }
  return ui::NativeThemeAura::instance();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, DesktopRootWindowHost implementation:

void DesktopRootWindowHostWin::Init(
    aura::Window* content_window,
    const Widget::InitParams& params,
    aura::RootWindow::CreateParams* rw_create_params) {
  // TODO(beng): SetInitParams().
  content_window_ = content_window;

  aura::client::SetAnimationHost(content_window_, this);

  ConfigureWindowStyles(message_handler_.get(), params,
                        GetWidget()->widget_delegate(),
                        native_widget_delegate_);

  HWND parent_hwnd = NULL;
  if (params.parent && params.parent->GetDispatcher()) {
    parent_hwnd =
        params.parent->GetDispatcher()->host()->GetAcceleratedWidget();
  }

  message_handler_->set_remove_standard_frame(params.remove_standard_frame);

  has_non_client_view_ = Widget::RequiresNonClientView(params.type);

  if (params.type == Widget::InitParams::TYPE_MENU) {
    ::SetProp(GetAcceleratedWidget(),
              kForceSoftwareCompositor,
              reinterpret_cast<HANDLE>(true));
  }

  gfx::Rect pixel_bounds = gfx::win::DIPToScreenRect(params.bounds);
  message_handler_->Init(parent_hwnd, pixel_bounds);

  rw_create_params->host = this;
}

void DesktopRootWindowHostWin::OnRootWindowCreated(
    aura::RootWindow* root,
    const Widget::InitParams& params) {
  root_window_ = root;

  root_window_->window()->SetProperty(kContentWindowForRootWindow,
                                      content_window_);
  root_window_->window()->SetProperty(kDesktopRootWindowHostKey, this);

  should_animate_window_close_ =
      content_window_->type() != aura::client::WINDOW_TYPE_NORMAL &&
      !views::corewm::WindowAnimationsDisabled(content_window_);

// TODO this is not invoked *after* Init(), but should be ok.
  SetWindowTransparency();
}

scoped_ptr<corewm::Tooltip> DesktopRootWindowHostWin::CreateTooltip() {
  DCHECK(!tooltip_);
  tooltip_ = new corewm::TooltipWin(GetAcceleratedWidget());
  return scoped_ptr<corewm::Tooltip>(tooltip_);
}

scoped_ptr<aura::client::DragDropClient>
DesktopRootWindowHostWin::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientWin(root_window_->window(),
                                                   GetHWND());
  return scoped_ptr<aura::client::DragDropClient>(drag_drop_client_).Pass();
}

void DesktopRootWindowHostWin::Close() {
  // TODO(beng): Move this entire branch to DNWA so it can be shared with X11.
  if (should_animate_window_close_) {
    pending_close_ = true;
    const bool is_animating =
        content_window_->layer()->GetAnimator()->IsAnimatingProperty(
            ui::LayerAnimationElement::VISIBILITY);
    // Animation may not start for a number of reasons.
    if (!is_animating)
      message_handler_->Close();
    // else case, OnWindowHidingAnimationCompleted does the actual Close.
  } else {
    message_handler_->Close();
  }
}

void DesktopRootWindowHostWin::CloseNow() {
  message_handler_->CloseNow();
}

aura::RootWindowHost* DesktopRootWindowHostWin::AsRootWindowHost() {
  return this;
}

void DesktopRootWindowHostWin::ShowWindowWithState(
    ui::WindowShowState show_state) {
  message_handler_->ShowWindowWithState(show_state);
}

void DesktopRootWindowHostWin::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  gfx::Rect pixel_bounds = gfx::win::DIPToScreenRect(restored_bounds);
  message_handler_->ShowMaximizedWithBounds(pixel_bounds);
}

bool DesktopRootWindowHostWin::IsVisible() const {
  return message_handler_->IsVisible();
}

void DesktopRootWindowHostWin::SetSize(const gfx::Size& size) {
  gfx::Size size_in_pixels = gfx::win::DIPToScreenSize(size);
  gfx::Size expanded = GetExpandedWindowSize(
      message_handler_->window_ex_style(), size_in_pixels);
  window_enlargement_ =
      gfx::Vector2d(expanded.width() - size_in_pixels.width(),
                    expanded.height() - size_in_pixels.height());
  message_handler_->SetSize(expanded);
}

void DesktopRootWindowHostWin::StackAtTop() {
  message_handler_->StackAtTop();
}

void DesktopRootWindowHostWin::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = gfx::win::DIPToScreenSize(size);
  gfx::Size expanded_size;
  expanded_size = GetExpandedWindowSize(message_handler_->window_ex_style(),
                                        size_in_pixels);
  window_enlargement_ =
      gfx::Vector2d(expanded_size.width() - size_in_pixels.width(),
                    expanded_size.height() - size_in_pixels.height());
  message_handler_->CenterWindow(expanded_size);
}

void DesktopRootWindowHostWin::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  message_handler_->GetWindowPlacement(bounds, show_state);
  InsetBottomRight(bounds, window_enlargement_);
  *bounds = gfx::win::ScreenToDIPRect(*bounds);
}

gfx::Rect DesktopRootWindowHostWin::GetWindowBoundsInScreen() const {
  gfx::Rect pixel_bounds = message_handler_->GetWindowBoundsInScreen();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

gfx::Rect DesktopRootWindowHostWin::GetClientAreaBoundsInScreen() const {
  gfx::Rect pixel_bounds = message_handler_->GetClientAreaBoundsInScreen();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

gfx::Rect DesktopRootWindowHostWin::GetRestoredBounds() const {
  gfx::Rect pixel_bounds = message_handler_->GetRestoredBounds();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

gfx::Rect DesktopRootWindowHostWin::GetWorkAreaBoundsInScreen() const {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(message_handler_->hwnd(),
                                   MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  gfx::Rect pixel_bounds = gfx::Rect(monitor_info.rcWork);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

void DesktopRootWindowHostWin::SetShape(gfx::NativeRegion native_region) {
  if (native_region) {
    message_handler_->SetRegion(gfx::CreateHRGNFromSkRegion(*native_region));
  } else {
    message_handler_->SetRegion(NULL);
  }

  delete native_region;
}

void DesktopRootWindowHostWin::Activate() {
  message_handler_->Activate();
}

void DesktopRootWindowHostWin::Deactivate() {
  message_handler_->Deactivate();
}

bool DesktopRootWindowHostWin::IsActive() const {
  return message_handler_->IsActive();
}

void DesktopRootWindowHostWin::Maximize() {
  message_handler_->Maximize();
}

void DesktopRootWindowHostWin::Minimize() {
  message_handler_->Minimize();
}

void DesktopRootWindowHostWin::Restore() {
  message_handler_->Restore();
}

bool DesktopRootWindowHostWin::IsMaximized() const {
  return message_handler_->IsMaximized();
}

bool DesktopRootWindowHostWin::IsMinimized() const {
  return message_handler_->IsMinimized();
}

bool DesktopRootWindowHostWin::HasCapture() const {
  return message_handler_->HasCapture();
}

void DesktopRootWindowHostWin::SetAlwaysOnTop(bool always_on_top) {
  message_handler_->SetAlwaysOnTop(always_on_top);
}

bool DesktopRootWindowHostWin::IsAlwaysOnTop() const {
  return message_handler_->IsAlwaysOnTop();
}

bool DesktopRootWindowHostWin::SetWindowTitle(const string16& title) {
  return message_handler_->SetTitle(title);
}

void DesktopRootWindowHostWin::ClearNativeFocus() {
  message_handler_->ClearNativeFocus();
}

Widget::MoveLoopResult DesktopRootWindowHostWin::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  const bool hide_on_escape =
      escape_behavior == Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_HIDE;
  return message_handler_->RunMoveLoop(drag_offset, hide_on_escape) ?
      Widget::MOVE_LOOP_SUCCESSFUL : Widget::MOVE_LOOP_CANCELED;
}

void DesktopRootWindowHostWin::EndMoveLoop() {
  message_handler_->EndMoveLoop();
}

void DesktopRootWindowHostWin::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  message_handler_->SetVisibilityChangedAnimationsEnabled(value);
  content_window_->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

bool DesktopRootWindowHostWin::ShouldUseNativeFrame() {
  return ui::win::IsAeroGlassEnabled();
}

void DesktopRootWindowHostWin::FrameTypeChanged() {
  message_handler_->FrameTypeChanged();
  SetWindowTransparency();
}

NonClientFrameView* DesktopRootWindowHostWin::CreateNonClientFrameView() {
  return GetWidget()->ShouldUseNativeFrame() ?
      new NativeFrameView(GetWidget()) : NULL;
}

void DesktopRootWindowHostWin::SetFullscreen(bool fullscreen) {
  message_handler_->fullscreen_handler()->SetFullscreen(fullscreen);
  // TODO(sky): workaround for ScopedFullscreenVisibility showing window
  // directly. Instead of this should listen for visibility changes and then
  // update window.
  if (message_handler_->IsVisible() && !content_window_->TargetVisibility())
    content_window_->Show();
  SetWindowTransparency();
}

bool DesktopRootWindowHostWin::IsFullscreen() const {
  return message_handler_->fullscreen_handler()->fullscreen();
}

void DesktopRootWindowHostWin::SetOpacity(unsigned char opacity) {
  message_handler_->SetOpacity(static_cast<BYTE>(opacity));
  content_window_->layer()->SetOpacity(opacity / 255.0);
}

void DesktopRootWindowHostWin::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
  message_handler_->SetWindowIcons(window_icon, app_icon);
}

void DesktopRootWindowHostWin::InitModalType(ui::ModalType modal_type) {
  message_handler_->InitModalType(modal_type);
}

void DesktopRootWindowHostWin::FlashFrame(bool flash_frame) {
  message_handler_->FlashFrame(flash_frame);
}

void DesktopRootWindowHostWin::OnRootViewLayout() const {
}

void DesktopRootWindowHostWin::OnNativeWidgetFocus() {
  // HWNDMessageHandler will perform the proper updating on its own.
}

void DesktopRootWindowHostWin::OnNativeWidgetBlur() {
}

bool DesktopRootWindowHostWin::IsAnimatingClosed() const {
  return pending_close_;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, RootWindowHost implementation:

aura::RootWindow* DesktopRootWindowHostWin::GetRootWindow() {
  return root_window_;
}

gfx::AcceleratedWidget DesktopRootWindowHostWin::GetAcceleratedWidget() {
  return message_handler_->hwnd();
}

void DesktopRootWindowHostWin::Show() {
  message_handler_->Show();
}

void DesktopRootWindowHostWin::Hide() {
  if (!pending_close_)
    message_handler_->Hide();
}

void DesktopRootWindowHostWin::ToggleFullScreen() {
  SetWindowTransparency();
}

// GetBounds and SetBounds work in pixel coordinates, whereas other get/set
// methods work in DIP.

gfx::Rect DesktopRootWindowHostWin::GetBounds() const {
  gfx::Rect bounds(message_handler_->GetClientAreaBounds());
  // If the window bounds were expanded we need to return the original bounds
  // To achieve this we do the reverse of the expansion, i.e. add the
  // window_expansion_top_left_delta_ to the origin and subtract the
  // window_expansion_bottom_right_delta_ from the width and height.
  gfx::Rect without_expansion(
      bounds.x() + window_expansion_top_left_delta_.x(),
      bounds.y() + window_expansion_top_left_delta_.y(),
      bounds.width() - window_expansion_bottom_right_delta_.x() -
          window_enlargement_.x(),
      bounds.height() - window_expansion_bottom_right_delta_.y() -
          window_enlargement_.y());
  return without_expansion;
}

void DesktopRootWindowHostWin::SetBounds(const gfx::Rect& bounds) {
  // If the window bounds have to be expanded we need to subtract the
  // window_expansion_top_left_delta_ from the origin and add the
  // window_expansion_bottom_right_delta_ to the width and height
  gfx::Size old_hwnd_size(message_handler_->GetClientAreaBounds().size());
  gfx::Size old_content_size = GetBounds().size();

  gfx::Rect expanded(
      bounds.x() - window_expansion_top_left_delta_.x(),
      bounds.y() - window_expansion_top_left_delta_.y(),
      bounds.width() + window_expansion_bottom_right_delta_.x(),
      bounds.height() + window_expansion_bottom_right_delta_.y());

  gfx::Rect new_expanded(
      expanded.origin(),
      GetExpandedWindowSize(message_handler_->window_ex_style(),
                            expanded.size()));
  window_enlargement_ =
      gfx::Vector2d(new_expanded.width() - expanded.width(),
                    new_expanded.height() - expanded.height());
  message_handler_->SetBounds(new_expanded);

  // The client area size may have changed even though the window bounds have
  // not, if the window bounds were expanded to 64 pixels both times.
  if (old_hwnd_size == new_expanded.size() && old_content_size != bounds.size())
    HandleClientSizeChanged(new_expanded.size());
}

gfx::Insets DesktopRootWindowHostWin::GetInsets() const {
  return gfx::Insets();
}

void DesktopRootWindowHostWin::SetInsets(const gfx::Insets& insets) {
}

gfx::Point DesktopRootWindowHostWin::GetLocationOnNativeScreen() const {
  return GetBounds().origin();
}

void DesktopRootWindowHostWin::SetCapture() {
  message_handler_->SetCapture();
}

void DesktopRootWindowHostWin::ReleaseCapture() {
  message_handler_->ReleaseCapture();
}

void DesktopRootWindowHostWin::SetCursor(gfx::NativeCursor cursor) {
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);

  message_handler_->SetCursor(cursor.platform());
}

bool DesktopRootWindowHostWin::QueryMouseLocation(gfx::Point* location_return) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_->window());
  if (cursor_client && !cursor_client->IsMouseEventsEnabled()) {
    *location_return = gfx::Point(0, 0);
    return false;
  }
  POINT pt = {0};
  ::GetCursorPos(&pt);
  *location_return =
      gfx::Point(static_cast<int>(pt.x), static_cast<int>(pt.y));
  return true;
}

bool DesktopRootWindowHostWin::ConfineCursorToRootWindow() {
  RECT window_rect = root_window_->window()->GetBoundsInScreen().ToRECT();
  ::ClipCursor(&window_rect);
  return true;
}

void DesktopRootWindowHostWin::UnConfineCursor() {
  ::ClipCursor(NULL);
}

void DesktopRootWindowHostWin::OnCursorVisibilityChanged(bool show) {
  if (is_cursor_visible_ == show)
    return;
  is_cursor_visible_ = show;
  ::ShowCursor(!!show);
}

void DesktopRootWindowHostWin::MoveCursorTo(const gfx::Point& location) {
  POINT cursor_location = location.ToPOINT();
  ::ClientToScreen(GetHWND(), &cursor_location);
  ::SetCursorPos(cursor_location.x, cursor_location.y);
}

void DesktopRootWindowHostWin::PostNativeEvent(
    const base::NativeEvent& native_event) {
}

void DesktopRootWindowHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopRootWindowHostWin::PrepareForShutdown() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, aura::AnimationHost implementation:

void DesktopRootWindowHostWin::SetHostTransitionOffsets(
    const gfx::Vector2d& top_left_delta,
    const gfx::Vector2d& bottom_right_delta) {
  gfx::Rect bounds_without_expansion = GetBounds();
  window_expansion_top_left_delta_ = top_left_delta;
  window_expansion_bottom_right_delta_ = bottom_right_delta;
  SetBounds(bounds_without_expansion);
}

void DesktopRootWindowHostWin::OnWindowHidingAnimationCompleted() {
  if (pending_close_)
    message_handler_->Close();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, HWNDMessageHandlerDelegate implementation:

bool DesktopRootWindowHostWin::IsWidgetWindow() const {
  return has_non_client_view_;
}

bool DesktopRootWindowHostWin::IsUsingCustomFrame() const {
  return !GetWidget()->ShouldUseNativeFrame();
}

void DesktopRootWindowHostWin::SchedulePaint() {
  GetWidget()->GetRootView()->SchedulePaint();
}

void DesktopRootWindowHostWin::EnableInactiveRendering() {
  native_widget_delegate_->EnableInactiveRendering();
}

bool DesktopRootWindowHostWin::IsInactiveRenderingDisabled() {
  return native_widget_delegate_->IsInactiveRenderingDisabled();
}

bool DesktopRootWindowHostWin::CanResize() const {
  return GetWidget()->widget_delegate()->CanResize();
}

bool DesktopRootWindowHostWin::CanMaximize() const {
  return GetWidget()->widget_delegate()->CanMaximize();
}

bool DesktopRootWindowHostWin::CanActivate() const {
  if (IsModalWindowActive())
    return true;
  return native_widget_delegate_->CanActivate();
}

bool DesktopRootWindowHostWin::WidgetSizeIsClientSize() const {
  const Widget* widget = GetWidget()->GetTopLevelWidget();
  return IsMaximized() || (widget && widget->ShouldUseNativeFrame());
}

bool DesktopRootWindowHostWin::CanSaveFocus() const {
  return GetWidget()->is_top_level();
}

void DesktopRootWindowHostWin::SaveFocusOnDeactivate() {
  GetWidget()->GetFocusManager()->StoreFocusedView(true);
}

void DesktopRootWindowHostWin::RestoreFocusOnActivate() {
  RestoreFocusOnEnable();
}

void DesktopRootWindowHostWin::RestoreFocusOnEnable() {
  GetWidget()->GetFocusManager()->RestoreFocusedView();
}

bool DesktopRootWindowHostWin::IsModal() const {
  return native_widget_delegate_->IsModal();
}

int DesktopRootWindowHostWin::GetInitialShowState() const {
  return SW_SHOWNORMAL;
}

bool DesktopRootWindowHostWin::WillProcessWorkAreaChange() const {
  return GetWidget()->widget_delegate()->WillProcessWorkAreaChange();
}

int DesktopRootWindowHostWin::GetNonClientComponent(
    const gfx::Point& point) const {
  gfx::Point dip_position = gfx::win::ScreenToDIPPoint(point);
  return native_widget_delegate_->GetNonClientComponent(dip_position);
}

void DesktopRootWindowHostWin::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* path) {
  if (GetWidget()->non_client_view()) {
    GetWidget()->non_client_view()->GetWindowMask(size, path);
  } else if (!window_enlargement_.IsZero()) {
    gfx::Rect bounds(WidgetSizeIsClientSize()
                         ? message_handler_->GetClientAreaBoundsInScreen()
                         : message_handler_->GetWindowBoundsInScreen());
    InsetBottomRight(&bounds, window_enlargement_);
    path->addRect(SkRect::MakeXYWH(0, 0, bounds.width(), bounds.height()));
  }
}

bool DesktopRootWindowHostWin::GetClientAreaInsets(gfx::Insets* insets) const {
  return false;
}

void DesktopRootWindowHostWin::GetMinMaxSize(gfx::Size* min_size,
                                             gfx::Size* max_size) const {
  *min_size = native_widget_delegate_->GetMinimumSize();
  *max_size = native_widget_delegate_->GetMaximumSize();
}

gfx::Size DesktopRootWindowHostWin::GetRootViewSize() const {
  return GetWidget()->GetRootView()->size();
}

void DesktopRootWindowHostWin::ResetWindowControls() {
  GetWidget()->non_client_view()->ResetWindowControls();
}

void DesktopRootWindowHostWin::PaintLayeredWindow(gfx::Canvas* canvas) {
  GetWidget()->GetRootView()->Paint(canvas);
}

gfx::NativeViewAccessible DesktopRootWindowHostWin::GetNativeViewAccessible() {
  return GetWidget()->GetRootView()->GetNativeViewAccessible();
}

InputMethod* DesktopRootWindowHostWin::GetInputMethod() {
  return GetWidget()->GetInputMethodDirect();
}

bool DesktopRootWindowHostWin::ShouldHandleSystemCommands() const {
  return GetWidget()->widget_delegate()->ShouldHandleSystemCommands();
}

void DesktopRootWindowHostWin::HandleAppDeactivated() {
  native_widget_delegate_->EnableInactiveRendering();
}

void DesktopRootWindowHostWin::HandleActivationChanged(bool active) {
  // This can be invoked from HWNDMessageHandler::Init(), at which point we're
  // not in a good state and need to ignore it.
  if (!delegate_)
    return;

  if (active)
    delegate_->OnHostActivated();
  desktop_native_widget_aura_->HandleActivationChanged(active);
}

bool DesktopRootWindowHostWin::HandleAppCommand(short command) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  return GetWidget()->widget_delegate() &&
      GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void DesktopRootWindowHostWin::HandleCancelMode() {
  delegate_->OnHostCancelMode();
}

void DesktopRootWindowHostWin::HandleCaptureLost() {
  delegate_->OnHostLostWindowCapture();
  native_widget_delegate_->OnMouseCaptureLost();
}

void DesktopRootWindowHostWin::HandleClose() {
  GetWidget()->Close();
}

bool DesktopRootWindowHostWin::HandleCommand(int command) {
  return GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void DesktopRootWindowHostWin::HandleAccelerator(
    const ui::Accelerator& accelerator) {
  GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
}

void DesktopRootWindowHostWin::HandleCreate() {
  // TODO(beng): moar
  NOTIMPLEMENTED();

  native_widget_delegate_->OnNativeWidgetCreated(true);

  // 1. Window property association
  // 2. MouseWheel.
}

void DesktopRootWindowHostWin::HandleDestroying() {
  drag_drop_client_->OnNativeWidgetDestroying(GetHWND());
  native_widget_delegate_->OnNativeWidgetDestroying();
}

void DesktopRootWindowHostWin::HandleDestroyed() {
  desktop_native_widget_aura_->OnHostClosed();
}

bool DesktopRootWindowHostWin::HandleInitialFocus() {
  return GetWidget()->SetInitialFocus();
}

void DesktopRootWindowHostWin::HandleDisplayChange() {
  GetWidget()->widget_delegate()->OnDisplayChanged();
}

void DesktopRootWindowHostWin::HandleBeginWMSizeMove() {
  native_widget_delegate_->OnNativeWidgetBeginUserBoundsChange();
}

void DesktopRootWindowHostWin::HandleEndWMSizeMove() {
  native_widget_delegate_->OnNativeWidgetEndUserBoundsChange();
}

void DesktopRootWindowHostWin::HandleMove() {
  native_widget_delegate_->OnNativeWidgetMove();
  if (delegate_)
    delegate_->OnHostMoved(GetBounds().origin());
}

void DesktopRootWindowHostWin::HandleWorkAreaChanged() {
  GetWidget()->widget_delegate()->OnWorkAreaChanged();
}

void DesktopRootWindowHostWin::HandleVisibilityChanging(bool visible) {
  native_widget_delegate_->OnNativeWidgetVisibilityChanging(visible);
}

void DesktopRootWindowHostWin::HandleVisibilityChanged(bool visible) {
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopRootWindowHostWin::HandleClientSizeChanged(
    const gfx::Size& new_size) {
  if (delegate_)
    delegate_->OnHostResized(new_size);
}

void DesktopRootWindowHostWin::HandleFrameChanged() {
  SetWindowTransparency();
  // Replace the frame and layout the contents.
  GetWidget()->non_client_view()->UpdateFrame();
}

void DesktopRootWindowHostWin::HandleNativeFocus(HWND last_focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnFocus();
}

void DesktopRootWindowHostWin::HandleNativeBlur(HWND focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnBlur();
}

bool DesktopRootWindowHostWin::HandleMouseEvent(const ui::MouseEvent& event) {
  if (base::win::IsTSFAwareRequired() && event.IsAnyButton())
    ui::TSFBridge::GetInstance()->CancelComposition();
  return delegate_->OnHostMouseEvent(const_cast<ui::MouseEvent*>(&event));
}

bool DesktopRootWindowHostWin::HandleKeyEvent(const ui::KeyEvent& event) {
  return false;
}

bool DesktopRootWindowHostWin::HandleUntranslatedKeyEvent(
    const ui::KeyEvent& event) {
  ui::KeyEvent duplicate_event(event);
  return delegate_->OnHostKeyEvent(&duplicate_event);
}

void DesktopRootWindowHostWin::HandleTouchEvent(
    const ui::TouchEvent& event) {
  // HWNDMessageHandler asynchronously processes touch events. Because of this
  // it's possible for the aura::RootWindow to have been destroyed by the time
  // we attempt to process them.
  if (!GetWidget()->GetNativeView())
    return;

  // Currently we assume the window that has capture gets touch events too.
  aura::RootWindow* root =
      aura::RootWindow::GetForAcceleratedWidget(GetCapture());
  if (root) {
    DesktopRootWindowHostWin* target =
        root->window()->GetProperty(kDesktopRootWindowHostKey);
    if (target && target->HasCapture() && target != this) {
      POINT target_location(event.location().ToPOINT());
      ClientToScreen(GetHWND(), &target_location);
      ScreenToClient(target->GetHWND(), &target_location);
      ui::TouchEvent target_event(event, static_cast<View*>(NULL),
                                  static_cast<View*>(NULL));
      target_event.set_location(gfx::Point(target_location));
      target_event.set_root_location(target_event.location());
      target->delegate_->OnHostTouchEvent(&target_event);
      return;
    }
  }
  delegate_->OnHostTouchEvent(
      const_cast<ui::TouchEvent*>(&event));
}

bool DesktopRootWindowHostWin::HandleIMEMessage(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                LRESULT* result) {
  MSG msg = {};
  msg.hwnd = GetHWND();
  msg.message = message;
  msg.wParam = w_param;
  msg.lParam = l_param;
  return desktop_native_widget_aura_->input_method_event_filter()->
      input_method()->OnUntranslatedIMEMessage(msg, result);
}

void DesktopRootWindowHostWin::HandleInputLanguageChange(
    DWORD character_set,
    HKL input_language_id) {
  desktop_native_widget_aura_->input_method_event_filter()->
      input_method()->OnInputLocaleChanged();
}

bool DesktopRootWindowHostWin::HandlePaintAccelerated(
    const gfx::Rect& invalid_rect) {
  return native_widget_delegate_->OnNativeWidgetPaintAccelerated(invalid_rect);
}

void DesktopRootWindowHostWin::HandlePaint(gfx::Canvas* canvas) {
  delegate_->OnHostPaint(gfx::Rect());
}

bool DesktopRootWindowHostWin::HandleTooltipNotify(int w_param,
                                                   NMHDR* l_param,
                                                   LRESULT* l_result) {
  return tooltip_ && tooltip_->HandleNotify(w_param, l_param, l_result);
}

void DesktopRootWindowHostWin::HandleTooltipMouseMove(UINT message,
                                                      WPARAM w_param,
                                                      LPARAM l_param) {
  // TooltipWin implementation doesn't need this.
  // TODO(sky): remove from HWNDMessageHandler once non-aura path nuked.
}

bool DesktopRootWindowHostWin::PreHandleMSG(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param,
                                            LRESULT* result) {
  return false;
}

void DesktopRootWindowHostWin::PostHandleMSG(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
}

bool DesktopRootWindowHostWin::HandleScrollEvent(
    const ui::ScrollEvent& event) {
  return delegate_->OnHostScrollEvent(const_cast<ui::ScrollEvent*>(&event));
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostWin, private:

Widget* DesktopRootWindowHostWin::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopRootWindowHostWin::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

HWND DesktopRootWindowHostWin::GetHWND() const {
  return message_handler_->hwnd();
}

void DesktopRootWindowHostWin::SetWindowTransparency() {
  bool transparent = ShouldUseNativeFrame() && !IsFullscreen();
  root_window_->compositor()->SetHostHasTransparentBackground(transparent);
  root_window_->window()->SetTransparent(transparent);
  content_window_->SetTransparent(transparent);
}

bool DesktopRootWindowHostWin::IsModalWindowActive() const {
  // This function can get called during window creation which occurs before
  // root_window_ has been created.
  if (!root_window_)
    return false;

  aura::Window::Windows::const_iterator index;
  for (index = root_window_->window()->children().begin();
       index != root_window_->window()->children().end();
       ++index) {
    if ((*index)->GetProperty(aura::client::kModalKey) !=
        ui:: MODAL_TYPE_NONE && (*index)->TargetVisibility())
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopRootWindowHostWin(native_widget_delegate,
                                      desktop_native_widget_aura);
}

}  // namespace views
