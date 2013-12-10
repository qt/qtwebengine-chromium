// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/strings/string_util.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/window_test_api.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

void SetSecondaryDisplayLayout(DisplayLayout::Position position) {
  DisplayLayout layout =
      Shell::GetInstance()->display_manager()->GetCurrentDisplayLayout();
  layout.position = position;
  Shell::GetInstance()->display_controller()->
      SetLayoutForCurrentDisplays(layout);
}

internal::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

class ModalWidgetDelegate : public views::WidgetDelegateView {
 public:
  ModalWidgetDelegate() {}
  virtual ~ModalWidgetDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWidgetDelegate);
};

// An event handler which moves the target window to the secondary root window
// at pre-handle phase of a mouse release event.
class MoveWindowByClickEventHandler : public ui::EventHandler {
 public:
  explicit MoveWindowByClickEventHandler(aura::Window* target)
      : target_(target) {}
  virtual ~MoveWindowByClickEventHandler() {}

 private:
  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (event->type() == ui::ET_MOUSE_RELEASED) {
      Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
      DCHECK_LT(1u, root_windows.size());
      root_windows[1]->AddChild(target_);
    }
  }

  aura::Window* target_;
  DISALLOW_COPY_AND_ASSIGN(MoveWindowByClickEventHandler);
};

// An event handler which records the event's locations.
class EventLocationRecordingEventHandler : public ui::EventHandler {
 public:
  explicit EventLocationRecordingEventHandler() {
    reset();
  }
  virtual ~EventLocationRecordingEventHandler() {}

  std::string GetLocationsAndReset() {
    std::string result =
        location_.ToString() + " " + root_location_.ToString();
    reset();
    return result;
  }

 private:
  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (event->type() == ui::ET_MOUSE_MOVED ||
        event->type() == ui::ET_MOUSE_DRAGGED) {
      location_ = event->location();
      root_location_ = event->root_location();
    }
  }

  void reset() {
    location_.SetPoint(-999, -999);
    root_location_.SetPoint(-999, -999);
  }

  gfx::Point root_location_;
  gfx::Point location_;

  DISALLOW_COPY_AND_ASSIGN(EventLocationRecordingEventHandler);
};

}  // namespace

class ExtendedDesktopTest : public test::AshTestBase {
 public:
  views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
    return CreateTestWidgetWithParentAndContext(
        NULL, CurrentContext(), bounds, false);
  }

  views::Widget* CreateTestWidgetWithParent(views::Widget* parent,
                                            const gfx::Rect& bounds,
                                            bool child) {
    CHECK(parent);
    return CreateTestWidgetWithParentAndContext(parent, NULL, bounds, child);
  }

  views::Widget* CreateTestWidgetWithParentAndContext(views::Widget* parent,
                                                      gfx::NativeView context,
                                                      const gfx::Rect& bounds,
                                                      bool child) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    if (parent)
      params.parent = parent->GetNativeView();
    params.context = context;
    params.bounds = bounds;
    params.child = child;
    views::Widget* widget = new views::Widget;
    widget->Init(params);
    widget->Show();
    return widget;
  }
};

// Test conditions that root windows in extended desktop mode
// must satisfy.
TEST_F(ExtendedDesktopTest, Basic) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // All root windows must have the root window controller.
  ASSERT_EQ(2U, root_windows.size());
  for (Shell::RootWindowList::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    EXPECT_TRUE(internal::GetRootWindowController(*iter) != NULL);
  }
  // Make sure root windows share the same controllers.
  EXPECT_EQ(aura::client::GetFocusClient(root_windows[0]),
            aura::client::GetFocusClient(root_windows[1]));
  EXPECT_EQ(aura::client::GetActivationClient(root_windows[0]),
            aura::client::GetActivationClient(root_windows[1]));
  EXPECT_EQ(aura::client::GetCaptureClient(root_windows[0]),
            aura::client::GetCaptureClient(root_windows[1]));
}

