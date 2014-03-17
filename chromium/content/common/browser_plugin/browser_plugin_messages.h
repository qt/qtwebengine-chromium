// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/values.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/edit_command.h"
#include "content/public/common/browser_plugin_permission_type.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/drop_data.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"
#include "webkit/common/cursors/webcursor.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START BrowserPluginMsgStart


IPC_ENUM_TRAITS(blink::WebDragStatus)

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_AutoSize_Params)
  IPC_STRUCT_MEMBER(bool, enable)
  IPC_STRUCT_MEMBER(gfx::Size, max_size)
  IPC_STRUCT_MEMBER(gfx::Size, min_size)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_ResizeGuest_Params)
  // Indicates whether the parameters have been populated or not.
  IPC_STRUCT_MEMBER(bool, size_changed)
  // The sequence number used to uniquely identify the damage buffer for the
  // current container size.
  IPC_STRUCT_MEMBER(uint32, damage_buffer_sequence_id)
  // The handle to use to map the damage buffer in the browser process.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, damage_buffer_handle)
  // The size of the damage buffer.
  IPC_STRUCT_MEMBER(size_t, damage_buffer_size)
  // The new rect of the guest view area.
  IPC_STRUCT_MEMBER(gfx::Rect, view_rect)
  // Indicates the scale factor of the embedder WebView.
  IPC_STRUCT_MEMBER(float, scale_factor)
  // Indicates a request for a full repaint of the page.
  // This is required for switching from compositing to the software
  // rendering path.
  IPC_STRUCT_MEMBER(bool, repaint)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_Attach_Params)
  IPC_STRUCT_MEMBER(std::string, storage_partition_id)
  IPC_STRUCT_MEMBER(bool, persist_storage)
  IPC_STRUCT_MEMBER(bool, focused)
  IPC_STRUCT_MEMBER(bool, visible)
  IPC_STRUCT_MEMBER(bool, opaque)
  IPC_STRUCT_MEMBER(std::string, name)
  IPC_STRUCT_MEMBER(std::string, src)
  IPC_STRUCT_MEMBER(GURL, embedder_frame_url)
  IPC_STRUCT_MEMBER(BrowserPluginHostMsg_AutoSize_Params, auto_size_params)
  IPC_STRUCT_MEMBER(BrowserPluginHostMsg_ResizeGuest_Params,
                    resize_guest_params)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginMsg_Attach_ACK_Params)
  IPC_STRUCT_MEMBER(std::string, storage_partition_id)
  IPC_STRUCT_MEMBER(bool, persist_storage)
  IPC_STRUCT_MEMBER(std::string, name)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginMsg_BuffersSwapped_Params)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::Rect, damage_rect)
  IPC_STRUCT_MEMBER(std::string, mailbox_name)
  IPC_STRUCT_MEMBER(int, route_id)
  IPC_STRUCT_MEMBER(int, host_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginMsg_UpdateRect_Params)
  // The sequence number of the damage buffer used by the browser process.
  IPC_STRUCT_MEMBER(uint32, damage_buffer_sequence_id)

  // The position and size of the bitmap.
  IPC_STRUCT_MEMBER(gfx::Rect, bitmap_rect)

  // The scroll delta.  Only one of the delta components can be non-zero, and if
  // they are both zero, then it means there is no scrolling and the scroll_rect
  // is ignored.
  IPC_STRUCT_MEMBER(gfx::Vector2d, scroll_delta)

  // The rectangular region to scroll.
  IPC_STRUCT_MEMBER(gfx::Rect, scroll_rect)

  // The scroll offset of the render view.
  IPC_STRUCT_MEMBER(gfx::Point, scroll_offset)

  // The regions of the bitmap (in view coords) that contain updated pixels.
  // In the case of scrolling, this includes the scroll damage rect.
  IPC_STRUCT_MEMBER(std::vector<gfx::Rect>, copy_rects)

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress. If auto-resize is enabled, this should update the corresponding
  // view size.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

  // All the above coordinates are in DIP. This is the scale factor needed
  // to convert them to pixels.
  IPC_STRUCT_MEMBER(float, scale_factor)

  // Is this UpdateRect an ACK to a resize request?
  IPC_STRUCT_MEMBER(bool, is_resize_ack)

  // Used in HW accelerated case to switch between sending an UpdateRect_ACK
  // with the new size or just resizing.
  IPC_STRUCT_MEMBER(bool, needs_ack)
IPC_STRUCT_END()

// Browser plugin messages

// -----------------------------------------------------------------------------
// These messages are from the embedder to the browser process.

// This message is sent to the browser process to request an instance ID.
// |request_id| is used by BrowserPluginEmbedder to route the response back
// to its origin.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_AllocateInstanceID,
                    int /* request_id */)

