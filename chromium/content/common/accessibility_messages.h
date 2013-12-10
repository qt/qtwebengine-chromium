// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for accessibility.
// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/content_export.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

IPC_ENUM_TRAITS(WebKit::WebAXEvent)
IPC_ENUM_TRAITS(WebKit::WebAXRole)

IPC_ENUM_TRAITS(content::AccessibilityNodeData::BoolAttribute)
IPC_ENUM_TRAITS(content::AccessibilityNodeData::FloatAttribute)
IPC_ENUM_TRAITS(content::AccessibilityNodeData::IntAttribute)
IPC_ENUM_TRAITS(content::AccessibilityNodeData::IntListAttribute)
IPC_ENUM_TRAITS(content::AccessibilityNodeData::StringAttribute)

IPC_STRUCT_TRAITS_BEGIN(content::AccessibilityNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_EventParams)
  // Vector of nodes in the tree that need to be updated before
  // sending the event.
  IPC_STRUCT_MEMBER(std::vector<content::AccessibilityNodeData>, nodes)

  // Type of event.
  IPC_STRUCT_MEMBER(WebKit::WebAXEvent, event_type)

  // ID of the node that the event applies to.
  IPC_STRUCT_MEMBER(int, id)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Relay a request from assistive technology to set focus to a given node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_SetFocus,
                    int /* object id */)

// Relay a request from assistive technology to perform the default action
// on a given node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_DoDefaultAction,
                    int /* object id */)

// Relay a request from assistive technology to make a given object
// visible by scrolling as many scrollable containers as possible.
// In addition, if it's not possible to make the entire object visible,
// scroll so that the |subfocus| rect is visible at least. The subfocus
// rect is in local coordinates of the object itself.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_ScrollToMakeVisible,
                    int /* object id */,
                    gfx::Rect /* subfocus */)

// Relay a request from assistive technology to move a given object
// to a specific location, in the WebContents area coordinate space, i.e.
// (0, 0) is the top-left corner of the WebContents.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_ScrollToPoint,
                    int /* object id */,
                    gfx::Point /* new location */)

// Relay a request from assistive technology to set the cursor or
// selection within an editable text element.
IPC_MESSAGE_ROUTED3(AccessibilityMsg_SetTextSelection,
                    int /* object id */,
                    int /* New start offset */,
                    int /* New end offset */)

// Tells the render view that a AccessibilityHostMsg_Events
// message was processed and it can send addition events.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_Events_ACK)


// Kill the renderer because we got a fatal error in the accessibility tree.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_FatalError)

// Messages sent from the renderer to the browser.

// Sent to notify the browser about renderer accessibility events.
// The browser responds with a AccessibilityMsg_Events_ACK.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_Events,
    std::vector<AccessibilityHostMsg_EventParams>)
