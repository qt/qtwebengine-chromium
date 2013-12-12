// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/event_conversion.h"

#include <map>

#include "base/basictypes.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/pepper/common.h"
#include "content/renderer/pepper/usb_key_code_conversion.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using ppapi::EventTimeToPPTimeTicks;
using ppapi::InputEventData;
using ppapi::PPTimeTicksToEventTime;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebString;
using WebKit::WebTouchEvent;
using WebKit::WebTouchPoint;
using WebKit::WebUChar;

namespace content {

namespace {

// Verify the modifier flags WebKit uses match the Pepper ones. If these start
// not matching, we'll need to write conversion code to preserve the Pepper
// values (since plugins will be depending on them).
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_SHIFTKEY) ==
               static_cast<int>(WebInputEvent::ShiftKey),
               ShiftKeyMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_CONTROLKEY) ==
               static_cast<int>(WebInputEvent::ControlKey),
               ControlKeyMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_ALTKEY) ==
               static_cast<int>(WebInputEvent::AltKey),
               AltKeyMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_METAKEY) ==
               static_cast<int>(WebInputEvent::MetaKey),
               MetaKeyMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISKEYPAD) ==
               static_cast<int>(WebInputEvent::IsKeyPad),
               KeyPadMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT) ==
               static_cast<int>(WebInputEvent::IsAutoRepeat),
               AutoRepeatMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) ==
               static_cast<int>(WebInputEvent::LeftButtonDown),
               LeftButtonMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN) ==
               static_cast<int>(WebInputEvent::MiddleButtonDown),
               MiddleButtonMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN) ==
               static_cast<int>(WebInputEvent::RightButtonDown),
               RightButtonMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY) ==
               static_cast<int>(WebInputEvent::CapsLockOn),
               CapsLockMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_NUMLOCKKEY) ==
               static_cast<int>(WebInputEvent::NumLockOn),
               NumLockMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISLEFT) ==
               static_cast<int>(WebInputEvent::IsLeft),
               LeftMatches);
COMPILE_ASSERT(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISRIGHT) ==
               static_cast<int>(WebInputEvent::IsRight),
               RightMatches);

PP_InputEvent_Type ConvertEventTypes(WebInputEvent::Type wetype) {
  switch (wetype) {
    case WebInputEvent::MouseDown:
      return PP_INPUTEVENT_TYPE_MOUSEDOWN;
    case WebInputEvent::MouseUp:
      return PP_INPUTEVENT_TYPE_MOUSEUP;
    case WebInputEvent::MouseMove:
      return PP_INPUTEVENT_TYPE_MOUSEMOVE;
    case WebInputEvent::MouseEnter:
      return PP_INPUTEVENT_TYPE_MOUSEENTER;
    case WebInputEvent::MouseLeave:
      return PP_INPUTEVENT_TYPE_MOUSELEAVE;
    case WebInputEvent::ContextMenu:
      return PP_INPUTEVENT_TYPE_CONTEXTMENU;
    case WebInputEvent::MouseWheel:
      return PP_INPUTEVENT_TYPE_WHEEL;
    case WebInputEvent::RawKeyDown:
      return PP_INPUTEVENT_TYPE_RAWKEYDOWN;
    case WebInputEvent::KeyDown:
      return PP_INPUTEVENT_TYPE_KEYDOWN;
    case WebInputEvent::KeyUp:
      return PP_INPUTEVENT_TYPE_KEYUP;
    case WebInputEvent::Char:
      return PP_INPUTEVENT_TYPE_CHAR;
    case WebInputEvent::TouchStart:
      return PP_INPUTEVENT_TYPE_TOUCHSTART;
    case WebInputEvent::TouchMove:
      return PP_INPUTEVENT_TYPE_TOUCHMOVE;
    case WebInputEvent::TouchEnd:
      return PP_INPUTEVENT_TYPE_TOUCHEND;
    case WebInputEvent::TouchCancel:
      return PP_INPUTEVENT_TYPE_TOUCHCANCEL;
    case WebInputEvent::Undefined:
    default:
      return PP_INPUTEVENT_TYPE_UNDEFINED;
  }
}

