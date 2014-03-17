// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/alternate_frame_size_button.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/caption_buttons/frame_caption_button.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/command_line.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

namespace {

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {}
  virtual ~TestWidgetDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }

  ash::FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE {
    caption_button_container_->Layout();

    // Right align the caption button container.
    gfx::Size preferred_size = caption_button_container_->GetPreferredSize();
    caption_button_container_->SetBounds(width() - preferred_size.width(), 0,
        preferred_size.width(), preferred_size.height());
  }

  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    if (details.is_add && details.child == this) {
      caption_button_container_ = new FrameCaptionButtonContainerView(
          GetWidget(), FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
      AddChildView(caption_button_container_);
    }
  }

  // Not owned.
  ash::FrameCaptionButtonContainerView* caption_button_container_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

class AlternateFrameSizeButtonTest : public AshTestBase {
 public:
  AlternateFrameSizeButtonTest() {}
  virtual ~AlternateFrameSizeButtonTest() {}

  // Returns the center point of |view| in screen coordinates.
  gfx::Point CenterPointInScreen(views::View* view) {
    return view->GetBoundsInScreen().CenterPoint();
  }

  // Returns true if the window is snapped to |edge|.
  bool IsSnapped(internal::SnapSizer::Edge edge) const {
    ash::wm::WindowShowType show_type = window_state()->window_show_type();
    if (edge == internal::SnapSizer::LEFT_EDGE)
      return show_type == ash::wm::SHOW_TYPE_LEFT_SNAPPED;
    else
      return show_type == ash::wm::SHOW_TYPE_RIGHT_SNAPPED;
  }

  // Returns true if all three buttons are in the normal state.
  bool AllButtonsInNormalState() const {
    return minimize_button_->state() == views::Button::STATE_NORMAL &&
        size_button_->state() == views::Button::STATE_NORMAL &&
        close_button_->state() == views::Button::STATE_NORMAL;
  }

  // Creates a widget with |delegate|. The returned widget takes ownership of
  // |delegate|.
  views::Widget* CreateWidget(views::WidgetDelegate* delegate) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.context = CurrentContext();
    params.delegate = delegate;
    params.bounds = gfx::Rect(10, 10, 100, 100);
    widget->Init(params);
    widget->Show();
    return widget;
  }

  // AshTestBase overrides:
  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(
        switches::kAshEnableAlternateFrameCaptionButtonStyle);
    CHECK(!command_line->HasSwitch(switches::kAshMultipleSnapWindowWidths));

    TestWidgetDelegate* delegate = new TestWidgetDelegate();
    window_state_ = ash::wm::GetWindowState(
        CreateWidget(delegate)->GetNativeWindow());

    FrameCaptionButtonContainerView::TestApi test(
        delegate->caption_button_container());

    minimize_button_ = test.minimize_button();
    size_button_ = test.size_button();
    static_cast<AlternateFrameSizeButton*>(
        size_button_)->set_delay_to_set_buttons_to_snap_mode(0);
    close_button_ = test.close_button();
  }

  ash::wm::WindowState* window_state() { return window_state_; }
  const ash::wm::WindowState* window_state() const { return window_state_; }

  FrameCaptionButton* minimize_button() { return minimize_button_; }
  FrameCaptionButton* size_button() { return size_button_; }
  FrameCaptionButton* close_button() { return close_button_; }

 private:
  // Not owned.
  ash::wm::WindowState* window_state_;
  FrameCaptionButton* minimize_button_;
  FrameCaptionButton* size_button_;
  FrameCaptionButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(AlternateFrameSizeButtonTest);
};

// Tests that pressing the left mouse button or tapping down on the size button
// puts the button into the pressed state.
TEST_F(AlternateFrameSizeButtonTest, PressedState) {
  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressLeftButton();
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->state());

  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressTouchId(3);
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  generator.ReleaseTouchId(3);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->state());
}

// Tests that clicking on the size button toggles between the maximized and
// normal state.
TEST_F(AlternateFrameSizeButtonTest, ClickSizeButtonTogglesMaximize) {
  EXPECT_FALSE(window_state()->IsMaximized());

  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(window_state()->IsMaximized());

  generator.GestureTapAt(CenterPointInScreen(size_button()));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator.GestureTapAt(CenterPointInScreen(size_button()));
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(window_state()->IsMaximized());
}

