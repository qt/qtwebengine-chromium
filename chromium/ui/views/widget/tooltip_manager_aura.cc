// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager_aura.h"

#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
int TooltipManager::GetTooltipHeight() {
  // Not used for linux and chromeos.
  NOTIMPLEMENTED();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura public:

TooltipManagerAura::TooltipManagerAura(Widget* widget) : widget_(widget) {
  aura::client::SetTooltipText(GetWindow(), &tooltip_text_);
}

TooltipManagerAura::~TooltipManagerAura() {
  aura::client::SetTooltipText(GetWindow(), NULL);
}

// static
const gfx::FontList& TooltipManagerAura::GetDefaultFontList() {
  return ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::BaseFont);
}

// static
void TooltipManagerAura::UpdateTooltipManagerForCapture(Widget* source) {
  if (!source->HasCapture())
    return;

  aura::Window* root_window = source->GetNativeView()->GetRootWindow();
  if (!root_window)
    return;

  gfx::Point screen_loc(
      root_window->GetDispatcher()->GetLastMouseLocationInRoot());
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (!screen_position_client)
    return;
  screen_position_client->ConvertPointToScreen(root_window, &screen_loc);
  gfx::Screen* screen = gfx::Screen::GetScreenFor(root_window);
  aura::Window* target = screen->GetWindowAtScreenPoint(screen_loc);
  if (!target)
    return;
  gfx::Point target_loc(screen_loc);
  screen_position_client =
      aura::client::GetScreenPositionClient(target->GetRootWindow());
  if (!screen_position_client)
    return;
  screen_position_client->ConvertPointFromScreen(target, &target_loc);
  target = target->GetEventHandlerForPoint(target_loc);
  while (target) {
    Widget* target_widget = Widget::GetWidgetForNativeView(target);
    if (target_widget == source)
      return;

    if (target_widget) {
      if (target_widget->GetTooltipManager())
        target_widget->GetTooltipManager()->UpdateTooltip();
      return;
    }
    target = target->parent();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura, TooltipManager implementation:

const gfx::FontList& TooltipManagerAura::GetFontList() const {
  return GetDefaultFontList();
}

void TooltipManagerAura::UpdateTooltip() {
  aura::Window* root_window = GetWindow()->GetRootWindow();
  if (aura::client::GetTooltipClient(root_window)) {
    gfx::Point view_point =
        root_window->GetDispatcher()->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(root_window, GetWindow(), &view_point);
    View* view = GetViewUnderPoint(view_point);
    UpdateTooltipForTarget(view, view_point, root_window);
  }
}

void TooltipManagerAura::TooltipTextChanged(View* view)  {
  aura::Window* root_window = GetWindow()->GetRootWindow();
  if (aura::client::GetTooltipClient(root_window)) {
    gfx::Point view_point =
        root_window->GetDispatcher()->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(root_window, GetWindow(), &view_point);
    View* target = GetViewUnderPoint(view_point);
    if (target != view)
      return;
    UpdateTooltipForTarget(view, view_point, root_window);
  }
}

View* TooltipManagerAura::GetViewUnderPoint(const gfx::Point& point) {
  View* root_view = widget_->GetRootView();
  if (root_view)
    return root_view->GetTooltipHandlerForPoint(point);
  return NULL;
}

void TooltipManagerAura::UpdateTooltipForTarget(View* target,
                                                const gfx::Point& point,
                                                aura::Window* root_window) {
  if (target) {
    gfx::Point view_point = point;
    View::ConvertPointFromWidget(target, &view_point);
    string16 new_tooltip_text;
    if (!target->GetTooltipText(view_point, &new_tooltip_text))
      tooltip_text_.clear();
    else
      tooltip_text_ = new_tooltip_text;
  } else {
    tooltip_text_.clear();
  }
  aura::client::GetTooltipClient(root_window)->UpdateTooltip(GetWindow());
}

aura::Window* TooltipManagerAura::GetWindow() {
  return widget_->GetNativeView();
}

}  // namespace views.
