// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/wm/ash_native_cursor_manager.h"

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "base/run_loop.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls.h"

#if defined(USE_X11)
#include <X11/Xlib.h>

#include "base/message_loop/message_pump_x11.h"
#endif

namespace ash {

using views::corewm::CursorManager;
typedef test::AshTestBase AshNativeCursorManagerTest;

namespace {

internal::DisplayInfo CreateDisplayInfo(int64 id,
                                        const gfx::Rect& bounds,
                                        float device_scale_factor) {
  internal::DisplayInfo info(id, "", false);
  info.SetBounds(bounds);
  info.set_device_scale_factor(device_scale_factor);
  return info;
}

void MoveMouseSync(aura::Window* window, int x, int y) {
#if defined(USE_X11)
  XWarpPointer(base::MessagePumpX11::GetDefaultXDisplay(),
               None,
               window->GetDispatcher()->host()->GetAcceleratedWidget(),
               0, 0, 0, 0,
               x, y);
#endif
  // Send and wait for a key event to make sure that mouse
  // events are fully processed.
  base::RunLoop loop;
  ui_controls::SendKeyPressNotifyWhenDone(
      window,
      ui::VKEY_SPACE,
      false,
      false,
      false,
      false,
      loop.QuitClosure());
  loop.Run();
}

}  // namespace

#if defined(USE_X11)
#define MAYBE_CursorChangeOnEnterNotify CursorChangeOnEnterNotify
#else
#define MAYBE_CursorChangeOnEnterNotify DISABLED_CursorChangeOnEnterNotify
#endif

TEST_F(AshNativeCursorManagerTest, MAYBE_CursorChangeOnEnterNotify) {
  CursorManager* cursor_manager = Shell::GetInstance()->cursor_manager();
  test::CursorManagerTestApi test_api(cursor_manager);

  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  internal::DisplayInfo display_info1 =
      CreateDisplayInfo(10, gfx::Rect(0, 0, 500, 300), 1.0f);
  internal::DisplayInfo display_info2 =
      CreateDisplayInfo(20, gfx::Rect(500, 0, 500, 300), 2.0f);
  std::vector<internal::DisplayInfo> display_info_list;
  display_info_list.push_back(display_info1);
  display_info_list.push_back(display_info2);
  display_manager->OnNativeDisplaysChanged(display_info_list);

  MoveMouseSync(Shell::GetAllRootWindows()[0], 10, 10);
  EXPECT_EQ(1.0f, test_api.GetDisplay().device_scale_factor());

  MoveMouseSync(Shell::GetAllRootWindows()[0], 600, 10);
  EXPECT_EQ(2.0f, test_api.GetDisplay().device_scale_factor());
}

}  // namespace ash
