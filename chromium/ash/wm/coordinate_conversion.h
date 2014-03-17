// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COORDINATE_CONVERSION_H_
#define ASH_WM_COORDINATE_CONVERSION_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}  // namespace gfx

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ash {
namespace wm {

// Returns the RootWindow at |point| in the virtual screen coordinates.
// Returns NULL if the root window does not exist at the given
// point.
ASH_EXPORT aura::Window* GetRootWindowAt(const gfx::Point& point);

// Returns the RootWindow that shares the most area with |rect| in
// the virtual scren coordinates.
ASH_EXPORT aura::Window* GetRootWindowMatching(const gfx::Rect& rect);

// Converts the |point| from a given |window|'s coordinates into the screen
// coordinates.
ASH_EXPORT void ConvertPointToScreen(aura::Window* window, gfx::Point* point);

// Converts the |point| from the screen coordinates to a given |window|'s
// coordinates.
ASH_EXPORT void ConvertPointFromScreen(aura::Window* window,
                                       gfx::Point* point_in_screen);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COORDINATE_CONVERSION_H_
