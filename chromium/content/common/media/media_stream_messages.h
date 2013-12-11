// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the media streaming.
// Multiply-included message file, hence no include guard.

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaStreamMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaStreamType,
                          content::NUM_MEDIA_TYPES - 1)

IPC_ENUM_TRAITS_MAX_VALUE(content::VideoFacingMode,
                          content::NUM_MEDIA_VIDEO_FACING_MODE - 1)

IPC_STRUCT_TRAITS_BEGIN(content::StreamOptions)
  IPC_STRUCT_TRAITS_MEMBER(audio_type)
  IPC_STRUCT_TRAITS_MEMBER(audio_device_id)
  IPC_STRUCT_TRAITS_MEMBER(video_type)
  IPC_STRUCT_TRAITS_MEMBER(video_device_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::StreamDeviceInfo)
  IPC_STRUCT_TRAITS_MEMBER(device.type)
  IPC_STRUCT_TRAITS_MEMBER(device.name)
  IPC_STRUCT_TRAITS_MEMBER(device.id)
  IPC_STRUCT_TRAITS_MEMBER(device.video_facing)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output_device_id)
  IPC_STRUCT_TRAITS_MEMBER(device.input.sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(device.input.channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(device.input.frames_per_buffer)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.frames_per_buffer)
  IPC_STRUCT_TRAITS_MEMBER(in_use)
  IPC_STRUCT_TRAITS_MEMBER(session_id)
IPC_STRUCT_TRAITS_END()

// Message sent from the browser to the renderer

// The browser has generated a stream successfully.
IPC_MESSAGE_ROUTED4(MediaStreamMsg_StreamGenerated,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfoArray /* audio_device_list */,
                    content::StreamDeviceInfoArray /* video_device_list */)

// The browser has failed to generate a stream.
IPC_MESSAGE_ROUTED1(MediaStreamMsg_StreamGenerationFailed,
                    int /* request id */)

// The browser requests to stop streaming.
// Note that this differs from MediaStreamHostMsg_StopGeneratedStream below
// which is a request from the renderer.
IPC_MESSAGE_ROUTED1(MediaStreamMsg_StopGeneratedStream,
                    std::string /* label */)

// The browser has enumerated devices successfully.
// Used by Pepper.
// TODO(vrk,wjia): Move this to pepper code.
IPC_MESSAGE_ROUTED3(MediaStreamMsg_DevicesEnumerated,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfoArray /* device_list */)

// The browser has failed to enumerate devices.
IPC_MESSAGE_ROUTED1(MediaStreamMsg_DevicesEnumerationFailed,
                    int /* request id */)

// TODO(wjia): should DeviceOpen* messages be merged with
// StreamGenerat* ones?
// The browser has opened a device successfully.
IPC_MESSAGE_ROUTED3(MediaStreamMsg_DeviceOpened,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfo /* the device */)

// The browser has failed to open a device.
IPC_MESSAGE_ROUTED1(MediaStreamMsg_DeviceOpenFailed,
                    int /* request id */)

// Response to enumerate devices request.
IPC_MESSAGE_CONTROL2(MediaStreamMsg_GetSourcesACK,
                     int /* request id */,
                     content::StreamDeviceInfoArray /* device_list */)

// Messages sent from the renderer to the browser.

// Request a new media stream.
IPC_MESSAGE_CONTROL4(MediaStreamHostMsg_GenerateStream,
                     int /* render view id */,
                     int /* request id */,
                     content::StreamOptions /* components */,
                     GURL /* security origin */)

// Request to cancel the request for a new media stream.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_CancelGenerateStream,
                     int /* render view id */,
                     int /* request id */)

// Request to stop streaming from the media stream.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_StopGeneratedStream,
                     int /* render view id */,
                     std::string /* label */)

// Request to enumerate devices.
// Used by Pepper.
// TODO(vrk,wjia): Move this to pepper code.
IPC_MESSAGE_CONTROL4(MediaStreamHostMsg_EnumerateDevices,
                     int /* render view id */,
                     int /* request id */,
                     content::MediaStreamType /* type */,
                     GURL /* security origin */)

// Request to open the device.
IPC_MESSAGE_CONTROL5(MediaStreamHostMsg_OpenDevice,
                     int /* render view id */,
                     int /* request id */,
                     std::string /* device_id */,
                     content::MediaStreamType /* type */,
                     GURL /* security origin */)

// Request to enumerate devices.
IPC_MESSAGE_CONTROL2(MediaStreamHostMsg_GetSources,
                     int /* request id */,
                     GURL /* origin */)
