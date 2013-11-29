// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LAYOUT_MANAGER_H_
#define ASH_SHELF_SHELF_LAYOUT_MANAGER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/launcher/launcher.h"
#include "ash/shelf/background_animator.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell_observer.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/dock/docked_window_layout_manager_observer.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace aura {
class RootWindow;
}

namespace ui {
class GestureEvent;
}

namespace ash {
class ScreenAsh;
class ShelfLayoutManagerObserver;
class ShelfWidget;
namespace internal {

class PanelLayoutManagerTest;
class ShelfBezelEventFilter;
class ShelfLayoutManagerTest;
class StatusAreaWidget;
class WorkspaceController;

// ShelfLayoutManager is the layout manager responsible for the launcher and
// status widgets. The launcher is given the total available width and told the
// width of the status area. This allows the launcher to draw the background and
// layout to the status area.
// To respond to bounds changes in the status area StatusAreaLayoutManager works
// closely with ShelfLayoutManager.
class ASH_EXPORT ShelfLayoutManager :
    public aura::LayoutManager,
    public ash::ShellObserver,
    public aura::client::ActivationChangeObserver,
    public DockedWindowLayoutManagerObserver,
    public keyboard::KeyboardControllerObserver {
 public:

  // We reserve a small area on the edge of the workspace area to ensure that
  // the resize handle at the edge of the window can be hit.
  static const int kWorkspaceAreaVisibleInset;

  // When autohidden we extend the touch hit target onto the screen so that the
  // user can drag the shelf out.
  static const int kWorkspaceAreaAutoHideInset;

  // Size of the shelf when auto-hidden.
  static const int kAutoHideSize;

  // The size of the shelf when shown (currently only used in alternate
  // settings see ash::switches::UseAlternateShelfLayout).
  static const int kShelfSize;

  // Returns the preferred size for the shelf (either kLauncherPreferredSize or
  // kShelfSize).
  static int GetPreferredShelfSize();

  explicit ShelfLayoutManager(ShelfWidget* shelf);
  virtual ~ShelfLayoutManager();

  // Sets the ShelfAutoHideBehavior. See enum description for details.
  void SetAutoHideBehavior(ShelfAutoHideBehavior behavior);
  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }

  // Sets the alignment. Returns true if the alignment is changed. Otherwise,
  // returns false.
  bool SetAlignment(ShelfAlignment alignment);
  ShelfAlignment GetAlignment() const { return alignment_; }

  void set_workspace_controller(WorkspaceController* controller) {
    workspace_controller_ = controller;
  }

  bool in_layout() const { return in_layout_; }

  // Clears internal data for shutdown process.
  void PrepareForShutdown();

  // Returns whether the shelf and its contents (launcher, status) are visible
  // on the screen.
  bool IsVisible() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds();

  // Stops any animations and sets the bounds of the launcher and status
  // widgets.
  void LayoutShelf();

  // Returns shelf visibility state based on current value of auto hide
  // behavior setting.
  ShelfVisibilityState CalculateShelfVisibility();

  // Updates the visibility state.
  void UpdateVisibilityState();

  // Invoked by the shelf/launcher when the auto-hide state may have changed.
  void UpdateAutoHideState();

  ShelfVisibilityState visibility_state() const {
    return state_.visibility_state;
  }
  ShelfAutoHideState auto_hide_state() const { return state_.auto_hide_state; }

  ShelfWidget* shelf_widget() { return shelf_; }

  // Sets whether any windows overlap the shelf. If a window overlaps the shelf
  // the shelf renders slightly differently.
  void SetWindowOverlapsShelf(bool value);
  bool window_overlaps_shelf() const { return window_overlaps_shelf_; }

  void AddObserver(ShelfLayoutManagerObserver* observer);
  void RemoveObserver(ShelfLayoutManagerObserver* observer);