// Generates a PP_InputEvent with the fields common to all events, as well as
// the event type from the given web event. Event-specific fields will be zero
// initialized.
InputEventData GetEventWithCommonFieldsAndType(const WebInputEvent& web_event) {
  InputEventData result;
  result.event_type = ConvertEventTypes(web_event.type);
  result.event_time_stamp = EventTimeToPPTimeTicks(web_event.timeStampSeconds);
  result.usb_key_code = 0;
  return result;
}

void AppendKeyEvent(const WebInputEvent& event,
                    std::vector<InputEventData>* result_events) {
  const WebKeyboardEvent& key_event =
      static_cast<const WebKeyboardEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = key_event.modifiers;
  result.key_code = key_event.windowsKeyCode;
  result.usb_key_code = UsbKeyCodeForKeyboardEvent(key_event);
  result.code = CodeForKeyboardEvent(key_event);
  result_events->push_back(result);
}

void AppendCharEvent(const WebInputEvent& event,
                     std::vector<InputEventData>* result_events) {
  const WebKeyboardEvent& key_event =
      static_cast<const WebKeyboardEvent&>(event);

  // This is a bit complex, the input event will normally just have one 16-bit
  // character in it, but may be zero or more than one. The text array is
  // just padded with 0 values for the unused ones, but is not necessarily
  // null-terminated.
  //
  // Here we see how many UTF-16 characters we have.
  size_t utf16_char_count = 0;
  while (utf16_char_count < WebKeyboardEvent::textLengthCap &&
         key_event.text[utf16_char_count])
    utf16_char_count++;

  // Make a separate InputEventData for each Unicode character in the input.
  base::i18n::UTF16CharIterator iter(key_event.text, utf16_char_count);
  while (!iter.end()) {
    InputEventData result = GetEventWithCommonFieldsAndType(event);
    result.event_modifiers = key_event.modifiers;
    base::WriteUnicodeCharacter(iter.get(), &result.character_text);

    result_events->push_back(result);
    iter.Advance();
  }
}

void AppendMouseEvent(const WebInputEvent& event,
                      std::vector<InputEventData>* result_events) {
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonNone) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_NONE),
                 MouseNone);
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonLeft) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_LEFT),
                 MouseLeft);
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonRight) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_RIGHT),
                 MouseRight);
  COMPILE_ASSERT(static_cast<int>(WebMouseEvent::ButtonMiddle) ==
                 static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_MIDDLE),
                 MouseMiddle);

  const WebMouseEvent& mouse_event =
      static_cast<const WebMouseEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = mouse_event.modifiers;
  if (mouse_event.type == WebInputEvent::MouseDown ||
      mouse_event.type == WebInputEvent::MouseMove ||
      mouse_event.type == WebInputEvent::MouseUp) {
    result.mouse_button =
        static_cast<PP_InputEvent_MouseButton>(mouse_event.button);
  }
  result.mouse_position.x = mouse_event.x;
  result.mouse_position.y = mouse_event.y;
  result.mouse_click_count = mouse_event.clickCount;
  result.mouse_movement.x = mouse_event.movementX;
  result.mouse_movement.y = mouse_event.movementY;
  result_events->push_back(result);
}

void AppendMouseWheelEvent(const WebInputEvent& event,
                           std::vector<InputEventData>* result_events) {
  const WebMouseWheelEvent& mouse_wheel_event =
      static_cast<const WebMouseWheelEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = mouse_wheel_event.modifiers;
  result.wheel_delta.x = mouse_wheel_event.deltaX;
  result.wheel_delta.y = mouse_wheel_event.deltaY;
  result.wheel_ticks.x = mouse_wheel_event.wheelTicksX;
  result.wheel_ticks.y = mouse_wheel_event.wheelTicksY;
  result.wheel_scroll_by_page = !!mouse_wheel_event.scrollByPage;
  result_events->push_back(result);
}

