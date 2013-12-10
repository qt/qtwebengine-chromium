// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_H_
#define ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "base/strings/string16.h"
#include "ui/base/models/simple_menu_model.h"

namespace aura {
class RootWindow;
}

namespace ui {
class Event;
}

namespace ash {

// A special menu model which keeps track of an "active" menu item.
class ASH_EXPORT LauncherMenuModel : public ui::SimpleMenuModel {
 public:
  explicit LauncherMenuModel(ui::SimpleMenuModel::Delegate* delegate)
      : ui::SimpleMenuModel(delegate) {}

  // Returns |true| when the given |command_id| is active and needs to be drawn
  // in a special state.
  virtual bool IsCommandActive(int command_id) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherMenuModel);
};

// Delegate for the LauncherItem.
// TODO(simon.hong81): Remove LauncherItem from abstract function parameters.
class ASH_EXPORT LauncherItemDelegate {
 public:
  virtual ~LauncherItemDelegate() {}

  // Invoked when the user clicks on a window entry in the launcher.
  // |event| is the click event. The |event| is dispatched by a view
  // and has an instance of |views::View| as the event target
  // but not |aura::Window|. If the |event| is of type KeyEvent, it is assumed
  // that this was triggered by keyboard action (Alt+<number>) and special
  // handling might happen.
  virtual void ItemSelected(const LauncherItem& item,
                            const ui::Event& event) = 0;

  // Returns the title to display for the specified launcher item.
  virtual base::string16 GetTitle(const LauncherItem& item) = 0;

  // Returns the context menumodel for the specified item on
  // |root_window|.  Return NULL if there should be no context
  // menu. The caller takes ownership of the returned model.
  virtual ui::MenuModel* CreateContextMenu(const LauncherItem& item,
                                           aura::RootWindow* root_window) = 0;

  // Returns the application menu model for the specified item. There are three
  // possible return values:
  //  - A return of NULL indicates that no menu is wanted for this item.
  //  - A return of a menu with one item means that only the name of the
  //    application/item was added and there are no active applications.
  //    Note: This is useful for hover menus which also show context help.
  //  - A list containing the title and the active list of items.
  // The caller takes ownership of the returned model.
  // |event_flags| specifies the flags of the event which triggered this menu.
  virtual LauncherMenuModel* CreateApplicationMenu(
      const LauncherItem& item,
      int event_flags) = 0;

  // Whether the given launcher item is draggable.
  virtual bool IsDraggable(const LauncherItem& item) = 0;

  // Returns true if a tooltip should be shown for the item.
  virtual bool ShouldShowTooltip(const LauncherItem& item) = 0;
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_ITEM_DELEGATE_H_
