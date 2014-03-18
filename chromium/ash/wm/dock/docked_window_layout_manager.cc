// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/rect.h"
#include "ui/views/background.h"

namespace ash {
namespace internal {

// Minimum, maximum width of the dock area and a width of the gap
// static
const int DockedWindowLayoutManager::kMaxDockWidth = 360;
// static
const int DockedWindowLayoutManager::kMinDockWidth = 200;
// static
const int DockedWindowLayoutManager::kMinDockGap = 2;
// static
const int DockedWindowLayoutManager::kIdealWidth = 250;
const int kMinimumHeight = 250;
const int kSlideDurationMs = 120;
const int kFadeDurationMs = 60;
const int kMinimizeDurationMs = 720;

class DockedBackgroundWidget : public views::Widget,
                               public internal::BackgroundAnimatorDelegate {
 public:
  explicit DockedBackgroundWidget(aura::Window* container)
      : alignment_(DOCKED_ALIGNMENT_NONE),
        background_animator_(this, 0, kLauncherBackgroundAlpha),
        alpha_(0),
        opaque_background_(ui::LAYER_SOLID_COLOR) {
    InitWidget(container);
  }

  // Sets widget bounds and sizes opaque background layer to fill the widget.
  void SetBackgroundBounds(const gfx::Rect bounds, DockedAlignment alignment) {
    SetBounds(bounds);
    opaque_background_.SetBounds(gfx::Rect(bounds.size()));
    alignment_ = alignment;
  }

  // Sets the docked area background type and starts transition animation.
  void SetPaintsBackground(
      ShelfBackgroundType background_type,
      BackgroundAnimatorChangeType change_type) {
    float target_opacity =
        (background_type == SHELF_BACKGROUND_MAXIMIZED) ? 1.0f : 0.0f;
    scoped_ptr<ui::ScopedLayerAnimationSettings> opaque_background_animation;
    if (change_type != BACKGROUND_CHANGE_IMMEDIATE) {
      opaque_background_animation.reset(new ui::ScopedLayerAnimationSettings(
          opaque_background_.GetAnimator()));
      opaque_background_animation->SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kTimeToSwitchBackgroundMs));
    }
    opaque_background_.SetOpacity(target_opacity);

    // TODO(varkha): use ui::Layer on both opaque_background and normal
    // background retire background_animator_ at all. It would be simpler.
    // See also ShelfWidget::SetPaintsBackground.
    background_animator_.SetPaintsBackground(
        background_type != SHELF_BACKGROUND_DEFAULT,
        change_type);
    SchedulePaintInRect(gfx::Rect(GetWindowBoundsInScreen().size()));
  }

  // views::Widget:
  virtual void OnNativeWidgetPaint(gfx::Canvas* canvas) OVERRIDE {
    const gfx::ImageSkia& launcher_background(
        alignment_ == DOCKED_ALIGNMENT_LEFT ?
            launcher_background_left_ : launcher_background_right_);
    gfx::Rect rect = gfx::Rect(GetWindowBoundsInScreen().size());
    SkPaint paint;
    paint.setAlpha(alpha_);
    canvas->DrawImageInt(
        launcher_background,
        0, 0, launcher_background.width(), launcher_background.height(),
        alignment_ == DOCKED_ALIGNMENT_LEFT ?
            rect.width() - launcher_background.width() : 0, 0,
        launcher_background.width(), rect.height(),
        false,
        paint);
    canvas->DrawImageInt(
        launcher_background,
        alignment_ == DOCKED_ALIGNMENT_LEFT ?
            0 : launcher_background.width() - 1, 0,
        1, launcher_background.height(),
        alignment_ == DOCKED_ALIGNMENT_LEFT ?
            0 : launcher_background.width(), 0,
        rect.width() - launcher_background.width(), rect.height(),
        false,
        paint);
  }

  // BackgroundAnimatorDelegate:
  virtual void UpdateBackground(int alpha) OVERRIDE {
    alpha_ = alpha;
    SchedulePaintInRect(gfx::Rect(GetWindowBoundsInScreen().size()));
  }

 private:
  void InitWidget(aura::Window* parent) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.can_activate = false;
    params.keep_on_top = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = parent;
    params.accept_events = false;
    set_focus_on_creation(false);
    Init(params);
    GetNativeWindow()->SetProperty(internal::kStayInSameRootWindowKey, true);
    opaque_background_.SetColor(SK_ColorBLACK);
    opaque_background_.SetBounds(gfx::Rect(GetWindowBoundsInScreen().size()));
    opaque_background_.SetOpacity(0.0f);
    GetNativeWindow()->layer()->Add(&opaque_background_);
    Hide();

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    gfx::ImageSkia launcher_background =
        *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_BACKGROUND);
    launcher_background_left_ = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background, SkBitmapOperations::ROTATION_90_CW);
    launcher_background_right_ = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background, SkBitmapOperations::ROTATION_270_CW);
  }

  DockedAlignment alignment_;

  // The animator for the background transitions.
  internal::BackgroundAnimator background_animator_;

  // The alpha to use for drawing image assets covering the docked background.
  int alpha_;

  // Solid black background that can be made fully opaque.
  ui::Layer opaque_background_;

  // Backgrounds created from shelf background by 90 or 270 degree rotation.
  gfx::ImageSkia launcher_background_left_;
  gfx::ImageSkia launcher_background_right_;

  DISALLOW_COPY_AND_ASSIGN(DockedBackgroundWidget);
};