void SetPPTouchPoints(const WebTouchPoint* touches, uint32_t touches_length,
                      std::vector<PP_TouchPoint>* result) {
  for (uint32_t i = 0; i < touches_length; i++) {
    const WebTouchPoint& touch_point = touches[i];
    PP_TouchPoint pp_pt;
    pp_pt.id = touch_point.id;
    pp_pt.position.x = touch_point.position.x;
    pp_pt.position.y = touch_point.position.y;
    pp_pt.radius.x = touch_point.radiusX;
    pp_pt.radius.y = touch_point.radiusY;
    pp_pt.rotation_angle = touch_point.rotationAngle;
    pp_pt.pressure = touch_point.force;
    result->push_back(pp_pt);
  }
}

void AppendTouchEvent(const WebInputEvent& event,
                      std::vector<InputEventData>* result_events) {
  const WebTouchEvent& touch_event =
      reinterpret_cast<const WebTouchEvent&>(event);

  InputEventData result = GetEventWithCommonFieldsAndType(event);
  SetPPTouchPoints(touch_event.touches, touch_event.touchesLength,
                   &result.touches);
  SetPPTouchPoints(touch_event.changedTouches, touch_event.changedTouchesLength,
                   &result.changed_touches);
  SetPPTouchPoints(touch_event.targetTouches, touch_event.targetTouchesLength,
                   &result.target_touches);

  result_events->push_back(result);
}

// Structure used to map touch point id's to touch states.  Since the pepper
// touch event structure does not have states for individual touch points and
// instead relies on the event type in combination with the set of touch lists,
// we have to set the state for the changed touches to be the same as the event
// type and all others to be 'stationary.'
typedef std::map<uint32_t, WebTouchPoint::State> TouchStateMap;

void SetWebTouchPoints(const std::vector<PP_TouchPoint>& pp_touches,
                       const TouchStateMap& states_map,
                       WebTouchPoint* web_touches,
                       uint32_t* web_touches_length) {

  for (uint32_t i = 0; i < pp_touches.size() &&
       i < WebTouchEvent::touchesLengthCap; i++) {
    WebTouchPoint pt;
    const PP_TouchPoint& pp_pt = pp_touches[i];
    pt.id = pp_pt.id;

    if (states_map.find(pt.id) == states_map.end())
      pt.state = WebTouchPoint::StateStationary;
    else
      pt.state = states_map.find(pt.id)->second;

    pt.position.x = pp_pt.position.x;
    pt.position.y = pp_pt.position.y;
    // TODO bug:http://code.google.com/p/chromium/issues/detail?id=93902
    pt.screenPosition.x = 0;
    pt.screenPosition.y = 0;
    pt.force = pp_pt.pressure;
    pt.radiusX = pp_pt.radius.x;
    pt.radiusY = pp_pt.radius.y;
    pt.rotationAngle = pp_pt.rotation_angle;
    web_touches[i] = pt;
    (*web_touches_length)++;
  }
}

WebTouchEvent* BuildTouchEvent(const InputEventData& event) {
  WebTouchEvent* web_event = new WebTouchEvent();
  WebTouchPoint::State state = WebTouchPoint::StateUndefined;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
      web_event->type = WebInputEvent::TouchStart;
      state = WebTouchPoint::StatePressed;
      break;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      web_event->type = WebInputEvent::TouchMove;
      state = WebTouchPoint::StateMoved;
      break;
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      web_event->type = WebInputEvent::TouchEnd;
      state = WebTouchPoint::StateReleased;
      break;
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      web_event->type = WebInputEvent::TouchCancel;
      state = WebTouchPoint::StateCancelled;
      break;
    default:
      NOTREACHED();
  }

  TouchStateMap states_map;
  for (uint32_t i = 0; i < event.changed_touches.size(); i++)
    states_map[event.changed_touches[i].id] = state;

  web_event->timeStampSeconds = PPTimeTicksToEventTime(event.event_time_stamp);

  SetWebTouchPoints(event.changed_touches, states_map,
                    web_event->changedTouches,
                    &web_event->changedTouchesLength);

  SetWebTouchPoints(event.touches, states_map, web_event->touches,
                    &web_event->touchesLength);

  SetWebTouchPoints(event.target_touches, states_map, web_event->targetTouches,
                    &web_event->targetTouchesLength);

  if (web_event->type == WebInputEvent::TouchEnd ||
      web_event->type == WebInputEvent::TouchCancel) {
    SetWebTouchPoints(event.changed_touches, states_map,
                      web_event->touches, &web_event->touchesLength);
    SetWebTouchPoints(event.changed_touches, states_map,
                      web_event->targetTouches,
                      &web_event->targetTouchesLength);
  }

  return web_event;
}

