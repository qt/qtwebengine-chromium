// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/evdev/key_event_converter.h"

namespace ui {

const int kInvalidFileDescriptor = -1;
const int kTestDeviceId = 0;

class MockKeyEventConverterEvdev : public KeyEventConverterEvdev {
 public:
  MockKeyEventConverterEvdev(EventModifiersEvdev* modifiers)
      : KeyEventConverterEvdev(kInvalidFileDescriptor,
                               kTestDeviceId,
                               modifiers) {}
  virtual ~MockKeyEventConverterEvdev() {};

  unsigned size() { return dispatched_events_.size(); }
  KeyEvent* event(unsigned index) { return dispatched_events_[index]; }

  virtual void DispatchEvent(scoped_ptr<Event> event) OVERRIDE;

 private:
  ScopedVector<KeyEvent> dispatched_events_;

  DISALLOW_COPY_AND_ASSIGN(MockKeyEventConverterEvdev);
};

void MockKeyEventConverterEvdev::DispatchEvent(scoped_ptr<Event> event) {
  dispatched_events_.push_back(static_cast<KeyEvent*>(event.release()));
}

}  // namespace ui

// Test fixture.
class KeyEventConverterEvdevTest : public testing::Test {
 public:
  KeyEventConverterEvdevTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    modifiers_ = new ui::EventModifiersEvdev();
    device_ = new ui::MockKeyEventConverterEvdev(modifiers_);
  }
  virtual void TearDown() OVERRIDE {
    delete device_;
    delete modifiers_;
  }

  ui::MockKeyEventConverterEvdev* device() { return device_; }
  ui::EventModifiersEvdev* modifiers() { return modifiers_; }

 private:
  ui::EventModifiersEvdev* modifiers_;
  ui::MockKeyEventConverterEvdev* device_;
  DISALLOW_COPY_AND_ASSIGN(KeyEventConverterEvdevTest);
};

TEST_F(KeyEventConverterEvdevTest, KeyPress) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(2u, dev->size());

  ui::KeyEvent* event;

  event = dev->event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dev->event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, KeyRepeat) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(4u, dev->size());

  ui::KeyEvent* event;

  event = dev->event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dev->event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dev->event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dev->event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, NoEvents) {
  ui::MockKeyEventConverterEvdev* dev = device();
  dev->ProcessEvents(NULL, 0);
  EXPECT_EQ(0u, dev->size());
}

TEST_F(KeyEventConverterEvdevTest, KeyWithModifier) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTSHIFT, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70004},
      {{0, 0}, EV_KEY, KEY_A, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70004},
      {{0, 0}, EV_KEY, KEY_A, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTSHIFT, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(4u, dev->size());

  ui::KeyEvent* event;

  event = dev->event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_SHIFT, event->key_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dev->event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dev->event(2);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dev->event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_SHIFT, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, KeyWithDuplicateModifier) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTCTRL, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e5},
      {{0, 0}, EV_KEY, KEY_RIGHTCTRL, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7001d},
      {{0, 0}, EV_KEY, KEY_Z, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7001d},
      {{0, 0}, EV_KEY, KEY_Z, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTCTRL, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e5},
      {{0, 0}, EV_KEY, KEY_RIGHTCTRL, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(6u, dev->size());

  ui::KeyEvent* event;

  event = dev->event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dev->event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dev->event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_Z, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dev->event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_Z, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dev->event(4);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dev->event(5);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(KeyEventConverterEvdevTest, KeyWithLock) {
  ui::MockKeyEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70014},
      {{0, 0}, EV_KEY, KEY_Q, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70014},
      {{0, 0}, EV_KEY, KEY_Q, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, arraysize(mock_kernel_queue));
  EXPECT_EQ(6u, dev->size());

  ui::KeyEvent* event;

  event = dev->event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dev->event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dev->event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_Q, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dev->event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_Q, event->key_code());
  EXPECT_EQ(ui::EF_CAPS_LOCK_DOWN, event->flags());

  event = dev->event(4);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(0, event->flags());

  event = dev->event(5);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(0, event->flags());
}