namespace {

// Returns true if a window is a popup or a transient child.
bool IsPopupOrTransient(const aura::Window* window) {
  return (window->type() == aura::client::WINDOW_TYPE_POPUP ||
          window->transient_parent());
}

// Certain windows (minimized, hidden or popups) do not matter to docking.
bool IsUsedByLayout(const aura::Window* window) {
  return (window->IsVisible() &&
          !wm::GetWindowState(window)->IsMinimized() &&
          !IsPopupOrTransient(window));
}

void UndockWindow(aura::Window* window) {
  gfx::Rect previous_bounds = window->bounds();
  aura::Window* old_parent = window->parent();
  aura::client::ParentWindowWithContext(window, window, gfx::Rect());
  if (window->parent() != old_parent)
    wm::ReparentTransientChildrenOfChild(window, old_parent, window->parent());
  // Start maximize or fullscreen (affecting packaged apps) animation from
  // previous window bounds.
  window->layer()->SetBounds(previous_bounds);
}

// Returns width that is as close as possible to |target_width| while being
// consistent with docked min and max restrictions and respects the |window|'s
// minimum and maximum size.
int GetWindowWidthCloseTo(const aura::Window* window, int target_width) {
  if (!wm::GetWindowState(window)->CanResize()) {
    DCHECK_LE(window->bounds().width(),
              DockedWindowLayoutManager::kMaxDockWidth);
    return window->bounds().width();
  }
  int width = std::max(DockedWindowLayoutManager::kMinDockWidth,
                       std::min(target_width,
                                DockedWindowLayoutManager::kMaxDockWidth));
  if (window->delegate()) {
    if (window->delegate()->GetMinimumSize().width() != 0)
      width = std::max(width, window->delegate()->GetMinimumSize().width());
    if (window->delegate()->GetMaximumSize().width() != 0)
      width = std::min(width, window->delegate()->GetMaximumSize().width());
  }
  DCHECK_LE(width, DockedWindowLayoutManager::kMaxDockWidth);
  return width;
}

// Returns height that is as close as possible to |target_height| while
// respecting the |window|'s minimum and maximum size.
int GetWindowHeightCloseTo(const aura::Window* window, int target_height) {
  if (!wm::GetWindowState(window)->CanResize())
    return window->bounds().height();
  int minimum_height = kMinimumHeight;
  int maximum_height = 0;
  const aura::WindowDelegate* delegate(window->delegate());
  if (delegate) {
    if (delegate->GetMinimumSize().height() != 0) {
      minimum_height = std::max(kMinimumHeight,
                                delegate->GetMinimumSize().height());
    }
    if (delegate->GetMaximumSize().height() != 0)
      maximum_height = delegate->GetMaximumSize().height();
  }
  if (minimum_height)
    target_height = std::max(target_height, minimum_height);
  if (maximum_height)
    target_height = std::min(target_height, maximum_height);
  return target_height;
}

// A functor used to sort the windows in order of their minimum height.
struct CompareMinimumHeight {
  bool operator()(WindowWithHeight win1, WindowWithHeight win2) {
    return GetWindowHeightCloseTo(win1.window(), 0) <
        GetWindowHeightCloseTo(win2.window(), 0);
  }
};

// A functor used to sort the windows in order of their center Y position.
// |delta| is a pre-calculated distance from the bottom of one window to the top
// of the next. Its value can be positive (gap) or negative (overlap).
// Half of |delta| is used as a transition point at which windows could ideally
// swap positions.
struct CompareWindowPos {
  CompareWindowPos(aura::Window* dragged_window, float delta)
      : dragged_window_(dragged_window),
        delta_(delta / 2) {}

  bool operator()(WindowWithHeight window_with_height1,
                  WindowWithHeight window_with_height2) {
    // Use target coordinates since animations may be active when windows are
    // reordered.
    aura::Window* win1(window_with_height1.window());
    aura::Window* win2(window_with_height2.window());
    gfx::Rect win1_bounds = ScreenAsh::ConvertRectToScreen(
        win1->parent(), win1->GetTargetBounds());
    gfx::Rect win2_bounds = ScreenAsh::ConvertRectToScreen(
        win2->parent(), win2->GetTargetBounds());
    win1_bounds.set_height(window_with_height1.height_);
    win2_bounds.set_height(window_with_height2.height_);
    // If one of the windows is the |dragged_window_| attempt to make an
    // earlier swap between the windows than just based on their centers.
    // This is possible if the dragged window is at least as tall as the other
    // window.
    if (win1 == dragged_window_)
      return compare_two_windows(win1_bounds, win2_bounds);
    if (win2 == dragged_window_)
      return !compare_two_windows(win2_bounds, win1_bounds);
    // Otherwise just compare the centers.
    return win1_bounds.CenterPoint().y() < win2_bounds.CenterPoint().y();
  }

  // Based on center point tries to deduce where the drag is coming from.
  // When dragging from below up the transition point is lower.
  // When dragging from above down the transition point is higher.
  bool compare_bounds(const gfx::Rect dragged, const gfx::Rect other) {
    if (dragged.CenterPoint().y() < other.CenterPoint().y())
      return dragged.CenterPoint().y() < other.y() - delta_;
    return dragged.CenterPoint().y() < other.bottom() + delta_;
  }

  // Performs comparison both ways and selects stable result.
  bool compare_two_windows(const gfx::Rect bounds1, const gfx::Rect bounds2) {
    // Try comparing windows in both possible orders and see if the comparison
    // is stable.
    bool result1 = compare_bounds(bounds1, bounds2);
    bool result2 = compare_bounds(bounds2, bounds1);
    if (result1 != result2)
      return result1;

    // Otherwise it is not possible to be sure that the windows will not bounce.
    // In this case just compare the centers.
    return bounds1.CenterPoint().y() < bounds2.CenterPoint().y();
  }

