// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace content {

#if defined(USE_X11) || defined(USE_OZONE)
// From third_party/WebKit/Source/web/gtk/WebInputEventFactory.cpp:
WebKit::WebUChar GetControlCharacter(int windows_key_code, bool shift) {
  if (windows_key_code >= ui::VKEY_A &&
    windows_key_code <= ui::VKEY_Z) {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - ui::VKEY_A + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case ui::VKEY_2:
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case ui::VKEY_6:
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case ui::VKEY_OEM_MINUS:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case ui::VKEY_OEM_4:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case ui::VKEY_OEM_5:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case ui::VKEY_OEM_6:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case ui::VKEY_RETURN:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  }
  return 0;
}
#endif
#if defined(OS_WIN)
WebKit::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    base::NativeEvent native_event);
#elif defined(USE_X11)
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event);
#elif defined(USE_OZONE)
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event) {
  base::NativeEvent native_event = event->native_event();
  ui::EventType type = ui::EventTypeFromNative(native_event);
  WebKit::WebKeyboardEvent webkit_event;

  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());

  switch (type) {
    case ui::ET_KEY_PRESSED:
      webkit_event.type = event->is_char() ? WebKit::WebInputEvent::Char :
          WebKit::WebInputEvent::RawKeyDown;
      break;
    case ui::ET_KEY_RELEASED:
      webkit_event.type = WebKit::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (webkit_event.modifiers & WebKit::WebInputEvent::AltKey)
    webkit_event.isSystemKey = true;

  wchar_t character = ui::KeyboardCodeFromNative(native_event);
  webkit_event.windowsKeyCode = character;
  webkit_event.nativeKeyCode = character;

  if (webkit_event.windowsKeyCode == ui::VKEY_RETURN)
    webkit_event.unmodifiedText[0] = '\r';
  else
    webkit_event.unmodifiedText[0] = character;

  if (webkit_event.modifiers & WebKit::WebInputEvent::ControlKey) {
    webkit_event.text[0] =
        GetControlCharacter(
            webkit_event.windowsKeyCode,
            webkit_event.modifiers & WebKit::WebInputEvent::ShiftKey);
  } else {
    webkit_event.text[0] = webkit_event.unmodifiedText[0];
  }

  webkit_event.setKeyIdentifierFromWindowsKeyCode();

  return webkit_event;
}
#endif
#if defined(USE_X11) || defined(USE_OZONE)
WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::ScrollEvent* event) {
  WebKit::WebMouseWheelEvent webkit_event;

  webkit_event.type = WebKit::WebInputEvent::MouseWheel;
  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.hasPreciseScrollingDeltas = true;
  webkit_event.deltaX = event->x_offset();
  if (event->x_offset_ordinal() != 0.f && event->x_offset() != 0.f) {
    webkit_event.accelerationRatioX =
        event->x_offset_ordinal() / event->x_offset();
  }
  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.deltaY = event->y_offset();
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;
  if (event->y_offset_ordinal() != 0.f && event->y_offset() != 0.f) {
    webkit_event.accelerationRatioY =
        event->y_offset_ordinal() / event->y_offset();
  }
  return webkit_event;
}

WebKit::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    ui::ScrollEvent* event) {
  WebKit::WebGestureEvent webkit_event;

  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
      webkit_event.type = WebKit::WebInputEvent::GestureFlingStart;
      webkit_event.data.flingStart.velocityX = event->x_offset();
      webkit_event.data.flingStart.velocityY = event->y_offset();
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      webkit_event.type = WebKit::WebInputEvent::GestureFlingCancel;
      break;
    case ui::ET_SCROLL:
      NOTREACHED() << "Invalid gesture type: " << event->type();
      break;
    default:
      NOTREACHED() << "Unknown gesture type: " << event->type();
  }

  webkit_event.sourceDevice = WebKit::WebGestureEvent::Touchpad;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  return webkit_event;
}

#endif

WebKit::WebMouseEvent MakeWebMouseEventFromAuraEvent(
    ui::MouseEvent* event);
WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::MouseWheelEvent* event);

// General approach:
//
// ui::Event only carries a subset of possible event data provided to Aura by
// the host platform. WebKit utilizes a larger subset of that information than
// Aura itself. WebKit includes some built in cracking functionality that we
// rely on to obtain this information cleanly and consistently.
//
// The only place where an ui::Event's data differs from what the underlying
// base::NativeEvent would provide is position data, since we would like to
// provide coordinates relative to the aura::Window that is hosting the
// renderer, not the top level platform window.
//
// The approach is to fully construct a WebKit::WebInputEvent from the
// ui::Event's base::NativeEvent, and then replace the coordinate fields with
// the translated values from the ui::Event.
//
// The exception is mouse events on linux. The ui::MouseEvent contains enough
// necessary information to construct a WebMouseEvent. So instead of extracting
// the information from the XEvent, which can be tricky when supporting both
// XInput2 and XInput, the WebMouseEvent is constructed from the
// ui::MouseEvent. This will not be necessary once only XInput2 is supported.
//

