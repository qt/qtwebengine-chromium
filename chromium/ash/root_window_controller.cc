// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <queue>
#include <vector>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/root_window_settings.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/dock/docked_window_layout_manager.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/panels/panel_window_event_handler.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/screen_dimmer.h"
#include "ash/wm/solo_window_tracker.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_background_controller.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/corewm/capture_controller.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"

#if defined(OS_CHROMEOS)
#include "ash/wm/boot_splash_screen_chromeos.h"
#endif

namespace ash {
namespace {

#if defined(OS_CHROMEOS)
// Duration for the animation that hides the boot splash screen, in
// milliseconds.  This should be short enough in relation to
// wm/window_animation.cc's brightness/grayscale fade animation that the login
// background image animation isn't hidden by the splash screen animation.
const int kBootSplashScreenHideDurationMs = 500;
#endif

// Creates a new window for use as a container.
aura::Window* CreateContainer(int window_id,
                              const char* name,
                              aura::Window* parent) {
  aura::Window* container = new aura::Window(NULL);
  container->set_id(window_id);
  container->SetName(name);
  container->Init(ui::LAYER_NOT_DRAWN);
  parent->AddChild(container);
  if (window_id != internal::kShellWindowId_UnparentedControlContainer)
    container->Show();
  return container;
}

// Reparents |window| to |new_parent|.
void ReparentWindow(aura::Window* window, aura::Window* new_parent) {
  // Update the restore bounds to make it relative to the display.
  wm::WindowState* state = wm::GetWindowState(window);
  gfx::Rect restore_bounds;
  bool has_restore_bounds = state->HasRestoreBounds();
  if (has_restore_bounds)
    restore_bounds = state->GetRestoreBoundsInParent();
  new_parent->AddChild(window);
  if (has_restore_bounds)
    state->SetRestoreBoundsInParent(restore_bounds);
}

// Reparents the appropriate set of windows from |src| to |dst|.
void ReparentAllWindows(aura::Window* src, aura::Window* dst) {
  // Set of windows to move.
  const int kContainerIdsToMove[] = {
    internal::kShellWindowId_DefaultContainer,
    internal::kShellWindowId_DockedContainer,
    internal::kShellWindowId_PanelContainer,
    internal::kShellWindowId_AlwaysOnTopContainer,
    internal::kShellWindowId_SystemModalContainer,
    internal::kShellWindowId_LockSystemModalContainer,
    internal::kShellWindowId_InputMethodContainer,
    internal::kShellWindowId_UnparentedControlContainer,
  };
  for (size_t i = 0; i < arraysize(kContainerIdsToMove); i++) {
    int id = kContainerIdsToMove[i];
    aura::Window* src_container = Shell::GetContainer(src, id);
    aura::Window* dst_container = Shell::GetContainer(dst, id);
    while (!src_container->children().empty()) {
      // Restart iteration from the source container windows each time as they
      // may change as a result of moving other windows.
      aura::Window::Windows::const_iterator iter =
          src_container->children().begin();
      while (iter != src_container->children().end() &&
             internal::SystemModalContainerLayoutManager::IsModalBackground(
                *iter)) {
        ++iter;
      }
      // If the entire window list is modal background windows then stop.
      if (iter == src_container->children().end())
        break;
      ReparentWindow(*iter, dst_container);
    }
  }
}

// Mark the container window so that a widget added to this container will
// use the virtual screeen coordinates instead of parent.
void SetUsesScreenCoordinates(aura::Window* container) {
  container->SetProperty(internal::kUsesScreenCoordinatesKey, true);
}

// Mark the container window so that a widget added to this container will
// say in the same root window regardless of the bounds specified.
void DescendantShouldStayInSameRootWindow(aura::Window* container) {
  container->SetProperty(internal::kStayInSameRootWindowKey, true);
}

// A window delegate which does nothing. Used to create a window that
// is a event target, but do nothing.
class EmptyWindowDelegate : public aura::WindowDelegate {
 public:
  EmptyWindowDelegate() {}
  virtual ~EmptyWindowDelegate() {}