 private:
  aura::Window* dragged_window_;
  float delta_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// A class that observes launcher shelf for bounds changes.
class DockedWindowLayoutManager::ShelfWindowObserver : public WindowObserver {
 public:
  explicit ShelfWindowObserver(
      DockedWindowLayoutManager* docked_layout_manager)
      : docked_layout_manager_(docked_layout_manager) {
    DCHECK(docked_layout_manager_->launcher()->shelf_widget());
    docked_layout_manager_->launcher()->shelf_widget()->GetNativeView()
        ->AddObserver(this);
  }

  virtual ~ShelfWindowObserver() {
    if (docked_layout_manager_->launcher() &&
        docked_layout_manager_->launcher()->shelf_widget())
      docked_layout_manager_->launcher()->shelf_widget()->GetNativeView()
          ->RemoveObserver(this);
  }

  // aura::WindowObserver:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    shelf_bounds_in_screen_ = ScreenAsh::ConvertRectToScreen(
        window->parent(), new_bounds);
    docked_layout_manager_->OnShelfBoundsChanged();
  }

  const gfx::Rect& shelf_bounds_in_screen() const {
    return shelf_bounds_in_screen_;
  }

 private:
  DockedWindowLayoutManager* docked_layout_manager_;
  gfx::Rect shelf_bounds_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager public implementation:
DockedWindowLayoutManager::DockedWindowLayoutManager(
    aura::Window* dock_container, WorkspaceController* workspace_controller)
    : dock_container_(dock_container),
      in_layout_(false),
      dragged_window_(NULL),
      is_dragged_window_docked_(false),
      is_dragged_from_dock_(false),
      launcher_(NULL),
      workspace_controller_(workspace_controller),
      in_fullscreen_(workspace_controller_->GetWindowState() ==
          WORKSPACE_WINDOW_STATE_FULL_SCREEN),
      docked_width_(0),
      alignment_(DOCKED_ALIGNMENT_NONE),
      last_active_window_(NULL),
      last_action_time_(base::Time::Now()),
      background_widget_(new DockedBackgroundWidget(dock_container_)) {
  DCHECK(dock_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

DockedWindowLayoutManager::~DockedWindowLayoutManager() {
  Shutdown();
}

void DockedWindowLayoutManager::Shutdown() {
  if (launcher_ && launcher_->shelf_widget()) {
    ShelfLayoutManager* shelf_layout_manager = ShelfLayoutManager::ForLauncher(
        launcher_->shelf_widget()->GetNativeWindow());
    shelf_layout_manager->RemoveObserver(this);
    shelf_observer_.reset();
  }
  launcher_ = NULL;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* child = dock_container_->children()[i];
    child->RemoveObserver(this);
    wm::GetWindowState(child)->RemoveObserver(this);
  }
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void DockedWindowLayoutManager::AddObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DockedWindowLayoutManager::RemoveObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DockedWindowLayoutManager::StartDragging(aura::Window* window) {
  DCHECK(!dragged_window_);
  dragged_window_ = window;
  DCHECK(!IsPopupOrTransient(window));
  // Start observing a window unless it is docked container's child in which
  // case it is already observed.
  if (dragged_window_->parent() != dock_container_) {
    dragged_window_->AddObserver(this);
    wm::GetWindowState(dragged_window_)->AddObserver(this);
  }
  is_dragged_from_dock_ = window->parent() == dock_container_;
  DCHECK(!is_dragged_window_docked_);
}

void DockedWindowLayoutManager::DockDraggedWindow(aura::Window* window) {
  DCHECK(!IsPopupOrTransient(window));
  OnDraggedWindowDocked(window);
  Relayout();
}

void DockedWindowLayoutManager::UndockDraggedWindow() {
  DCHECK(!IsPopupOrTransient(dragged_window_));
  OnDraggedWindowUndocked();
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
  is_dragged_from_dock_ = false;
}

void DockedWindowLayoutManager::FinishDragging(DockedAction action,
                                               DockedActionSource source) {
  DCHECK(dragged_window_);
  DCHECK(!IsPopupOrTransient(dragged_window_));
  if (is_dragged_window_docked_)
    OnDraggedWindowUndocked();
  DCHECK (!is_dragged_window_docked_);
  // Stop observing a window unless it is docked container's child in which
  // case it needs to keep being observed after the drag completes.
  if (dragged_window_->parent() != dock_container_) {
    dragged_window_->RemoveObserver(this);
    wm::GetWindowState(dragged_window_)->RemoveObserver(this);
    if (last_active_window_ == dragged_window_)
      last_active_window_ = NULL;
  } else {
    // A window is no longer dragged and is a child.
    // When a window becomes a child at drag start this is
    // the only opportunity we will have to enforce a window
    // count limit so do it here.
    MaybeMinimizeChildrenExcept(dragged_window_);
  }
  dragged_window_ = NULL;
  dragged_bounds_ = gfx::Rect();
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
  RecordUmaAction(action, source);
}

void DockedWindowLayoutManager::SetLauncher(ash::Launcher* launcher) {
  DCHECK(!launcher_);
  launcher_ = launcher;
  if (launcher_->shelf_widget()) {
    ShelfLayoutManager* shelf_layout_manager = ShelfLayoutManager::ForLauncher(
        launcher_->shelf_widget()->GetNativeWindow());
    shelf_layout_manager->AddObserver(this);
    shelf_observer_.reset(new ShelfWindowObserver(this));
  }
}

DockedAlignment DockedWindowLayoutManager::GetAlignmentOfWindow(
    const aura::Window* window) const {
  const gfx::Rect& bounds(window->GetBoundsInScreen());

  // Test overlap with an existing docked area first.
  if (docked_bounds_.Intersects(bounds) &&
      alignment_ != DOCKED_ALIGNMENT_NONE) {
    // A window is being added to other docked windows (on the same side).
    return alignment_;
  }

  const gfx::Rect container_bounds = dock_container_->GetBoundsInScreen();
  if (bounds.x() <= container_bounds.x() &&
      bounds.right() > container_bounds.x()) {
    return DOCKED_ALIGNMENT_LEFT;
  } else if (bounds.x() < container_bounds.right() &&
             bounds.right() >= container_bounds.right()) {
    return DOCKED_ALIGNMENT_RIGHT;
  }
  return DOCKED_ALIGNMENT_NONE;
}

DockedAlignment DockedWindowLayoutManager::CalculateAlignment() const {
  // Find a child that is not being dragged and is not a popup.
  // If such exists the current alignment is returned - even if some of the
  // children are hidden or minimized (so they can be restored without losing
  // the docked state).
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);
    if (window != dragged_window_ && !IsPopupOrTransient(window))
      return alignment_;
  }
  // No docked windows remain other than possibly the window being dragged.
  // Return |NONE| to indicate that windows may get docked on either side.
  return DOCKED_ALIGNMENT_NONE;
}

bool DockedWindowLayoutManager::CanDockWindow(aura::Window* window,
                                              SnapType edge) {
  if (!switches::UseDockedWindows())
    return false;
  // Don't allow interactive docking of windows with transient parents such as
  // modal browser dialogs.
  if (IsPopupOrTransient(window))
    return false;
  // If a window is wide and cannot be resized down to maximum width allowed
  // then it cannot be docked.
  // TODO(varkha). Prevent windows from changing size programmatically while
  // they are docked. The size will take effect only once a window is undocked.
  // See http://crbug.com/307792.
  if (window->bounds().width() > kMaxDockWidth &&
      (!wm::GetWindowState(window)->CanResize() ||
       (window->delegate() &&
        window->delegate()->GetMinimumSize().width() != 0 &&
        window->delegate()->GetMinimumSize().width() > kMaxDockWidth))) {
    return false;
  }
  // If a window is tall and cannot be resized down to maximum height allowed
  // then it cannot be docked.
  const gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(dock_container_).work_area();
  if (GetWindowHeightCloseTo(window, work_area.height() - 2 * kMinDockGap) >
      work_area.height() - 2 * kMinDockGap) {
    return false;
  }
  // Cannot dock on the other size from an existing dock.
  const DockedAlignment alignment = CalculateAlignment();
  if ((edge == SNAP_LEFT && alignment == DOCKED_ALIGNMENT_RIGHT) ||
      (edge == SNAP_RIGHT && alignment == DOCKED_ALIGNMENT_LEFT)) {
    return false;
  }
  // Do not allow docking on the same side as launcher shelf.
  ShelfAlignment shelf_alignment = SHELF_ALIGNMENT_BOTTOM;
  if (launcher_)
    shelf_alignment = launcher_->alignment();
  if ((edge == SNAP_LEFT && shelf_alignment == SHELF_ALIGNMENT_LEFT) ||
      (edge == SNAP_RIGHT && shelf_alignment == SHELF_ALIGNMENT_RIGHT)) {
    return false;
  }
  return true;
}

void DockedWindowLayoutManager::OnShelfBoundsChanged() {
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::DISPLAY_INSETS_CHANGED);
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, aura::LayoutManager implementation:
void DockedWindowLayoutManager::OnWindowResized() {
  MaybeMinimizeChildrenExcept(dragged_window_);
  Relayout();
  // When screen resizes update the insets even when dock width or alignment
  // does not change.
  UpdateDockBounds(DockedWindowLayoutManagerObserver::DISPLAY_RESIZED);
}

void DockedWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (IsPopupOrTransient(child))
    return;
  // Dragged windows are already observed by StartDragging and do not change
  // docked alignment during the drag.
  if (child == dragged_window_)
    return;
  // If this is the first window getting docked - update alignment.
  if (alignment_ == DOCKED_ALIGNMENT_NONE) {
    alignment_ = GetAlignmentOfWindow(child);
    DCHECK(alignment_ != DOCKED_ALIGNMENT_NONE);
  }
  MaybeMinimizeChildrenExcept(child);
  child->AddObserver(this);
  wm::GetWindowState(child)->AddObserver(this);
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  if (IsPopupOrTransient(child))
    return;
  // Dragged windows are stopped being observed by FinishDragging and do not
  // change alignment during the drag. They also cannot be set to be the
  // |last_active_window_|.
  if (child == dragged_window_)
    return;
  // If this is the last window, set alignment and maximize the workspace.
  if (!IsAnyWindowDocked()) {
    alignment_ = DOCKED_ALIGNMENT_NONE;
    UpdateDockedWidth(0);
  }
  if (last_active_window_ == child)
    last_active_window_ = NULL;
  child->RemoveObserver(this);
  wm::GetWindowState(child)->RemoveObserver(this);
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (IsPopupOrTransient(child))
    return;
  if (visible)
    wm::GetWindowState(child)->Restore();
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Whenever one of our windows is moved or resized enforce layout.
  SetChildBoundsDirect(child, requested_bounds);
  if (IsPopupOrTransient(child))
    return;
  ShelfLayoutManager* shelf_layout = internal::ShelfLayoutManager::ForLauncher(
      dock_container_);
  if (shelf_layout)
    shelf_layout->UpdateVisibilityState();
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, ash::ShellObserver implementation:

void DockedWindowLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::DISPLAY_INSETS_CHANGED);
  MaybeMinimizeChildrenExcept(dragged_window_);
}

