// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/WebStorageArea.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START DOMStorageMsgStart

// Signals a local storage event.
IPC_STRUCT_BEGIN(DOMStorageMsg_Event_Params)
  // The key that generated the storage event.  Null if clear() was called.
  IPC_STRUCT_MEMBER(base::NullableString16, key)

  // The old value of this key.  Null on clear() or if it didn't have a value.
  IPC_STRUCT_MEMBER(base::NullableString16, old_value)

  // The new value of this key.  Null on removeItem() or clear().
  IPC_STRUCT_MEMBER(base::NullableString16, new_value)

  // The origin this is associated with.
  IPC_STRUCT_MEMBER(GURL, origin)

  // The URL of the page that caused the storage event.
  IPC_STRUCT_MEMBER(GURL, page_url)

  // The non-zero connection_id which caused the event or 0 if the event
  // was not caused by the target renderer process.
  IPC_STRUCT_MEMBER(int, connection_id)

  // The non-zero session namespace_id associated with the event or 0 if
  // this is a local storage event.
  IPC_STRUCT_MEMBER(int64, namespace_id)
IPC_STRUCT_END()

IPC_ENUM_TRAITS(blink::WebStorageArea::Result)

// DOM Storage messages sent from the browser to the renderer.

// Storage events are broadcast to all renderer processes.
IPC_MESSAGE_CONTROL1(DOMStorageMsg_Event,
                     DOMStorageMsg_Event_Params)

// Completion notification sent in response to each async
// load, set, remove, and clear operation.
// Used to maintain the integrity  of the renderer-side cache.
IPC_MESSAGE_CONTROL1(DOMStorageMsg_AsyncOperationComplete,
                     bool /* success */)

// Notification instructing the renderer to refresh all cached values for
// the given namespace.
IPC_MESSAGE_CONTROL1(DOMStorageMsg_ResetCachedValues,
                     int64 /* namespace_id */)

// DOM Storage messages sent from the renderer to the browser.
// Note: The 'connection_id' must be the first parameter in these message.

// Open the storage area for a particular origin within a namespace.
IPC_MESSAGE_CONTROL3(DOMStorageHostMsg_OpenStorageArea,
                     int /* connection_id */,
                     int64 /* namespace_id */,
                     GURL /* origin */)

// Close a previously opened storage area.
IPC_MESSAGE_CONTROL1(DOMStorageHostMsg_CloseStorageArea,
                     int /* connection_id */)

// Retrieves the set of key/value pairs for the area. Used to prime
// the renderer-side cache. A completion notification is sent in response.
// The response will also indicate whether the renderer should send
// messagse to the browser for get operations for logging purposes.
IPC_SYNC_MESSAGE_CONTROL1_2(DOMStorageHostMsg_LoadStorageArea,
                            int /* connection_id */,
                            content::DOMStorageValuesMap,
                            bool /* send_log_get_messages */)

// Set a value that's associated with a key in a storage area.
// A completion notification is sent in response.
IPC_MESSAGE_CONTROL4(DOMStorageHostMsg_SetItem,
                     int /* connection_id */,
                     base::string16 /* key */,
                     base::string16 /* value */,
                     GURL /* page_url */)

// Logs that a get operation was performed on a key/value pair.
IPC_MESSAGE_CONTROL3(DOMStorageHostMsg_LogGetItem,
                     int /* connection_id */,
                     base::string16 /* key */,
                     base::NullableString16 /* value */)

// Remove the value associated with a key in a storage area.
// A completion notification is sent in response.
IPC_MESSAGE_CONTROL3(DOMStorageHostMsg_RemoveItem,
                     int /* connection_id */,
                     base::string16 /* key */,
                     GURL /* page_url */)

// Clear the storage area. A completion notification is sent in response.
IPC_MESSAGE_CONTROL2(DOMStorageHostMsg_Clear,
                     int /* connection_id */,
                     GURL /* page_url */)

// Used to flush the ipc message queue.
IPC_SYNC_MESSAGE_CONTROL0_0(DOMStorageHostMsg_FlushMessages)
