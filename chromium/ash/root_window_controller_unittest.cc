// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::Window;
using views::Widget;

namespace ash {
namespace {

class TestDelegate : public views::WidgetDelegateView {
 public:
  explicit TestDelegate(bool system_modal) : system_modal_(system_modal) {}
  virtual ~TestDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  virtual ui::ModalType GetModalType() const OVERRIDE {
    return system_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_NONE;
  }

 private:
  bool system_modal_;
  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class DeleteOnBlurDelegate : public aura::test::TestWindowDelegate,
                             public aura::client::FocusChangeObserver {
 public:
  DeleteOnBlurDelegate() : window_(NULL) {}
  virtual ~DeleteOnBlurDelegate() {}

  void SetWindow(aura::Window* window) {
    window_ = window;
    aura::client::SetFocusChangeObserver(window_, this);
  }

 private:
  // aura::test::TestWindowDelegate overrides:
  virtual bool CanFocus() OVERRIDE {
    return true;
  }

  // aura::client::FocusChangeObserver implementation:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) OVERRIDE {
    if (window_ == lost_focus)
      delete window_;
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnBlurDelegate);
};

}  // namespace

namespace test {

class RootWindowControllerTest : public test::AshTestBase {
 public:
  views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
    views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
        NULL, CurrentContext(), bounds);
    widget->Show();
    return widget;
  }

  views::Widget* CreateModalWidget(const gfx::Rect& bounds) {
    views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
        new TestDelegate(true), CurrentContext(), bounds);
    widget->Show();
    return widget;
  }

  views::Widget* CreateModalWidgetWithParent(const gfx::Rect& bounds,
                                             gfx::NativeWindow parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParentAndBounds(new TestDelegate(true),
                                                       parent,
                                                       bounds);
    widget->Show();
    return widget;
  }

  aura::Window* GetModalContainer(aura::RootWindow* root_window) {
    return Shell::GetContainer(
        root_window,
        ash::internal::kShellWindowId_SystemModalContainer);
  }
};

