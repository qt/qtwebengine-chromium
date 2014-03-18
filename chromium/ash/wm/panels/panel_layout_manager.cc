// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_layout_manager.h"

#include <algorithm>
#include <map>

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {
const int kPanelIdealSpacing = 4;

const float kMaxHeightFactor = .80f;
const float kMaxWidthFactor = .50f;

// Duration for panel animations.
const int kPanelSlideDurationMilliseconds = 50;
const int kCalloutFadeDurationMilliseconds = 50;

// Offset used when sliding panel in/out of the launcher. Used for minimizing,
// restoring and the initial showing of a panel.
const int kPanelSlideInOffset = 20;

// Callout arrow dimensions.
const int kArrowWidth = 18;
const int kArrowHeight = 9;

class CalloutWidgetBackground : public views::Background {
 public:
  CalloutWidgetBackground() : alignment_(SHELF_ALIGNMENT_BOTTOM) {
  }

  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPath path;
    switch (alignment_) {
      case SHELF_ALIGNMENT_BOTTOM:
        path.moveTo(SkIntToScalar(0), SkIntToScalar(0));
        path.lineTo(SkIntToScalar(kArrowWidth / 2),
                    SkIntToScalar(kArrowHeight));
        path.lineTo(SkIntToScalar(kArrowWidth), SkIntToScalar(0));
        break;
      case SHELF_ALIGNMENT_LEFT:
        path.moveTo(SkIntToScalar(kArrowHeight), SkIntToScalar(kArrowWidth));
        path.lineTo(SkIntToScalar(0), SkIntToScalar(kArrowWidth / 2));
        path.lineTo(SkIntToScalar(kArrowHeight), SkIntToScalar(0));
        break;
      case SHELF_ALIGNMENT_TOP:
        path.moveTo(SkIntToScalar(0), SkIntToScalar(kArrowHeight));
        path.lineTo(SkIntToScalar(kArrowWidth / 2), SkIntToScalar(0));
        path.lineTo(SkIntToScalar(kArrowWidth), SkIntToScalar(kArrowHeight));
        break;
      case SHELF_ALIGNMENT_RIGHT:
        path.moveTo(SkIntToScalar(0), SkIntToScalar(0));
        path.lineTo(SkIntToScalar(kArrowHeight),
                    SkIntToScalar(kArrowWidth / 2));
        path.lineTo(SkIntToScalar(0), SkIntToScalar(kArrowWidth));
        break;
    }
    // Hard code the arrow color for now.
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetARGB(0xff, 0xe5, 0xe5, 0xe5));
    canvas->DrawPath(path, paint);
  }

  ShelfAlignment alignment() {
    return alignment_;
  }

  void set_alignment(ShelfAlignment alignment) {
    alignment_ = alignment;
  }

 private:
  ShelfAlignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(CalloutWidgetBackground);
};

struct VisiblePanelPositionInfo {
  VisiblePanelPositionInfo()
      : min_major(0),
        max_major(0),
        major_pos(0),
        major_length(0),
        window(NULL),
        slide_in(false) {}

  int min_major;
  int max_major;
  int major_pos;
  int major_length;
  aura::Window* window;
  bool slide_in;
};

bool CompareWindowMajor(const VisiblePanelPositionInfo& win1,
                        const VisiblePanelPositionInfo& win2) {
  return win1.major_pos < win2.major_pos;
}

void FanOutPanels(std::vector<VisiblePanelPositionInfo>::iterator first,
                  std::vector<VisiblePanelPositionInfo>::iterator last) {
  int num_panels = last - first;
  if (num_panels == 1) {
    (*first).major_pos = std::max((*first).min_major, std::min(
        (*first).max_major, (*first).major_pos));
  }
  if (num_panels <= 1)
    return;

  if (num_panels == 2) {
    // If there are two adjacent overlapping windows, separate them by the
    // minimum major_length necessary.
    std::vector<VisiblePanelPositionInfo>::iterator second = first + 1;
    int separation = (*first).major_length / 2 + (*second).major_length / 2 +
                     kPanelIdealSpacing;
    int overlap = (*first).major_pos + separation - (*second).major_pos;
    (*first).major_pos = std::max((*first).min_major,
                                  (*first).major_pos - overlap / 2);
    (*second).major_pos = std::min((*second).max_major,
                                   (*first).major_pos + separation);
    // Recalculate the first panel position in case the second one was
    // constrained on the right.
    (*first).major_pos = std::max((*first).min_major,
                                  (*second).major_pos - separation);
    return;
  }

  // If there are more than two overlapping windows, fan them out from minimum
  // position to maximum position equally spaced.
  int delta = ((*(last - 1)).max_major - (*first).min_major) / (num_panels - 1);
  int major_pos = (*first).min_major;
  for (std::vector<VisiblePanelPositionInfo>::iterator iter = first;
      iter != last; ++iter) {
    (*iter).major_pos = std::max((*iter).min_major,
                                 std::min((*iter).max_major, major_pos));
    major_pos += delta;
  }
}

