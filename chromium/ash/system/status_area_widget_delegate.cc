// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget_delegate.h"

#include "ash/ash_export.h"
#include "ash/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {

const int kStatusTrayOffsetFromScreenEdge = 4;

}

StatusAreaWidgetDelegate::StatusAreaWidgetDelegate()
    : focus_cycler_for_testing_(NULL),
      alignment_(SHELF_ALIGNMENT_BOTTOM) {
  // Allow the launcher to surrender the focus to another window upon
  // navigation completion by the user.
  set_allow_deactivate_on_esc(true);
}

StatusAreaWidgetDelegate::~StatusAreaWidgetDelegate() {
}

void StatusAreaWidgetDelegate::SetFocusCyclerForTesting(
    const FocusCycler* focus_cycler) {
  focus_cycler_for_testing_ = focus_cycler;
}

views::View* StatusAreaWidgetDelegate::GetDefaultFocusableChild() {
  return child_at(0);
}

views::Widget* StatusAreaWidgetDelegate::GetWidget() {
  return View::GetWidget();
}

const views::Widget* StatusAreaWidgetDelegate::GetWidget() const {
  return View::GetWidget();
}

void StatusAreaWidgetDelegate::OnGestureEvent(ui::GestureEvent* event) {
  if (gesture_handler_.ProcessGestureEvent(*event))
    event->StopPropagation();
  else
    views::AccessiblePaneView::OnGestureEvent(event);
}

bool StatusAreaWidgetDelegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  const FocusCycler* focus_cycler = focus_cycler_for_testing_ ?
      focus_cycler_for_testing_ : Shell::GetInstance()->focus_cycler();
  return focus_cycler->widget_activating() == GetWidget();
}

void StatusAreaWidgetDelegate::DeleteDelegate() {
}

void StatusAreaWidgetDelegate::AddTray(views::View* tray) {
  SetLayoutManager(NULL);  // Reset layout manager before adding a child.
  AddChildView(tray);
  // Set the layout manager with the new list of children.
  UpdateLayout();
}

void StatusAreaWidgetDelegate::UpdateLayout() {
  // Use a grid layout so that the trays can be centered in each cell, and
  // so that the widget gets laid out correctly when tray sizes change.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  if (alignment_ == SHELF_ALIGNMENT_BOTTOM ||
      alignment_ == SHELF_ALIGNMENT_TOP) {
    // Alternate shelf layout insets are all handled by tray_background_view.
    if (!ash::switches::UseAlternateShelfLayout()) {
      if (alignment_ == SHELF_ALIGNMENT_TOP)
        layout->SetInsets(kStatusTrayOffsetFromScreenEdge, 0, 0, 0);
      else
        layout->SetInsets(0, 0, kStatusTrayOffsetFromScreenEdge, 0);
    }
    bool is_first_visible_child = true;
    for (int c = 0; c < child_count(); ++c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
      if (!is_first_visible_child)
        columns->AddPaddingColumn(0, GetTraySpacing());
      is_first_visible_child = false;
      columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL,
                         0, /* resize percent */
                         views::GridLayout::USE_PREF, 0, 0);
    }
    layout->StartRow(0, 0);
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (child->visible())
        layout->AddView(child);
    }
  } else {
    if (!ash::switches::UseAlternateShelfLayout()) {
      if (alignment_ == SHELF_ALIGNMENT_LEFT)
        layout->SetInsets(0, kStatusTrayOffsetFromScreenEdge, 0, 0);
      else
        layout->SetInsets(0, 0, 0, kStatusTrayOffsetFromScreenEdge);
    }
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::USE_PREF, 0, 0);
    bool is_first_visible_child = true;
    for (int c = child_count() - 1; c >= 0; --c) {
      views::View* child = child_at(c);
      if (!child->visible())
        continue;
      if (!is_first_visible_child)
        layout->AddPaddingRow(0, GetTraySpacing());
      is_first_visible_child = false;
      layout->StartRow(0, 0);
      layout->AddView(child);
    }
  }
  Layout();
  UpdateWidgetSize();
}

void StatusAreaWidgetDelegate::ChildPreferredSizeChanged(View* child) {
  // Need to resize the window when trays or items are added/removed.
  UpdateWidgetSize();
}

void StatusAreaWidgetDelegate::ChildVisibilityChanged(View* child) {
  UpdateLayout();
}

void StatusAreaWidgetDelegate::UpdateWidgetSize() {
  if (GetWidget())
    GetWidget()->SetSize(GetPreferredSize());
}

}  // namespace internal
}  // namespace ash