// This message is sent from BrowserPlugin to BrowserPluginGuest to issue an
// edit command.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ExecuteEditCommand,
                     int /* instance_id */,
                     std::string /* command */)

// This message must be sent just before sending a key event.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent,
                    int /* instance_id */,
                    std::vector<content::EditCommand> /* edit_commands */)

// This message is sent from BrowserPlugin to BrowserPluginGuest whenever IME
// composition state is updated.
IPC_MESSAGE_ROUTED5(
    BrowserPluginHostMsg_ImeSetComposition,
    int /* instance_id */,
    std::string /* text */,
    std::vector<blink::WebCompositionUnderline> /* underlines */,
    int /* selectiont_start */,
    int /* selection_end */)

// This message is sent from BrowserPlugin to BrowserPluginGuest to notify that
// confirming the current composition is requested.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_ImeConfirmComposition,
                    int /* instance_id */,
                    std::string /* text */,
                    bool /* keep selection */)

// Deletes the current selection plus the specified number of characters before
// and after the selection or caret.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_ExtendSelectionAndDelete,
                    int /* instance_id */,
                    int /* before */,
                    int /* after */)

// This message is sent to the browser process to enable or disable autosize
// mode.
IPC_MESSAGE_ROUTED3(
    BrowserPluginHostMsg_SetAutoSize,
    int /* instance_id */,
    BrowserPluginHostMsg_AutoSize_Params /* auto_size_params */,
    BrowserPluginHostMsg_ResizeGuest_Params /* resize_guest_params */)

// This message is sent to the browser process to indicate that a BrowserPlugin
// has taken ownership of the lifetime of the guest of the given |instance_id|.
// |params| is the state of the BrowserPlugin taking ownership of
// the guest. If a guest doesn't already exist with the given |instance_id|,
// a new one will be created.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_Attach,
                    int /* instance_id */,
                    BrowserPluginHostMsg_Attach_Params /* params */,
                    base::DictionaryValue /* extra_params */)

// Tells the guest to focus or defocus itself.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetFocus,
                    int /* instance_id */,
                    bool /* enable */)

// Sends an input event to the guest.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_HandleInputEvent,
                    int /* instance_id */,
                    gfx::Rect /* guest_window_rect */,
                    IPC::WebInputEventPointer /* event */)

// An ACK to the guest process letting it know that the embedder has handled
// the previous frame and is ready for the next frame. If the guest sent the
// embedder a bitmap that does not match the size of the BrowserPlugin's
// container, the BrowserPlugin requests a new size as well.
IPC_MESSAGE_ROUTED4(BrowserPluginHostMsg_UpdateRect_ACK,
    int /* instance_id */,
    bool /* needs_ack */,
    BrowserPluginHostMsg_AutoSize_Params /* auto_size_params */,
    BrowserPluginHostMsg_ResizeGuest_Params /* resize_guest_params */)

// A BrowserPlugin sends this to BrowserPluginEmbedder (browser process) when it
// wants to navigate to a given src URL. If a guest WebContents already exists,
// it will navigate that WebContents. If not, it will create the WebContents,
// associate it with the BrowserPluginGuest, and navigate it to the requested
// URL.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_NavigateGuest,
                    int /* instance_id*/,
                    std::string /* src */)

// Acknowledge that we presented a HW buffer and provide a sync point
// to specify the location in the command stream when the compositor
// is no longer using it.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_BuffersSwappedACK,
                    int /* instance_id */,
                    int /* route_id */,
                    int /* gpu_host_id */,
                    std::string /* mailbox_name */,
                    uint32 /* sync_point */)

IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_CopyFromCompositingSurfaceAck,
                    int /* instance_id */,
                    int /* request_id */,
                    SkBitmap);

// Acknowledge that we presented an ubercomp frame.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_CompositorFrameACK,
                    int /* instance_id */,
                    int /* route_id */,
                    uint32 /* output_surface_id */,
                    int /* renderer_host_id */,
                    cc::CompositorFrameAck /* ack */)

// Notify the guest renderer that some resources given to the embededer
// are not used any more.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_ReclaimCompositorResources,
                    int /* instance_id */,
                    int /* route_id */,
                    uint32 /* output_surface_id */,
                    int /* renderer_host_id */,
                    cc::CompositorFrameAck /* ack */)

// When a BrowserPlugin has been removed from the embedder's DOM, it informs
// the browser process to cleanup the guest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_PluginDestroyed,
                    int /* instance_id */)

// Tells the guest it has been shown or hidden.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetVisibility,
                    int /* instance_id */,
                    bool /* visible */)