bool BoundsAdjacent(const gfx::Rect& bounds1, const gfx::Rect& bounds2) {
  return bounds1.x() == bounds2.right() ||
         bounds1.y() == bounds2.bottom() ||
         bounds1.right() == bounds2.x() ||
         bounds1.bottom() == bounds2.y();
}

gfx::Vector2d GetSlideInAnimationOffset(ShelfAlignment alignment) {
  gfx::Vector2d offset;
  switch (alignment) {
    case SHELF_ALIGNMENT_BOTTOM:
      offset.set_y(kPanelSlideInOffset);
      break;
    case SHELF_ALIGNMENT_LEFT:
      offset.set_x(-kPanelSlideInOffset);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      offset.set_x(kPanelSlideInOffset);
      break;
    case SHELF_ALIGNMENT_TOP:
      offset.set_y(-kPanelSlideInOffset);
      break;
  }
  return offset;
}

}  // namespace

class PanelCalloutWidget : public views::Widget {
 public:
  explicit PanelCalloutWidget(aura::Window* container)
      : background_(NULL) {
    InitWidget(container);
  }

  void SetAlignment(ShelfAlignment alignment) {
    gfx::Rect callout_bounds = GetWindowBoundsInScreen();
    if (alignment == SHELF_ALIGNMENT_BOTTOM ||
        alignment == SHELF_ALIGNMENT_TOP) {
      callout_bounds.set_width(kArrowWidth);
      callout_bounds.set_height(kArrowHeight);
    } else {
      callout_bounds.set_width(kArrowHeight);
      callout_bounds.set_height(kArrowWidth);
    }
    GetNativeWindow()->SetBounds(callout_bounds);
    if (background_->alignment() != alignment) {
      background_->set_alignment(alignment);
      SchedulePaintInRect(gfx::Rect(gfx::Point(), callout_bounds.size()));
    }
  }