  // Gesture dragging related functions:
  void StartGestureDrag(const ui::GestureEvent& gesture);
  enum DragState {
    DRAG_SHELF,
    DRAG_TRAY
  };
  // Returns DRAG_SHELF if the gesture should continue to drag the entire shelf.
  // Returns DRAG_TRAY if the gesture can start dragging the tray-bubble from
  // this point on.
  DragState UpdateGestureDrag(const ui::GestureEvent& gesture);
  void CompleteGestureDrag(const ui::GestureEvent& gesture);
  void CancelGestureDrag();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overridden from ash::ShellObserver:
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

  // Overriden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // TODO(harrym|oshima): These templates will be moved to
  // new Shelf class.
  // A helper function that provides a shortcut for choosing
  // values specific to a shelf alignment.
  template<typename T>
  T SelectValueForShelfAlignment(T bottom, T left, T right, T top) const {
    switch (alignment_) {
      case SHELF_ALIGNMENT_BOTTOM:
        return bottom;
      case SHELF_ALIGNMENT_LEFT:
        return left;
      case SHELF_ALIGNMENT_RIGHT:
        return right;
      case SHELF_ALIGNMENT_TOP:
        return top;
    }
    NOTREACHED();
    return right;
  }

  template<typename T>
  T PrimaryAxisValue(T horizontal, T vertical) const {
    return IsHorizontalAlignment() ? horizontal : vertical;
  }

  // Is the shelf's alignment horizontal?
  bool IsHorizontalAlignment() const;

  // Tests if the browser is currently in fullscreen mode with minimal
  // Chrome. When minimal Chrome is present the shelf should be displayed.
  bool FullscreenWithMinimalChrome() const;

  // Returns a ShelfLayoutManager on the display which has a launcher for
  // given |window|. See RootWindowController::ForLauncher for more info.
  static ShelfLayoutManager* ForLauncher(aura::Window* window);

 private:
  class AutoHideEventFilter;
  class UpdateShelfObserver;
  friend class ash::ScreenAsh;
  friend class PanelLayoutManagerTest;
  friend class ShelfLayoutManagerTest;

  struct TargetBounds {
    TargetBounds();
    ~TargetBounds();

    float opacity;
    float status_opacity;
    gfx::Rect shelf_bounds_in_root;
    gfx::Rect launcher_bounds_in_shelf;
    gfx::Rect status_bounds_in_shelf;
    gfx::Insets work_area_insets;
  };

  struct State {
    State() : visibility_state(SHELF_VISIBLE),
              auto_hide_state(SHELF_AUTO_HIDE_HIDDEN),
              window_state(WORKSPACE_WINDOW_STATE_DEFAULT),
              is_screen_locked(false) {}

    // Returns true if the two states are considered equal. As
    // |auto_hide_state| only matters if |visibility_state| is
    // |SHELF_AUTO_HIDE|, Equals() ignores the |auto_hide_state| as
    // appropriate.
    bool Equals(const State& other) const {
      return other.visibility_state == visibility_state &&
          (visibility_state != SHELF_AUTO_HIDE ||
           other.auto_hide_state == auto_hide_state) &&
          other.window_state == window_state &&
          other.is_screen_locked == is_screen_locked;
    }

    ShelfVisibilityState visibility_state;
    ShelfAutoHideState auto_hide_state;
    WorkspaceWindowState window_state;
    bool is_screen_locked;
  };

  // Sets the visibility of the shelf to |state|.
  void SetState(ShelfVisibilityState visibility_state);

  // Stops any animations and progresses them to the end.
  void StopAnimating();

  // Returns the width (if aligned to the side) or height (if aligned to the
  // bottom).
  void GetShelfSize(int* width, int* height);

  // Insets |bounds| by |inset| on the edge the shelf is aligned to.
  void AdjustBoundsBasedOnAlignment(int inset, gfx::Rect* bounds) const;

  // Calculates the target bounds assuming visibility of |visible|.
  void CalculateTargetBounds(const State& state, TargetBounds* target_bounds);

  // Updates the target bounds if a gesture-drag is in progress. This is only
  // used by |CalculateTargetBounds()|.
  void UpdateTargetBoundsForGesture(TargetBounds* target_bounds) const;

  // Updates the background of the shelf.
  void UpdateShelfBackground(BackgroundAnimator::ChangeType type);

  // Returns how the shelf background is painted.
  ShelfBackgroundType GetShelfBackgroundType() const;

