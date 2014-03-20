// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_
#define ASH_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_

#include "ash/shelf/shelf_item_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

// ShelfItemDelegate for TYPE_APP_LIST.
class AppListShelfItemDelegate : public ShelfItemDelegate {
 public:
  AppListShelfItemDelegate();

  virtual ~AppListShelfItemDelegate();

  // ShelfItemDelegate:
  virtual bool ItemSelected(const ui::Event& event) OVERRIDE;
  virtual base::string16 GetTitle() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(aura::Window* root_window) OVERRIDE;
  virtual ShelfMenuModel* CreateApplicationMenu(int event_flags) OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual bool ShouldShowTooltip() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListShelfItemDelegate);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_APP_LIST_SHELF_ITEM_DELEGATE_H_