 private:
  void InitWidget(aura::Window* parent) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.can_activate = false;
    params.keep_on_top = true;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = parent;
    params.bounds = ScreenAsh::ConvertRectToScreen(parent, gfx::Rect());
    params.bounds.set_width(kArrowWidth);
    params.bounds.set_height(kArrowHeight);
    // Why do we need this and can_activate = false?
    set_focus_on_creation(false);
    Init(params);
    DCHECK_EQ(GetNativeView()->GetRootWindow(), parent->GetRootWindow());
    views::View* content_view = new views::View;
    background_ = new CalloutWidgetBackground;
    content_view->set_background(background_);
    SetContentsView(content_view);
    GetNativeWindow()->layer()->SetOpacity(0);
  }

  // Weak pointer owned by this widget's content view.
  CalloutWidgetBackground* background_;

  DISALLOW_COPY_AND_ASSIGN(PanelCalloutWidget);
};

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager public implementation:
PanelLayoutManager::PanelLayoutManager(aura::Window* panel_container)
    : panel_container_(panel_container),
      in_add_window_(false),
      in_layout_(false),
      dragged_panel_(NULL),
      launcher_(NULL),
      shelf_layout_manager_(NULL),
      last_active_panel_(NULL),
      weak_factory_(this) {
  DCHECK(panel_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->display_controller()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

PanelLayoutManager::~PanelLayoutManager() {
  Shutdown();
}

void PanelLayoutManager::Shutdown() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = NULL;
  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    delete iter->callout_widget;
  }
  panel_windows_.clear();
  if (launcher_)
    launcher_->RemoveIconObserver(this);
  launcher_ = NULL;
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void PanelLayoutManager::StartDragging(aura::Window* panel) {
  DCHECK(!dragged_panel_);
  dragged_panel_ = panel;
  Relayout();
}

void PanelLayoutManager::FinishDragging() {
  dragged_panel_ = NULL;
  Relayout();
}

void PanelLayoutManager::SetLauncher(ash::Launcher* launcher) {
  DCHECK(!launcher_);
  DCHECK(!shelf_layout_manager_);
  launcher_ = launcher;
  launcher_->AddIconObserver(this);
  if (launcher_->shelf_widget()) {
    shelf_layout_manager_ = ash::internal::ShelfLayoutManager::ForLauncher(
        launcher_->shelf_widget()->GetNativeWindow());
    WillChangeVisibilityState(shelf_layout_manager_->visibility_state());
    shelf_layout_manager_->AddObserver(this);
  }
}

void PanelLayoutManager::ToggleMinimize(aura::Window* panel) {
  DCHECK(panel->parent() == panel_container_);
  wm::WindowState* window_state = wm::GetWindowState(panel);
  if (window_state->IsMinimized())
    window_state->Restore();
  else
    window_state->Minimize();
}

views::Widget* PanelLayoutManager::GetCalloutWidgetForPanel(
    aura::Window* panel) {
  DCHECK(panel->parent() == panel_container_);
  PanelList::iterator found =
      std::find(panel_windows_.begin(), panel_windows_.end(), panel);
  DCHECK(found != panel_windows_.end());
  return found->callout_widget;
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, aura::LayoutManager implementation:
void PanelLayoutManager::OnWindowResized() {
  Relayout();
}

void PanelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  if (in_add_window_)
    return;
  base::AutoReset<bool> auto_reset_in_add_window(&in_add_window_, true);
  if (!wm::GetWindowState(child)->panel_attached()) {
    // This should only happen when a window is added to panel container as a
    // result of bounds change from within the application during a drag.
    // If so we have already stopped the drag and should reparent the panel
    // back to appropriate container and ignore it.
    // TODO(varkha): Updating bounds during a drag can cause problems and a more
    // general solution is needed. See http://crbug.com/251813 .
    aura::Window* old_parent = child->parent();
    aura::client::ParentWindowWithContext(
        child, child, child->GetRootWindow()->GetBoundsInScreen());
    wm::ReparentTransientChildrenOfChild(child, old_parent, child->parent());
    DCHECK(child->parent()->id() != kShellWindowId_PanelContainer);
    return;
  }
  PanelInfo panel_info;
  panel_info.window = child;
  panel_info.callout_widget = new PanelCalloutWidget(panel_container_);
  if (child != dragged_panel_) {
    // Set the panel to 0 opacity until it has been positioned to prevent it
    // from flashing briefly at position (0, 0).
    child->layer()->SetOpacity(0);
    panel_info.slide_in = true;
  }
  panel_windows_.push_back(panel_info);
  child->AddObserver(this);
  wm::GetWindowState(child)->AddObserver(this);
  Relayout();
}

void PanelLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void PanelLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  PanelList::iterator found =
      std::find(panel_windows_.begin(), panel_windows_.end(), child);
  if (found != panel_windows_.end()) {
    delete found->callout_widget;
    panel_windows_.erase(found);
  }
  child->RemoveObserver(this);
  wm::GetWindowState(child)->RemoveObserver(this);

  if (dragged_panel_ == child)
    dragged_panel_ = NULL;

  if (last_active_panel_ == child)
    last_active_panel_ = NULL;

  Relayout();
}

void PanelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
  Relayout();
}

void PanelLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  gfx::Rect bounds(requested_bounds);
  const gfx::Rect& max_bounds = panel_container_->GetRootWindow()->bounds();
  const int max_width = max_bounds.width() * kMaxWidthFactor;
  const int max_height = max_bounds.height() * kMaxHeightFactor;
  if (bounds.width() > max_width)
    bounds.set_width(max_width);
  if (bounds.height() > max_height)
    bounds.set_height(max_height);

  // Reposition dragged panel in the panel order.
  if (dragged_panel_ == child) {
    PanelList::iterator dragged_panel_iter =
      std::find(panel_windows_.begin(), panel_windows_.end(), dragged_panel_);
    DCHECK(dragged_panel_iter != panel_windows_.end());
    PanelList::iterator new_position;
    for (new_position = panel_windows_.begin();
         new_position != panel_windows_.end();
         ++new_position) {
      const gfx::Rect& bounds = (*new_position).window->bounds();
      if (bounds.x() + bounds.width()/2 <= requested_bounds.x()) break;
    }
    if (new_position != dragged_panel_iter) {
      PanelInfo dragged_panel_info = *dragged_panel_iter;
      panel_windows_.erase(dragged_panel_iter);
      panel_windows_.insert(new_position, dragged_panel_info);
    }
  }

  SetChildBoundsDirect(child, bounds);
  Relayout();
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, ShelfIconObserver implementation:

