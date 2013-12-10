// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_UTIL_H_
#define ASH_WM_WINDOW_UTIL_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ui {
class Event;
}

namespace ash {
// We force at least this many DIPs for any window on the screen.
const int kMinimumOnScreenArea = 10;

namespace wm {

// Utility functions for window activation.
ASH_EXPORT void ActivateWindow(aura::Window* window);
ASH_EXPORT void DeactivateWindow(aura::Window* window);
ASH_EXPORT bool IsActiveWindow(aura::Window* window);
ASH_EXPORT aura::Window* GetActiveWindow();
ASH_EXPORT bool CanActivateWindow(aura::Window* window);

// Retrieves the activatable window for |window|. If |window| is activatable,
// this will just return it, otherwise it will climb the parent/transient parent
// chain looking for a window that is activatable, per the ActivationController.
// If you're looking for a function to get the activatable "top level" window,
// this is probably what you're looking for.
ASH_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

// TODO(oshima): remove this.
ASH_EXPORT bool IsWindowMinimized(aura::Window* window);

// Moves the window to the center of the display.
ASH_EXPORT void CenterWindow(aura::Window* window);

// Change the availability of animation to the fullscreen of the |window|.
ASH_EXPORT void SetAnimateToFullscreen(aura::Window* window, bool animate);

// Move the given bounds inside the given |visible_area|, including a
// safety margin given by |kMinimumOnScreenArea|.
ASH_EXPORT void AdjustBoundsToEnsureMinimumWindowVisibility(
    const gfx::Rect& visible_area,
    gfx::Rect* bounds);

// Move the given bounds inside the given |visible_area|, including a
// safety margin given by |min_width| and |min_height|.
ASH_EXPORT void AdjustBoundsToEnsureWindowVisibility(
    const gfx::Rect& visible_area,
    int min_width,
    int min_height,
    gfx::Rect* bounds);

// Moves |window| to the root window where the |event| occured if it is not
// already in the same root window. Returns true if |window| was moved.
ASH_EXPORT bool MoveWindowToEventRoot(aura::Window* window,
                                      const ui::Event& event);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_UTIL_H_