void DockedWindowLayoutManager::OnFullscreenStateChanged(
    bool is_fullscreen, aura::Window* root_window) {
  if (dock_container_->GetRootWindow() != root_window)
    return;
  // Entering fullscreen mode (including immersive) hides docked windows.
  in_fullscreen_ = workspace_controller_->GetWindowState() ==
      WORKSPACE_WINDOW_STATE_FULL_SCREEN;
  {
    // prevent Relayout from getting called multiple times during this
    base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
    // Use a copy of children array because a call to MinimizeDockedWindow or
    // RestoreDockedWindow can change order.
    aura::Window::Windows children(dock_container_->children());
    for (aura::Window::Windows::const_iterator iter = children.begin();
         iter != children.end(); ++iter) {
      aura::Window* window(*iter);
      if (IsPopupOrTransient(window))
        continue;
      wm::WindowState* window_state = wm::GetWindowState(window);
      if (in_fullscreen_) {
        if (window->IsVisible())
          MinimizeDockedWindow(window_state);
      } else {
        if (!window_state->IsMinimized())
          RestoreDockedWindow(window_state);
      }
    }
  }
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::OnShelfAlignmentChanged(
    aura::Window* root_window) {
  if (dock_container_->GetRootWindow() != root_window)
    return;

  if (!launcher_ || !launcher_->shelf_widget())
    return;

  if (alignment_ == DOCKED_ALIGNMENT_NONE)
    return;

  // Do not allow launcher and dock on the same side. Switch side that
  // the dock is attached to and move all dock windows to that new side.
  ShelfAlignment shelf_alignment = launcher_->shelf_widget()->GetAlignment();
  if (alignment_ == DOCKED_ALIGNMENT_LEFT &&
      shelf_alignment == SHELF_ALIGNMENT_LEFT) {
    alignment_ = DOCKED_ALIGNMENT_RIGHT;
  } else if (alignment_ == DOCKED_ALIGNMENT_RIGHT &&
             shelf_alignment == SHELF_ALIGNMENT_RIGHT) {
    alignment_ = DOCKED_ALIGNMENT_LEFT;
  }
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::SHELF_ALIGNMENT_CHANGED);
}