TEST_F(RootWindowControllerTest, MoveWindows_Basic) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,500x500");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  internal::RootWindowController* controller =
      Shell::GetPrimaryRootWindowController();
  internal::ShelfLayoutManager* shelf_layout_manager =
      controller->GetShelfLayoutManager();
  shelf_layout_manager->SetAutoHideBehavior(
      ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  views::Widget* normal = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("650,10 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("50,10 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* maximized = CreateTestWidget(gfx::Rect(700, 10, 100, 100));
  maximized->Maximize();
  EXPECT_EQ(root_windows[1], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("600,0 500x452", maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 500x452",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* minimized = CreateTestWidget(gfx::Rect(800, 10, 100, 100));
  minimized->Minimize();
  EXPECT_EQ(root_windows[1], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("800,10 100x100",
            minimized->GetWindowBoundsInScreen().ToString());

  views::Widget* fullscreen = CreateTestWidget(gfx::Rect(900, 10, 100, 100));
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[1], fullscreen->GetNativeView()->GetRootWindow());

  EXPECT_EQ("600,0 500x500",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 500x500",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* unparented_control = new Widget;
  Widget::InitParams params;
  params.bounds = gfx::Rect(650, 10, 100, 100);
  params.context = CurrentContext();
  params.type = Widget::InitParams::TYPE_CONTROL;
  unparented_control->Init(params);
  EXPECT_EQ(root_windows[1],
            unparented_control->GetNativeView()->GetRootWindow());
  EXPECT_EQ(internal::kShellWindowId_UnparentedControlContainer,
            unparented_control->GetNativeView()->parent()->id());

  aura::Window* panel = CreateTestWindowInShellWithDelegateAndType(
      NULL, aura::client::WINDOW_TYPE_PANEL, 0, gfx::Rect(700, 100, 100, 100));
  EXPECT_EQ(root_windows[1], panel->GetRootWindow());
  EXPECT_EQ(internal::kShellWindowId_PanelContainer, panel->parent()->id());

  // Make sure a window that will delete itself when losing focus
  // will not crash.
  aura::WindowTracker tracker;
  DeleteOnBlurDelegate delete_on_blur_delegate;
  aura::Window* d2 = CreateTestWindowInShellWithDelegate(
      &delete_on_blur_delegate, 0, gfx::Rect(50, 50, 100, 100));
  delete_on_blur_delegate.SetWindow(d2);
  aura::client::GetFocusClient(root_windows[0])->FocusWindow(d2);
  tracker.Add(d2);

  UpdateDisplay("600x600");

  // d2 must have been deleted.
  EXPECT_FALSE(tracker.Contains(d2));

  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("50,10 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("50,10 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Maximized area on primary display has 3px (given as
  // kAutoHideSize in shelf_layout_manager.cc) inset at the bottom.

  // First clear fullscreen status, since both fullscreen and maximized windows
  // share the same desktop workspace, which cancels the shelf status.
  fullscreen->SetFullscreen(false);
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("0,0 600x597",
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x597",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Set fullscreen to true. In that case the 3px inset becomes invisible so
  // the maximized window can also use the area fully.
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("0,0 600x600",
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x600",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  EXPECT_EQ(root_windows[0], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("200,10 100x100",
            minimized->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], fullscreen->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(fullscreen->IsFullscreen());
  EXPECT_EQ("0,0 600x600",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x600",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the restore bounds are correctly updated.
  wm::RestoreWindow(maximized->GetNativeView());
  EXPECT_EQ("100,10 100x100", maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("100,10 100x100",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  fullscreen->SetFullscreen(false);
  EXPECT_EQ("300,10 100x100",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("300,10 100x100",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the unparented widget has moved.
  EXPECT_EQ(root_windows[0],
            unparented_control->GetNativeView()->GetRootWindow());
  EXPECT_EQ(internal::kShellWindowId_UnparentedControlContainer,
            unparented_control->GetNativeView()->parent()->id());

  // Test if the panel has moved.
  EXPECT_EQ(root_windows[0], panel->GetRootWindow());
  EXPECT_EQ(internal::kShellWindowId_PanelContainer, panel->parent()->id());
}

TEST_F(RootWindowControllerTest, MoveWindows_Modal) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x500,500x500");

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  // Emulate virtual screen coordinate system.
  root_windows[0]->SetBounds(gfx::Rect(0, 0, 500, 500));
  root_windows[1]->SetBounds(gfx::Rect(500, 0, 500, 500));

  views::Widget* normal = CreateTestWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(normal->GetNativeView()));

  views::Widget* modal = CreateModalWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(GetModalContainer(root_windows[1])->Contains(
      modal->GetNativeView()));
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  aura::test::EventGenerator generator_1st(root_windows[0]);
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  UpdateDisplay("500x500");
  EXPECT_EQ(root_windows[0], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
}

TEST_F(RootWindowControllerTest, ModalContainer) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();
  internal::RootWindowController* controller =
      shell->GetPrimaryRootWindowController();
  EXPECT_EQ(user::LOGGED_IN_USER,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  views::Widget* session_modal_widget =
      CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              session_modal_widget->GetNativeView()));

  shell->session_state_delegate()->LockScreen();
  EXPECT_EQ(user::LOGGED_IN_LOCKED,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  aura::Window* lock_container =
      Shell::GetContainer(controller->root_window(),
                          internal::kShellWindowId_LockScreenContainer);
  views::Widget* lock_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              lock_modal_widget->GetNativeView()));
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_SystemModalContainer)->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));

  shell->session_state_delegate()->UnlockScreen();
}

TEST_F(RootWindowControllerTest, ModalContainerNotLoggedInLoggedIn) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();

  // Configure login screen environment.
  SetUserLoggedIn(false);
  EXPECT_EQ(user::LOGGED_IN_NONE,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(0, shell->session_state_delegate()->NumberOfLoggedInUsers());
  EXPECT_FALSE(shell->session_state_delegate()->IsActiveUserSessionStarted());

  internal::RootWindowController* controller =
      shell->GetPrimaryRootWindowController();
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  aura::Window* lock_container =
      Shell::GetContainer(controller->root_window(),
                          internal::kShellWindowId_LockScreenContainer);
  views::Widget* login_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              login_modal_widget->GetNativeView()));
  login_modal_widget->Close();

  // Configure user session environment.
  SetUserLoggedIn(true);
  SetSessionStarted(true);
  EXPECT_EQ(user::LOGGED_IN_USER,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(1, shell->session_state_delegate()->NumberOfLoggedInUsers());
  EXPECT_TRUE(shell->session_state_delegate()->IsActiveUserSessionStarted());
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  views::Widget* session_modal_widget =
        CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              session_modal_widget->GetNativeView()));
}

TEST_F(RootWindowControllerTest, ModalContainerBlockedSession) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();
  internal::RootWindowController* controller =
      shell->GetPrimaryRootWindowController();
  aura::Window* lock_container =
      Shell::GetContainer(controller->root_window(),
                          internal::kShellWindowId_LockScreenContainer);
  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS;
       ++block_reason) {
    views::Widget* session_modal_widget =
          CreateModalWidget(gfx::Rect(300, 10, 100, 100));
    EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_SystemModalContainer)->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));
    EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_SystemModalContainer)->layout_manager(),
            controller->GetSystemModalLayoutManager(NULL));
    session_modal_widget->Close();

    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));

    EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
            controller->GetSystemModalLayoutManager(NULL));

    views::Widget* lock_modal_widget =
        CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100),
                                    lock_container);
    EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
              controller->GetSystemModalLayoutManager(
                  lock_modal_widget->GetNativeView()));

    session_modal_widget =
          CreateModalWidget(gfx::Rect(300, 10, 100, 100));
    EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_SystemModalContainer)->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));
    session_modal_widget->Close();

    lock_modal_widget->Close();
    UnblockUserSession();
  }
}

