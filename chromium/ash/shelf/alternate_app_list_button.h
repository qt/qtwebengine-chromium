// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_ALTERNATE_APP_LIST_BUTTON_H_
#define ASH_SHELF_ALTERNATE_APP_LIST_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace ash {

class ShelfWidget;

namespace internal {

class ShelfButtonHost;

// Button used for the AppList icon on the shelf.
// This class is an alternate implementation to ash::internal::AppListButton
// for the purposes of testing an alternate shelf layout
// (see ash_switches: UseAlternateShelfLayout).
class AlternateAppListButton : public views::ImageButton {
 public:
  // Bounds size (inset) required for the app icon image (in pixels).
  static const int kImageBoundsSize;

  AlternateAppListButton(views::ButtonListener* listener,
                         ShelfButtonHost* host,
                         ShelfWidget* shelf_widget);
  virtual ~AlternateAppListButton();

 protected:
  // views::ImageButton overrides:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  ShelfButtonHost* host_;
  // Reference to the shelf widget containing this button, owned by the
  // root window controller.
  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(AlternateAppListButton);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_ALTERNATE_APP_LIST_BUTTON_H_