WebKeyboardEvent* BuildKeyEvent(const InputEventData& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      key_event->type = WebInputEvent::RawKeyDown;
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      key_event->type = WebInputEvent::KeyDown;
      break;
    case PP_INPUTEVENT_TYPE_KEYUP:
      key_event->type = WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }
  key_event->timeStampSeconds = PPTimeTicksToEventTime(event.event_time_stamp);
  key_event->modifiers = event.event_modifiers;
  key_event->windowsKeyCode = event.key_code;
  key_event->setKeyIdentifierFromWindowsKeyCode();
  return key_event;
}

WebKeyboardEvent* BuildCharEvent(const InputEventData& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent();
  key_event->type = WebInputEvent::Char;
  key_event->timeStampSeconds = PPTimeTicksToEventTime(event.event_time_stamp);
  key_event->modifiers = event.event_modifiers;

  // Make sure to not read beyond the buffer in case some bad code doesn't
  // NULL-terminate it (this is called from plugins).
  size_t text_length_cap = WebKeyboardEvent::textLengthCap;
  base::string16 text16 = UTF8ToUTF16(event.character_text);

  memset(key_event->text, 0, text_length_cap);
  memset(key_event->unmodifiedText, 0, text_length_cap);
  for (size_t i = 0;
       i < std::min(text_length_cap, text16.size());
       ++i)
    key_event->text[i] = text16[i];
  return key_event;
}

WebMouseEvent* BuildMouseEvent(const InputEventData& event) {
  WebMouseEvent* mouse_event = new WebMouseEvent();
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      mouse_event->type = WebInputEvent::MouseDown;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      mouse_event->type = WebInputEvent::MouseUp;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      mouse_event->type = WebInputEvent::MouseMove;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      mouse_event->type = WebInputEvent::MouseEnter;
      break;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      mouse_event->type = WebInputEvent::MouseLeave;
      break;
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      mouse_event->type = WebInputEvent::ContextMenu;
      break;
    default:
      NOTREACHED();
  }
  mouse_event->timeStampSeconds =
      PPTimeTicksToEventTime(event.event_time_stamp);
  mouse_event->modifiers = event.event_modifiers;
  mouse_event->button =
      static_cast<WebMouseEvent::Button>(event.mouse_button);
  if (mouse_event->type == WebInputEvent::MouseMove) {
    if (mouse_event->modifiers & WebInputEvent::LeftButtonDown)
      mouse_event->button = WebMouseEvent::ButtonLeft;
    else if (mouse_event->modifiers & WebInputEvent::MiddleButtonDown)
      mouse_event->button = WebMouseEvent::ButtonMiddle;
    else if (mouse_event->modifiers & WebInputEvent::RightButtonDown)
      mouse_event->button = WebMouseEvent::ButtonRight;
  }
  mouse_event->x = event.mouse_position.x;
  mouse_event->y = event.mouse_position.y;
  mouse_event->clickCount = event.mouse_click_count;
  mouse_event->movementX = event.mouse_movement.x;
  mouse_event->movementY = event.mouse_movement.y;
  return mouse_event;
}