/////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, ShelfLayoutManagerObserver implementation:
void DockedWindowLayoutManager::OnBackgroundUpdated(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  background_widget_->SetPaintsBackground(background_type, change_type);
}

/////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, WindowStateObserver implementation:

void DockedWindowLayoutManager::OnWindowShowTypeChanged(
    wm::WindowState* window_state,
    wm::WindowShowType old_type) {
  aura::Window* window = window_state->window();
  if (IsPopupOrTransient(window))
    return;
  // The window property will still be set, but no actual change will occur
  // until OnFullscreenStateChange is called when exiting fullscreen.
  if (in_fullscreen_)
    return;
  if (window_state->IsMinimized()) {
    MinimizeDockedWindow(window_state);
  } else if (window_state->IsMaximizedOrFullscreen() ||
             window_state->IsSnapped()) {
    if (window != dragged_window_) {
      UndockWindow(window);
      RecordUmaAction(DOCKED_ACTION_MAXIMIZE, DOCKED_ACTION_SOURCE_UNKNOWN);
    }
  } else if (old_type == wm::SHOW_TYPE_MINIMIZED) {
    RestoreDockedWindow(window_state);
  }
}

/////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, WindowObserver implementation:

void DockedWindowLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // Only relayout if the dragged window would get docked.
  if (window == dragged_window_ && is_dragged_window_docked_)
    Relayout();
}

void DockedWindowLayoutManager::OnWindowVisibilityChanging(
    aura::Window* window, bool visible) {
  if (IsPopupOrTransient(window))
    return;
  int animation_type = views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT;
  if (visible) {
    animation_type = views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_DROP;
    views::corewm::SetWindowVisibilityAnimationDuration(
        window, base::TimeDelta::FromMilliseconds(kFadeDurationMs));
  } else if (wm::GetWindowState(window)->IsMinimized()) {
    animation_type = WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE;
  }
  views::corewm::SetWindowVisibilityAnimationType(window, animation_type);
}

void DockedWindowLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (dragged_window_ == window) {
    FinishDragging(DOCKED_ACTION_NONE, DOCKED_ACTION_SOURCE_UNKNOWN);
    DCHECK(!dragged_window_);
    DCHECK (!is_dragged_window_docked_);
  }
  if (window == last_active_window_)
    last_active_window_ = NULL;
  RecordUmaAction(DOCKED_ACTION_CLOSE, DOCKED_ACTION_SOURCE_UNKNOWN);
}


////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, aura::client::ActivationChangeObserver
// implementation:

void DockedWindowLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  if (gained_active && IsPopupOrTransient(gained_active))
    return;
  // Ignore if the window that is not managed by this was activated.
  aura::Window* ancestor = NULL;
  for (aura::Window* parent = gained_active;
       parent; parent = parent->parent()) {
    if (parent->parent() == dock_container_) {
      ancestor = parent;
      break;
    }
  }
  if (ancestor)
    UpdateStacking(ancestor);
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager private implementation:

