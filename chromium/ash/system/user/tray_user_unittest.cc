// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/user/tray_user.h"
#include "ash/system/user/tray_user_separator.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "ui/aura/test/event_generator.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/system/tray/system_tray_notifier.h"
#endif

namespace ash {
namespace internal {

class TrayUserTest : public ash::test::AshTestBase {
 public:
  TrayUserTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;

  // This has to be called prior to first use with the proper configuration.
  void InitializeParameters(int users_logged_in, bool multiprofile);

  // Show the system tray menu using the provided event generator.
  void ShowTrayMenu(aura::test::EventGenerator* generator);

  // Move the mouse over the user item.
  void MoveOverUserItem(aura::test::EventGenerator* generator, int index);

  // Click on the user item. Note that the tray menu needs to be shown.
  void ClickUserItem(aura::test::EventGenerator* generator, int index);

  // Accessors to various system components.
  ShelfLayoutManager* shelf() { return shelf_; }
  SystemTray* tray() { return tray_; }
  ash::test::TestSessionStateDelegate* delegate() { return delegate_; }
  ash::internal::TrayUser* tray_user(int index) { return tray_user_[index]; }
  ash::internal::TrayUserSeparator* tray_user_separator() {
    return tray_user_separator_;
  }

 private:
  ShelfLayoutManager* shelf_;
  SystemTray* tray_;
  ash::test::TestSessionStateDelegate* delegate_;

  // Note that the ownership of these items is on the shelf.
  std::vector<ash::internal::TrayUser*> tray_user_;

  // The separator between the tray users and the rest of the menu.
  // Note: The item will get owned by the shelf.
  TrayUserSeparator* tray_user_separator_;