TEST_F(ExtendedDesktopTest, Activation) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  views::Widget* widget_on_2nd =
      CreateTestWidget(gfx::Rect(1200, 10, 100, 100));
  EXPECT_EQ(root_windows[0], widget_on_1st->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], widget_on_2nd->GetNativeView()->GetRootWindow());

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            aura::client::GetFocusClient(root_windows[0])->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));

  aura::test::EventGenerator& event_generator(GetEventGenerator());
  // Clicking a window changes the active window and active root window.
  event_generator.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  event_generator.ClickLeftButton();

  EXPECT_EQ(widget_on_1st->GetNativeView(),
            aura::client::GetFocusClient(root_windows[0])->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));

  event_generator.MoveMouseToCenterOf(widget_on_2nd->GetNativeView());
  event_generator.ClickLeftButton();

  EXPECT_EQ(widget_on_2nd->GetNativeView(),
            aura::client::GetFocusClient(root_windows[0])->GetFocusedWindow());
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_2nd->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, SystemModal) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  views::Widget* widget_on_1st = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], widget_on_1st->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], Shell::GetTargetRootWindow());

  // Open system modal. Make sure it's on 2nd root window and active.
  views::Widget* modal_widget = views::Widget::CreateWindowWithContextAndBounds(
      new ModalWidgetDelegate(),
      CurrentContext(),
      gfx::Rect(1200, 100, 100, 100));
  modal_widget->Show();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], modal_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], Shell::GetTargetRootWindow());

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  // Clicking a widget on widget_on_1st display should not change activation.
  event_generator.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal_widget->GetNativeView()));
  EXPECT_EQ(root_windows[1], Shell::GetTargetRootWindow());

  // Close system modal and so clicking a widget should work now.
  modal_widget->Close();
  event_generator.MoveMouseToCenterOf(widget_on_1st->GetNativeView());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(widget_on_1st->GetNativeView()));
  EXPECT_EQ(root_windows[0], Shell::GetTargetRootWindow());
}

TEST_F(ExtendedDesktopTest, TestCursor) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(ui::kCursorPointer, root_windows[0]->last_cursor().native_type());
  EXPECT_EQ(ui::kCursorPointer, root_windows[1]->last_cursor().native_type());
  Shell::GetInstance()->cursor_manager()->SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, root_windows[0]->last_cursor().native_type());
  EXPECT_EQ(ui::kCursorCopy, root_windows[1]->last_cursor().native_type());
}

TEST_F(ExtendedDesktopTest, TestCursorLocation) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::test::WindowTestApi root_window0_test_api(root_windows[0]);
  aura::test::WindowTestApi root_window1_test_api(root_windows[1]);

  root_windows[0]->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,10", Shell::GetScreen()->GetCursorScreenPoint().ToString());
  EXPECT_TRUE(root_window0_test_api.ContainsMouse());
  EXPECT_FALSE(root_window1_test_api.ContainsMouse());
  root_windows[1]->MoveCursorTo(gfx::Point(10, 20));
  EXPECT_EQ("1010,20", Shell::GetScreen()->GetCursorScreenPoint().ToString());
  EXPECT_FALSE(root_window0_test_api.ContainsMouse());
  EXPECT_TRUE(root_window1_test_api.ContainsMouse());
  root_windows[0]->MoveCursorTo(gfx::Point(20, 10));
  EXPECT_EQ("20,10", Shell::GetScreen()->GetCursorScreenPoint().ToString());
  EXPECT_TRUE(root_window0_test_api.ContainsMouse());
  EXPECT_FALSE(root_window1_test_api.ContainsMouse());
}

TEST_F(ExtendedDesktopTest, CycleWindows) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("700x500,500x500");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  views::Widget* d1_w1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ(root_windows[0], d1_w1->GetNativeView()->GetRootWindow());
  views::Widget* d2_w1 = CreateTestWidget(gfx::Rect(800, 10, 100, 100));
  EXPECT_EQ(root_windows[1], d2_w1->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));

  // Cycle through all windows across root windows.
  views::Widget* d1_w2 = CreateTestWidget(gfx::Rect(10, 200, 100, 100));
  EXPECT_EQ(root_windows[0], d1_w2->GetNativeView()->GetRootWindow());
  views::Widget* d2_w2 = CreateTestWidget(gfx::Rect(800, 200, 100, 100));
  EXPECT_EQ(root_windows[1], d2_w2->GetNativeView()->GetRootWindow());

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w2->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w2->GetNativeView()));

  // Backwards
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w1->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d1_w2->GetNativeView()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(d2_w2->GetNativeView()));
}