void DockedWindowLayoutManager::MaybeMinimizeChildrenExcept(
    aura::Window* child) {
  // Minimize any windows that don't fit without overlap.
  const gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(dock_container_).work_area();
  int available_room = work_area.height() - kMinDockGap;
  if (child)
    available_room -= (GetWindowHeightCloseTo(child, 0) + kMinDockGap);
  // Use a copy of children array because a call to Minimize can change order.
  aura::Window::Windows children(dock_container_->children());
  aura::Window::Windows::const_reverse_iterator iter = children.rbegin();
  while (iter != children.rend()) {
    aura::Window* window(*iter++);
    if (window == child || !IsUsedByLayout(window))
      continue;
    int room_needed = GetWindowHeightCloseTo(window, 0) + kMinDockGap;
    if (available_room > room_needed) {
      available_room -= room_needed;
    } else {
      // Slow down minimizing animations. Lock duration so that it is not
      // overridden by other ScopedLayerAnimationSettings down the stack.
      ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
      settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kMinimizeDurationMs));
      settings.LockTransitionDuration();
      wm::GetWindowState(window)->Minimize();
    }
  }
}

void DockedWindowLayoutManager::MinimizeDockedWindow(
    wm::WindowState* window_state) {
  DCHECK(!IsPopupOrTransient(window_state->window()));
  window_state->window()->Hide();
  if (window_state->IsActive())
    window_state->Deactivate();
  RecordUmaAction(DOCKED_ACTION_MINIMIZE, DOCKED_ACTION_SOURCE_UNKNOWN);
}

void DockedWindowLayoutManager::RestoreDockedWindow(
    wm::WindowState* window_state) {
  aura::Window* window = window_state->window();
  DCHECK(!IsPopupOrTransient(window));
  // Always place restored window at the bottom shuffling the other windows up.
  // TODO(varkha): add a separate container for docked windows to keep track
  // of ordering.
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      dock_container_);
  const gfx::Rect work_area = display.work_area();

  // Evict the window if it can no longer be docked because of its height.
  if (!CanDockWindow(window, SNAP_NONE)) {
    UndockWindow(window);
    RecordUmaAction(DOCKED_ACTION_EVICT, DOCKED_ACTION_SOURCE_UNKNOWN);
    return;
  }
  gfx::Rect bounds(window->bounds());
  bounds.set_y(work_area.bottom());
  window->SetBounds(bounds);
  window->Show();
  MaybeMinimizeChildrenExcept(window);
  RecordUmaAction(DOCKED_ACTION_RESTORE, DOCKED_ACTION_SOURCE_UNKNOWN);
}

void DockedWindowLayoutManager::RecordUmaAction(DockedAction action,
                                                DockedActionSource source) {
  if (action == DOCKED_ACTION_NONE)
    return;
  UMA_HISTOGRAM_ENUMERATION("Ash.Dock.Action", action, DOCKED_ACTION_COUNT);
  UMA_HISTOGRAM_ENUMERATION("Ash.Dock.ActionSource", source,
                            DOCKED_ACTION_SOURCE_COUNT);
  base::Time time_now = base::Time::Now();
  base::TimeDelta time_between_use = time_now - last_action_time_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.Dock.TimeBetweenUse",
                              time_between_use.InSeconds(),
                              1,
                              base::TimeDelta::FromHours(10).InSeconds(),
                              100);
  last_action_time_ = time_now;
  int docked_all_count = 0;
  int docked_visible_count = 0;
  int docked_panels_count = 0;
  int large_windows_count = 0;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    const aura::Window* window(dock_container_->children()[i]);
    if (IsPopupOrTransient(window))
      continue;
    docked_all_count++;
    if (!IsUsedByLayout(window))
      continue;
    docked_visible_count++;
    if (window->type() == aura::client::WINDOW_TYPE_PANEL)
      docked_panels_count++;
    const wm::WindowState* window_state = wm::GetWindowState(window);
    if (window_state->HasRestoreBounds()) {
      const gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();
      if (restore_bounds.width() > kMaxDockWidth)
        large_windows_count++;
    }
  }
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsAll", docked_all_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsLarge", large_windows_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsPanels", docked_panels_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsVisible", docked_visible_count);
}

void DockedWindowLayoutManager::UpdateDockedWidth(int width) {
  if (docked_width_ == width)
    return;
  docked_width_ = width;
  UMA_HISTOGRAM_COUNTS_10000("Ash.Dock.Width", docked_width_);
}

void DockedWindowLayoutManager::OnDraggedWindowDocked(aura::Window* window) {
  DCHECK(!is_dragged_window_docked_);
  is_dragged_window_docked_ = true;

  // If there are no other docked windows update alignment.
  if (!IsAnyWindowDocked())
    alignment_ = DOCKED_ALIGNMENT_NONE;
}

void DockedWindowLayoutManager::OnDraggedWindowUndocked() {
  // If this is the first window getting docked - update alignment.
  if (!IsAnyWindowDocked())
    alignment_ = GetAlignmentOfWindow(dragged_window_);

  DCHECK (is_dragged_window_docked_);
  is_dragged_window_docked_ = false;
}

bool DockedWindowLayoutManager::IsAnyWindowDocked() {
  return CalculateAlignment() != DOCKED_ALIGNMENT_NONE;
}

