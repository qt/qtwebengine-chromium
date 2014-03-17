// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/virtual_keyboard_window_controller.h"

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/keyboard/keyboard_switches.h"

namespace ash {
namespace test {

class VirtualKeyboardWindowControllerTest : public AshTestBase {
 public:
  VirtualKeyboardWindowControllerTest()
      : virtual_keyboard_window_controller_(NULL) {}
  virtual ~VirtualKeyboardWindowControllerTest() {}

  virtual void SetUp() OVERRIDE {
    if (SupportsMultipleDisplays()) {
      CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kAshHostWindowBounds, "1+1-300x300,1+301-300x300");
      CommandLine::ForCurrentProcess()->AppendSwitch(
          keyboard::switches::kKeyboardUsabilityExperiment);
    }
    test::AshTestBase::SetUp();
  }

  void set_virtual_keyboard_window_controller(
      internal::VirtualKeyboardWindowController* controller) {
    virtual_keyboard_window_controller_ = controller;
  }

  internal::RootWindowController* root_window_controller() {
    return virtual_keyboard_window_controller_->
        root_window_controller_for_test();
  }

 private:
  internal::VirtualKeyboardWindowController*
      virtual_keyboard_window_controller_;
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardWindowControllerTest);
};


TEST_F(VirtualKeyboardWindowControllerTest, VirtualKeyboardWindowTest) {
  if (!SupportsMultipleDisplays())
    return;
  RunAllPendingInMessageLoop();
  set_virtual_keyboard_window_controller(
      Shell::GetInstance()->display_controller()->
          virtual_keyboard_window_controller());
  EXPECT_TRUE(root_window_controller());
  // Keyboard container is added to virtual keyboard window.
  EXPECT_TRUE(root_window_controller()->GetContainer(
      internal::kShellWindowId_VirtualKeyboardContainer));
}

}  // namespace test
}  // namespace ash
