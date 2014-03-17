// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_fullscreen_controller.h"

#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// For now, immersive fullscreen is Chrome OS only.
#if defined(OS_CHROMEOS)

namespace ash {

namespace {

class MockImmersiveFullscreenControllerDelegate
    : public ImmersiveFullscreenController::Delegate {
 public:
  MockImmersiveFullscreenControllerDelegate(views::View* top_container_view)
      : top_container_view_(top_container_view),
        enabled_(false),
        visible_fraction_(1) {
  }
  virtual ~MockImmersiveFullscreenControllerDelegate() {}

  // ImmersiveFullscreenController::Delegate overrides:
  virtual void OnImmersiveRevealStarted() OVERRIDE {
    enabled_ = true;
    visible_fraction_ = 0;
  }
  virtual void OnImmersiveRevealEnded() OVERRIDE {
    visible_fraction_ = 0;
  }
  virtual void OnImmersiveFullscreenExited() OVERRIDE {
    enabled_ = false;
    visible_fraction_ = 1;
  }
  virtual void SetVisibleFraction(double visible_fraction) OVERRIDE {
    visible_fraction_ = visible_fraction;
  }
  virtual std::vector<gfx::Rect> GetVisibleBoundsInScreen() const OVERRIDE {
    std::vector<gfx::Rect> bounds_in_screen;
    bounds_in_screen.push_back(top_container_view_->GetBoundsInScreen());
    return bounds_in_screen;
  }

  bool is_enabled() const {
    return enabled_;
  }

  double visible_fraction() const {
    return visible_fraction_;
  }

 private:
  views::View* top_container_view_;
  bool enabled_;
  double visible_fraction_;

  DISALLOW_COPY_AND_ASSIGN(MockImmersiveFullscreenControllerDelegate);
};

}  // namespace

/////////////////////////////////////////////////////////////////////////////

class ImmersiveFullscreenControllerTest : public ash::test::AshTestBase {
 public:
  enum Modality {
    MODALITY_MOUSE,
    MODALITY_TOUCH,
    MODALITY_GESTURE
  };

  ImmersiveFullscreenControllerTest() : widget_(NULL), top_container_(NULL) {}
  virtual ~ImmersiveFullscreenControllerTest() {}

  ImmersiveFullscreenController* controller() {
    return controller_.get();
  }

  views::View* top_container() {
    return top_container_;
  }

  aura::Window* window() {
    return widget_->GetNativeWindow();
  }

  MockImmersiveFullscreenControllerDelegate* delegate() {
    return delegate_.get();
  }

  // Access to private data from the controller.
  bool top_edge_hover_timer_running() const {
    return controller_->top_edge_hover_timer_.IsRunning();
  }
  int mouse_x_when_hit_top() const {
    return controller_->mouse_x_when_hit_top_in_screen_;
  }

  // ash::test::AshTestBase overrides:
  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();

    widget_ = new views::Widget();
    views::Widget::InitParams params;
    params.context = CurrentContext();
    widget_->Init(params);
    widget_->Show();

    window()->SetProperty(aura::client::kShowStateKey,
                          ui::SHOW_STATE_FULLSCREEN);

    top_container_ = new views::View();
    top_container_->SetBounds(
        0, 0, widget_->GetWindowBoundsInScreen().width(), 100);
    top_container_->SetFocusable(true);
    widget_->GetContentsView()->AddChildView(top_container_);

    delegate_.reset(
        new MockImmersiveFullscreenControllerDelegate(top_container_));
    controller_.reset(new ImmersiveFullscreenController);
    controller_->Init(delegate_.get(), widget_, top_container_);
    controller_->SetupForTest();

    // The mouse is moved so that it is not over |top_container_| by
    // AshTestBase.
  }

  // Enables / disables immersive fullscreen.
  void SetEnabled(bool enabled) {
    controller_->SetEnabled(ImmersiveFullscreenController::WINDOW_TYPE_OTHER,
                            enabled);
  }