void DockedWindowLayoutManager::Relayout() {
  if (in_layout_)
    return;
  if (alignment_ == DOCKED_ALIGNMENT_NONE && !is_dragged_window_docked_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  gfx::Rect dock_bounds = dock_container_->GetBoundsInScreen();
  aura::Window* active_window = NULL;
  std::vector<WindowWithHeight> visible_windows;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);

    if (!IsUsedByLayout(window) || window == dragged_window_)
      continue;

    // If the shelf is currently hidden (full-screen mode), hide window until
    // full-screen mode is exited.
    if (in_fullscreen_) {
      // The call to Hide does not set the minimize property, so the window will
      // be restored when the shelf becomes visible again.
      window->Hide();
      continue;
    }
    if (window->HasFocus() ||
        window->Contains(
            aura::client::GetFocusClient(window)->GetFocusedWindow())) {
      DCHECK(!active_window);
      active_window = window;
    }
    visible_windows.push_back(WindowWithHeight(window));
  }
  // Consider docked dragged_window_ when fanning out other child windows.
  if (is_dragged_window_docked_) {
    visible_windows.push_back(WindowWithHeight(dragged_window_));
    DCHECK(!active_window);
    active_window = dragged_window_;
  }

  // Position docked windows as well as the window being dragged.
  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(dock_container_).work_area();
  if (shelf_observer_)
    work_area.Subtract(shelf_observer_->shelf_bounds_in_screen());
  int available_room = CalculateWindowHeightsAndRemainingRoom(work_area,
                                                              &visible_windows);
  FanOutChildren(work_area,
                 CalculateIdealWidth(visible_windows),
                 available_room,
                 &visible_windows);

  // After the first Relayout allow the windows to change their order easier
  // since we know they are docked.
  is_dragged_from_dock_ = true;
  UpdateStacking(active_window);
}

int DockedWindowLayoutManager::CalculateWindowHeightsAndRemainingRoom(
    const gfx::Rect work_area,
    std::vector<WindowWithHeight>* visible_windows) {
  int available_room = work_area.height() - kMinDockGap;
  int remaining_windows = visible_windows->size();

  // Sort windows by their minimum heights and calculate target heights.
  std::sort(visible_windows->begin(), visible_windows->end(),
            CompareMinimumHeight());
  // Distribute the free space among the docked windows. Since the windows are
  // sorted (tall windows first) we can now assume that any window which
  // required more space than the current window will have already been
  // accounted for previously in this loop, so we can safely give that window
  // its proportional share of the remaining space.
  for (std::vector<WindowWithHeight>::reverse_iterator iter =
           visible_windows->rbegin();
      iter != visible_windows->rend(); ++iter) {
    iter->height_ = GetWindowHeightCloseTo(
        iter->window(), available_room / remaining_windows - kMinDockGap);
    available_room -= (iter->height_ + kMinDockGap);
    remaining_windows--;
  }
  return available_room;
}

int DockedWindowLayoutManager::CalculateIdealWidth(
    const std::vector<WindowWithHeight>& visible_windows) {
  int smallest_max_width = kMaxDockWidth;
  int largest_min_width = kMinDockWidth;
  // Ideal width of the docked area is as close to kIdealWidth as possible
  // while still respecting the minimum and maximum width restrictions on the
  // individual docked windows as well as the width that was possibly set by a
  // user (which needs to be preserved when dragging and rearranging windows).
  for (std::vector<WindowWithHeight>::const_iterator iter =
           visible_windows.begin();
       iter != visible_windows.end(); ++iter) {
    const aura::Window* window = iter->window();
    int min_window_width = window->bounds().width();
    int max_window_width = min_window_width;
    if (!wm::GetWindowState(window)->bounds_changed_by_user()) {
      min_window_width = GetWindowWidthCloseTo(window, kMinDockWidth);
      max_window_width = GetWindowWidthCloseTo(window, kMaxDockWidth);
    }
    largest_min_width = std::max(largest_min_width, min_window_width);
    smallest_max_width = std::min(smallest_max_width, max_window_width);
  }
  int ideal_width = std::max(largest_min_width,
                             std::min(smallest_max_width, kIdealWidth));
  // Restrict docked area width regardless of window restrictions.
  ideal_width = std::max(std::min(ideal_width, kMaxDockWidth), kMinDockWidth);
  return ideal_width;
}

