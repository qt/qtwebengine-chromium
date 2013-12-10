// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_window.h"

#include "ui/aura/window.h"

namespace ash {

WindowSelectorWindow::WindowSelectorWindow(aura::Window* window)
    : transform_window_(window) {
}

WindowSelectorWindow::~WindowSelectorWindow() {
}

aura::RootWindow* WindowSelectorWindow::GetRootWindow() {
  return transform_window_.window()->GetRootWindow();
}

aura::Window* WindowSelectorWindow::TargetedWindow(const aura::Window* target) {
  if (transform_window_.Contains(target))
    return transform_window_.window();
  return NULL;
}

void WindowSelectorWindow::RestoreWindowOnExit(aura::Window* window) {
  transform_window_.RestoreWindowOnExit();
}

aura::Window* WindowSelectorWindow::SelectionWindow() {
  return transform_window_.window();
}

void WindowSelectorWindow::RemoveWindow(const aura::Window* window) {
  DCHECK_EQ(transform_window_.window(), window);
  transform_window_.OnWindowDestroyed();
}

bool WindowSelectorWindow::empty() const {
  return transform_window_.window() == NULL;
}

void WindowSelectorWindow::SetItemBounds(aura::RootWindow* root_window,
                                         const gfx::Rect& target_bounds) {
  gfx::Rect bounding_rect = transform_window_.window()->GetBoundsInScreen();
  transform_window_.SetTransform(root_window,
      ScopedTransformOverviewWindow::GetTransformForRectPreservingAspectRatio(
          transform_window_.window()->GetBoundsInScreen(), target_bounds));
}

}  // namespace ash