  // Updates the auto hide state immediately.
  void UpdateAutoHideStateNow();

  // Stops the auto hide timer and clears
  // |mouse_over_shelf_when_auto_hide_timer_started_|.
  void StopAutoHideTimer();

  // Returns the bounds of an additional region which can trigger showing the
  // shelf. This region exists to make it easier to trigger showing the shelf
  // when the shelf is auto hidden and the shelf is on the boundary between
  // two displays.
  gfx::Rect GetAutoHideShowShelfRegionInScreen() const;

  // Returns the AutoHideState. This value is determined from the launcher and
  // tray.
  ShelfAutoHideState CalculateAutoHideState(
      ShelfVisibilityState visibility_state) const;

  // Updates the hit test bounds override for launcher and status area.
  void UpdateHitTestBounds();

  // Returns true if |window| is a descendant of the shelf.
  bool IsShelfWindow(aura::Window* window);

  int GetWorkAreaSize(const State& state, int size) const;

  // Return the bounds available in the parent, taking into account the bounds
  // of the keyboard if necessary.
  gfx::Rect GetAvailableBounds() const;

  // Overridden from keyboard::KeyboardControllerObserver:
  virtual void OnKeyboardBoundsChanging(
      const gfx::Rect& keyboard_bounds) OVERRIDE;

  // Overridden from dock::DockObserver:
  virtual void OnDockBoundsChanging(const gfx::Rect& dock_bounds) OVERRIDE;

  // Generates insets for inward edge based on the current shelf alignment.
  gfx::Insets GetInsetsForAlignment(int distance) const;

  // The RootWindow is cached so that we don't invoke Shell::GetInstance() from
  // our destructor. We avoid that as at the time we're deleted Shell is being
  // deleted too.
  aura::RootWindow* root_window_;

  // True when inside LayoutShelf method. Used to prevent calling LayoutShelf
  // again from SetChildBounds().
  bool in_layout_;

  // See description above setter.
  ShelfAutoHideBehavior auto_hide_behavior_;

  ShelfAlignment alignment_;

  // Current state.
  State state_;

  ShelfWidget* shelf_;

  WorkspaceController* workspace_controller_;

  // Do any windows overlap the shelf? This is maintained by WorkspaceManager.
  bool window_overlaps_shelf_;

  base::OneShotTimer<ShelfLayoutManager> auto_hide_timer_;

  // Whether the mouse was over the shelf when the auto hide timer started.
  // False when neither the auto hide timer nor the timer task are running.
  bool mouse_over_shelf_when_auto_hide_timer_started_;

  // EventFilter used to detect when user moves the mouse over the launcher to
  // trigger showing the launcher.
  scoped_ptr<AutoHideEventFilter> auto_hide_event_filter_;

  // EventFilter used to detect when user issues a gesture on a bezel sensor.
  scoped_ptr<ShelfBezelEventFilter> bezel_event_filter_;

  ObserverList<ShelfLayoutManagerObserver> observers_;

  // The shelf reacts to gesture-drags, and can be set to auto-hide for certain
  // gestures. Some shelf behaviour (e.g. visibility state, background color
  // etc.) are affected by various stages of the drag. The enum keeps track of
  // the present status of the gesture drag.
  enum GestureDragStatus {
    GESTURE_DRAG_NONE,
    GESTURE_DRAG_IN_PROGRESS,
    GESTURE_DRAG_COMPLETE_IN_PROGRESS
  };
  GestureDragStatus gesture_drag_status_;

  // Tracks the amount of the drag. The value is only valid when
  // |gesture_drag_status_| is set to GESTURE_DRAG_IN_PROGRESS.
  float gesture_drag_amount_;

  // Manage the auto-hide state during the gesture.
  ShelfAutoHideState gesture_drag_auto_hide_state_;

  // Used to delay updating shelf background.
  UpdateShelfObserver* update_shelf_observer_;

  // The bounds of the keyboard.
  gfx::Rect keyboard_bounds_;

  // The bounds of the dock.
  gfx::Rect dock_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_SHELF_LAYOUT_MANAGER_H_