void DockedWindowLayoutManager::FanOutChildren(
    const gfx::Rect& work_area,
    int ideal_docked_width,
    int available_room,
    std::vector<WindowWithHeight>* visible_windows) {
  gfx::Rect dock_bounds = dock_container_->GetBoundsInScreen();

  // Calculate initial vertical offset and the gap or overlap between windows.
  const int num_windows = visible_windows->size();
  const float delta = kMinDockGap + (float)available_room /
      ((available_room > 0 || num_windows <= 1) ?
          num_windows + 1 : num_windows - 1);
  float y_pos = work_area.y() + ((delta > 0) ? delta : kMinDockGap);

  // Docked area is shown only if there is at least one non-dragged visible
  // docked window.
  int new_width = ideal_docked_width;
  if (visible_windows->empty() ||
      (visible_windows->size() == 1 &&
          (*visible_windows)[0].window() == dragged_window_)) {
    new_width = 0;
  }
  UpdateDockedWidth(new_width);
  // Sort windows by their center positions and fan out overlapping
  // windows.
  std::sort(visible_windows->begin(), visible_windows->end(),
            CompareWindowPos(is_dragged_from_dock_ ? dragged_window_ : NULL,
                             delta));
  for (std::vector<WindowWithHeight>::iterator iter = visible_windows->begin();
       iter != visible_windows->end(); ++iter) {
    aura::Window* window = iter->window();
    gfx::Rect bounds = ScreenAsh::ConvertRectToScreen(
        window->parent(), window->GetTargetBounds());
    // A window is extended or shrunk to be as close as possible to the ideal
    // docked area width. Windows that were resized by a user are kept at their
    // existing size.
    // This also enforces the min / max restrictions on the docked area width.
    bounds.set_width(GetWindowWidthCloseTo(
        window,
        wm::GetWindowState(window)->bounds_changed_by_user() ?
            bounds.width() : ideal_docked_width));
    DCHECK_LE(bounds.width(), ideal_docked_width);

    DockedAlignment alignment = alignment_;
    if (alignment == DOCKED_ALIGNMENT_NONE && window == dragged_window_) {
      alignment = GetAlignmentOfWindow(window);
      if (alignment == DOCKED_ALIGNMENT_NONE)
        bounds.set_size(gfx::Size());
    }

    // Fan out windows evenly distributing the overlap or remaining free space.
    bounds.set_height(iter->height_);
    bounds.set_y(std::max(work_area.y(),
                          std::min(work_area.bottom() - bounds.height(),
                                   static_cast<int>(y_pos + 0.5))));
    y_pos += bounds.height() + delta;

    // All docked windows other than the one currently dragged remain stuck
    // to the screen edge (flush with the edge or centered in the dock area).
    switch (alignment) {
      case DOCKED_ALIGNMENT_LEFT:
        bounds.set_x(dock_bounds.x() +
                     (ideal_docked_width - bounds.width()) / 2);
        break;
      case DOCKED_ALIGNMENT_RIGHT:
        bounds.set_x(dock_bounds.right() -
                     (ideal_docked_width + bounds.width()) / 2);
        break;
      case DOCKED_ALIGNMENT_NONE:
        break;
    }
    if (window == dragged_window_) {
      dragged_bounds_ = bounds;
      continue;
    }
    // If the following asserts it is probably because not all the children
    // have been removed when dock was closed.
    DCHECK_NE(alignment_, DOCKED_ALIGNMENT_NONE);
    bounds = ScreenAsh::ConvertRectFromScreen(dock_container_, bounds);
    if (bounds != window->GetTargetBounds()) {
      ui::Layer* layer = window->layer();
      ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
      slide_settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      slide_settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kSlideDurationMs));
      SetChildBoundsDirect(window, bounds);
    }
  }
}

void DockedWindowLayoutManager::UpdateDockBounds(
    DockedWindowLayoutManagerObserver::Reason reason) {
  int dock_inset = docked_width_ + (docked_width_ > 0 ? kMinDockGap : 0);
  const gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(dock_container_).work_area();
  gfx::Rect bounds = gfx::Rect(
      alignment_ == DOCKED_ALIGNMENT_RIGHT && dock_inset > 0 ?
          dock_container_->bounds().right() - dock_inset:
          dock_container_->bounds().x(),
      dock_container_->bounds().y(),
      dock_inset,
      work_area.height());
  docked_bounds_ = bounds +
      dock_container_->GetBoundsInScreen().OffsetFromOrigin();
  FOR_EACH_OBSERVER(
      DockedWindowLayoutManagerObserver,
      observer_list_,
      OnDockBoundsChanging(bounds, reason));
  // Show or hide background for docked area.
  gfx::Rect background_bounds(docked_bounds_);
  if (shelf_observer_)
    background_bounds.Subtract(shelf_observer_->shelf_bounds_in_screen());
  background_widget_->SetBackgroundBounds(background_bounds, alignment_);
  if (docked_width_ > 0)
    background_widget_->Show();
  else
    background_widget_->Hide();
}

void DockedWindowLayoutManager::UpdateStacking(aura::Window* active_window) {
  if (!active_window) {
    if (!last_active_window_)
      return;
    active_window = last_active_window_;
  }

  // Windows are stacked like a deck of cards:
  //  ,------.
  // |,------.|
  // |,------.|
  // | active |
  // | window |
  // |`------'|
  // |`------'|
  //  `------'
  // Use the middle of each window to figure out how to stack the window.
  // This allows us to update the stacking when a window is being dragged around
  // by the titlebar.
  std::map<int, aura::Window*> window_ordering;
  for (aura::Window::Windows::const_iterator it =
           dock_container_->children().begin();
       it != dock_container_->children().end(); ++it) {
    if (!IsUsedByLayout(*it) ||
        ((*it) == dragged_window_ && !is_dragged_window_docked_)) {
      continue;
    }
    gfx::Rect bounds = (*it)->bounds();
    window_ordering.insert(std::make_pair(bounds.y() + bounds.height() / 2,
                                          *it));
  }
  int active_center_y = active_window->bounds().CenterPoint().y();

  aura::Window* previous_window = NULL;
  for (std::map<int, aura::Window*>::const_iterator it =
       window_ordering.begin();
       it != window_ordering.end() && it->first < active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }
  for (std::map<int, aura::Window*>::const_reverse_iterator it =
       window_ordering.rbegin();
       it != window_ordering.rend() && it->first > active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }

  if (previous_window && active_window->parent() == dock_container_)
    dock_container_->StackChildAbove(active_window, previous_window);
  if (active_window != dragged_window_)
    last_active_window_ = active_window;
}

////////////////////////////////////////////////////////////////////////////////
// keyboard::KeyboardControllerObserver implementation:

void DockedWindowLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& keyboard_bounds) {
  // This bounds change will have caused a change to the Shelf which does not
  // propagate automatically to this class, so manually recalculate bounds.
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::KEYBOARD_BOUNDS_CHANGING);
}

}  // namespace internal
}  // namespace ash
