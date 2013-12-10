// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_OVERVIEW_H_
#define ASH_WM_OVERVIEW_WINDOW_OVERVIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace aura {

class Window;
class RootWindow;

namespace client {
class CursorClient;
}

}  // namespace aura

namespace ui {
class LocatedEvent;
}

namespace views {
class Widget;
}

namespace ash {

class WindowSelector;
class WindowSelectorItem;

// The WindowOverview shows a grid of all of your windows and allows selecting
// a window by clicking or tapping on it. It also displays a selection widget
// used to indicate the current selection when alt-tabbing between windows.
class WindowOverview : public ui::EventHandler {
 public:
  typedef ScopedVector<WindowSelectorItem> WindowSelectorItemList;

  // Enters an overview mode displaying |windows| and dispatches methods
  // on |window_selector| when a window is selected or selection is canceled.
  // If |single_root_window| is not NULL, all windows will be positioned on the
  // given root window.
  WindowOverview(WindowSelector* window_selector,
                 WindowSelectorItemList* windows,
                 aura::RootWindow* single_root_window);
  virtual ~WindowOverview();

  // Sets the selected window to be the window in position |index|.
  void SetSelection(size_t index);

  // Dispatched when the list of windows has changed.
  void OnWindowsChanged();

  // ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE;
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

 private:
  // Returns the target of |event| or NULL if the event is not targeted at
  // any of the windows in the selector.
  aura::Window* GetEventTarget(ui::LocatedEvent* event);

  // Position all of the windows based on the current selection mode.
  void PositionWindows();
  // Position all of the windows from |root_window| on |root_window|.
  void PositionWindowsFromRoot(aura::RootWindow* root_window);
  // Position all of the |windows| to fit on the |root_window|.
  void PositionWindowsOnRoot(aura::RootWindow* root_window,
                             const std::vector<WindowSelectorItem*>& windows);

  void InitializeSelectionWidget();

  // Updates the selection widget's location to the currently selected window.
  // If |animate| the transition to the new location is animated.
  void UpdateSelectionLocation(bool animate);

  // Weak pointer to the window selector which owns this class.
  WindowSelector* window_selector_;

  // A weak pointer to the collection of windows in the overview wrapped by a
  // helper class which restores their state and helps transform them to other
  // root windows.
  WindowSelectorItemList* windows_;

  // Widget indicating which window is currently selected.
  scoped_ptr<views::Widget> selection_widget_;

  // If NULL, each root window displays an overview of the windows in that
  // display. Otherwise, all windows are in a single overview on
  // |single_root_window_|.
  aura::RootWindow* single_root_window_;

  // The time when overview was started.
  base::Time overview_start_time_;

  // The cursor client used to lock the current cursor during overview.
  aura::client::CursorClient* cursor_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowOverview);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_OVERVIEW_H_