WebMouseWheelEvent* BuildMouseWheelEvent(const InputEventData& event) {
  WebMouseWheelEvent* mouse_wheel_event = new WebMouseWheelEvent();
  mouse_wheel_event->type = WebInputEvent::MouseWheel;
  mouse_wheel_event->timeStampSeconds =
      PPTimeTicksToEventTime(event.event_time_stamp);
  mouse_wheel_event->modifiers = event.event_modifiers;
  mouse_wheel_event->deltaX = event.wheel_delta.x;
  mouse_wheel_event->deltaY = event.wheel_delta.y;
  mouse_wheel_event->wheelTicksX = event.wheel_ticks.x;
  mouse_wheel_event->wheelTicksY = event.wheel_ticks.y;
  mouse_wheel_event->scrollByPage = event.wheel_scroll_by_page;
  return mouse_wheel_event;
}

#if !defined(OS_WIN)
#define VK_RETURN         0x0D

#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E

#define VK_APPS           0x5D

#define VK_F1             0x70
#endif

// Convert a character string to a Windows virtual key code. Adapted from
// src/third_party/WebKit/Tools/DumpRenderTree/chromium/EventSender.cpp. This
// is used by CreateSimulatedWebInputEvents to convert keyboard events.
void GetKeyCode(const std::string& char_text,
                WebUChar* code,
                WebUChar* text,
                bool* needs_shift_modifier,
                bool* generate_char) {
  WebUChar vk_code = 0;
  WebUChar vk_text = 0;
  *needs_shift_modifier = false;
  *generate_char = false;
  if ("\n" == char_text) {
    vk_text = vk_code = VK_RETURN;
    *generate_char = true;
  } else if ("rightArrow" == char_text) {
    vk_code = VK_RIGHT;
  } else if ("downArrow" == char_text) {
    vk_code = VK_DOWN;
  } else if ("leftArrow" == char_text) {
    vk_code = VK_LEFT;
  } else if ("upArrow" == char_text) {
    vk_code = VK_UP;
  } else if ("insert" == char_text) {
    vk_code = VK_INSERT;
  } else if ("delete" == char_text) {
    vk_code = VK_DELETE;
  } else if ("pageUp" == char_text) {
    vk_code = VK_PRIOR;
  } else if ("pageDown" == char_text) {
    vk_code = VK_NEXT;
  } else if ("home" == char_text) {
    vk_code = VK_HOME;
  } else if ("end" == char_text) {
    vk_code = VK_END;
  } else if ("printScreen" == char_text) {
    vk_code = VK_SNAPSHOT;
  } else if ("menu" == char_text) {
    vk_code = VK_APPS;
  } else {
    // Compare the input string with the function-key names defined by the
    // DOM spec (i.e. "F1",...,"F24").
    for (int i = 1; i <= 24; ++i) {
      std::string functionKeyName = base::StringPrintf("F%d", i);
      if (functionKeyName == char_text) {
        vk_code = VK_F1 + (i - 1);
        break;
      }
    }
    if (!vk_code) {
      WebString web_char_text =
          WebString::fromUTF8(char_text.data(), char_text.size());
      DCHECK_EQ(web_char_text.length(), 1U);
      vk_text = vk_code = web_char_text.at(0);
      *needs_shift_modifier =
          (vk_code & 0xFF) >= 'A' && (vk_code & 0xFF) <= 'Z';
      if ((vk_code & 0xFF) >= 'a' && (vk_code & 0xFF) <= 'z')
          vk_code -= 'a' - 'A';
      *generate_char = true;
    }
  }

  *code = vk_code;
  *text = vk_text;
}

}  // namespace

void CreateInputEventData(const WebInputEvent& event,
                          std::vector<InputEventData>* result) {
  result->clear();

  switch (event.type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::ContextMenu:
      AppendMouseEvent(event, result);
      break;
    case WebInputEvent::MouseWheel:
      AppendMouseWheelEvent(event, result);
      break;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      AppendKeyEvent(event, result);
      break;
    case WebInputEvent::Char:
      AppendCharEvent(event, result);
      break;
    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
      AppendTouchEvent(event, result);
      break;
    case WebInputEvent::Undefined:
    default:
      break;
  }
}