  // Attempt to reveal the top-of-window views via |modality|.
  // The top-of-window views can only be revealed via mouse hover or a gesture.
  void AttemptReveal(Modality modality) {
    ASSERT_NE(modality, MODALITY_TOUCH);
    AttemptRevealStateChange(true, modality);
  }

  // Attempt to unreveal the top-of-window views via |modality|. The
  // top-of-window views can be unrevealed via any modality.
  void AttemptUnreveal(Modality modality) {
    AttemptRevealStateChange(false, modality);
  }

  // Sets whether the mouse is hovered above |top_container_|.
  // SetHovered(true) moves the mouse over the |top_container_| but does not
  // move it to the top of the screen so will not initiate a reveal.
  void SetHovered(bool is_mouse_hovered) {
    MoveMouse(0, is_mouse_hovered ? 10 : top_container_->height() + 100);
  }

  // Move the mouse to the given coordinates. The coordinates should be in
  // |top_container_| coordinates.
  void MoveMouse(int x, int y) {
    gfx::Point screen_position(x, y);
    views::View::ConvertPointToScreen(top_container_, &screen_position);
    GetEventGenerator().MoveMouseTo(screen_position.x(), screen_position.y());

    // If the top edge timer started running as a result of the mouse move, run
    // the task which occurs after the timer delay. This reveals the
    // top-of-window views synchronously if the mouse is hovered at the top of
    // the screen.
    if (controller()->top_edge_hover_timer_.IsRunning()) {
      controller()->top_edge_hover_timer_.user_task().Run();
      controller()->top_edge_hover_timer_.Stop();
    }
  }

 private:
  // Attempt to change the revealed state to |revealed| via |modality|.
  void AttemptRevealStateChange(bool revealed, Modality modality) {
    // Compute the event position in |top_container_| coordinates.
    gfx::Point event_position(0, revealed ? 0 : top_container_->height() + 100);
    switch (modality) {
      case MODALITY_MOUSE: {
        MoveMouse(event_position.x(), event_position.y());
        break;
      }
      case MODALITY_TOUCH: {
        gfx::Point screen_position = event_position;
        views::View::ConvertPointToScreen(top_container_, &screen_position);

        aura::test::EventGenerator& event_generator(GetEventGenerator());
        event_generator.MoveTouch(event_position);
        event_generator.PressTouch();
        event_generator.ReleaseTouch();
        break;
      }
      case MODALITY_GESTURE: {
        aura::client::GetCursorClient(CurrentContext())->DisableMouseEvents();
        ImmersiveFullscreenController::SwipeType swipe_type = revealed ?
            ImmersiveFullscreenController::SWIPE_OPEN :
            ImmersiveFullscreenController::SWIPE_CLOSE;
        controller_->UpdateRevealedLocksForSwipe(swipe_type);
        break;
      }
    }
  }

  scoped_ptr<ImmersiveFullscreenController> controller_;
  scoped_ptr<MockImmersiveFullscreenControllerDelegate> delegate_;
  views::Widget* widget_;  // Owned by the native widget.
  views::View* top_container_;  // Owned by |root_view_|.

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenControllerTest);
};

// Test the initial state and that the delegate gets notified of the
// top-of-window views getting hidden and revealed.
TEST_F(ImmersiveFullscreenControllerTest, Delegate) {
  // Initial state.
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(delegate()->is_enabled());

  // Enabling initially hides the top views.
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(delegate()->is_enabled());
  EXPECT_EQ(0, delegate()->visible_fraction());

  // Revealing shows the top views.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(delegate()->is_enabled());
  EXPECT_EQ(1, delegate()->visible_fraction());

  // Disabling ends the immersive reveal.
  SetEnabled(false);
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(delegate()->is_enabled());
}

