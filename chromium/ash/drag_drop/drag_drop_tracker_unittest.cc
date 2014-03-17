// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_tracker.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

class DragDropTrackerTest : public test::AshTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay("200x200,300x300");
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    static int window_id = 0;
    return CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
        window_id++,
        bounds);
  }

  static aura::Window* GetTarget(const gfx::Point& location) {
    scoped_ptr<internal::DragDropTracker> tracker(
        new internal::DragDropTracker(Shell::GetPrimaryRootWindow(),
                                      NULL));
    ui::MouseEvent e(ui::ET_MOUSE_DRAGGED,
                     location,
                     location,
                     ui::EF_NONE);
    aura::Window* target = tracker->GetTarget(e);
    return target;
  }

  static ui::LocatedEvent* ConvertEvent(aura::Window* target,
                                           const ui::MouseEvent& event) {
    scoped_ptr<internal::DragDropTracker> tracker(
        new internal::DragDropTracker(Shell::GetPrimaryRootWindow(),
                                      NULL));
    ui::LocatedEvent* converted = tracker->ConvertEvent(target, event);
    return converted;
  }
};

// TODO(mazda): Remove this once ash/wm/coordinate_conversion.h supports
// non-X11 platforms.
#if defined(USE_X11)
#define MAYBE_GetTarget GetTarget
#else
#define MAYBE_GetTarget DISABLED_GetTarget
#endif

TEST_F(DragDropTrackerTest, MAYBE_GetTarget) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  scoped_ptr<aura::Window> window0(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window0->Show();

  scoped_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(300, 100, 100, 100)));
  window1->Show();
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  EXPECT_EQ(root_windows[1], window1->GetRootWindow());
  EXPECT_EQ("0,0 100x100", window0->GetBoundsInScreen().ToString());
  EXPECT_EQ("300,100 100x100", window1->GetBoundsInScreen().ToString());

  // Make RootWindow0 active so that capture window is parented to it.
  Shell::GetInstance()->set_target_root_window(root_windows[0]);

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // |window0| covers.
  EXPECT_EQ(window0.get(), GetTarget(gfx::Point(50, 50)));

  // Start tracking from the RootWindow0 and check the point on RootWindow0 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(150, 150)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(150, 150)));

  // Start tracking from the RootWindow0 and check the point on RootWindow1 that
  // |window1| covers.
  EXPECT_EQ(window1.get(), GetTarget(gfx::Point(350, 150)));

  // Start tracking from the RootWindow0 and check the point on RootWindow1 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(50, 250)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(50, 250)));

  // Make RootWindow1 active so that capture window is parented to it.
  Shell::GetInstance()->set_target_root_window(root_windows[1]);

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // |window0| covers.
  EXPECT_EQ(window0.get(), GetTarget(gfx::Point(-150, 50)));

  // Start tracking from the RootWindow1 and check the point on RootWindow0 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(150, -50)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(150, -50)));

  // Start tracking from the RootWindow1 and check the point on RootWindow1 that
  // |window1| covers.
  EXPECT_EQ(window1.get(), GetTarget(gfx::Point(150, 150)));

  // Start tracking from the RootWindow1 and check the point on RootWindow1 that
  // neither |window0| nor |window1| covers.
  EXPECT_NE(window0.get(), GetTarget(gfx::Point(50, 50)));
  EXPECT_NE(window1.get(), GetTarget(gfx::Point(50, 50)));
}

// TODO(mazda): Remove this once ash/wm/coordinate_conversion.h supports
// non-X11 platforms.
#if defined(USE_X11)
#define MAYBE_ConvertEvent ConvertEvent
#else
#define MAYBE_ConvertEvent DISABLED_ConvertEvent
#endif

TEST_F(DragDropTrackerTest, MAYBE_ConvertEvent) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  scoped_ptr<aura::Window> window0(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window0->Show();

  scoped_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(300, 100, 100, 100)));
  window1->Show();

  // Make RootWindow0 active so that capture window is parented to it.
  Shell::GetInstance()->set_target_root_window(root_windows[0]);

  // Start tracking from the RootWindow0 and converts the mouse event into
  // |window0|'s coodinates.
  ui::MouseEvent original00(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(50, 50),
                            gfx::Point(50, 50),
                            ui::EF_NONE);
  scoped_ptr<ui::LocatedEvent> converted00(ConvertEvent(window0.get(),
                                                        original00));
  EXPECT_EQ(original00.type(), converted00->type());
  EXPECT_EQ("50,50", converted00->location().ToString());
  EXPECT_EQ("50,50", converted00->root_location().ToString());
  EXPECT_EQ(original00.flags(), converted00->flags());

  // Start tracking from the RootWindow0 and converts the mouse event into
  // |window1|'s coodinates.
  ui::MouseEvent original01(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(350, 150),
                            gfx::Point(350, 150),
                            ui::EF_NONE);
  scoped_ptr<ui::LocatedEvent> converted01(ConvertEvent(window1.get(),
                                                        original01));
  EXPECT_EQ(original01.type(), converted01->type());
  EXPECT_EQ("50,50", converted01->location().ToString());
  EXPECT_EQ("150,150", converted01->root_location().ToString());
  EXPECT_EQ(original01.flags(), converted01->flags());

  // Make RootWindow1 active so that capture window is parented to it.
  Shell::GetInstance()->set_target_root_window(root_windows[1]);

  // Start tracking from the RootWindow1 and converts the mouse event into
  // |window0|'s coodinates.
  ui::MouseEvent original10(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(-150, 50),
                            gfx::Point(-150, 50),
                            ui::EF_NONE);
  scoped_ptr<ui::LocatedEvent> converted10(ConvertEvent(window0.get(),
                                                        original10));
  EXPECT_EQ(original10.type(), converted10->type());
  EXPECT_EQ("50,50", converted10->location().ToString());
  EXPECT_EQ("50,50", converted10->root_location().ToString());
  EXPECT_EQ(original10.flags(), converted10->flags());

  // Start tracking from the RootWindow1 and converts the mouse event into
  // |window1|'s coodinates.
  ui::MouseEvent original11(ui::ET_MOUSE_DRAGGED,
                            gfx::Point(150, 150),
                            gfx::Point(150, 150),
                            ui::EF_NONE);
  scoped_ptr<ui::LocatedEvent> converted11(ConvertEvent(window1.get(),
                                                           original11));
  EXPECT_EQ(original11.type(), converted11->type());
  EXPECT_EQ("50,50", converted11->location().ToString());
  EXPECT_EQ("150,150", converted11->root_location().ToString());
  EXPECT_EQ(original11.flags(), converted11->flags());
}

}  // namespace test
}  // namespace aura