// Tells the guest to change its background opacity.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetContentsOpaque,
                    int /* instance_id */,
                    bool /* opaque */)

// Tells the guest that a drag event happened on the plugin.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_DragStatusUpdate,
                    int /* instance_id */,
                    blink::WebDragStatus /* drag_status */,
                    content::DropData /* drop_data */,
                    blink::WebDragOperationsMask /* operation_mask */,
                    gfx::Point /* plugin_location */)

// Response to BrowserPluginMsg_PluginAtPositionRequest, returns the browser
// plugin instace id and the coordinates (local to the plugin).
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_PluginAtPositionResponse,
                    int /* instance_id */,
                    int /* request_id */,
                    gfx::Point /* position */)

// Sets the name of the guest window to the provided |name|.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetName,
                    int /* instance_id */,
                    std::string /* name */)

// Sends a PointerLock Lock ACK to the BrowserPluginGuest.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_LockMouse_ACK,
                    int /* instance_id */,
                    bool /* succeeded */)

// Sends a PointerLock Unlock ACK to the BrowserPluginGuest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_UnlockMouse_ACK, int /* instance_id */)

// Sent when plugin's position has changed without UpdateRect.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_UpdateGeometry,
                    int /* instance_id */,
                    gfx::Rect /* view_rect */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

// A embedder sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ResizeGuest,
                    int /* instance_id*/,
                    BrowserPluginHostMsg_ResizeGuest_Params)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the embedder.

// This message is sent from the browser process to the embedder render process
// in response to a request to allocate an instance ID. The |request_id| is used
// to route the response to the requestor.
IPC_MESSAGE_ROUTED2(BrowserPluginMsg_AllocateInstanceID_ACK,
                    int /* request_id */,
                    int /* instance_id */)

// This message is sent in response to a completed attachment of a guest
// to a BrowserPlugin. This message carries information about the guest
// that is used to update the attributes of the browser plugin.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_Attach_ACK,
                     int /* instance_id */,
                     BrowserPluginMsg_Attach_ACK_Params /* params */)

// Once the swapped out guest RenderView has been created in the embedder render
// process, the browser process informs the embedder of its routing ID.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_GuestContentWindowReady,
                     int /* instance_id */,
                     int /* source_routing_id */)

// When the guest crashes, the browser process informs the embedder through this
// message.
IPC_MESSAGE_CONTROL1(BrowserPluginMsg_GuestGone,
                     int /* instance_id */)

// When the user tabs to the end of the tab stops of a guest, the browser
// process informs the embedder to tab out of the browser plugin.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_AdvanceFocus,
                     int /* instance_id */,
                     bool /* reverse */)

// When the guest starts/stops listening to touch events, it needs to notify the
// plugin in the embedder about it.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_ShouldAcceptTouchEvents,
                     int /* instance_id */,
                     bool /* accept */)

// Inform the embedder of the cursor the guest wishes to display.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_SetCursor,
                     int /* instance_id */,
                     WebCursor /* cursor */)

// The guest has damage it wants to convey to the embedder so that it can
// update its backing store.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_UpdateRect,
                     int /* instance_id */,
                     BrowserPluginMsg_UpdateRect_Params)

IPC_MESSAGE_CONTROL4(BrowserPluginMsg_CopyFromCompositingSurface,
                     int /* instance_id */,
                     int /* request_id */,
                     gfx::Rect  /* source_rect */,
                     gfx::Size  /* dest_size */)

// Requests the renderer to find out if a browser plugin is at position
// (|x|, |y|) within the embedder.
// The response message is BrowserPluginHostMsg_PluginAtPositionResponse.
// The |request_id| uniquely identifies a request from an embedder.
IPC_MESSAGE_ROUTED2(BrowserPluginMsg_PluginAtPositionRequest,
                    int /* request_id */,
                    gfx::Point /* position */)

// Informs BrowserPlugin of a new name set for the top-level guest frame.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_UpdatedName,
                     int /* instance_id */,
                     std::string /* name */)

// Guest renders into an FBO with textures provided by the embedder.
// When HW accelerated buffers are swapped in the guest, the message
// is forwarded to the embedder to notify it of a new texture
// available for compositing.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_BuffersSwapped,
                     int /* instance_id */,
                     BrowserPluginMsg_BuffersSwapped_Params)

IPC_MESSAGE_CONTROL5(BrowserPluginMsg_CompositorFrameSwapped,
                     int /* instance_id */,
                     cc::CompositorFrame /* frame */,
                     int /* route_id */,
                     uint32 /* output_surface_id */,
                     int /* renderer_host_id */)

// Forwards a PointerLock Unlock request to the BrowserPlugin.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_SetMouseLock,
                     int /* instance_id */,
                     bool /* enable */)
