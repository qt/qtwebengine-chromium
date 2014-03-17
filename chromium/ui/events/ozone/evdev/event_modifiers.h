// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_MODIFIERS_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_MODIFIERS_H_

#include "ui/events/events_export.h"
#include "ui/events/ozone/event_converter_ozone.h"

namespace ui {

enum {
  EVDEV_MODIFIER_NONE,
  EVDEV_MODIFIER_CAPS_LOCK,
  EVDEV_MODIFIER_SHIFT,
  EVDEV_MODIFIER_CONTROL,
  EVDEV_MODIFIER_ALT,
  EVDEV_MODIFIER_LEFT_MOUSE_BUTTON,
  EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON,
  EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON,
  EVDEV_MODIFIER_COMMAND,
  EVDEV_MODIFIER_ALTGR,
  EVDEV_NUM_MODIFIERS
};

// Modifier key state for Evdev.
//
// Chrome relies on the underlying OS to interpret modifier keys such as Shift,
// Ctrl, and Alt. The Linux input subsystem does not assign any special meaning
// to these keys, so this work must happen at a higher layer (normally X11 or
// the console driver). When using evdev directly, we must do it ourselves.
//
// The modifier state is shared between all input devices connected to the
// system. This is to support actions such as Shift-Clicking that use multiple
// devices.
//
// Normally a modifier is set if any of the keys or buttons assigned to it are
// currently pressed. However some keys toggle a persistent "lock" for the
// modifier instead, such as CapsLock. If a modifier is "locked" then its state
// is inverted until it is unlocked.
class EVENTS_EXPORT EventModifiersEvdev {
 public:
  EventModifiersEvdev();
  ~EventModifiersEvdev();

  // Record key press or release for regular modifier key (shift, alt, etc).
  void UpdateModifier(unsigned int modifier, bool down);

  // Record key press or release for locking modifier key (caps lock).
  void UpdateModifierLock(unsigned int modifier, bool down);

  // Return current flags to use for incoming events.
  int GetModifierFlags();

 private:
  // Count of keys pressed for each modifier.
  int modifiers_down_[EVDEV_NUM_MODIFIERS];

  // Mask of modifier flags currently "locked".
  int modifier_flags_locked_;

  // Mask of modifier flags currently active (nonzero keys pressed xor locked).
  int modifier_flags_;

  // Update modifier_flags_ from modifiers_down_ and modifier_flags_locked_.
  void UpdateFlags(unsigned int modifier);

  DISALLOW_COPY_AND_ASSIGN(EventModifiersEvdev);
};

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_MODIFIERS_H_