void PanelLayoutManager::OnShelfIconPositionsChanged() {
  // TODO: As this is called for every animation step now. Relayout needs to be
  // updated to use current icon position instead of use the ideal bounds so
  // that the panels slide with their icons instead of jumping.
  Relayout();
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, ash::ShellObserver implementation:

void PanelLayoutManager::OnShelfAlignmentChanged(aura::Window* root_window) {
  if (panel_container_->GetRootWindow() == root_window)
    Relayout();
}

/////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, WindowObserver implementation:

void PanelLayoutManager::OnWindowShowTypeChanged(
    wm::WindowState* window_state,
    wm::WindowShowType old_type) {
  // If the shelf is currently hidden then windows will not actually be shown
  // but the set to restore when the shelf becomes visible is updated.
  if (restore_windows_on_shelf_visible_) {
    if (window_state->IsMinimized()) {
      MinimizePanel(window_state->window());
      restore_windows_on_shelf_visible_->Remove(window_state->window());
    } else {
      restore_windows_on_shelf_visible_->Add(window_state->window());
    }
    return;
  }

  if (window_state->IsMinimized())
    MinimizePanel(window_state->window());
  else
    RestorePanel(window_state->window());
}

void PanelLayoutManager::OnWindowVisibilityChanged(
    aura::Window* window, bool visible) {
  if (visible)
    wm::GetWindowState(window)->Restore();
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, aura::client::ActivationChangeObserver implementation:

void PanelLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                           aura::Window* lost_active) {
  // Ignore if the panel that is not managed by this was activated.
  if (gained_active &&
      gained_active->type() == aura::client::WINDOW_TYPE_PANEL &&
      gained_active->parent() == panel_container_) {
    UpdateStacking(gained_active);
    UpdateCallouts();
  }
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, DisplayController::Observer implementation:

void PanelLayoutManager::OnDisplayConfigurationChanged() {
  Relayout();
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, ShelfLayoutManagerObserver implementation:

void PanelLayoutManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  // On entering / leaving full screen mode the shelf visibility state is
  // changed to / from SHELF_HIDDEN. In this state, panel windows should hide
  // to allow the full-screen application to use the full screen.
  bool shelf_hidden = new_state == ash::SHELF_HIDDEN;
  if (!shelf_hidden) {
    if (restore_windows_on_shelf_visible_) {
      scoped_ptr<aura::WindowTracker> restore_windows(
          restore_windows_on_shelf_visible_.Pass());
      for (aura::WindowTracker::Windows::const_iterator iter =
           restore_windows->windows().begin(); iter !=
           restore_windows->windows().end(); ++iter) {
        RestorePanel(*iter);
      }
    }
    return;
  }

  if (restore_windows_on_shelf_visible_)
    return;
  scoped_ptr<aura::WindowTracker> minimized_windows(new aura::WindowTracker);
  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    if (iter->window->IsVisible()) {
      minimized_windows->Add(iter->window);
      wm::GetWindowState(iter->window)->Minimize();
    }
  }
  restore_windows_on_shelf_visible_ = minimized_windows.Pass();
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager private implementation:

void PanelLayoutManager::MinimizePanel(aura::Window* panel) {
  views::corewm::SetWindowVisibilityAnimationType(
      panel, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
  ui::Layer* layer = panel->layer();
  ui::ScopedLayerAnimationSettings panel_slide_settings(layer->GetAnimator());
  panel_slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  panel_slide_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kPanelSlideDurationMilliseconds));
  gfx::Rect bounds(panel->bounds());
  bounds.Offset(GetSlideInAnimationOffset(
      launcher_->shelf_widget()->GetAlignment()));
  SetChildBoundsDirect(panel, bounds);
  panel->Hide();
  PanelList::iterator found =
      std::find(panel_windows_.begin(), panel_windows_.end(), panel);
  if (found != panel_windows_.end()) {
    layer->SetOpacity(0);
    // The next time the window is visible it should slide into place.
    found->slide_in = true;
  }
  if (wm::IsActiveWindow(panel))
    wm::DeactivateWindow(panel);
  Relayout();
}

void PanelLayoutManager::RestorePanel(aura::Window* panel) {
  panel->Show();
  Relayout();
}

void PanelLayoutManager::Relayout() {
  if (!launcher_ || !launcher_->shelf_widget())
    return;

  if (in_layout_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  ShelfAlignment alignment = launcher_->shelf_widget()->GetAlignment();
  bool horizontal = alignment == SHELF_ALIGNMENT_TOP ||
                    alignment == SHELF_ALIGNMENT_BOTTOM;
  gfx::Rect launcher_bounds = ash::ScreenAsh::ConvertRectFromScreen(
      panel_container_, launcher_->shelf_widget()->GetWindowBoundsInScreen());
  int panel_start_bounds = kPanelIdealSpacing;
  int panel_end_bounds = horizontal ?
      panel_container_->bounds().width() - kPanelIdealSpacing :
      panel_container_->bounds().height() - kPanelIdealSpacing;
  aura::Window* active_panel = NULL;
  std::vector<VisiblePanelPositionInfo> visible_panels;
  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    aura::Window* panel = iter->window;
    iter->callout_widget->SetAlignment(alignment);

    // Consider the dragged panel as part of the layout as long as it is
    // touching the launcher.
    if (!panel->IsVisible() ||
        (panel == dragged_panel_ &&
         !BoundsAdjacent(panel->bounds(), launcher_bounds))) {
      continue;
    }

    // If the shelf is currently hidden (full-screen mode), minimize panel until
    // full-screen mode is exited.
    if (restore_windows_on_shelf_visible_) {
      wm::GetWindowState(panel)->Minimize();
      restore_windows_on_shelf_visible_->Add(panel);
      continue;
    }

    gfx::Rect icon_bounds =
        launcher_->GetScreenBoundsOfItemIconForWindow(panel);

    // If both the icon width and height are 0 then there is no icon in the
    // launcher. If the launcher is hidden, one of the height or width will be
    // 0 but the position in the launcher and major dimension is still reported
    // correctly and the panel can be aligned above where the hidden icon is.
    if (icon_bounds.width() == 0 && icon_bounds.height() == 0)
      continue;

    if (panel->HasFocus() ||
        panel->Contains(
            aura::client::GetFocusClient(panel)->GetFocusedWindow())) {
      DCHECK(!active_panel);
      active_panel = panel;
    }
    icon_bounds = ScreenAsh::ConvertRectFromScreen(panel_container_,
                                                   icon_bounds);
    gfx::Point icon_origin = icon_bounds.origin();
    VisiblePanelPositionInfo position_info;
    int icon_start = horizontal ? icon_origin.x() : icon_origin.y();
    int icon_end = icon_start + (horizontal ? icon_bounds.width() :
                                 icon_bounds.height());
    position_info.major_length = horizontal ?
        panel->bounds().width() : panel->bounds().height();
    position_info.min_major = std::max(
        panel_start_bounds + position_info.major_length / 2,
        icon_end - position_info.major_length / 2);
    position_info.max_major = std::min(
        icon_start + position_info.major_length / 2,
        panel_end_bounds - position_info.major_length / 2);
    position_info.major_pos = (icon_start + icon_end) / 2;
    position_info.window = panel;
    position_info.slide_in = iter->slide_in;
    iter->slide_in = false;
    visible_panels.push_back(position_info);
  }

  // Sort panels by their X positions and fan out groups of overlapping panels.
  // The fan out method may result in new overlapping panels however given that
  // the panels start at least a full panel width apart this overlap will
  // never completely obscure a panel.
  // TODO(flackr): Rearrange panels if new overlaps are introduced.
  std::sort(visible_panels.begin(), visible_panels.end(), CompareWindowMajor);
  size_t first_overlapping_panel = 0;
  for (size_t i = 1; i < visible_panels.size(); ++i) {
    if (visible_panels[i - 1].major_pos +
        visible_panels[i - 1].major_length / 2 < visible_panels[i].major_pos -
        visible_panels[i].major_length / 2) {
      FanOutPanels(visible_panels.begin() + first_overlapping_panel,
                   visible_panels.begin() + i);
      first_overlapping_panel = i;
    }
  }
  FanOutPanels(visible_panels.begin() + first_overlapping_panel,
               visible_panels.end());

  for (size_t i = 0; i < visible_panels.size(); ++i) {
    if (visible_panels[i].window == dragged_panel_)
      continue;
    bool slide_in = visible_panels[i].slide_in;
    gfx::Rect bounds = visible_panels[i].window->GetTargetBounds();
    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        bounds.set_y(launcher_bounds.y() - bounds.height());
        break;
      case SHELF_ALIGNMENT_LEFT:
        bounds.set_x(launcher_bounds.right());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        bounds.set_x(launcher_bounds.x() - bounds.width());
        break;
      case SHELF_ALIGNMENT_TOP:
        bounds.set_y(launcher_bounds.bottom());
        break;
    }
    bool on_launcher = visible_panels[i].window->GetTargetBounds() == bounds;

    if (horizontal) {
      bounds.set_x(visible_panels[i].major_pos -
                   visible_panels[i].major_length / 2);
    } else {
      bounds.set_y(visible_panels[i].major_pos -
                   visible_panels[i].major_length / 2);
    }

    ui::Layer* layer = visible_panels[i].window->layer();
    if (slide_in) {
      // New windows shift up from the launcher  into position.
      gfx::Rect initial_bounds(bounds);
      initial_bounds.Offset(GetSlideInAnimationOffset(alignment));
      SetChildBoundsDirect(visible_panels[i].window, initial_bounds);
      // Set on launcher so that the panel animates into its target position.
      on_launcher = true;
    }

    if (on_launcher) {
      ui::ScopedLayerAnimationSettings panel_slide_settings(
          layer->GetAnimator());
      panel_slide_settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      panel_slide_settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kPanelSlideDurationMilliseconds));
      SetChildBoundsDirect(visible_panels[i].window, bounds);
      if (slide_in)
        layer->SetOpacity(1);
    } else {
      // If the launcher moved don't animate, move immediately to the new
      // target location.
      SetChildBoundsDirect(visible_panels[i].window, bounds);
    }
  }

  UpdateStacking(active_panel);
  UpdateCallouts();
}