TEST_F(ExtendedDesktopTest, GetRootWindowAt) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("700x500,500x500");
  SetSecondaryDisplayLayout(DisplayLayout::LEFT);
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  EXPECT_EQ(root_windows[1], wm::GetRootWindowAt(gfx::Point(-400, 100)));
  EXPECT_EQ(root_windows[1], wm::GetRootWindowAt(gfx::Point(-1, 100)));
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(0, 300)));
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(700,300)));

  // Zero origin.
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(0, 0)));

  // Out of range point should return the primary root window
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(-600, 0)));
  EXPECT_EQ(root_windows[0], wm::GetRootWindowAt(gfx::Point(701, 100)));
}

TEST_F(ExtendedDesktopTest, GetRootWindowMatching) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("700x500,500x500");
  SetSecondaryDisplayLayout(DisplayLayout::LEFT);

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // Containing rect.
  EXPECT_EQ(root_windows[1],
            wm::GetRootWindowMatching(gfx::Rect(-300, 10, 50, 50)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(100, 10, 50, 50)));

  // Intersecting rect.
  EXPECT_EQ(root_windows[1],
            wm::GetRootWindowMatching(gfx::Rect(-200, 0, 300, 300)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(-100, 0, 300, 300)));

  // Zero origin.
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(0, 0, 0, 0)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(0, 0, 1, 1)));

  // Empty rect.
  EXPECT_EQ(root_windows[1],
            wm::GetRootWindowMatching(gfx::Rect(-400, 100, 0, 0)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(100, 100, 0, 0)));

  // Out of range rect should return the primary root window.
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(-600, -300, 50, 50)));
  EXPECT_EQ(root_windows[0],
            wm::GetRootWindowMatching(gfx::Rect(0, 1000, 50, 50)));
}

TEST_F(ExtendedDesktopTest, Capture) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  aura::test::EventCountDelegate r1_d1;
  aura::test::EventCountDelegate r1_d2;
  aura::test::EventCountDelegate r2_d1;

  scoped_ptr<aura::Window> r1_w1(aura::test::CreateTestWindowWithDelegate(
      &r1_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  scoped_ptr<aura::Window> r1_w2(aura::test::CreateTestWindowWithDelegate(
      &r1_d2, 0, gfx::Rect(10, 100, 100, 100), root_windows[0]));
  scoped_ptr<aura::Window> r2_w1(aura::test::CreateTestWindowWithDelegate(
      &r2_d1, 0, gfx::Rect(10, 10, 100, 100), root_windows[1]));

  r1_w1->SetCapture();

  EXPECT_EQ(r1_w1.get(),
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));

  aura::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseToCenterOf(r2_w1.get());
  generator2.ClickLeftButton();
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r2_d1.GetMouseButtonCountsAndReset());
  // The mouse is outside. On chromeos, the mouse is warped to the
  // dest root window, but it's not implemented on Win yet, so
  // no mouse move event on Win.
  EXPECT_EQ("1 1 0", r1_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r1_d1.GetMouseButtonCountsAndReset());
  // Emulate passive grab. (15,15) on 1st display is (-985,15) on 2nd
  // display.
  generator2.MoveMouseTo(-985, 15);
  EXPECT_EQ("0 1 0", r1_d1.GetMouseMotionCountsAndReset());

  r1_w2->SetCapture();
  EXPECT_EQ(r1_w2.get(),
            aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));
  generator2.MoveMouseBy(10, 10);
  generator2.ClickLeftButton();
  EXPECT_EQ("0 0 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r2_d1.GetMouseButtonCountsAndReset());
  // mouse is already entered.
  EXPECT_EQ("0 1 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r1_d2.GetMouseButtonCountsAndReset());
  r1_w2->ReleaseCapture();
  EXPECT_EQ(NULL, aura::client::GetCaptureWindow(r2_w1->GetRootWindow()));
  generator2.MoveMouseTo(15, 15);
  generator2.ClickLeftButton();
  EXPECT_EQ("1 1 0", r2_d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1", r2_d1.GetMouseButtonCountsAndReset());
  // Make sure the mouse_moved_handler_ is properly reset.
  EXPECT_EQ("0 0 0", r1_d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0", r1_d2.GetMouseButtonCountsAndReset());
}

TEST_F(ExtendedDesktopTest, MoveWindow) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  views::Widget* d1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));

  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  d1->SetBounds(gfx::Rect(1010, 10, 100, 100));
  EXPECT_EQ("1010,10 100x100",
            d1->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());

  d1->SetBounds(gfx::Rect(10, 10, 100, 100));
  EXPECT_EQ("10,10 100x100",
            d1->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  // Make sure the bounds which doesn't fit to the root window
  // works correctly.
  d1->SetBounds(gfx::Rect(1560, 30, 100, 100));
  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("1560,30 100x100",
            d1->GetWindowBoundsInScreen().ToString());

  // Setting outside of root windows will be moved to primary root window.
  // TODO(oshima): This one probably should pick the closest root window.
  d1->SetBounds(gfx::Rect(200, 10, 100, 100));
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());
}