  // aura::WindowDelegate overrides:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }
  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return gfx::Size();
  }
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {
  }
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(
      const gfx::Point& point) const OVERRIDE {
    return HTNOWHERE;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE {
    return false;
  }
  virtual bool CanFocus() OVERRIDE {
    return false;
  }
  virtual void OnCaptureLost() OVERRIDE {
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
  }
  virtual void OnDeviceScaleFactorChanged(
      float device_scale_factor) OVERRIDE {
  }
  virtual void OnWindowDestroying() OVERRIDE {}
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {
  }
  virtual bool HasHitTestMask() const OVERRIDE {
    return false;
  }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {}
  virtual void DidRecreateLayer(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyWindowDelegate);
};

}  // namespace

namespace internal {

void RootWindowController::CreateForPrimaryDisplay(
    aura::RootWindow* root) {
  RootWindowController* controller = new RootWindowController(root);
  controller->Init(RootWindowController::PRIMARY,
                   Shell::GetInstance()->delegate()->IsFirstRunAfterBoot());
}

void RootWindowController::CreateForSecondaryDisplay(aura::RootWindow * root) {
  RootWindowController* controller = new RootWindowController(root);
  controller->Init(RootWindowController::SECONDARY, false /* first run */);
}

void RootWindowController::CreateForVirtualKeyboardDisplay(
    aura::RootWindow * root) {
  RootWindowController* controller = new RootWindowController(root);
  controller->Init(RootWindowController::VIRTUAL_KEYBOARD,
                   false /* first run */);
}

// static
RootWindowController* RootWindowController::ForLauncher(aura::Window* window) {
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForWindow(
    const aura::Window* window) {
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForTargetRootWindow() {
  return internal::GetRootWindowController(Shell::GetTargetRootWindow());
}

// static
aura::Window* RootWindowController::GetContainerForWindow(
    aura::Window* window) {
  aura::Window* container = window->parent();
  while (container && container->type() != aura::client::WINDOW_TYPE_UNKNOWN)
    container = container->parent();
  return container;
}

RootWindowController::~RootWindowController() {
  Shutdown();
  root_window_.reset();
  // The CaptureClient needs to be around for as long as the RootWindow is
  // valid.
  capture_client_.reset();
}

void RootWindowController::SetWallpaperController(
    DesktopBackgroundWidgetController* controller) {
  wallpaper_controller_.reset(controller);
}

void RootWindowController::SetAnimatingWallpaperController(
    AnimatingDesktopController* controller) {
  if (animating_wallpaper_controller_.get())
    animating_wallpaper_controller_->StopAnimating();
  animating_wallpaper_controller_.reset(controller);
}

void RootWindowController::Shutdown() {
  Shell::GetInstance()->RemoveShellObserver(this);

  if (animating_wallpaper_controller_.get())
    animating_wallpaper_controller_->StopAnimating();
  wallpaper_controller_.reset();
  animating_wallpaper_controller_.reset();

  // Change the target root window before closing child windows. If any child
  // being removed triggers a relayout of the shelf it will try to build a
  // window list adding windows from the target root window's containers which
  // may have already gone away.
  if (Shell::GetTargetRootWindow() == root_window()) {
    Shell::GetInstance()->set_target_root_window(
        Shell::GetPrimaryRootWindow() == root_window() ?
        NULL : Shell::GetPrimaryRootWindow());
  }

  CloseChildWindows();
  GetRootWindowSettings(root_window())->controller = NULL;
  screen_dimmer_.reset();
  workspace_controller_.reset();
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  internal::GetRootWindowSettings(root_window())->display_id =
      gfx::Display::kInvalidDisplayID;
  // And this root window should no longer process events.
  root_window_->PrepareForShutdown();

  system_background_.reset();
}

SystemModalContainerLayoutManager*
RootWindowController::GetSystemModalLayoutManager(aura::Window* window) {
  aura::Window* modal_container = NULL;
  if (window) {
    aura::Window* window_container = GetContainerForWindow(window);
    if (window_container &&
        window_container->id() >= kShellWindowId_LockScreenContainer) {
      modal_container = GetContainer(kShellWindowId_LockSystemModalContainer);
    } else {
      modal_container = GetContainer(kShellWindowId_SystemModalContainer);
    }
  } else {
    int modal_window_id = Shell::GetInstance()->session_state_delegate()
        ->IsUserSessionBlocked() ? kShellWindowId_LockSystemModalContainer :
                                   kShellWindowId_SystemModalContainer;
    modal_container = GetContainer(modal_window_id);
  }
  return modal_container ? static_cast<SystemModalContainerLayoutManager*>(
      modal_container->layout_manager()) : NULL;
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return root_window()->GetChildById(container_id);
}

const aura::Window* RootWindowController::GetContainer(int container_id) const {
  return root_window_->window()->GetChildById(container_id);
}

void RootWindowController::ShowLauncher() {
  if (!shelf_->launcher())
    return;
  shelf_->launcher()->SetVisible(true);
  shelf_->status_area_widget()->Show();
}

void RootWindowController::OnLauncherCreated() {
  if (panel_layout_manager_)
    panel_layout_manager_->SetLauncher(shelf_->launcher());
  if (docked_layout_manager_) {
    docked_layout_manager_->SetLauncher(shelf_->launcher());
    if (shelf_->shelf_layout_manager())
      docked_layout_manager_->AddObserver(shelf_->shelf_layout_manager());
  }
}

void RootWindowController::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  if (status != user::LOGGED_IN_NONE)
    mouse_event_target_.reset();
  if (shelf_->status_area_widget())
    shelf_->status_area_widget()->UpdateAfterLoginStatusChange(status);
}

void RootWindowController::HandleInitialDesktopBackgroundAnimationStarted() {
#if defined(OS_CHROMEOS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshAnimateFromBootSplashScreen) &&
      boot_splash_screen_.get()) {
    // Make the splash screen fade out so it doesn't obscure the desktop
    // wallpaper's brightness/grayscale animation.
    boot_splash_screen_->StartHideAnimation(
        base::TimeDelta::FromMilliseconds(kBootSplashScreenHideDurationMs));
  }
#endif
}

void RootWindowController::OnWallpaperAnimationFinished(views::Widget* widget) {
  // Make sure the wallpaper is visible.
  system_background_->SetColor(SK_ColorBLACK);
#if defined(OS_CHROMEOS)
  boot_splash_screen_.reset();
#endif

  Shell::GetInstance()->user_wallpaper_delegate()->
      OnWallpaperAnimationFinished();
  // Only removes old component when wallpaper animation finished. If we
  // remove the old one before the new wallpaper is done fading in there will
  // be a white flash during the animation.
  if (animating_wallpaper_controller()) {
    DesktopBackgroundWidgetController* controller =
        animating_wallpaper_controller()->GetController(true);
    // |desktop_widget_| should be the same animating widget we try to move
    // to |kDesktopController|. Otherwise, we may close |desktop_widget_|
    // before move it to |kDesktopController|.
    DCHECK_EQ(controller->widget(), widget);
    // Release the old controller and close its background widget.
    SetWallpaperController(controller);
  }
}

void RootWindowController::CloseChildWindows() {
  mouse_event_target_.reset();

  // |solo_window_tracker_| must be shut down before windows are destroyed.
  if (solo_window_tracker_) {
    if (docked_layout_manager_)
      docked_layout_manager_->RemoveObserver(solo_window_tracker_.get());
    solo_window_tracker_.reset();
  }

  // Deactivate keyboard container before closing child windows and shutting
  // down associated layout managers.
  DeactivateKeyboard(Shell::GetInstance()->keyboard_controller());

  // panel_layout_manager_ needs to be shut down before windows are destroyed.
  if (panel_layout_manager_) {
    panel_layout_manager_->Shutdown();
    panel_layout_manager_ = NULL;
  }
  // docked_layout_manager_ needs to be shut down before windows are destroyed.
  if (docked_layout_manager_) {
    if (shelf_ && shelf_->shelf_layout_manager())
      docked_layout_manager_->RemoveObserver(shelf_->shelf_layout_manager());
    docked_layout_manager_->Shutdown();
    docked_layout_manager_ = NULL;
  }

  aura::client::SetDragDropClient(root_window(), NULL);

  // TODO(harrym): Remove when Status Area Widget is a child view.
  if (shelf_) {
    shelf_->ShutdownStatusAreaWidget();

    if (shelf_->shelf_layout_manager())
      shelf_->shelf_layout_manager()->PrepareForShutdown();
  }

  // Close background widget first as it depends on tooltip.
  wallpaper_controller_.reset();
  animating_wallpaper_controller_.reset();

  workspace_controller_.reset();
  aura::client::SetTooltipClient(root_window(), NULL);

  // Explicitly destroy top level windows. We do this as during part of
  // destruction such windows may query the RootWindow for state.
  std::queue<aura::Window*> non_toplevel_windows;
  non_toplevel_windows.push(root_window());
  while (!non_toplevel_windows.empty()) {
    aura::Window* non_toplevel_window = non_toplevel_windows.front();
    non_toplevel_windows.pop();
    aura::WindowTracker toplevel_windows;
    for (size_t i = 0; i < non_toplevel_window->children().size(); ++i) {
      aura::Window* child = non_toplevel_window->children()[i];
      if (!child->owned_by_parent())
        continue;
      if (child->delegate())
        toplevel_windows.Add(child);
      else
        non_toplevel_windows.push(child);
    }
    while (!toplevel_windows.windows().empty())
      delete *toplevel_windows.windows().begin();
  }
  // And then remove the containers.
  while (!root_window()->children().empty()) {
    aura::Window* window = root_window()->children()[0];
    if (window->owned_by_parent()) {
      delete window;
    } else {
      root_window()->RemoveChild(window);
    }
  }

  shelf_.reset();
}

void RootWindowController::MoveWindowsTo(aura::Window* dst) {
  // Forget the shelf early so that shelf don't update itself using wrong
  // display info.
  workspace_controller_->SetShelf(NULL);
  ReparentAllWindows(root_window(), dst);
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return shelf_->shelf_layout_manager();
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  CHECK(shelf_->status_area_widget());
  return shelf_->status_area_widget()->system_tray();
}

void RootWindowController::ShowContextMenu(const gfx::Point& location_in_screen,
                                           ui::MenuSourceType source_type) {
  DCHECK(Shell::GetInstance()->delegate());
  scoped_ptr<ui::MenuModel> menu_model(
      Shell::GetInstance()->delegate()->CreateContextMenu(root_window()));
  if (!menu_model)
    return;

  // Background controller may not be set yet if user clicked on status are
  // before initial animation completion. See crbug.com/222218
  if (!wallpaper_controller_.get())
    return;

  views::MenuRunner menu_runner(menu_model.get());
  if (menu_runner.RunMenuAt(wallpaper_controller_->widget(),
          NULL, gfx::Rect(location_in_screen, gfx::Size()),
          views::MenuItemView::TOPLEFT, source_type,
          views::MenuRunner::CONTEXT_MENU) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }

  Shell::GetInstance()->UpdateShelfVisibility();
}

void RootWindowController::UpdateShelfVisibility() {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

const aura::Window* RootWindowController::GetWindowForFullscreenMode() const {
  const aura::Window::Windows& windows =
      GetContainer(kShellWindowId_DefaultContainer)->children();
  const aura::Window* topmost_window = NULL;
  for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
       iter != windows.rend(); ++iter) {
    if (((*iter)->type() == aura::client::WINDOW_TYPE_NORMAL ||
         (*iter)->type() == aura::client::WINDOW_TYPE_PANEL) &&
        (*iter)->layer()->GetTargetVisibility()) {
      topmost_window = *iter;
      break;
    }
  }
  while (topmost_window) {
    if (wm::GetWindowState(topmost_window)->IsFullscreen())
      return topmost_window;
    topmost_window = topmost_window->transient_parent();
  }
  return NULL;
}

void RootWindowController::ActivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard::IsKeyboardEnabled() ||
      GetContainer(kShellWindowId_VirtualKeyboardContainer)) {
    return;
  }
  DCHECK(keyboard_controller);
  if (!keyboard::IsKeyboardUsabilityExperimentEnabled()) {
    keyboard_controller->AddObserver(shelf()->shelf_layout_manager());
    keyboard_controller->AddObserver(panel_layout_manager_);
    keyboard_controller->AddObserver(docked_layout_manager_);
  }
  aura::Window* parent = root_window();
  aura::Window* keyboard_container =
      keyboard_controller->GetContainerWindow();
  keyboard_container->set_id(kShellWindowId_VirtualKeyboardContainer);
  parent->AddChild(keyboard_container);
  // TODO(oshima): Bounds of keyboard container should be handled by
  // RootWindowLayoutManager. Remove this after fixed RootWindowLayoutManager.
  keyboard_container->SetBounds(parent->bounds());
}