void PanelLayoutManager::UpdateStacking(aura::Window* active_panel) {
  if (!active_panel) {
    if (!last_active_panel_)
      return;
    active_panel = last_active_panel_;
  }

  ShelfAlignment alignment = launcher_->alignment();
  bool horizontal = alignment == SHELF_ALIGNMENT_TOP ||
                    alignment == SHELF_ALIGNMENT_BOTTOM;

  // We want to to stack the panels like a deck of cards:
  // ,--,--,--,-------.--.--.
  // |  |  |  |       |  |  |
  // |  |  |  |       |  |  |
  //
  // We use the middle of each panel to figure out how to stack the panels. This
  // allows us to update the stacking when a panel is being dragged around by
  // the titlebar--even though it doesn't update the launcher icon positions, we
  // still want the visual effect.
  std::map<int, aura::Window*> window_ordering;
  for (PanelList::const_iterator it = panel_windows_.begin();
       it != panel_windows_.end(); ++it) {
    gfx::Rect bounds = it->window->bounds();
    window_ordering.insert(std::make_pair(horizontal ?
                                              bounds.x() + bounds.width() / 2 :
                                              bounds.y() + bounds.height() / 2,
                                          it->window));
  }

  aura::Window* previous_panel = NULL;
  for (std::map<int, aura::Window*>::const_iterator it =
       window_ordering.begin();
       it != window_ordering.end() && it->second != active_panel; ++it) {
    if (previous_panel)
      panel_container_->StackChildAbove(it->second, previous_panel);
    previous_panel = it->second;
  }

  previous_panel = NULL;
  for (std::map<int, aura::Window*>::const_reverse_iterator it =
       window_ordering.rbegin();
       it != window_ordering.rend() && it->second != active_panel; ++it) {
    if (previous_panel)
      panel_container_->StackChildAbove(it->second, previous_panel);
    previous_panel = it->second;
  }

  panel_container_->StackChildAtTop(active_panel);
  if (dragged_panel_ && dragged_panel_->parent() == panel_container_)
    panel_container_->StackChildAtTop(dragged_panel_);
  last_active_panel_ = active_panel;
}