WebKit::WebMouseEvent MakeWebMouseEvent(ui::MouseEvent* event) {
  // Construct an untranslated event from the platform event data.
  WebKit::WebMouseEvent webkit_event =
#if defined(OS_WIN)
  // On Windows we have WM_ events comming from desktop and pure aura
  // events comming from metro mode.
  event->native_event().message ?
      MakeUntranslatedWebMouseEventFromNativeEvent(event->native_event()) :
      MakeWebMouseEventFromAuraEvent(event);
#else
  MakeWebMouseEventFromAuraEvent(event);
#endif
  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

#if defined(OS_WIN)
  if (event->native_event().message)
    return webkit_event;
#endif
  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEvent(ui::MouseWheelEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  WebKit::WebMouseWheelEvent webkit_event = event->native_event().message ?
      MakeUntranslatedWebMouseWheelEventFromNativeEvent(event->native_event()) :
      MakeWebMouseWheelEventFromAuraEvent(event);
#else
  WebKit::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEvent(ui::ScrollEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  WebKit::WebMouseWheelEvent webkit_event =
      MakeUntranslatedWebMouseWheelEventFromNativeEvent(event->native_event());
#else
  WebKit::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

WebKit::WebKeyboardEvent MakeWebKeyboardEvent(ui::KeyEvent* event) {
  // Windows can figure out whether or not to construct a RawKeyDown or a Char
  // WebInputEvent based on the type of message carried in
  // event->native_event(). X11 is not so fortunate, there is no separate
  // translated event type, so DesktopHostLinux sends an extra KeyEvent with
  // is_char() == true. We need to pass the ui::KeyEvent to the X11 function
  // to detect this case so the right event type can be constructed.
#if defined(OS_WIN)
  // Key events require no translation by the aura system.
  return MakeWebKeyboardEventFromNativeEvent(event->native_event());
#else
  return MakeWebKeyboardEventFromAuraEvent(event);
#endif
}

WebKit::WebGestureEvent MakeWebGestureEvent(ui::GestureEvent* event) {
  WebKit::WebGestureEvent gesture_event;
#if defined(OS_WIN)
  if (event->HasNativeEvent())
    gesture_event = MakeWebGestureEventFromNativeEvent(event->native_event());
  else
    gesture_event = MakeWebGestureEventFromUIEvent(*event);
#else
  gesture_event = MakeWebGestureEventFromUIEvent(*event);
#endif

  gesture_event.x = event->x();
  gesture_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  gesture_event.globalX = root_point.x();
  gesture_event.globalY = root_point.y();

  return gesture_event;
}

WebKit::WebGestureEvent MakeWebGestureEvent(ui::ScrollEvent* event) {
  WebKit::WebGestureEvent gesture_event;

#if defined(OS_WIN)
  gesture_event = MakeWebGestureEventFromNativeEvent(event->native_event());
#else
  gesture_event = MakeWebGestureEventFromAuraEvent(event);
#endif

  gesture_event.x = event->x();
  gesture_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  gesture_event.globalX = root_point.x();
  gesture_event.globalY = root_point.y();

  return gesture_event;
}

WebKit::WebGestureEvent MakeWebGestureEventFlingCancel() {
  WebKit::WebGestureEvent gesture_event;

  // All other fields are ignored on a GestureFlingCancel event.
  gesture_event.type = WebKit::WebInputEvent::GestureFlingCancel;
  gesture_event.sourceDevice = WebKit::WebGestureEvent::Touchpad;
  return gesture_event;
}

WebKit::WebMouseEvent MakeWebMouseEventFromAuraEvent(ui::MouseEvent* event) {
  WebKit::WebMouseEvent webkit_event;

  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();

  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  if (event->flags() & ui::EF_LEFT_MOUSE_BUTTON)
    webkit_event.button = WebKit::WebMouseEvent::ButtonLeft;
  if (event->flags() & ui::EF_MIDDLE_MOUSE_BUTTON)
    webkit_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  if (event->flags() & ui::EF_RIGHT_MOUSE_BUTTON)
    webkit_event.button = WebKit::WebMouseEvent::ButtonRight;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      webkit_event.type = WebKit::WebInputEvent::MouseDown;
      webkit_event.clickCount = event->GetClickCount();
      break;
    case ui::ET_MOUSE_RELEASED:
      webkit_event.type = WebKit::WebInputEvent::MouseUp;
      webkit_event.clickCount = event->GetClickCount();
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      webkit_event.type = WebKit::WebInputEvent::MouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: " << event->type();
      break;
  }

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::MouseWheelEvent* event) {
  WebKit::WebMouseWheelEvent webkit_event;

  webkit_event.type = WebKit::WebInputEvent::MouseWheel;
  webkit_event.button = WebKit::WebMouseEvent::ButtonNone;
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());
  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.deltaX = event->x_offset();
  webkit_event.deltaY = event->y_offset();
  webkit_event.wheelTicksX = webkit_event.deltaX / kPixelsPerTick;
  webkit_event.wheelTicksY = webkit_event.deltaY / kPixelsPerTick;

  return webkit_event;
}

}  // namespace content
