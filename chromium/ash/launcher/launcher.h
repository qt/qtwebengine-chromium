// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_H_
#define ASH_LAUNCHER_LAUNCHER_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
}

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class View;
}

namespace ash {

namespace internal {
class FocusCycler;
class LauncherView;
class ShelfLayoutManager;
}

namespace test {
class LauncherTestAPI;
}

class LauncherIconObserver;
class LauncherDelegate;
class LauncherModel;
class ShelfWidget;

class ASH_EXPORT Launcher {
 public:
  static const char kNativeViewName[];

  Launcher(LauncherModel* launcher_model,
           LauncherDelegate* launcher_delegate,
           ShelfWidget* shelf_widget);
  virtual ~Launcher();

  // Return the launcher for the primary display. NULL if no user is
  // logged in yet.
  static Launcher* ForPrimaryDisplay();

  // Return the launcher for the display that |window| is currently on,
  // or a launcher on primary display if the launcher per display feature
  // is disabled. NULL if no user is logged in yet.
  static Launcher* ForWindow(aura::Window* window);

  void SetAlignment(ShelfAlignment alignment);
  ShelfAlignment alignment() const { return alignment_; }

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Updates the icon position given the current window bounds. This is used
  // when dragging panels to reposition them with respect to the other panels.
  void UpdateIconPositionForWindow(aura::Window* window);

  // Activates the the launcher item specified by the index in the list
  // of launcher items.
  void ActivateLauncherItem(int index);

  // Cycles the window focus linearly over the current launcher items.
  void CycleWindowLinear(CycleDirection direction);

  void AddIconObserver(LauncherIconObserver* observer);
  void RemoveIconObserver(LauncherIconObserver* observer);

  // Returns true if the Launcher is showing a context menu.
  bool IsShowingMenu() const;

  bool IsShowingOverflowBubble() const;

  void SetVisible(bool visible) const;
  bool IsVisible() const;

  void SchedulePaint();

  views::View* GetAppListButtonView() const;

  // Launch a 0-indexed launcher item in the Launcher.
  // A negative index launches the last launcher item in the launcher.
  void LaunchAppIndexAt(int item_index);

  ShelfWidget* shelf_widget() { return shelf_widget_; }

  // Set the bounds of the launcher view.
  void SetLauncherViewBounds(gfx::Rect bounds);
  gfx::Rect GetLauncherViewBounds() const;

  // Returns ApplicationDragAndDropHost for this Launcher.
  app_list::ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

 private:
  friend class ash::test::LauncherTestAPI;

  // LauncherView used to display icons.
  internal::LauncherView* launcher_view_;

  ShelfAlignment alignment_;

  LauncherDelegate* delegate_;

  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_H_
