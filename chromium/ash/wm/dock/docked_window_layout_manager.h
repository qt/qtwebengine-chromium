// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_H_
#define ASH_WM_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/dock/dock_types.h"
#include "ash/wm/workspace/snap_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
class Widget;
}

namespace ash {
class Launcher;

namespace internal {
class DockedWindowLayoutManagerObserver;
class DockedWindowResizerTest;
class ShelfLayoutManager;

// DockedWindowLayoutManager is responsible for organizing windows when they are
// docked to the side of a screen. It is associated with a specific container
// window (i.e. kShellWindowId_DockContainer) and controls the layout of any
// windows added to that container.
//
// The constructor takes a |dock_container| argument which is expected to set
// its layout manager to this instance, e.g.:
// dock_container->SetLayoutManager(
//     new DockedWindowLayoutManager(dock_container));
//
// TODO(varkha): extend BaseLayoutManager instead of LayoutManager to inherit
// common functionality.
class ASH_EXPORT DockedWindowLayoutManager
    : public aura::LayoutManager,
      public ash::ShellObserver,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver,
      public keyboard::KeyboardControllerObserver,
      public ash::ShelfLayoutManagerObserver {
 public:
  // Maximum width of the docked windows area.
  static const int kMaxDockWidth;

  explicit DockedWindowLayoutManager(aura::Window* dock_container);
  virtual ~DockedWindowLayoutManager();

  // Disconnects observers before container windows get destroyed.
  void Shutdown();

  // Management of the observer list.
  virtual void AddObserver(DockedWindowLayoutManagerObserver* observer);
  virtual void RemoveObserver(DockedWindowLayoutManagerObserver* observer);

  // Called by a DockedWindowResizer to update which window is being dragged.
  // Starts observing the window unless it is a child.
  void StartDragging(aura::Window* window);

  // Called by a DockedWindowResizer when a dragged window is docked.
  void DockDraggedWindow(aura::Window* window);

  // Called by a DockedWindowResizer when a dragged window is no longer docked.
  void UndockDraggedWindow();

  // Called by a DockedWindowResizer when a window is no longer being dragged.
  // Stops observing the window unless it is a child.
  void FinishDragging();

  ash::Launcher* launcher() { return launcher_; }
  void SetLauncher(ash::Launcher* launcher);

  // Calculates if a window is touching the screen edges and returns edge.
  DockedAlignment GetAlignmentOfWindow(const aura::Window* window) const;

  // Used to snap docked windows to the side of screen during drag.
  DockedAlignment CalculateAlignment() const;

  // Returns true when a window can be docked. Windows cannot be docked at the
  // edge used by the launcher shelf or the edge opposite from existing dock.
  bool CanDockWindow(aura::Window* window, SnapType edge);

  aura::Window* dock_container() const { return dock_container_; }

  // Returns current bounding rectangle of docked windows area.
  const gfx::Rect& docked_bounds() const { return docked_bounds_; }

  // Returns last known coordinates of |dragged_window_| after Relayout.
  const gfx::Rect dragged_bounds() const { return dragged_bounds_;}

  // Returns true if currently dragged window is docked at the screen edge.
  bool is_dragged_window_docked() const { return is_dragged_window_docked_; }

  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // ash::ShellObserver:
  virtual void OnShelfAlignmentChanged(aura::RootWindow* root_window) OVERRIDE;

  // aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // ShelfLayoutManagerObserver:
  virtual void WillChangeVisibilityState(
      ShelfVisibilityState new_state) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DockedWindowResizerTest, AttachTwoWindowsDetachOne);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowResizerTest, AttachWindowMaximizeOther);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowResizerTest, AttachOneTestSticky);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowResizerTest, ResizeTwoWindows);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowResizerTest, DragToShelf);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowLayoutManagerTest, AutoPlacingLeft);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowLayoutManagerTest, AutoPlacingRight);
  FRIEND_TEST_ALL_PREFIXES(DockedWindowLayoutManagerTest,
                           AutoPlacingRightSecondScreen);
  friend class DockedWindowLayoutManagerTest;
  friend class DockedWindowResizerTest;

  // Minimum width of the docked windows area.
  static const int kMinDockWidth;

  // Width of the gap between the docked windows and a workspace.
  static const int kMinDockGap;

  // Minimize / restore window and relayout.
  void MinimizeDockedWindow(aura::Window* window);
  void RestoreDockedWindow(aura::Window* window);

  // Updates docked layout state when a window gets inside the dock.
  void OnWindowDocked(aura::Window* window);

  // Updates docked layout state when a window gets outside the dock.
  void OnWindowUndocked();

  // Returns true if there are any windows currently docked.
  bool IsAnyWindowDocked();

  // Called whenever the window layout might change.
  void Relayout();

  // Updates |docked_bounds_| and workspace insets when bounds of docked windows
  // area change.
  void UpdateDockBounds();

  // Called whenever the window stacking order needs to be updated (e.g. focus
  // changes or a window is moved).
  void UpdateStacking(aura::Window* active_window);

  // keyboard::KeyboardControllerObserver:
  virtual void OnKeyboardBoundsChanging(
      const gfx::Rect& keyboard_bounds) OVERRIDE;

  // Parent window associated with this layout manager.
  aura::Window* dock_container_;
  // Protect against recursive calls to Relayout().
  bool in_layout_;

  // A window that is being dragged (whether docked or not).
  // Windows are tracked by docked layout manager only if they are docked;
  // however we need to know if a window is being dragged in order to avoid
  // positioning it or even considering it for layout.
  aura::Window* dragged_window_;

  // True if the window being dragged is currently docked.
  bool is_dragged_window_docked_;

  // Previously docked windows use a more relaxed dragging sorting algorithm
  // that uses assumption that a window starts being dragged out of position
  // that was previously established in Relayout. This allows easier reordering.
  bool is_dragged_from_dock_;

  // The launcher we are observing for launcher icon changes.
  Launcher* launcher_;
  // The shelf layout manager being observed for visibility changes.
  ShelfLayoutManager* shelf_layout_manager_;
  // Tracks the visibility of the shelf. Defaults to false when there is no
  // shelf.
  bool shelf_hidden_;
  // Current width of the dock.
  int docked_width_;

  // Last bounds that were sent to observers.
  gfx::Rect docked_bounds_;

  // Target bounds of a docked window being dragged.
  gfx::Rect dragged_bounds_;

  // Side of the screen that the dock is positioned at.
  DockedAlignment alignment_;

  // The last active window. Used to maintain stacking order even if no windows
  // are currently focused.
  aura::Window* last_active_window_;

  // Widget used to paint a background for the docked area.
  scoped_ptr<views::Widget> background_widget_;

  // Observers of dock bounds changes.
  ObserverList<DockedWindowLayoutManagerObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DockedWindowLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_H_
