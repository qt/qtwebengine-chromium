// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/video_capture_messages.h"

namespace content {

VideoCaptureHost::VideoCaptureHost(MediaStreamManager* media_stream_manager)
    : media_stream_manager_(media_stream_manager) {
}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  // Since the IPC channel is gone, close all requested VideoCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); it++) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller) {
      VideoCaptureControllerID controller_id(it->first);
      media_stream_manager_->video_capture_manager()->StopCaptureForClient(
          controller.get(), controller_id, this);
    }
  }
}

void VideoCaptureHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////

// Implements VideoCaptureControllerEventHandler.
void VideoCaptureHost::OnError(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnError";
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoHandleErrorOnIOThread,
                 this, controller_id));
}

void VideoCaptureHost::OnBufferCreated(
    const VideoCaptureControllerID& controller_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendNewBufferOnIOThread,
                 this, controller_id, handle, length, buffer_id));
}

void VideoCaptureHost::OnBufferDestroyed(
    const VideoCaptureControllerID& controller_id,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFreeBufferOnIOThread,
                 this, controller_id, buffer_id));
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& controller_id,
    int buffer_id,
    base::Time timestamp,
    const media::VideoCaptureFormat& frame_format) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFilledBufferOnIOThread,
                 this, controller_id, buffer_id, timestamp,
                 frame_format));
}

void VideoCaptureHost::OnEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnEnded";
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoEndedOnIOThread, this, controller_id));
}

void VideoCaptureHost::DoSendNewBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_NewBuffer(controller_id.device_id, handle,
                                     length, buffer_id));
}

void VideoCaptureHost::DoSendFreeBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_FreeBuffer(controller_id.device_id, buffer_id));
}

void VideoCaptureHost::DoSendFilledBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int buffer_id, base::Time timestamp,
    const media::VideoCaptureFormat& format) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_BufferReady(controller_id.device_id, buffer_id,
                                       timestamp, format));
}

void VideoCaptureHost::DoHandleErrorOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_ERROR));
  DeleteVideoCaptureControllerOnIOThread(controller_id);
}

void VideoCaptureHost::DoEndedOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::DoEndedOnIOThread";
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_ENDED));
  DeleteVideoCaptureControllerOnIOThread(controller_id);
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler.
bool VideoCaptureHost::OnMessageReceived(const IPC::Message& message,
                                         bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(VideoCaptureHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, OnStartCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, OnPauseCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, OnStopCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady, OnReceiveEmptyBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void VideoCaptureHost::OnStartCapture(int device_id,
                                      media::VideoCaptureSessionId session_id,
                                      const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::OnStartCapture:"
           << " session_id=" << session_id
           << ", device_id=" << device_id
           << ", format=" << params.requested_format.frame_size.ToString()
           << "@" << params.requested_format.frame_rate
           << " (" << (params.allow_resolution_change ? "variable" : "constant")
           << ")";
  VideoCaptureControllerID controller_id(device_id);
  DCHECK(entries_.find(controller_id) == entries_.end());

  entries_[controller_id] = base::WeakPtr<VideoCaptureController>();
  media_stream_manager_->video_capture_manager()->StartCaptureForClient(
      session_id,
      params,
      PeerHandle(),
      controller_id,
      this,
      base::Bind(&VideoCaptureHost::OnControllerAdded, this, device_id));
}

void VideoCaptureHost::OnControllerAdded(
    int device_id,
    const base::WeakPtr<VideoCaptureController>& controller) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureHost::DoControllerAddedOnIOThread,
                 this,
                 device_id,
                 controller));
}

void VideoCaptureHost::DoControllerAddedOnIOThread(
    int device_id,
    const base::WeakPtr<VideoCaptureController>& controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end()) {
    if (controller) {
      media_stream_manager_->video_capture_manager()->StopCaptureForClient(
          controller.get(), controller_id, this);
    }
    return;
  }

  if (!controller) {
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          VIDEO_CAPTURE_STATE_ERROR));
    entries_.erase(controller_id);
    return;
  }

  DCHECK(!it->second);
  it->second = controller;
}

void VideoCaptureHost::OnStopCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::OnStopCapture, device_id " << device_id;

  VideoCaptureControllerID controller_id(device_id);

  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        VIDEO_CAPTURE_STATE_STOPPED));
  DeleteVideoCaptureControllerOnIOThread(controller_id);
}

void VideoCaptureHost::OnPauseCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::OnPauseCapture, device_id " << device_id;
  // Not used.
  Send(new VideoCaptureMsg_StateChanged(device_id, VIDEO_CAPTURE_STATE_ERROR));
}

void VideoCaptureHost::OnReceiveEmptyBuffer(int device_id, int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    const base::WeakPtr<VideoCaptureController>& controller = it->second;
    if (controller)
      controller->ReturnBuffer(controller_id, this, buffer_id);
  }
}

void VideoCaptureHost::DeleteVideoCaptureControllerOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end())
    return;

  if (it->second) {
    media_stream_manager_->video_capture_manager()->StopCaptureForClient(
        it->second.get(), controller_id, this);
  }
  entries_.erase(it);
}

}  // namespace content
