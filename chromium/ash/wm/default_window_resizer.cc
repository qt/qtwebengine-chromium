// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace ash {

DefaultWindowResizer::~DefaultWindowResizer() {
  ash::Shell::GetInstance()->cursor_manager()->UnlockCursor();
}

// static
DefaultWindowResizer*
DefaultWindowResizer::Create(aura::Window* window,
                             const gfx::Point& location,
                             int window_component,
                             aura::client::WindowMoveSource source) {
  Details details(window, location, window_component, source);
  return details.is_resizable ? new DefaultWindowResizer(details) : NULL;
}

void DefaultWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location));
  if (bounds != details_.window->bounds()) {
    if (!did_move_or_resize_ && !details_.restore_bounds.IsEmpty())
      wm::GetWindowState(details_.window)->ClearRestoreBounds();
    did_move_or_resize_ = true;
    details_.window->SetBounds(bounds);
  }
}

void DefaultWindowResizer::CompleteDrag(int event_flags) {
}

void DefaultWindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  details_.window->SetBounds(details_.initial_bounds_in_parent);

  if (!details_.restore_bounds.IsEmpty())
    wm::GetWindowState(details_.window)->SetRestoreBoundsInScreen(
        details_.restore_bounds);
}

aura::Window* DefaultWindowResizer::GetTarget() {
  return details_.window;
}

const gfx::Point& DefaultWindowResizer::GetInitialLocation() const {
  return details_.initial_location_in_parent;
}

DefaultWindowResizer::DefaultWindowResizer(const Details& details)
    : details_(details),
      did_move_or_resize_(false) {
  DCHECK(details_.is_resizable);
  ash::Shell::GetInstance()->cursor_manager()->LockCursor();
}

}  // namespace aura