// Verifies if the mouse event arrives to the window even when the window
// moves to another root in a pre-target handler.  See: crbug.com/157583
TEST_F(ExtendedDesktopTest, MoveWindowByMouseClick) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(10, 10, 100, 100), root_windows[0]));
  MoveWindowByClickEventHandler event_handler(window.get());
  window->AddPreTargetHandler(&event_handler);

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  event_generator.MoveMouseToCenterOf(window.get());
  event_generator.ClickLeftButton();
  // Both mouse pressed and released arrive at the window and its delegate.
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
  // Also event_handler moves the window to another root at mouse release.
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, MoveWindowToDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x1000,1000x1000");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  gfx::Display display0 = Shell::GetScreen()->GetDisplayMatching(
      root_windows[0]->GetBoundsInScreen());
  gfx::Display display1 = Shell::GetScreen()->GetDisplayMatching(
      root_windows[1]->GetBoundsInScreen());
  EXPECT_NE(display0.id(), display1.id());

  views::Widget* d1 = CreateTestWidget(gfx::Rect(10, 10, 1000, 100));
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());

  // Move the window where the window spans both root windows. Since the second
  // parameter is |display1|, the window should be shown on the secondary root.
  d1->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(500, 10, 1000, 100),
                                           display1);
  EXPECT_EQ("500,10 1000x100",
            d1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(root_windows[1], d1->GetNativeView()->GetRootWindow());

  // Move to the primary root.
  d1->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(500, 10, 1000, 100),
                                           display0);
  EXPECT_EQ("500,10 1000x100",
            d1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(root_windows[0], d1->GetNativeView()->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, MoveWindowWithTransient) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  views::Widget* w1 = CreateTestWidget(gfx::Rect(10, 10, 100, 100));
  views::Widget* w1_t1 = CreateTestWidgetWithParent(
      w1, gfx::Rect(50, 50, 50, 50), false /* transient */);
  // Transient child of the transient child.
  views::Widget* w1_t11 = CreateTestWidgetWithParent(
      w1_t1, gfx::Rect(1200, 70, 30, 30), false /* transient */);

  views::Widget* w11 = CreateTestWidgetWithParent(
      w1, gfx::Rect(10, 10, 40, 40), true /* child */);
  views::Widget* w11_t1 = CreateTestWidgetWithParent(
      w1, gfx::Rect(1300, 100, 80, 80), false /* transient */);

  EXPECT_EQ(root_windows[0], w1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w1_t11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[0], w11_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("50,50 50x50",
            w1_t1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1200,70 30x30",
            w1_t11->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("20,20 40x40",
            w11->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1300,100 80x80",
            w11_t1->GetWindowBoundsInScreen().ToString());

  w1->SetBounds(gfx::Rect(1100,10,100,100));

  EXPECT_EQ(root_windows[1], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w1_t11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w11->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], w11_t1->GetNativeView()->GetRootWindow());

  EXPECT_EQ("1110,20 40x40",
            w11->GetWindowBoundsInScreen().ToString());
  // Transient window's screen bounds stays the same.
  EXPECT_EQ("50,50 50x50",
            w1_t1->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1200,70 30x30",
            w1_t11->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("1300,100 80x80",
            w11_t1->GetWindowBoundsInScreen().ToString());

  // Transient window doesn't move between root window unless
  // its transient parent moves.
  w1_t1->SetBounds(gfx::Rect(10, 50, 50, 50));
  EXPECT_EQ(root_windows[1], w1_t1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("10,50 50x50",
            w1_t1->GetWindowBoundsInScreen().ToString());
}

// Test if the Window::ConvertPointToTarget works across root windows.
// TODO(oshima): Move multiple display suport and this test to aura.
TEST_F(ExtendedDesktopTest, ConvertPoint) {
  if (!SupportsMultipleDisplays())
    return;
  gfx::Screen* screen = Shell::GetInstance()->screen();
  UpdateDisplay("1000x600,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  gfx::Display display_1 = screen->GetDisplayNearestWindow(root_windows[0]);
  EXPECT_EQ("0,0", display_1.bounds().origin().ToString());
  gfx::Display display_2 = screen->GetDisplayNearestWindow(root_windows[1]);
  EXPECT_EQ("1000,0", display_2.bounds().origin().ToString());

  aura::Window* d1 =
      CreateTestWidget(gfx::Rect(10, 10, 100, 100))->GetNativeView();
  aura::Window* d2 =
      CreateTestWidget(gfx::Rect(1020, 20, 100, 100))->GetNativeView();
  EXPECT_EQ(root_windows[0], d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], d2->GetRootWindow());

  // Convert point in the Root2's window to the Root1's window Coord.
  gfx::Point p(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("1000,0", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d2, d1, &p);
  EXPECT_EQ("1010,10", p.ToString());

  // Convert point in the Root1's window to the Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("-1000,0", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d1, d2, &p);
  EXPECT_EQ("-1010,-10", p.ToString());

  // Move the 2nd display to the bottom and test again.
  SetSecondaryDisplayLayout(DisplayLayout::BOTTOM);

  display_2 = screen->GetDisplayNearestWindow(root_windows[1]);
  EXPECT_EQ("0,600", display_2.bounds().origin().ToString());

  // Convert point in Root2's window to Root1's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[1], root_windows[0], &p);
  EXPECT_EQ("0,600", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d2, d1, &p);
  EXPECT_EQ("10,610", p.ToString());

  // Convert point in Root1's window to Root2's window Coord.
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(root_windows[0], root_windows[1], &p);
  EXPECT_EQ("0,-600", p.ToString());
  p.SetPoint(0, 0);
  aura::Window::ConvertPointToTarget(d1, d2, &p);
  EXPECT_EQ("-10,-610", p.ToString());
}

TEST_F(ExtendedDesktopTest, OpenSystemTray) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x600,600x400");
  SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  ASSERT_FALSE(tray->HasSystemBubble());

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  // Opens the tray by a dummy click event and makes sure that adding/removing
  // displays doesn't break anything.
  event_generator.MoveMouseToCenterOf(tray->GetWidget()->GetNativeWindow());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(tray->HasSystemBubble());

  UpdateDisplay("500x600");
  EXPECT_TRUE(tray->HasSystemBubble());
  UpdateDisplay("500x600,600x400");
  EXPECT_TRUE(tray->HasSystemBubble());

  // Closes the tray and again makes sure that adding/removing displays doesn't
  // break anything.
  event_generator.ClickLeftButton();
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(tray->HasSystemBubble());

  UpdateDisplay("500x600");
  EXPECT_FALSE(tray->HasSystemBubble());
  UpdateDisplay("500x600,600x400");
  EXPECT_FALSE(tray->HasSystemBubble());
}

TEST_F(ExtendedDesktopTest, StayInSameRootWindow) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100x100,200x200");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  views::Widget* w1 = CreateTestWidget(gfx::Rect(10, 10, 50, 50));
  EXPECT_EQ(root_windows[0], w1->GetNativeView()->GetRootWindow());
  w1->SetBounds(gfx::Rect(150, 10, 50, 50));
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());

  // The widget stays in the same root if kStayInSameRootWindowKey is set to
  // true.
  w1->GetNativeView()->SetProperty(internal::kStayInSameRootWindowKey, true);
  w1->SetBounds(gfx::Rect(10, 10, 50, 50));
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());

  // The widget should now move to the 1st root window without the property.
  w1->GetNativeView()->ClearProperty(internal::kStayInSameRootWindowKey);
  w1->SetBounds(gfx::Rect(10, 10, 50, 50));
  EXPECT_EQ(root_windows[0], w1->GetNativeView()->GetRootWindow());

  // a window in SettingsBubbleContainer and StatusContainer should
  // not move to another root window regardles of the bounds specified.
  aura::Window* settings_bubble_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          internal::kShellWindowId_SettingBubbleContainer);
  aura::Window* window = aura::test::CreateTestWindowWithId(
      100, settings_bubble_container);
  window->SetBoundsInScreen(gfx::Rect(150, 10, 50, 50),
                            ScreenAsh::GetSecondaryDisplay());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  aura::Window* status_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          internal::kShellWindowId_StatusContainer);
  window = aura::test::CreateTestWindowWithId(100, status_container);
  window->SetBoundsInScreen(gfx::Rect(150, 10, 50, 50),
                            ScreenAsh::GetSecondaryDisplay());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
}