void RootWindowController::DeactivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard::IsKeyboardEnabled())
    return;

  DCHECK(keyboard_controller);
  aura::Window* keyboard_container =
      keyboard_controller->GetContainerWindow();
  if (keyboard_container->GetRootWindow() == root_window()) {
    root_window()->RemoveChild(keyboard_container);
    if (!keyboard::IsKeyboardUsabilityExperimentEnabled()) {
      keyboard_controller->RemoveObserver(shelf()->shelf_layout_manager());
      keyboard_controller->RemoveObserver(panel_layout_manager_);
      keyboard_controller->RemoveObserver(docked_layout_manager_);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

RootWindowController::RootWindowController(aura::RootWindow* root_window)
    : root_window_(root_window),
      root_window_layout_(NULL),
      docked_layout_manager_(NULL),
      panel_layout_manager_(NULL),
      touch_hud_debug_(NULL),
      touch_hud_projection_(NULL) {
  GetRootWindowSettings(root_window_->window())->controller = this;
  screen_dimmer_.reset(new ScreenDimmer(root_window_->window()));

  stacking_controller_.reset(new StackingController);
  aura::client::SetWindowTreeClient(root_window_->window(),
                                    stacking_controller_.get());
  capture_client_.reset(
      new views::corewm::ScopedCaptureClient(root_window_->window()));
}

void RootWindowController::Init(RootWindowType root_window_type,
                                bool first_run_after_boot) {
  Shell* shell = Shell::GetInstance();
  shell->InitRootWindow(root_window());

  root_window_->SetCursor(ui::kCursorPointer);
  CreateContainersInRootWindow(root_window_->window());

  if (root_window_type == VIRTUAL_KEYBOARD) {
    shell->InitKeyboard();
    return;
  }

  CreateSystemBackground(first_run_after_boot);

  InitLayoutManagers();
  InitTouchHuds();

  if (Shell::GetPrimaryRootWindowController()->
      GetSystemModalLayoutManager(NULL)->has_modal_background()) {
    GetSystemModalLayoutManager(NULL)->CreateModalBackground();
  }

  shell->AddShellObserver(this);

  if (root_window_type == PRIMARY) {
    root_window_layout()->OnWindowResized();
    if (!keyboard::IsKeyboardUsabilityExperimentEnabled())
      shell->InitKeyboard();
  } else {
    root_window_layout()->OnWindowResized();
    shell->desktop_background_controller()->OnRootWindowAdded(root_window());
    shell->high_contrast_controller()->OnRootWindowAdded(
        root_window_->window());
    root_window_->host()->Show();

    // Create a launcher if a user is already logged in.
    if (shell->session_state_delegate()->NumberOfLoggedInUsers())
      shelf()->CreateLauncher();
  }

  solo_window_tracker_.reset(new SoloWindowTracker(root_window_.get()));
  if (docked_layout_manager_)
    docked_layout_manager_->AddObserver(solo_window_tracker_.get());
}

void RootWindowController::InitLayoutManagers() {
  root_window_layout_ = new RootWindowLayoutManager(root_window());
  root_window()->SetLayoutManager(root_window_layout_);

  aura::Window* default_container =
      GetContainer(kShellWindowId_DefaultContainer);
  // Workspace manager has its own layout managers.
  workspace_controller_.reset(
      new WorkspaceController(default_container));

  aura::Window* always_on_top_container =
      GetContainer(kShellWindowId_AlwaysOnTopContainer);
  always_on_top_container->SetLayoutManager(
      new BaseLayoutManager(
          always_on_top_container->GetRootWindow()));
  always_on_top_controller_.reset(new internal::AlwaysOnTopController);
  always_on_top_controller_->SetAlwaysOnTopContainer(always_on_top_container);

  DCHECK(!shelf_.get());
  aura::Window* shelf_container =
      GetContainer(internal::kShellWindowId_ShelfContainer);
  // TODO(harrym): Remove when status area is view.
  aura::Window* status_container =
      GetContainer(internal::kShellWindowId_StatusContainer);
  shelf_.reset(new ShelfWidget(
      shelf_container, status_container, workspace_controller()));

  if (!Shell::GetInstance()->session_state_delegate()->
      IsActiveUserSessionStarted()) {
    // This window exists only to be a event target on login screen.
    // It does not have to handle events, nor be visible.
    mouse_event_target_.reset(new aura::Window(new EmptyWindowDelegate));
    mouse_event_target_->Init(ui::LAYER_NOT_DRAWN);

    aura::Window* lock_background_container =
        GetContainer(internal::kShellWindowId_LockScreenBackgroundContainer);
    lock_background_container->AddChild(mouse_event_target_.get());
    mouse_event_target_->Show();
  }

  // Create Docked windows layout manager
  aura::Window* docked_container = GetContainer(
      internal::kShellWindowId_DockedContainer);
  docked_layout_manager_ =
      new internal::DockedWindowLayoutManager(docked_container,
                                              workspace_controller());
  docked_container_handler_.reset(
      new ToplevelWindowEventHandler(docked_container));
  docked_container->SetLayoutManager(docked_layout_manager_);

  // Create Panel layout manager
  aura::Window* panel_container = GetContainer(
      internal::kShellWindowId_PanelContainer);
  panel_layout_manager_ =
      new internal::PanelLayoutManager(panel_container);
  panel_container_handler_.reset(
      new PanelWindowEventHandler(panel_container));
  panel_container->SetLayoutManager(panel_layout_manager_);
}

void RootWindowController::InitTouchHuds() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshTouchHud))
    set_touch_hud_debug(new TouchHudDebug(root_window()));
  if (Shell::GetInstance()->is_touch_hud_projection_enabled())
    EnableTouchHudProjection();
}

