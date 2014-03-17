// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/test_shelf_item_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

typedef ash::test::AshTestBase LauncherTest;
using ash::internal::ShelfView;
using ash::internal::ShelfButton;

namespace ash {

class LauncherTest : public ash::test::AshTestBase {
 public:
  LauncherTest() : launcher_(NULL),
                   shelf_view_(NULL),
                   shelf_model_(NULL),
                   item_delegate_manager_(NULL) {
  }

  virtual ~LauncherTest() {}

  virtual void SetUp() {
    test::AshTestBase::SetUp();

    launcher_ = Launcher::ForPrimaryDisplay();
    ASSERT_TRUE(launcher_);

    ash::test::LauncherTestAPI test(launcher_);
    shelf_view_ = test.shelf_view();
    shelf_model_ = shelf_view_->model();
    item_delegate_manager_ =
        Shell::GetInstance()->shelf_item_delegate_manager();

    test_.reset(new ash::test::ShelfViewTestAPI(shelf_view_));
  }

  virtual void TearDown() OVERRIDE {
    test::AshTestBase::TearDown();
  }

  Launcher* launcher() {
    return launcher_;
  }

  ShelfView* shelf_view() {
    return shelf_view_;
  }

  ShelfModel* shelf_model() {
    return shelf_model_;
  }

  ShelfItemDelegateManager* item_manager() {
    return item_delegate_manager_;
  }

  ash::test::ShelfViewTestAPI* test_api() {
    return test_.get();
  }

 private:
  Launcher* launcher_;
  ShelfView* shelf_view_;
  ShelfModel* shelf_model_;
  ShelfItemDelegateManager* item_delegate_manager_;
  scoped_ptr<test::ShelfViewTestAPI> test_;

  DISALLOW_COPY_AND_ASSIGN(LauncherTest);
};

// Confirms that LauncherItem reflects the appropriated state.
TEST_F(LauncherTest, StatusReflection) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add running platform app.
  LauncherItem item;
  item.type = TYPE_PLATFORM_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);
  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfButton* button = test_api()->GetButton(index);
  EXPECT_EQ(ShelfButton::STATE_RUNNING, button->state());

  // Remove it.
  shelf_model()->RemoveItemAt(index);
  ASSERT_EQ(--button_count, test_api()->GetButtonCount());
}

// Confirm that using the menu will clear the hover attribute. To avoid another
// browser test we check this here.
TEST_F(LauncherTest, checkHoverAfterMenu) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add running platform app.
  LauncherItem item;
  item.type = TYPE_PLATFORM_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);

  scoped_ptr<ShelfItemDelegate> delegate(
      new test::TestShelfItemDelegate(NULL));
  item_manager()->SetShelfItemDelegate(shelf_model()->items()[index].id,
                                       delegate.Pass());

  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfButton* button = test_api()->GetButton(index);
  button->AddState(ShelfButton::STATE_HOVERED);
  button->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_FALSE(button->state() & ShelfButton::STATE_HOVERED);

  // Remove it.
  shelf_model()->RemoveItemAt(index);
}

TEST_F(LauncherTest, ShowOverflowBubble) {
  LauncherID first_item_id = shelf_model()->next_id();

  // Add platform app button until overflow.
  int items_added = 0;
  while (!test_api()->IsOverflowButtonVisible()) {
    LauncherItem item;
    item.type = TYPE_PLATFORM_APP;
    item.status = STATUS_RUNNING;
    shelf_model()->Add(item);

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Shows overflow bubble.
  test_api()->ShowOverflowBubble();
  EXPECT_TRUE(launcher()->IsShowingOverflowBubble());

  // Removes the first item in main shelf view.
  shelf_model()->RemoveItemAt(shelf_model()->ItemIndexByID(first_item_id));

  // Waits for all transitions to finish and there should be no crash.
  test_api()->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(launcher()->IsShowingOverflowBubble());
}

}  // namespace ash
