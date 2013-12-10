// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/toplevel_window.h"

#include "ash/display/display_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace shell {

ToplevelWindow::CreateParams::CreateParams()
    : can_resize(false),
      can_maximize(false) {
}

// static
void ToplevelWindow::CreateToplevelWindow(const CreateParams& params) {
  static int count = 0;
  int x = count == 0 ? 150 : 750;
  count = (count + 1) % 2;

  gfx::Rect bounds(x, 150, 300, 300);
  gfx::Display display =
      ash::Shell::GetScreen()->GetDisplayMatching(bounds);
  aura::RootWindow* root = ash::Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display.id());
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      new ToplevelWindow(params), root, bounds);
  widget->GetNativeView()->SetName("Examples:ToplevelWindow");
  widget->Show();
}

ToplevelWindow::ToplevelWindow(const CreateParams& params) : params_(params) {
}

ToplevelWindow::~ToplevelWindow() {
}

void ToplevelWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorDKGRAY);
}

base::string16 ToplevelWindow::GetWindowTitle() const {
  return ASCIIToUTF16("Examples: Toplevel Window");
}

views::View* ToplevelWindow::GetContentsView() {
  return this;
}

bool ToplevelWindow::CanResize() const {
  return params_.can_resize;
}

bool ToplevelWindow::CanMaximize() const {
  return params_.can_maximize;
}

}  // namespace shell
}  // namespace ash
