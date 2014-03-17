// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles video capture messages and delegates them to
// video captures. VideoCaptureMessageFilter is operated on IO thread of
// render process. It intercepts video capture messages and process them on
// IO thread since these messages are time critical.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_

#include <map>

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/video/capture/video_capture.h"

namespace content {

class CONTENT_EXPORT VideoCaptureMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    // Called when a video frame buffer is created in the browser process.
    virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                                 int length,
                                 int buffer_id) = 0;

    virtual void OnBufferDestroyed(int buffer_id) = 0;

    // Called when a video frame buffer is received from the browser process.
    virtual void OnBufferReceived(int buffer_id,
                                  base::Time timestamp,
                                  const media::VideoCaptureFormat& format) = 0;

    // Called when state of a video capture device has changed in the browser
    // process.
    virtual void OnStateChanged(VideoCaptureState state) = 0;

    // Called when the delegate has been added to filter's delegate list.
    // |device_id| is the device id for the delegate.
    virtual void OnDelegateAdded(int32 device_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  VideoCaptureMessageFilter();

  // Add a delegate to the map.
  void AddDelegate(Delegate* delegate);

  // Remove a delegate from the map.
  void RemoveDelegate(Delegate* delegate);

  // Send a message asynchronously.
  virtual bool Send(IPC::Message* message);

  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

 protected:
  virtual ~VideoCaptureMessageFilter();

 private:
  typedef std::map<int32, Delegate*> Delegates;

  // Receive a newly created buffer from browser process.
  void OnBufferCreated(int device_id,
                       base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id);

  // Release a buffer received by OnBufferCreated.
  void OnBufferDestroyed(int device_id,
                         int buffer_id);

  // Receive a filled buffer from browser process.
  void OnBufferReceived(int device_id,
                        int buffer_id,
                        base::Time timestamp,
                        const media::VideoCaptureFormat& format);

  // State of browser process' video capture device has changed.
  void OnDeviceStateChanged(int device_id, VideoCaptureState state);

  // Finds the delegate associated with |device_id|, NULL if not found.
  Delegate* find_delegate(int device_id) const;

  // A map of device ids to delegates.
  Delegates delegates_;
  Delegates pending_delegates_;
  int32 last_device_id_;

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
