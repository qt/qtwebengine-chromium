// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureHost serves video capture related messages from
// VideoCaptureMessageFilter which lives inside the render process.
//
// This class is owned by RenderProcessHostImpl, and instantiated on UI
// thread, but all other operations and method calls happen on IO thread.
//
// Here's an example of a typical IPC dialog for video capture:
//
//   Renderer                             VideoCaptureHost
//      |                                        |
//      |  VideoCaptureHostMsg_Start >           |
//      | < VideoCaptureMsg_StateChanged         |
//      |        (VIDEO_CAPTURE_STATE_STARTED)   |
//      | < VideoCaptureMsg_NewBuffer(1)         |
//      | < VideoCaptureMsg_NewBuffer(2)         |
//      | < VideoCaptureMsg_NewBuffer(3)         |
//      |                                        |
//      | < VideoCaptureMsg_BufferReady(1)       |
//      | < VideoCaptureMsg_BufferReady(2)       |
//      | VideoCaptureHostMsg_BufferReady(1) >   |
//      | < VideoCaptureMsg_BufferReady(3)       |
//      | VideoCaptureHostMsg_BufferReady(2) >   |
//      | < VideoCaptureMsg_BufferReady(1)       |
//      | VideoCaptureHostMsg_BufferReady(3) >   |
//      | < VideoCaptureMsg_BufferReady(2)       |
//      | VideoCaptureHostMsg_BufferReady(1) >   |
//      |             ...                        |
//      | < VideoCaptureMsg_BufferReady(3)       |
//      |                                        |
//      |             ... (resolution change)    |
//      | < VideoCaptureMsg_FreeBuffer(1)        |  Buffers are re-allocated
//      | < VideoCaptureMsg_NewBuffer(4)         |  at a larger size, as
//      | < VideoCaptureMsg_BufferReady(4)       |  needed.
//      | VideoCaptureHostMsg_BufferReady(2) >   |
//      | < VideoCaptureMsg_FreeBuffer(2)        |
//      | < VideoCaptureMsg_NewBuffer(5)         |
//      | < VideoCaptureMsg_BufferReady(5)       |
//      |             ...                        |
//      |                                        |
//      | < VideoCaptureMsg_BufferReady          |
//      | VideoCaptureHostMsg_Stop >             |
//      | VideoCaptureHostMsg_BufferReady >      |
//      | < VideoCaptureMsg_StateChanged         |
//      |         (VIDEO_CAPTURE_STATE_STOPPED)  |
//      v                                        v

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_message.h"

namespace content {
class MediaStreamManager;

class CONTENT_EXPORT VideoCaptureHost
    : public BrowserMessageFilter,
      public VideoCaptureControllerEventHandler {
 public:
  explicit VideoCaptureHost(MediaStreamManager* media_stream_manager);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // VideoCaptureControllerEventHandler implementation.
  virtual void OnError(const VideoCaptureControllerID& id) OVERRIDE;
  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length,
                               int buffer_id) OVERRIDE;
  virtual void OnBufferDestroyed(const VideoCaptureControllerID& id,
                                 int buffer_id) OVERRIDE;
  virtual void OnBufferReady(
      const VideoCaptureControllerID& id,
      int buffer_id,
      base::Time timestamp,
      const media::VideoCaptureFormat& format) OVERRIDE;
  virtual void OnEnded(const VideoCaptureControllerID& id) OVERRIDE;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<VideoCaptureHost>;
  friend class MockVideoCaptureHost;
  friend class VideoCaptureHostTest;

  virtual ~VideoCaptureHost();

  // IPC message: Start capture on the VideoCaptureDevice referenced by
  // |session_id|. |device_id| is an id created by VideoCaptureMessageFilter
  // to identify a session between a VideoCaptureMessageFilter and a
  // VideoCaptureHost.
  void OnStartCapture(int device_id,
                      media::VideoCaptureSessionId session_id,
                      const media::VideoCaptureParams& params);
  void OnControllerAdded(
      int device_id,
      const base::WeakPtr<VideoCaptureController>& controller);
  void DoControllerAddedOnIOThread(
      int device_id,
      const base::WeakPtr<VideoCaptureController>& controller);

  // IPC message: Stop capture on device referenced by |device_id|.
  void OnStopCapture(int device_id);

  // IPC message: Pause capture on device referenced by |device_id|.
  void OnPauseCapture(int device_id);

  // IPC message: Receive an empty buffer from renderer. Send it to device
  // referenced by |device_id|.
  void OnReceiveEmptyBuffer(int device_id, int buffer_id);

  // Send a newly created buffer to the VideoCaptureMessageFilter.
  void DoSendNewBufferOnIOThread(
      const VideoCaptureControllerID& controller_id,
      base::SharedMemoryHandle handle,
      int length,
      int buffer_id);

  void DoSendFreeBufferOnIOThread(
      const VideoCaptureControllerID& controller_id,
      int buffer_id);

  // Send a filled buffer to the VideoCaptureMessageFilter.
  void DoSendFilledBufferOnIOThread(
      const VideoCaptureControllerID& controller_id,
      int buffer_id,
      base::Time timestamp,
      const media::VideoCaptureFormat& format);

  // Handle error coming from VideoCaptureDevice.
  void DoHandleErrorOnIOThread(const VideoCaptureControllerID& controller_id);

  void DoEndedOnIOThread(const VideoCaptureControllerID& controller_id);

  void DeleteVideoCaptureControllerOnIOThread(
      const VideoCaptureControllerID& controller_id);

  MediaStreamManager* media_stream_manager_;

  typedef std::map<VideoCaptureControllerID,
                   base::WeakPtr<VideoCaptureController> > EntryMap;

  // A map of VideoCaptureControllerID to the VideoCaptureController to which it
  // is connected. An entry in this map holds a null controller while it is in
  // the process of starting.
  EntryMap entries_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