  DISALLOW_COPY_AND_ASSIGN(TrayUserTest);
};

#if defined(OS_CHROMEOS)
// The tray user test which tests functionality where multiple tray items are
// visible in the system tray.
class MultiTrayUserTest : public TrayUserTest {
 public:
  MultiTrayUserTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(ash::switches::kAshEnableMultiUserTray);
    TrayUserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiTrayUserTest);
};
#endif

TrayUserTest::TrayUserTest()
    : shelf_(NULL),
      tray_(NULL),
      delegate_(NULL),
      tray_user_separator_(NULL) {
}

void TrayUserTest::SetUp() {
  ash::test::AshTestBase::SetUp();
  shelf_ = Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  tray_ = Shell::GetPrimaryRootWindowController()->GetSystemTray();
  delegate_ = static_cast<ash::test::TestSessionStateDelegate*>(
      ash::Shell::GetInstance()->session_state_delegate());
}

void TrayUserTest::InitializeParameters(int users_logged_in,
                                        bool multiprofile) {
  // Show the shelf.
  shelf()->LayoutShelf();
  shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Set our default assumptions. Note that it is sufficient to set these
  // after everything was created.
  delegate_->set_logged_in_users(users_logged_in);
  ash::test::TestShellDelegate* shell_delegate =
      static_cast<ash::test::TestShellDelegate*>(
          ash::Shell::GetInstance()->delegate());
  shell_delegate->set_multi_profiles_enabled(multiprofile);

  // Instead of using the existing tray panels we create new ones which makes
  // the access easier.
  for (int i = 0; i < delegate_->GetMaximumNumberOfLoggedInUsers(); i++) {
    tray_user_.push_back(new ash::internal::TrayUser(tray_, i));
    tray_->AddTrayItem(tray_user_[i]);
  }
  // We then add also the separator.
  tray_user_separator_ = new ash::internal::TrayUserSeparator(tray_);
  tray_->AddTrayItem(tray_user_separator_);
}

void TrayUserTest::ShowTrayMenu(aura::test::EventGenerator* generator) {
  gfx::Point center = tray()->GetBoundsInScreen().CenterPoint();

  generator->MoveMouseTo(center.x(), center.y());
  EXPECT_FALSE(tray()->IsAnyBubbleVisible());
  generator->ClickLeftButton();
}

void TrayUserTest::MoveOverUserItem(aura::test::EventGenerator* generator,
    int index) {
  gfx::Point center =
      tray_user(index)->GetUserPanelBoundsInScreenForTest().CenterPoint();

  generator->MoveMouseTo(center.x(), center.y());
}

void TrayUserTest::ClickUserItem(aura::test::EventGenerator* generator,
                                 int index) {
  MoveOverUserItem(generator, index);
  generator->ClickLeftButton();
}

// Make sure that in single user mode the user panel cannot be activated and no
// separators are being created.
TEST_F(TrayUserTest, SingleUserModeDoesNotAllowAddingUser) {
  InitializeParameters(1, false);

  // Move the mouse over the status area and click to open the status menu.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  EXPECT_FALSE(tray()->IsAnyBubbleVisible());

  for (int i = 0; i < delegate()->GetMaximumNumberOfLoggedInUsers(); i++)
    EXPECT_EQ(ash::internal::TrayUser::HIDDEN,
              tray_user(i)->GetStateForTest());
  EXPECT_FALSE(tray_user_separator()->separator_shown());

  ShowTrayMenu(&generator);

  EXPECT_TRUE(tray()->HasSystemBubble());
  EXPECT_TRUE(tray()->IsAnyBubbleVisible());

  for (int i = 0; i < delegate()->GetMaximumNumberOfLoggedInUsers(); i++)
    EXPECT_EQ(i == 0 ? ash::internal::TrayUser::SHOWN :
                       ash::internal::TrayUser::HIDDEN,
              tray_user(i)->GetStateForTest());
  EXPECT_FALSE(tray_user_separator()->separator_shown());
  tray()->CloseSystemBubble();
}

#if defined(OS_CHROMEOS)
// Make sure that in multi user mode the user panel can be activated and there
// will be one panel for each user plus one additional separator at the end.
// Note: the mouse watcher (for automatic closing upon leave) cannot be tested
// here since it does not work with the event system in unit tests.
TEST_F(TrayUserTest, MutiUserModeDoesNotAllowToAddUser) {
  InitializeParameters(1, true);

  // Move the mouse over the status area and click to open the status menu.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.set_async(false);

  int max_users = delegate()->GetMaximumNumberOfLoggedInUsers();
  // Checking now for each amount of users that the correct is done.
  for (int j = 1; j < max_users; j++) {
    // Set the number of logged in users.
    delegate()->set_logged_in_users(j);

    // Verify that nothing is shown.
    EXPECT_FALSE(tray()->IsAnyBubbleVisible());
    for (int i = 0; i < max_users; i++)
      EXPECT_FALSE(tray_user(i)->GetStateForTest());
    EXPECT_FALSE(tray_user_separator()->separator_shown());
    // After clicking on the tray the menu should get shown and for each logged
    // in user we should get a visible item. In addition, the separator should
    // show up when we reach more then one user.
    ShowTrayMenu(&generator);

    EXPECT_TRUE(tray()->HasSystemBubble());
    EXPECT_TRUE(tray()->IsAnyBubbleVisible());
    for (int i = 0; i < max_users; i++) {
      EXPECT_EQ(i < j ? ash::internal::TrayUser::SHOWN :
                        ash::internal::TrayUser::HIDDEN,
                tray_user(i)->GetStateForTest());
    }

    // Check the visibility of the separator.
    EXPECT_EQ(j > 1 ? true : false, tray_user_separator()->separator_shown());

    // Move the mouse over the user item and it should hover.
    MoveOverUserItem(&generator, 0);
    EXPECT_EQ(ash::internal::TrayUser::HOVERED,
              tray_user(0)->GetStateForTest());
    for (int i = 1; i < max_users; i++) {
      EXPECT_EQ(i < j ? ash::internal::TrayUser::SHOWN :
                        ash::internal::TrayUser::HIDDEN,
                tray_user(i)->GetStateForTest());
    }

    // Check that clicking the button allows to add item if we have still room
    // for one more user.
    ClickUserItem(&generator, 0);
    EXPECT_EQ(j == max_users ?
                  ash::internal::TrayUser::ACTIVE_BUT_DISABLED :
                  ash::internal::TrayUser::ACTIVE,
              tray_user(0)->GetStateForTest());

    // Click the button again to see that the menu goes away.
    ClickUserItem(&generator, 0);
    EXPECT_EQ(ash::internal::TrayUser::HOVERED,
              tray_user(0)->GetStateForTest());

    // Close and check that everything is deleted.
    tray()->CloseSystemBubble();
    EXPECT_FALSE(tray()->IsAnyBubbleVisible());
    for (int i = 0; i < delegate()->GetMaximumNumberOfLoggedInUsers(); i++)
      EXPECT_EQ(ash::internal::TrayUser::HIDDEN,
                tray_user(i)->GetStateForTest());
  }
}

// Make sure that user changing gets properly executed.
TEST_F(TrayUserTest, MutiUserModeButtonClicks) {
  // Have two users.
  InitializeParameters(2, true);
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ShowTrayMenu(&generator);

  // Switch to a new user - which has a capitalized name.
  ClickUserItem(&generator, 1);
  EXPECT_EQ(delegate()->get_activated_user(), delegate()->GetUserID(1));
  // Since the name is capitalized, the email should be different then the
  // user_id.
  EXPECT_NE(delegate()->get_activated_user(), delegate()->GetUserEmail(1));
  tray()->CloseSystemBubble();
}

// Make sure that we show items for all users in the tray accordingly.
TEST_F(MultiTrayUserTest, CheckTrayUserItems) {
  InitializeParameters(1, true);

  int max_users = delegate()->GetMaximumNumberOfLoggedInUsers();
  // Checking now for each amount of users that the proper items are visible in
  // the tray. The proper item is hereby:
  // 2 -> User #1
  // 1 -> User #2
  // 0 -> User #3
  // Note: Tray items are required to populate system tray items as well as the
  // system tray menu. The system tray menu changes it's appearance with the
  // addition of more users, but the system tray does not create new items after
  // it got created.
  for (int present_users = 1; present_users <= max_users; ++present_users) {
    // We simulate the user addition by telling the delegate the new number of
    // users, then change all user tray items and finally tell the tray to
    // re-layout itself.
    delegate()->set_logged_in_users(present_users);
    Shell::GetInstance()->system_tray_notifier()->NotifyUserAddedToSession();
    tray()->Layout();

    // Check that the tray items are being shown in the reverse order.
    for (int i = 0; i < max_users; i++) {
      gfx::Rect rect =
          tray()->GetTrayItemViewForTest(tray_user(i))->GetBoundsInScreen();
      if (max_users - 1 - i < present_users)
        EXPECT_FALSE(rect.IsEmpty());
      else
        EXPECT_TRUE(rect.IsEmpty());
    }
  }

  // Click on the last item to see that the user changes.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.set_async(false);

  // Switch to a new user - again, note that we have to click on the reverse
  // item in the list. Since the first clickable item is 1, we get user #2.
  gfx::Point point =
      tray()->GetTrayItemViewForTest(tray_user(1))->
                         GetBoundsInScreen().CenterPoint();

  generator.MoveMouseTo(point.x(), point.y());
  generator.ClickLeftButton();
  EXPECT_EQ(delegate()->get_activated_user(), delegate()->GetUserID(1));
}

#endif

}  // namespace internal
}  // namespace ash