void PanelLayoutManager::UpdateCallouts() {
  ShelfAlignment alignment = launcher_->alignment();
  bool horizontal = alignment == SHELF_ALIGNMENT_TOP ||
                    alignment == SHELF_ALIGNMENT_BOTTOM;

  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    aura::Window* panel = iter->window;
    views::Widget* callout_widget = iter->callout_widget;

    gfx::Rect current_bounds = panel->GetBoundsInScreen();
    gfx::Rect bounds = ScreenAsh::ConvertRectToScreen(panel->parent(),
                                                      panel->GetTargetBounds());
    gfx::Rect icon_bounds =
        launcher_->GetScreenBoundsOfItemIconForWindow(panel);
    if (icon_bounds.IsEmpty() || !panel->layer()->GetTargetVisibility() ||
        panel == dragged_panel_) {
      callout_widget->Hide();
      callout_widget->GetNativeWindow()->layer()->SetOpacity(0);
      continue;
    }

    gfx::Rect callout_bounds = callout_widget->GetWindowBoundsInScreen();
    gfx::Vector2d slide_vector = bounds.origin() - current_bounds.origin();
    int slide_distance = horizontal ? slide_vector.x() : slide_vector.y();
    int distance_until_over_panel = 0;
    if (horizontal) {
      callout_bounds.set_x(
          icon_bounds.x() + (icon_bounds.width() - callout_bounds.width()) / 2);
      distance_until_over_panel = std::max(
          current_bounds.x() - callout_bounds.x(),
          callout_bounds.right() - current_bounds.right());
    } else {
      callout_bounds.set_y(
          icon_bounds.y() + (icon_bounds.height() -
                             callout_bounds.height()) / 2);
      distance_until_over_panel = std::max(
          current_bounds.y() - callout_bounds.y(),
          callout_bounds.bottom() - current_bounds.bottom());
    }
    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        callout_bounds.set_y(bounds.bottom());
        break;
      case SHELF_ALIGNMENT_LEFT:
        callout_bounds.set_x(bounds.x() - callout_bounds.width());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        callout_bounds.set_x(bounds.right());
        break;
      case SHELF_ALIGNMENT_TOP:
        callout_bounds.set_y(bounds.y() - callout_bounds.height());
        break;
    }
    callout_bounds = ScreenAsh::ConvertRectFromScreen(
        callout_widget->GetNativeWindow()->parent(),
        callout_bounds);

    SetChildBoundsDirect(callout_widget->GetNativeWindow(), callout_bounds);
    panel_container_->StackChildAbove(callout_widget->GetNativeWindow(),
                                      panel);
    callout_widget->Show();

    ui::Layer* layer = callout_widget->GetNativeWindow()->layer();
    // If the panel is not over the callout position or has just become visible
    // then fade in the callout.
    if ((distance_until_over_panel > 0 || layer->GetTargetOpacity() < 1) &&
        panel->layer()->GetTargetTransform().IsIdentity()) {
      if (distance_until_over_panel > 0 &&
          slide_distance >= distance_until_over_panel) {
        layer->SetOpacity(0);
        // If the panel is not yet over the callout, then delay fading in
        // the callout until after the panel should be over it.
        int delay = kPanelSlideDurationMilliseconds *
            distance_until_over_panel / slide_distance;
        layer->SetOpacity(0);
        layer->GetAnimator()->StopAnimating();
        layer->GetAnimator()->SchedulePauseForProperties(
            base::TimeDelta::FromMilliseconds(delay),
            ui::LayerAnimationElement::OPACITY);
      }
      {
        ui::ScopedLayerAnimationSettings callout_settings(layer->GetAnimator());
        callout_settings.SetPreemptionStrategy(
            ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
        callout_settings.SetTransitionDuration(
            base::TimeDelta::FromMilliseconds(
                kCalloutFadeDurationMilliseconds));
        layer->SetOpacity(1);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// keyboard::KeyboardControllerObserver implementation:

void PanelLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& keyboard_bounds) {
  // This bounds change will have caused a change to the Shelf which does not
  // propogate automatically to this class, so manually recalculate bounds.
  OnWindowResized();
}

}  // namespace internal
}  // namespace ash
