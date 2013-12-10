// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/keyboard_event_counter.h"
#include "media/base/user_input_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace media {

class MockMouseListener : public UserInputMonitor::MouseEventListener {
 public:
  MOCK_METHOD1(OnMouseMoved, void(const SkIPoint& position));

  virtual ~MockMouseListener() {}
};

#if defined(OS_LINUX) || defined(OS_WIN)
TEST(UserInputMonitorTest, KeyPressCounter) {
  KeyboardEventCounter counter;

  EXPECT_EQ(0u, counter.GetKeyPressCount());

  counter.OnKeyboardEvent(ui::ET_KEY_PRESSED, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  // Holding the same key without releasing it does not increase the count.
  counter.OnKeyboardEvent(ui::ET_KEY_PRESSED, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  // Releasing the key does not affect the total count.
  counter.OnKeyboardEvent(ui::ET_KEY_RELEASED, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  counter.OnKeyboardEvent(ui::ET_KEY_PRESSED, ui::VKEY_0);
  counter.OnKeyboardEvent(ui::ET_KEY_RELEASED, ui::VKEY_0);
  EXPECT_EQ(2u, counter.GetKeyPressCount());
}
#endif  // defined(OS_LINUX) || defined(OS_WIN)

TEST(UserInputMonitorTest, CreatePlatformSpecific) {
#if defined(OS_LINUX)
  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
#else
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
#endif  // defined(OS_LINUX)

  base::RunLoop run_loop;
  scoped_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      message_loop.message_loop_proxy(), message_loop.message_loop_proxy());

  if (!monitor)
    return;

  MockMouseListener listener;
  // Ignore any callbacks.
  EXPECT_CALL(listener, OnMouseMoved(testing::_)).Times(testing::AnyNumber());

#if !defined(OS_MACOSX)
  monitor->AddMouseListener(&listener);
  monitor->RemoveMouseListener(&listener);
#endif  // !define(OS_MACOSX)

  monitor->EnableKeyPressMonitoring();
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  run_loop.RunUntilIdle();
}

}  // namespace media