WebInputEvent* CreateWebInputEvent(const InputEventData& event) {
  scoped_ptr<WebInputEvent> web_input_event;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return NULL;
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      web_input_event.reset(BuildMouseEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_WHEEL:
      web_input_event.reset(BuildMouseWheelEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
      web_input_event.reset(BuildKeyEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_CHAR:
      web_input_event.reset(BuildCharEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      // TODO(kinaba) implement in WebKit an event structure to handle
      // composition events.
      NOTREACHED();
      break;
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      web_input_event.reset(BuildTouchEvent(event));
      break;
  }

  return web_input_event.release();
}

// Generate a coherent sequence of input events to simulate a user event.
// From src/third_party/WebKit/Tools/DumpRenderTree/chromium/EventSender.cpp.
std::vector<linked_ptr<WebInputEvent> > CreateSimulatedWebInputEvents(
    const ppapi::InputEventData& event,
    int plugin_x,
    int plugin_y) {
  std::vector<linked_ptr<WebInputEvent> > events;
  linked_ptr<WebInputEvent> original_event(CreateWebInputEvent(event));

  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      events.push_back(original_event);
      break;

    case PP_INPUTEVENT_TYPE_WHEEL: {
      WebMouseWheelEvent* web_mouse_wheel_event =
          static_cast<WebMouseWheelEvent*>(original_event.get());
      web_mouse_wheel_event->x = plugin_x;
      web_mouse_wheel_event->y = plugin_y;
      events.push_back(original_event);
      break;
    }

    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP: {
      // Windows key down events should always be "raw" to avoid an ASSERT.
#if defined(OS_WIN)
      WebKeyboardEvent* web_keyboard_event =
          static_cast<WebKeyboardEvent*>(original_event.get());
      if (web_keyboard_event->type == WebInputEvent::KeyDown)
        web_keyboard_event->type = WebInputEvent::RawKeyDown;
#endif
      events.push_back(original_event);
      break;
    }

    case PP_INPUTEVENT_TYPE_CHAR: {
      WebKeyboardEvent* web_char_event =
          static_cast<WebKeyboardEvent*>(original_event.get());

      WebUChar code = 0, text = 0;
      bool needs_shift_modifier = false, generate_char = false;
      GetKeyCode(event.character_text,
                 &code,
                 &text,
                 &needs_shift_modifier,
                 &generate_char);

      // Synthesize key down and key up events in all cases.
      scoped_ptr<WebKeyboardEvent> key_down_event(new WebKeyboardEvent());
      scoped_ptr<WebKeyboardEvent> key_up_event(new WebKeyboardEvent());

      key_down_event->type = WebInputEvent::RawKeyDown;
      key_down_event->windowsKeyCode = code;
      key_down_event->nativeKeyCode = code;
      if (needs_shift_modifier)
        key_down_event->modifiers |= WebInputEvent::ShiftKey;

      // If a char event is needed, set the text fields.
      if (generate_char) {
        key_down_event->text[0] = text;
        key_down_event->unmodifiedText[0] = text;
      }
      // Convert the key code to a string identifier.
      key_down_event->setKeyIdentifierFromWindowsKeyCode();

      *key_up_event = *web_char_event = *key_down_event;

      events.push_back(linked_ptr<WebInputEvent>(key_down_event.release()));

      if (generate_char) {
        web_char_event->type = WebInputEvent::Char;
        web_char_event->keyIdentifier[0] = '\0';
        events.push_back(original_event);
      }

      key_up_event->type = WebInputEvent::KeyUp;
      events.push_back(linked_ptr<WebInputEvent>(key_up_event.release()));
      break;
    }

    default:
      break;
  }
  return events;
}

PP_InputEvent_Class ClassifyInputEvent(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::ContextMenu:
      return PP_INPUTEVENT_CLASS_MOUSE;
    case WebInputEvent::MouseWheel:
      return PP_INPUTEVENT_CLASS_WHEEL;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
      return PP_INPUTEVENT_CLASS_KEYBOARD;
    case WebInputEvent::TouchCancel:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchStart:
      return PP_INPUTEVENT_CLASS_TOUCH;
    case WebInputEvent::Undefined:
    default:
      NOTREACHED();
      return PP_InputEvent_Class(0);
  }
}

}  // namespace content