// GetRevealedLock() specific tests.
TEST_F(ImmersiveFullscreenControllerTest, RevealedLock) {
  scoped_ptr<ImmersiveRevealedLock> lock1;
  scoped_ptr<ImmersiveRevealedLock> lock2;

  // Immersive fullscreen is not on by default.
  EXPECT_FALSE(controller()->IsEnabled());

  // 1) Test acquiring and releasing a revealed state lock while immersive
  // fullscreen is disabled. Acquiring or releasing the lock should have no
  // effect till immersive fullscreen is enabled.
  lock1.reset(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Immersive fullscreen should start in the revealed state due to the lock.
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(controller()->IsRevealed());

  SetEnabled(false);
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  lock1.reset();
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Immersive fullscreen should start in the closed state because the lock is
  // no longer held.
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that acquiring a lock reveals the top-of-window views if they are
  // hidden.
  lock1.reset(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());

  // 3) Test that the top-of-window views are only hidden when all of the locks
  // are released.
  lock2.reset(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  lock1.reset();
  EXPECT_TRUE(controller()->IsRevealed());

  lock2.reset();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test mouse event processing for top-of-screen reveal triggering.
TEST_F(ImmersiveFullscreenControllerTest, OnMouseEvent) {
  // Set up initial state.
  UpdateDisplay("800x600,800x600");
  ash::DisplayLayout display_layout(ash::DisplayLayout::RIGHT, 0);
  ash::Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      display_layout);

  // Set up initial state.
  SetEnabled(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  gfx::Rect top_container_bounds_in_screen =
      top_container()->GetBoundsInScreen();
  // A position along the top edge of TopContainerView in screen coordinates.
  gfx::Point top_edge_pos(top_container_bounds_in_screen.x() + 100,
                          top_container_bounds_in_screen.y());

  // Mouse wheel event does nothing.
  ui::MouseEvent wheel(
      ui::ET_MOUSEWHEEL, top_edge_pos, top_edge_pos, ui::EF_NONE);
  event_generator.Dispatch(&wheel);
  EXPECT_FALSE(top_edge_hover_timer_running());

  // Move to top edge of screen starts hover timer running. We cannot use
  // MoveMouse() because MoveMouse() stops the timer if it started running.
  event_generator.MoveMouseTo(top_edge_pos);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Moving |ImmersiveFullscreenControllerTest::kMouseRevealBoundsHeight| down
  // from the top edge stops it.
  event_generator.MoveMouseBy(0, 3);
  EXPECT_FALSE(top_edge_hover_timer_running());

  // Moving back to the top starts the timer again.
  event_generator.MoveMouseTo(top_edge_pos);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Slight move to the right keeps the timer running for the same hit point.
  event_generator.MoveMouseBy(1, 0);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Moving back to the left also keeps the timer running.
  event_generator.MoveMouseBy(-1, 0);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x(), mouse_x_when_hit_top());

  // Large move right restarts the timer (so it is still running) and considers
  // this a new hit at the top.
  event_generator.MoveMouseTo(top_edge_pos.x() + 100, top_edge_pos.y());
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(top_edge_pos.x() + 100, mouse_x_when_hit_top());

  // Moving off the top edge horizontally stops the timer.
  event_generator.MoveMouseTo(top_container_bounds_in_screen.right() + 1,
                              top_container_bounds_in_screen.y());
  EXPECT_FALSE(top_edge_hover_timer_running());

  // Once revealed, a move just a little below the top container doesn't end a
  // reveal.
  AttemptReveal(MODALITY_MOUSE);
  event_generator.MoveMouseTo(top_container_bounds_in_screen.x(),
                              top_container_bounds_in_screen.bottom() + 1);
  EXPECT_TRUE(controller()->IsRevealed());

  // Once revealed, clicking just below the top container ends the reveal.
  event_generator.ClickLeftButton();
  EXPECT_FALSE(controller()->IsRevealed());

  // Moving a lot below the top container ends a reveal.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  event_generator.MoveMouseTo(top_container_bounds_in_screen.x(),
                              top_container_bounds_in_screen.bottom() + 50);
  EXPECT_FALSE(controller()->IsRevealed());

  // The mouse position cannot cause a reveal when the top container's widget
  // has capture.
  views::Widget* widget = top_container()->GetWidget();
  widget->SetCapture(top_container());
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_FALSE(controller()->IsRevealed());
  widget->ReleaseCapture();

  // The mouse position cannot end the reveal while the top container's widget
  // has capture.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  widget->SetCapture(top_container());
  event_generator.MoveMouseTo(top_container_bounds_in_screen.x(),
                              top_container_bounds_in_screen.bottom() + 51);
  EXPECT_TRUE(controller()->IsRevealed());

  // Releasing capture should end the reveal.
  widget->ReleaseCapture();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test mouse event processing for top-of-screen reveal triggering when the
// top container's widget is inactive.
TEST_F(ImmersiveFullscreenControllerTest, Inactive) {
  // Set up initial state.
  views::Widget* popup_widget = views::Widget::CreateWindowWithContextAndBounds(
      NULL,
      CurrentContext(),
      gfx::Rect(0, 0, 200, 200));
  popup_widget->Show();
  ASSERT_FALSE(top_container()->GetWidget()->IsActive());

  SetEnabled(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());

  gfx::Rect top_container_bounds_in_screen =
      top_container()->GetBoundsInScreen();
  gfx::Rect popup_bounds_in_screen = popup_widget->GetWindowBoundsInScreen();
  ASSERT_EQ(top_container_bounds_in_screen.origin().ToString(),
            popup_bounds_in_screen.origin().ToString());
  ASSERT_GT(top_container_bounds_in_screen.right(),
            popup_bounds_in_screen.right());

  // The top-of-window views should stay hidden if the cursor is at the top edge
  // but above an obscured portion of the top-of-window views.
  MoveMouse(popup_bounds_in_screen.x(),
            top_container_bounds_in_screen.y());
  EXPECT_FALSE(controller()->IsRevealed());

  // The top-of-window views should reveal if the cursor is at the top edge and
  // above an unobscured portion of the top-of-window views.
  MoveMouse(top_container_bounds_in_screen.right() - 1,
            top_container_bounds_in_screen.y());
  EXPECT_TRUE(controller()->IsRevealed());

  // The top-of-window views should stay revealed if the cursor is moved off
  // of the top edge.
  MoveMouse(top_container_bounds_in_screen.right() - 1,
            top_container_bounds_in_screen.bottom() - 1);
  EXPECT_TRUE(controller()->IsRevealed());

  // Moving way off of the top-of-window views should end the immersive reveal.
  MoveMouse(top_container_bounds_in_screen.right() - 1,
            top_container_bounds_in_screen.bottom() + 50);
  EXPECT_FALSE(controller()->IsRevealed());

  // Moving way off of the top-of-window views in a region where the
  // top-of-window views are obscured should also end the immersive reveal.
  // Ideally, the immersive reveal would end immediately when the cursor moves
  // to an obscured portion of the top-of-window views.
  MoveMouse(top_container_bounds_in_screen.right() - 1,
            top_container_bounds_in_screen.y());
  EXPECT_TRUE(controller()->IsRevealed());
  MoveMouse(top_container_bounds_in_screen.x(),
            top_container_bounds_in_screen.bottom() + 50);
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test mouse event processing for top-of-screen reveal triggering when the user
// has a vertical display layout (primary display above/below secondary display)
// and the immersive fullscreen window is on the bottom display.
TEST_F(ImmersiveFullscreenControllerTest, MouseEventsVerticalDisplayLayout) {
  if (!SupportsMultipleDisplays())
    return;

  // Set up initial state.
  UpdateDisplay("800x600,800x600");
  ash::DisplayLayout display_layout(ash::DisplayLayout::TOP, 0);
  ash::Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      display_layout);

  SetEnabled(true);
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  ASSERT_EQ(root_windows[0],
            top_container()->GetWidget()->GetNativeWindow()->GetRootWindow());

  gfx::Rect primary_root_window_bounds_in_screen =
      root_windows[0]->GetBoundsInScreen();
  // Do not set |x| to the root window's x position because the display's
  // corners have special behavior.
  int x = primary_root_window_bounds_in_screen.x() + 10;
  // The y position of the top edge of the primary display.
  int y_top_edge = primary_root_window_bounds_in_screen.y();

  aura::test::EventGenerator& event_generator(GetEventGenerator());

  // Moving right below the top edge starts the hover timer running. We
  // cannot use MoveMouse() because MoveMouse() stops the timer if it started
  // running.
  event_generator.MoveMouseTo(x, y_top_edge + 1);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_EQ(y_top_edge + 1,
            aura::Env::GetInstance()->last_mouse_location().y());

  // The timer should continue running if the user moves the mouse to the top
  // edge even though the mouse is warped to the secondary display.
  event_generator.MoveMouseTo(x, y_top_edge);
  EXPECT_TRUE(top_edge_hover_timer_running());
  EXPECT_NE(y_top_edge,
            aura::Env::GetInstance()->last_mouse_location().y());

  // The timer should continue running if the user overshoots the top edge
  // a bit.
  event_generator.MoveMouseTo(x, y_top_edge - 2);
  EXPECT_TRUE(top_edge_hover_timer_running());

  // The timer should stop running if the user overshoots the top edge by
  // a lot.
  event_generator.MoveMouseTo(x, y_top_edge - 20);
  EXPECT_FALSE(top_edge_hover_timer_running());

  // The timer should not start if the user moves the mouse to the bottom of the
  // secondary display without crossing the top edge first.
  event_generator.MoveMouseTo(x, y_top_edge - 2);

  // Reveal the top-of-window views by overshooting the top edge slightly.
  event_generator.MoveMouseTo(x, y_top_edge + 1);
  // MoveMouse() runs the timer task.
  MoveMouse(x, y_top_edge - 2);
  EXPECT_TRUE(controller()->IsRevealed());

  // The top-of-window views should stay revealed if the user moves the mouse
  // around in the bottom region of the secondary display.
  event_generator.MoveMouseTo(x + 10, y_top_edge - 3);
  EXPECT_TRUE(controller()->IsRevealed());

  // The top-of-window views should hide if the user moves the mouse away from
  // the bottom region of the secondary display.
  event_generator.MoveMouseTo(x, y_top_edge - 20);
  EXPECT_FALSE(controller()->IsRevealed());

  // Test that it is possible to reveal the top-of-window views by overshooting
  // the top edge slightly when the top container's widget is not active.
  views::Widget* popup_widget = views::Widget::CreateWindowWithContextAndBounds(
      NULL,
      CurrentContext(),
      gfx::Rect(0, 200, 100, 100));
  popup_widget->Show();
  ASSERT_FALSE(top_container()->GetWidget()->IsActive());
  ASSERT_FALSE(top_container()->GetBoundsInScreen().Intersects(
      popup_widget->GetWindowBoundsInScreen()));
  event_generator.MoveMouseTo(x, y_top_edge + 1);
  MoveMouse(x, y_top_edge - 2);
  EXPECT_TRUE(controller()->IsRevealed());
}

// Test behavior when the mouse becomes hovered without moving.
TEST_F(ImmersiveFullscreenControllerTest, MouseHoveredWithoutMoving) {
  SetEnabled(true);
  scoped_ptr<ImmersiveRevealedLock> lock;

  // 1) Test that if the mouse becomes hovered without the mouse moving due to a
  // lock causing the top-of-window views to be revealed (and the mouse
  // happening to be near the top of the screen), the top-of-window views do not
  // hide till the mouse moves off of the top-of-window views.
  SetHovered(true);
  EXPECT_FALSE(controller()->IsRevealed());
  lock.reset(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_TRUE(controller()->IsRevealed());
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that if the mouse becomes hovered without moving because of a
  // reveal in ImmersiveFullscreenController::SetEnabled(true) and there are no
  // locks keeping the top-of-window views revealed, that mouse hover does not
  // prevent the top-of-window views from closing.
  SetEnabled(false);
  SetHovered(true);
  EXPECT_FALSE(controller()->IsRevealed());
  SetEnabled(true);
  EXPECT_FALSE(controller()->IsRevealed());

  // 3) Test that if the mouse becomes hovered without moving because of a
  // reveal in ImmersiveFullscreenController::SetEnabled(true) and there is a
  // lock keeping the top-of-window views revealed, that the top-of-window views
  // do not hide till the mouse moves off of the top-of-window views.
  SetEnabled(false);
  SetHovered(true);
  lock.reset(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_FALSE(controller()->IsRevealed());
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_TRUE(controller()->IsRevealed());
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test revealing the top-of-window views using one modality and ending
// the reveal via another. For instance, initiating the reveal via a SWIPE_OPEN
// edge gesture, switching to using the mouse and ending the reveal by moving
// the mouse off of the top-of-window views.
TEST_F(ImmersiveFullscreenControllerTest, DifferentModalityEnterExit) {
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via gesture, end reveal via mouse.
  AttemptReveal(MODALITY_GESTURE);
  EXPECT_TRUE(controller()->IsRevealed());
  MoveMouse(1, 1);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_MOUSE);
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via gesture, end reveal via touch.
  AttemptReveal(MODALITY_GESTURE);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_TOUCH);
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via mouse, end reveal via gesture.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_GESTURE);
  EXPECT_FALSE(controller()->IsRevealed());

  // Initiate reveal via mouse, end reveal via touch.
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_TOUCH);
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test when the SWIPE_CLOSE edge gesture closes the top-of-window views.
TEST_F(ImmersiveFullscreenControllerTest, EndRevealViaGesture) {
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // A gesture should be able to close the top-of-window views when
  // top-of-window views have focus.
  AttemptReveal(MODALITY_MOUSE);
  top_container()->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_GESTURE);
  EXPECT_FALSE(controller()->IsRevealed());

  // The top-of-window views should no longer have focus. Clearing focus is
  // important because it closes focus-related popup windows like the touch
  // selection handles.
  EXPECT_FALSE(top_container()->HasFocus());

  // If some other code is holding onto a lock, a gesture should not be able to
  // end the reveal.
  AttemptReveal(MODALITY_MOUSE);
  scoped_ptr<ImmersiveRevealedLock> lock(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());
  AttemptUnreveal(MODALITY_GESTURE);
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Do not test under windows because focus testing is not reliable on
// Windows. (crbug.com/79493)
#if !defined(OS_WIN)

// Test how focus and activation affects whether the top-of-window views are
// revealed.
TEST_F(ImmersiveFullscreenControllerTest, Focus) {
  // Add views to the view hierarchy which we will focus and unfocus during the
  // test.
  views::View* child_view = new views::View();
  child_view->SetBounds(0, 0, 10, 10);
  child_view->SetFocusable(true);
  top_container()->AddChildView(child_view);
  views::View* unrelated_view = new views::View();
  unrelated_view->SetBounds(0, 100, 10, 10);
  unrelated_view->SetFocusable(true);
  top_container()->parent()->AddChildView(unrelated_view);
  views::FocusManager* focus_manager =
      top_container()->GetWidget()->GetFocusManager();

  SetEnabled(true);

  // 1) Test that the top-of-window views stay revealed as long as either a
  // |child_view| has focus or the mouse is hovered above the top-of-window
  // views.
  AttemptReveal(MODALITY_MOUSE);
  child_view->RequestFocus();
  focus_manager->ClearFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  child_view->RequestFocus();
  SetHovered(false);
  EXPECT_TRUE(controller()->IsRevealed());
  focus_manager->ClearFocus();
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that focusing |unrelated_view| hides the top-of-window views.
  // Note: In this test we can cheat and trigger a reveal via focus because
  // the top container does not hide when the top-of-window views are not
  // revealed.
  child_view->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  unrelated_view->RequestFocus();
  EXPECT_FALSE(controller()->IsRevealed());

  // 3) Test that a loss of focus of |child_view| to |unrelated_view|
  // while immersive mode is disabled is properly registered.
  child_view->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  SetEnabled(false);
  EXPECT_FALSE(controller()->IsRevealed());
  unrelated_view->RequestFocus();
  SetEnabled(true);
  EXPECT_FALSE(controller()->IsRevealed());

  // Repeat test but with a revealed lock acquired when immersive mode is
  // disabled because the code path is different.
  child_view->RequestFocus();
  EXPECT_TRUE(controller()->IsRevealed());
  SetEnabled(false);
  scoped_ptr<ImmersiveRevealedLock> lock(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_FALSE(controller()->IsRevealed());
  unrelated_view->RequestFocus();
  SetEnabled(true);
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test how transient windows affect whether the top-of-window views are
// revealed.
TEST_F(ImmersiveFullscreenControllerTest, Transient) {
  views::Widget* top_container_widget = top_container()->GetWidget();

  SetEnabled(true);
  ASSERT_FALSE(controller()->IsRevealed());

  // 1) Test that a transient window which is not a bubble does not trigger a
  // reveal but does keep the top-of-window views revealed if they are already
  // revealed.
  views::Widget::InitParams transient_params;
  transient_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  transient_params.parent = top_container_widget->GetNativeView();
  transient_params.bounds = gfx::Rect(0, 100, 100, 100);
  scoped_ptr<views::Widget> transient_widget(new views::Widget());
  transient_widget->Init(transient_params);

  EXPECT_FALSE(controller()->IsRevealed());
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  transient_widget->Show();
  SetHovered(false);
  EXPECT_TRUE(controller()->IsRevealed());
  transient_widget.reset();
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that activating a non-transient window does not keep the
  // top-of-window views revealed.
  views::Widget::InitParams non_transient_params;
  non_transient_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  non_transient_params.context = top_container_widget->GetNativeView();
  non_transient_params.bounds = gfx::Rect(0, 100, 100, 100);
  scoped_ptr<views::Widget> non_transient_widget(new views::Widget());
  non_transient_widget->Init(non_transient_params);

  EXPECT_FALSE(controller()->IsRevealed());
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());
  non_transient_widget->Show();
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());
}

// Test how bubbles affect whether the top-of-window views are revealed.
TEST_F(ImmersiveFullscreenControllerTest, Bubbles) {
  scoped_ptr<ImmersiveRevealedLock> revealed_lock;
  views::Widget* top_container_widget = top_container()->GetWidget();

  // Add views to the view hierarchy to which we will anchor bubbles.
  views::View* child_view = new views::View();
  child_view->SetBounds(0, 0, 10, 10);
  top_container()->AddChildView(child_view);
  views::View* unrelated_view = new views::View();
  unrelated_view->SetBounds(0, 100, 10, 10);
  top_container()->parent()->AddChildView(unrelated_view);

  SetEnabled(true);
  ASSERT_FALSE(controller()->IsRevealed());

  // 1) Test that a bubble anchored to a child of the top container triggers
  // a reveal and keeps the top-of-window views revealed for the duration of
  // its visibility.
  views::Widget* bubble_widget1(views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE)));
  bubble_widget1->Show();
  EXPECT_TRUE(controller()->IsRevealed());

  // Activating |top_container_widget| will close |bubble_widget1|.
  top_container_widget->Activate();
  AttemptReveal(MODALITY_MOUSE);
  revealed_lock.reset(controller()->GetRevealedLock(
      ImmersiveFullscreenController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());

  views::Widget* bubble_widget2 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE));
  bubble_widget2->Show();
  EXPECT_TRUE(controller()->IsRevealed());
  revealed_lock.reset();
  SetHovered(false);
  EXPECT_TRUE(controller()->IsRevealed());
  bubble_widget2->Close();
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that transitioning from keeping the top-of-window views revealed
  // because of a bubble to keeping the top-of-window views revealed because of
  // mouse hover by activating |top_container_widget| works.
  views::Widget* bubble_widget3 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE));
  bubble_widget3->Show();
  SetHovered(true);
  EXPECT_TRUE(controller()->IsRevealed());
  top_container_widget->Activate();
  EXPECT_TRUE(controller()->IsRevealed());

  // 3) Test that the top-of-window views stay revealed as long as at least one
  // bubble anchored to a child of the top container is visible.
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());

  views::BubbleDelegateView* bubble_delegate4(new views::BubbleDelegateView(
      child_view, views::BubbleBorder::NONE));
  bubble_delegate4->set_use_focusless(true);
  views::Widget* bubble_widget4(views::BubbleDelegateView::CreateBubble(
      bubble_delegate4));
  bubble_widget4->Show();

  views::BubbleDelegateView* bubble_delegate5(new views::BubbleDelegateView(
      child_view, views::BubbleBorder::NONE));
  bubble_delegate5->set_use_focusless(true);
  views::Widget* bubble_widget5(views::BubbleDelegateView::CreateBubble(
      bubble_delegate5));
  bubble_widget5->Show();

  EXPECT_TRUE(controller()->IsRevealed());
  bubble_widget4->Hide();
  EXPECT_TRUE(controller()->IsRevealed());
  bubble_widget5->Hide();
  EXPECT_FALSE(controller()->IsRevealed());
  bubble_widget5->Show();
  EXPECT_TRUE(controller()->IsRevealed());

  // 4) Test that visibility changes which occur while immersive fullscreen is
  // disabled are handled upon reenabling immersive fullscreen.
  SetEnabled(false);
  bubble_widget5->Hide();
  SetEnabled(true);
  EXPECT_FALSE(controller()->IsRevealed());

  // We do not need |bubble_widget4| or |bubble_widget5| anymore, close them.
  bubble_widget4->Close();
  bubble_widget5->Close();

  // 5) Test that a bubble added while immersive fullscreen is disabled is
  // handled upon reenabling immersive fullscreen.
  SetEnabled(false);

  views::Widget* bubble_widget6 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(child_view, views::BubbleBorder::NONE));
  bubble_widget6->Show();

  SetEnabled(true);
  EXPECT_TRUE(controller()->IsRevealed());

  bubble_widget6->Close();

  // 6) Test that a bubble which is not anchored to a child of the
  // TopContainerView does not trigger a reveal or keep the
  // top-of-window views revealed if they are already revealed.
  views::Widget* bubble_widget7 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(unrelated_view, views::BubbleBorder::NONE));
  bubble_widget7->Show();
  EXPECT_FALSE(controller()->IsRevealed());

  // Activating |top_container_widget| will close |bubble_widget6|.
  top_container_widget->Activate();
  AttemptReveal(MODALITY_MOUSE);
  EXPECT_TRUE(controller()->IsRevealed());

  views::Widget* bubble_widget8 = views::BubbleDelegateView::CreateBubble(
      new views::BubbleDelegateView(unrelated_view, views::BubbleBorder::NONE));
  bubble_widget8->Show();
  SetHovered(false);
  EXPECT_FALSE(controller()->IsRevealed());
  bubble_widget8->Close();
}

#endif  // defined(OS_WIN)

// Test that the shelf is set to auto hide as long as the window is in
// immersive fullscreen and that the shelf's state before entering immersive
// fullscreen is restored upon exiting immersive fullscreen.
TEST_F(ImmersiveFullscreenControllerTest, Shelf) {
  ash::internal::ShelfLayoutManager* shelf =
      ash::Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();

  // Shelf is visible by default.
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_FALSE(controller()->IsEnabled());
  ASSERT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // Entering immersive fullscreen sets the shelf to auto hide.
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  SetEnabled(true);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive fullscreen puts it back.
  SetEnabled(false);
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // The user could toggle the shelf auto-hide behavior.
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Entering immersive fullscreen keeps auto-hide.
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  SetEnabled(true);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive fullscreen maintains the user's auto-hide selection.
  SetEnabled(false);
  window()->SetProperty(aura::client::kShowStateKey,
                        ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
}

}  // namespase ash

#endif  // defined(OS_CHROMEOS)
