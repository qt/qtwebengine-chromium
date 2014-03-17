// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_HOST_H_
#define ASH_SHELF_SHELF_BUTTON_HOST_H_

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
}

namespace ash {
namespace internal {

// The shelf buttons communicate back to the host by way of this interface.
// This interface is used to enable reordering the items on the shelf.
class ASH_EXPORT ShelfButtonHost {
 public:
  enum Pointer {
    NONE,
    DRAG_AND_DROP,
    MOUSE,
    TOUCH,
  };

  // Invoked when a pointer device is pressed on a view.
  virtual void PointerPressedOnButton(views::View* view,
                                      Pointer pointer,
                                      const ui::LocatedEvent& event) = 0;

  // Invoked when a pointer device is dragged over a view.
  virtual void PointerDraggedOnButton(views::View* view,
                                      Pointer pointer,
                                      const ui::LocatedEvent& event) = 0;

  // Invoked either if a pointer device is released or mouse capture canceled.
  virtual void PointerReleasedOnButton(views::View* view,
                                       Pointer pointer,
                                       bool canceled) = 0;

  // Invoked when the mouse moves on the item.
  virtual void MouseMovedOverButton(views::View* view) = 0;

  // Invoked when the mouse enters the item.
  virtual void MouseEnteredButton(views::View* view) = 0;

  // Invoked when the mouse exits the item.
  virtual void MouseExitedButton(views::View* view) = 0;

  // Invoked to get the accessible name of the item.
  virtual base::string16 GetAccessibleName(const views::View* view) = 0;

 protected:
  virtual ~ShelfButtonHost() {}
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_HOST_H_