void RootWindowController::CreateSystemBackground(
    bool is_first_run_after_boot) {
  SkColor color = SK_ColorBLACK;
#if defined(OS_CHROMEOS)
  if (is_first_run_after_boot)
    color = kChromeOsBootColor;
#endif
  system_background_.reset(
    new SystemBackgroundController(root_window(), color));

#if defined(OS_CHROMEOS)
  // Make a copy of the system's boot splash screen so we can composite it
  // onscreen until the desktop background is ready.
  if (is_first_run_after_boot &&
      (CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshCopyHostBackgroundAtBoot) ||
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshAnimateFromBootSplashScreen)))
    boot_splash_screen_.reset(new BootSplashScreen(root_window_.get()));
#endif
}

void RootWindowController::CreateContainersInRootWindow(
    aura::Window* root_window) {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the root window; all of
  // the other containers are their children.

  // The desktop background container is not part of the lock animation, so it
  // is not included in those animate groups.
  // When screen is locked desktop background is moved to lock screen background
  // container (moved back on unlock). We want to make sure that there's an
  // opaque layer occluding the non-lock-screen layers.
  aura::Window* desktop_background_container = CreateContainer(
      kShellWindowId_DesktopBackgroundContainer,
      "DesktopBackgroundContainer",
      root_window);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      desktop_background_container);

  aura::Window* non_lock_screen_containers = CreateContainer(
      kShellWindowId_NonLockScreenContainersContainer,
      "NonLockScreenContainersContainer",
      root_window);

  aura::Window* lock_background_containers = CreateContainer(
      kShellWindowId_LockScreenBackgroundContainer,
      "LockScreenBackgroundContainer",
      root_window);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      lock_background_containers);

  aura::Window* lock_screen_containers = CreateContainer(
      kShellWindowId_LockScreenContainersContainer,
      "LockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_related_containers = CreateContainer(
      kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer",
      root_window);

  CreateContainer(kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer",
                  non_lock_screen_containers);

  aura::Window* default_container = CreateContainer(
      kShellWindowId_DefaultContainer,
      "DefaultContainer",
      non_lock_screen_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(default_container);
  SetUsesScreenCoordinates(default_container);

  aura::Window* always_on_top_container = CreateContainer(
      kShellWindowId_AlwaysOnTopContainer,
      "AlwaysOnTopContainer",
      non_lock_screen_containers);
  always_on_top_container_handler_.reset(
      new ToplevelWindowEventHandler(always_on_top_container));
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      always_on_top_container);
  SetUsesScreenCoordinates(always_on_top_container);

  aura::Window* docked_container = CreateContainer(
      kShellWindowId_DockedContainer,
      "DockedContainer",
      non_lock_screen_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(docked_container);
  SetUsesScreenCoordinates(docked_container);

  aura::Window* shelf_container =
      CreateContainer(kShellWindowId_ShelfContainer,
                      "ShelfContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(shelf_container);
  DescendantShouldStayInSameRootWindow(shelf_container);

  aura::Window* panel_container = CreateContainer(
      kShellWindowId_PanelContainer,
      "PanelContainer",
      non_lock_screen_containers);
  SetUsesScreenCoordinates(panel_container);

  aura::Window* shelf_bubble_container =
      CreateContainer(kShellWindowId_ShelfBubbleContainer,
                      "ShelfBubbleContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(shelf_bubble_container);
  DescendantShouldStayInSameRootWindow(shelf_bubble_container);

  aura::Window* app_list_container =
      CreateContainer(kShellWindowId_AppListContainer,
                      "AppListContainer",
                      non_lock_screen_containers);
  SetUsesScreenCoordinates(app_list_container);

  aura::Window* modal_container = CreateContainer(
      kShellWindowId_SystemModalContainer,
      "SystemModalContainer",
      non_lock_screen_containers);
  modal_container_handler_.reset(
      new ToplevelWindowEventHandler(modal_container));
  modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(modal_container));
  views::corewm::SetChildWindowVisibilityChangesAnimated(modal_container);
  SetUsesScreenCoordinates(modal_container);

  aura::Window* input_method_container = CreateContainer(
      kShellWindowId_InputMethodContainer,
      "InputMethodContainer",
      non_lock_screen_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      input_method_container);
  SetUsesScreenCoordinates(input_method_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = CreateContainer(
      kShellWindowId_LockScreenContainer,
      "LockScreenContainer",
      lock_screen_containers);
  lock_container->SetLayoutManager(
      new BaseLayoutManager(root_window));
  SetUsesScreenCoordinates(lock_container);
  // TODO(beng): stopsevents

  aura::Window* lock_modal_container = CreateContainer(
      kShellWindowId_LockSystemModalContainer,
      "LockSystemModalContainer",
      lock_screen_containers);
  lock_modal_container_handler_.reset(
      new ToplevelWindowEventHandler(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(lock_modal_container));
  views::corewm::SetChildWindowVisibilityChangesAnimated(lock_modal_container);
  SetUsesScreenCoordinates(lock_modal_container);

  aura::Window* status_container =
      CreateContainer(kShellWindowId_StatusContainer,
                      "StatusContainer",
                      lock_screen_related_containers);
  SetUsesScreenCoordinates(status_container);
  DescendantShouldStayInSameRootWindow(status_container);

  aura::Window* settings_bubble_container = CreateContainer(
      kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(
      settings_bubble_container);
  SetUsesScreenCoordinates(settings_bubble_container);
  DescendantShouldStayInSameRootWindow(settings_bubble_container);

  aura::Window* menu_container = CreateContainer(
      kShellWindowId_MenuContainer,
      "MenuContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(menu_container);
  SetUsesScreenCoordinates(menu_container);

  aura::Window* drag_drop_container = CreateContainer(
      kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer",
      lock_screen_related_containers);
  views::corewm::SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  SetUsesScreenCoordinates(drag_drop_container);

  aura::Window* overlay_container = CreateContainer(
      kShellWindowId_OverlayContainer,
      "OverlayContainer",
      lock_screen_related_containers);
  SetUsesScreenCoordinates(overlay_container);

  CreateContainer(kShellWindowId_PowerButtonAnimationContainer,
                  "PowerButtonAnimationContainer", root_window) ;
}

void RootWindowController::EnableTouchHudProjection() {
  if (touch_hud_projection_)
    return;
  set_touch_hud_projection(new TouchHudProjection(root_window()));
}

void RootWindowController::DisableTouchHudProjection() {
  if (!touch_hud_projection_)
    return;
  touch_hud_projection_->Remove();
}

void RootWindowController::OnLoginStateChanged(user::LoginStatus status) {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

void RootWindowController::OnTouchHudProjectionToggled(bool enabled) {
  if (enabled)
    EnableTouchHudProjection();
  else
    DisableTouchHudProjection();
}

RootWindowController* GetRootWindowController(
    const aura::Window* root_window) {
  return root_window ? GetRootWindowSettings(root_window)->controller : NULL;
}

}  // namespace internal
}  // namespace ash