TEST_F(ExtendedDesktopTest, KeyEventsOnLockScreen) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100x100,200x200");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  // Create normal windows on both displays.
  views::Widget* widget1 = CreateTestWidget(
      Shell::GetScreen()->GetPrimaryDisplay().bounds());
  widget1->Show();
  EXPECT_EQ(root_windows[0], widget1->GetNativeView()->GetRootWindow());
  views::Widget* widget2 = CreateTestWidget(
      ScreenAsh::GetSecondaryDisplay().bounds());
  widget2->Show();
  EXPECT_EQ(root_windows[1], widget2->GetNativeView()->GetRootWindow());

  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWidget(
      Shell::GetScreen()->GetPrimaryDisplay().bounds());
  views::Textfield* textfield = new views::Textfield;
  lock_widget->client_view()->AddChildView(textfield);

  ash::Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_LockScreenContainer)->
      AddChild(lock_widget->GetNativeView());
  lock_widget->Show();
  textfield->RequestFocus();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(root_windows[0]);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());

  // The lock window should get events on both root windows.
  aura::test::EventGenerator& event_generator(GetEventGenerator());

  event_generator.set_current_root_window(root_windows[0]);
  event_generator.PressKey(ui::VKEY_A, 0);
  event_generator.ReleaseKey(ui::VKEY_A, 0);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("a", UTF16ToASCII(textfield->text()));

  event_generator.set_current_root_window(root_windows[1]);
  event_generator.PressKey(ui::VKEY_B, 0);
  event_generator.ReleaseKey(ui::VKEY_B, 0);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("ab", UTF16ToASCII(textfield->text()));

  // Deleting 2nd display. The lock window still should get the events.
  UpdateDisplay("100x100");
  event_generator.PressKey(ui::VKEY_C, 0);
  event_generator.ReleaseKey(ui::VKEY_C, 0);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("abc", UTF16ToASCII(textfield->text()));

  // Creating 2nd display again, and lock window still should get events
  // on both root windows.
  UpdateDisplay("100x100,200x200");
  root_windows = Shell::GetAllRootWindows();
  event_generator.set_current_root_window(root_windows[0]);
  event_generator.PressKey(ui::VKEY_D, 0);
  event_generator.ReleaseKey(ui::VKEY_D, 0);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("abcd", UTF16ToASCII(textfield->text()));

  event_generator.set_current_root_window(root_windows[1]);
  event_generator.PressKey(ui::VKEY_E, 0);
  event_generator.ReleaseKey(ui::VKEY_E, 0);
  EXPECT_EQ(lock_widget->GetNativeView(), focus_client->GetFocusedWindow());
  EXPECT_EQ("abcde", UTF16ToASCII(textfield->text()));
}

TEST_F(ExtendedDesktopTest, PassiveGrab) {
  if (!SupportsMultipleDisplays())
    return;

  EventLocationRecordingEventHandler event_handler;
  ash::Shell::GetInstance()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("300x300,200x200");

  views::Widget* widget = CreateTestWidget(gfx::Rect(50, 50, 200, 200));
  widget->Show();
  ASSERT_EQ("50,50 200x200", widget->GetWindowBoundsInScreen().ToString());

  aura::test::EventGenerator& generator(GetEventGenerator());
  generator.MoveMouseTo(150, 150);
  EXPECT_EQ("100,100 150,150", event_handler.GetLocationsAndReset());

  generator.PressLeftButton();
  generator.MoveMouseTo(400, 150);

  EXPECT_EQ("350,100 400,150", event_handler.GetLocationsAndReset());

  generator.ReleaseLeftButton();
  EXPECT_EQ("-999,-999 -999,-999", event_handler.GetLocationsAndReset());

  generator.MoveMouseTo(400, 150);
  EXPECT_EQ("100,150 100,150", event_handler.GetLocationsAndReset());

  ash::Shell::GetInstance()->RemovePreTargetHandler(&event_handler);
}

}  // namespace ash