// Test that clicking + dragging to a button adjacent to the size button snaps
// the window left or right.
TEST_F(AlternateFrameSizeButtonTest, ButtonDrag) {
  EXPECT_TRUE(window_state()->IsNormalShowState());
  EXPECT_FALSE(window_state()->IsSnapped());

  // 1) Test by dragging the mouse.
  // Snap right.
  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressLeftButton();
  generator.MoveMouseTo(CenterPointInScreen(close_button()));
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::RIGHT_EDGE));

  // Snap left.
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressLeftButton();
  generator.MoveMouseTo(CenterPointInScreen(minimize_button()));
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::LEFT_EDGE));

  // 2) Test with scroll gestures.
  // Snap right.
  generator.GestureScrollSequence(
      CenterPointInScreen(size_button()),
      CenterPointInScreen(close_button()),
      base::TimeDelta::FromMilliseconds(100),
      3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::RIGHT_EDGE));

  // Snap left.
  generator.GestureScrollSequence(
      CenterPointInScreen(size_button()),
      CenterPointInScreen(minimize_button()),
      base::TimeDelta::FromMilliseconds(100),
      3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::LEFT_EDGE));

  // 3) Test with tap gestures.
  const int touch_default_radius =
      ui::GestureConfiguration::default_radius();
  ui::GestureConfiguration::set_default_radius(0);
  // Snap right.
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressMoveAndReleaseTouchTo(CenterPointInScreen(close_button()));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::RIGHT_EDGE));
  // Snap left.
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressMoveAndReleaseTouchTo(CenterPointInScreen(minimize_button()));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::LEFT_EDGE));
  ui::GestureConfiguration::set_default_radius(touch_default_radius);
}

// Test that clicking, dragging, and overshooting the minimize button a bit
// horizontally still snaps the window left.
TEST_F(AlternateFrameSizeButtonTest, SnapLeftOvershootMinimize) {
  EXPECT_TRUE(window_state()->IsNormalShowState());
  EXPECT_FALSE(window_state()->IsSnapped());

  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(CenterPointInScreen(size_button()));

  generator.PressLeftButton();
  // Move to the minimize button.
  generator.MoveMouseTo(CenterPointInScreen(minimize_button()));
  // Overshoot the minimize button.
  generator.MoveMouseBy(-minimize_button()->width(), 0);
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::LEFT_EDGE));
}

// Test that right clicking the size button has no effect.
TEST_F(AlternateFrameSizeButtonTest, RightMouseButton) {
  EXPECT_TRUE(window_state()->IsNormalShowState());
  EXPECT_FALSE(window_state()->IsSnapped());

  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressRightButton();
  generator.ReleaseRightButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window_state()->IsNormalShowState());
  EXPECT_FALSE(window_state()->IsSnapped());
}

// Test that upon releasing the mouse button after having pressed the size
// button
// - The state of all the caption buttons is reset.
// - The icon displayed by all of the caption buttons is reset.
TEST_F(AlternateFrameSizeButtonTest, ResetButtonsAfterClick) {
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Dragging the mouse over the minimize button should press the minimize
  // button and the minimize and close button icons should stay changed.
  generator.MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_PRESSED, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Release the mouse, snapping the window left.
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::LEFT_EDGE));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());

  // Repeat test but release button where it does not affect the window's state
  // because the code path is different.
  generator.MoveMouseTo(CenterPointInScreen(size_button()));
  generator.PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  const gfx::Rect& kWorkAreaBoundsInScreen =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  generator.MoveMouseTo(kWorkAreaBoundsInScreen.bottom_left());

  // None of the buttons should be pressed because we are really far away from
  // any of the caption buttons. The minimize and close button icons should
  // be changed because the mouse is pressed.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Release the mouse. The window should stay snapped left.
  generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSnapped(internal::SnapSizer::LEFT_EDGE));

  // The buttons should stay unpressed and the buttons should now have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
}

}  // namespace test
}  // namespace ash
