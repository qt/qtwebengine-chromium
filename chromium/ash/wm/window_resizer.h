// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESIZER_H_
#define ASH_WM_WINDOW_RESIZER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/window_move_client.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ash {
namespace wm {
class WindowState;
}

// WindowResizer is used by ToplevelWindowEventFilter to handle dragging, moving
// or resizing a window. All coordinates passed to this are in the parent
// windows coordinates.
class ASH_EXPORT WindowResizer {
 public:
  // Constants to identify the type of resize.
  static const int kBoundsChange_None;
  static const int kBoundsChange_Repositions;
  static const int kBoundsChange_Resizes;

  // Used to indicate which direction the resize occurs in.
  static const int kBoundsChangeDirection_None;
  static const int kBoundsChangeDirection_Horizontal;
  static const int kBoundsChangeDirection_Vertical;

  WindowResizer();
  virtual ~WindowResizer();

  // Returns a bitmask of the kBoundsChange_ values.
  static int GetBoundsChangeForWindowComponent(int component);

  // Invoked to drag/move/resize the window. |location| is in the coordinates
  // of the window supplied to the constructor. |event_flags| is the event
  // flags from the event.
  virtual void Drag(const gfx::Point& location, int event_flags) = 0;

  // Invoked to complete the drag.
  virtual void CompleteDrag(int event_flags) = 0;

  // Reverts the drag.
  virtual void RevertDrag() = 0;

  // Returns the target window the resizer was created for.
  virtual aura::Window* GetTarget() = 0;

  // See comment for |Details::initial_location_in_parent|.
  virtual const gfx::Point& GetInitialLocation() const = 0;

 protected:
  struct Details {
    Details();
    Details(aura::Window* window,
            const gfx::Point& location,
            int window_component,
            aura::client::WindowMoveSource source);
    ~Details();

    // The window we're resizing.
    // TODO(oshima): replace this with accessor method to
    // |window_state->window()|.
    aura::Window* window;

    // The ash window state for the |window| above.
    wm::WindowState* window_state;

    // Initial bounds of the window in parent coordinates.
    gfx::Rect initial_bounds_in_parent;

    // Restore bounds (in screen coordinates) of the window before the drag
    // started. Only set if the window is normal and is being dragged.
    gfx::Rect restore_bounds;

    // Location passed to the constructor, in |window->parent()|'s coordinates.
    gfx::Point initial_location_in_parent;

    // Initial opacity of the window.
    float initial_opacity;

    // The component the user pressed on.
    int window_component;

    // Bitmask of the |kBoundsChange_| constants.
    int bounds_change;

    // Bitmask of the |kBoundsChangeDirection_| constants.
    int position_change_direction;

    // Bitmask of the |kBoundsChangeDirection_| constants.
    int size_change_direction;

    // Will the drag actually modify the window?
    bool is_resizable;

    // Source of the event initiating the drag.
    aura::client::WindowMoveSource source;
  };

  static gfx::Rect CalculateBoundsForDrag(const Details& details,
                                          const gfx::Point& location);

  static gfx::Rect AdjustBoundsToGrid(const gfx::Rect& bounds,
                                      int grid_size);

  static bool IsBottomEdge(int component);

 private:
  // In case of touch resizing, adjusts deltas so that the border is positioned
  // just under the touch point.
  static void AdjustDeltaForTouchResize(const Details& details,
                                        int* delta_x,
                                        int* delta_y);

  // Returns the new origin of the window. The arguments are the difference
  // between the current location and the initial location.
  static gfx::Point GetOriginForDrag(const Details& details,
                                     int delta_x,
                                     int delta_y);

  // Returns the size of the window for the drag.
  static gfx::Size GetSizeForDrag(const Details& details,
                                  int* delta_x,
                                  int* delta_y);

  // Returns the width of the window.
  static int GetWidthForDrag(const Details& details,
                             int min_width,
                             int* delta_x);

  // Returns the height of the drag.
  static int GetHeightForDrag(const Details& details,
                              int min_height,
                              int* delta_y);
};

// Creates a WindowResizer for |window|. This can return a scoped_ptr
// initialized with NULL if |window| should not be resized nor dragged.
ASH_EXPORT scoped_ptr<WindowResizer> CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    aura::client::WindowMoveSource source);

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESIZER_H_
