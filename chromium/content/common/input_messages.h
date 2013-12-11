// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for input events and other messages that require processing in
// order relative to input events.
// Multiply-included message file, hence no include guard.

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_event.h"
#include "content/common/input/input_event_disposition.h"
#include "content/common/input/input_param_traits.h"
#include "content/common/input/ipc_input_event_payload.h"
#include "content/common/input/event_packet.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#ifdef IPC_MESSAGE_START
#error IPC_MESSAGE_START
#endif

#define IPC_MESSAGE_START InputMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::InputEvent::Payload::Type,
                          content::InputEvent::Payload::PAYLOAD_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::InputEventAckState,
                          content::INPUT_EVENT_ACK_STATE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::InputEventDisposition,
                          content::INPUT_EVENT_DISPOSITION_MAX)

IPC_STRUCT_TRAITS_BEGIN(content::EditCommand)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::IPCInputEventPayload)
  IPC_STRUCT_TRAITS_MEMBER(message)
IPC_STRUCT_TRAITS_END()

// Sends an input event to the render widget.
IPC_MESSAGE_ROUTED3(InputMsg_HandleInputEvent,
                    IPC::WebInputEventPointer /* event */,
                    ui::LatencyInfo /* latency_info */,
                    bool /* is_keyboard_shortcut */)

// Sends an event packet to the render widget.
IPC_MESSAGE_ROUTED2(InputMsg_HandleEventPacket,
                    content::EventPacket /* event_packet */,
                    content::InputEventDispositions /* dispositions */)

// Sends the cursor visibility state to the render widget.
IPC_MESSAGE_ROUTED1(InputMsg_CursorVisibilityChange,
                    bool /* is_visible */)

// This message notifies the renderer that the next key event is bound to one
// or more pre-defined edit commands. If the next key event is not handled
// by webkit, the specified edit commands shall be executed against current
// focused frame.
// Parameters
// * edit_commands (see chrome/common/edit_command_types.h)
//   Contains one or more edit commands.
// See third_party/WebKit/Source/WebCore/editing/EditorCommand.cpp for detailed
// definition of webkit edit commands.
//
// This message must be sent just before sending a key event.
IPC_MESSAGE_ROUTED1(InputMsg_SetEditCommandsForNextKeyEvent,
                    std::vector<content::EditCommand> /* edit_commands */)

// Message payload is the name/value of a WebCore edit command to execute.
IPC_MESSAGE_ROUTED2(InputMsg_ExecuteEditCommand,
                    std::string, /* name */
                    std::string /* value */)

IPC_MESSAGE_ROUTED0(InputMsg_MouseCaptureLost)

// TODO(darin): figure out how this meshes with RestoreFocus
IPC_MESSAGE_ROUTED1(InputMsg_SetFocus,
                    bool /* enable */)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(InputMsg_SetInitialFocus,
                    bool /* reverse */)

// Tells the renderer to scroll the currently focused node into rect only if
// the currently focused node is a Text node (textfield, text area or content
// editable divs).
IPC_MESSAGE_ROUTED1(InputMsg_ScrollFocusedEditableNodeIntoRect, gfx::Rect)

// These messages are typically generated from context menus and request the
// renderer to apply the specified operation to the current selection.
IPC_MESSAGE_ROUTED0(InputMsg_Undo)
IPC_MESSAGE_ROUTED0(InputMsg_Redo)
IPC_MESSAGE_ROUTED0(InputMsg_Cut)
IPC_MESSAGE_ROUTED0(InputMsg_Copy)
#if defined(OS_MACOSX)
IPC_MESSAGE_ROUTED0(InputMsg_CopyToFindPboard)
#endif
IPC_MESSAGE_ROUTED0(InputMsg_Paste)
IPC_MESSAGE_ROUTED0(InputMsg_PasteAndMatchStyle)
// Replaces the selected region or a word around the cursor with the
// specified string.
IPC_MESSAGE_ROUTED1(InputMsg_Replace,
                    string16)
// Replaces the misspelling in the selected region with the specified string.
IPC_MESSAGE_ROUTED1(InputMsg_ReplaceMisspelling,
                    string16)
IPC_MESSAGE_ROUTED0(InputMsg_Delete)
IPC_MESSAGE_ROUTED0(InputMsg_SelectAll)

IPC_MESSAGE_ROUTED0(InputMsg_Unselect)

// Requests the renderer to select the region between two points.
// Expects a SelectRange_ACK message when finished.
IPC_MESSAGE_ROUTED2(InputMsg_SelectRange,
                    gfx::Point /* start */,
                    gfx::Point /* end */)

// Requests the renderer to move the caret selection toward the point.
// Expects a MoveCaret_ACK message when finished.
IPC_MESSAGE_ROUTED1(InputMsg_MoveCaret,
                    gfx::Point /* location */)

#if defined(OS_ANDROID)
// Sent when the user clicks on the find result bar to activate a find result.
// The point (x,y) is in fractions of the content document's width and height.
IPC_MESSAGE_ROUTED3(InputMsg_ActivateNearestFindResult,
                    int /* request_id */,
                    float /* x */,
                    float /* y */)
#endif

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Acknowledges receipt of a InputMsg_HandleInputEvent message.
IPC_MESSAGE_ROUTED3(InputHostMsg_HandleInputEvent_ACK,
                    WebKit::WebInputEvent::Type,
                    content::InputEventAckState /* ack_result */,
                    ui::LatencyInfo /* latency_info */)

IPC_MESSAGE_ROUTED2(InputHostMsg_HandleEventPacket_ACK,
                    int64 /* event_packet_id */,
                    content::InputEventDispositions /* dispositions */)


// Adding a new message? Stick to the sort order above: first platform
// independent InputMsg, then ifdefs for platform specific InputMsg, then
// platform independent InputHostMsg, then ifdefs for platform specific
// InputHostMsg.
