// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_

#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"

namespace aura {
class Window;
}

namespace ash {
namespace wm {
class WindowState;
}

namespace internal {

class WorkspaceEventHandlerTestHelper;

class WorkspaceEventHandler : public ToplevelWindowEventHandler {
 public:
  explicit WorkspaceEventHandler(aura::Window* owner);
  virtual ~WorkspaceEventHandler();

  // Overridden from ToplevelWindowEventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  friend class WorkspaceEventHandlerTestHelper;

  // Determines if |event| corresponds to a double click on either the top or
  // bottom vertical resize edge, and if so toggles the vertical height of the
  // window between its restored state and the full available height of the
  // workspace.
  void HandleVerticalResizeDoubleClick(wm::WindowState* window_state,
                                       ui::MouseEvent* event);

  MultiWindowResizeController multi_window_resize_controller_;

  // If non-NULL, set to true in the destructor.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_H_