// Test that GetFullscreenWindow() returns a fullscreen window only if the
// fullscreen window is in the active workspace.
TEST_F(RootWindowControllerTest, GetFullscreenWindow) {
  UpdateDisplay("600x600");
  internal::RootWindowController* controller =
      Shell::GetInstance()->GetPrimaryRootWindowController();

  Widget* w1 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w1->Maximize();
  Widget* w2 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w2->SetFullscreen(true);
  // |w3| is a transient child of |w2|.
  Widget* w3 = Widget::CreateWindowWithParentAndBounds(NULL,
      w2->GetNativeWindow(), gfx::Rect(0, 0, 100, 100));

  // Test that GetFullscreenWindow() finds the fullscreen window when one of
  // its transient children is active.
  w3->Activate();
  EXPECT_EQ(w2->GetNativeWindow(), controller->GetFullscreenWindow());

  // Since there's only one desktop workspace, it always returns the same
  // fullscreen window.
  w1->Activate();
  EXPECT_EQ(w2->GetNativeWindow(), controller->GetFullscreenWindow());
}

// Test that user session window can't be focused if user session blocked by
// some overlapping UI.
TEST_F(RootWindowControllerTest, FocusBlockedWindow) {
  UpdateDisplay("600x600");
  internal::RootWindowController* controller =
      Shell::GetInstance()->GetPrimaryRootWindowController();
  aura::Window* lock_container =
      Shell::GetContainer(controller->root_window(),
                          internal::kShellWindowId_LockScreenContainer);
  aura::Window* lock_window = Widget::CreateWindowWithParentAndBounds(NULL,
      lock_container, gfx::Rect(0, 0, 100, 100))->GetNativeView();
  lock_window->Show();
  aura::Window* session_window =
      CreateTestWidget(gfx::Rect(0, 0, 100, 100))->GetNativeView();
  session_window->Show();

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS;
       ++block_reason) {
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));
    lock_window->Focus();
    EXPECT_TRUE(lock_window->HasFocus());
    session_window->Focus();
    EXPECT_FALSE(session_window->HasFocus());
    UnblockUserSession();
  }
}

typedef test::NoSessionAshTestBase NoSessionRootWindowControllerTest;

// Make sure that an event handler exists for entire display area.
TEST_F(NoSessionRootWindowControllerTest, Event) {
  aura::RootWindow* root = Shell::GetPrimaryRootWindow();
  const gfx::Size size = root->bounds().size();
  aura::Window* event_target = root->GetEventHandlerForPoint(gfx::Point(0, 0));
  EXPECT_TRUE(event_target);
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(gfx::Point(0, size.height() - 1)));
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(gfx::Point(size.width() - 1, 0)));
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(gfx::Point(0, size.height() - 1)));
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(
                gfx::Point(size.width() - 1, size.height() - 1)));
}

}  // namespace test
}  // namespace ash
