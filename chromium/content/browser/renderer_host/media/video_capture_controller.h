// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureController is the glue between a VideoCaptureDevice and all
// VideoCaptureHosts that have connected to it. A controller exists on behalf of
// one (and only one) VideoCaptureDevice; both are owned by the
// VideoCaptureManager.
//
// The VideoCaptureController is responsible for:
//
//   * Allocating and keeping track of shared memory buffers, and filling them
//     with I420 video frames for IPC communication between VideoCaptureHost (in
//     the browser process) and VideoCaptureMessageFilter (in the renderer
//     process).
//   * Broadcasting the events from a single VideoCaptureDevice, fanning them
//     out to multiple clients.
//   * Keeping track of the clients on behalf of the VideoCaptureManager, making
//     it possible for the Manager to delete the Controller and its Device when
//     there are no clients left.
//
// A helper class, VCC::VideoCaptureDeviceClient, is responsible for:
//
//   * Conveying events from the device thread (where VideoCaptureDevices live)
//     the IO thread (where the VideoCaptureController lives).
//   * Performing some image transformations on the output of the Device;
//     specifically, colorspace conversion and rotation.
//
// Interactions between VideoCaptureController and other classes:
//
//   * VideoCaptureController indirectly observes a VideoCaptureDevice
//     by means of its proxy, VideoCaptureDeviceClient, which implements
//     the VideoCaptureDevice::EventHandler interface. The proxy forwards
//     observed events to the VideoCaptureController on the IO thread.
//   * A VideoCaptureController interacts with its clients (VideoCaptureHosts)
//     via the VideoCaptureControllerEventHandler interface.
//   * Conversely, a VideoCaptureControllerEventHandler (typically,
//     VideoCaptureHost) will interact directly with VideoCaptureController to
//     return leased buffers by means of the ReturnBuffer() public method of
//     VCC.
//   * VideoCaptureManager (which owns the VCC) interacts directly with
//     VideoCaptureController through its public methods, to add and remove
//     clients.
//
// VideoCaptureController is not thread safe and operates on the IO thread only.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace content {
class VideoCaptureBufferPool;

class CONTENT_EXPORT VideoCaptureController {
 public:
  VideoCaptureController();
  virtual ~VideoCaptureController();

  base::WeakPtr<VideoCaptureController> GetWeakPtr();

  // Return a new VideoCaptureDeviceClient to forward capture events to this
  // instance.
  scoped_ptr<media::VideoCaptureDevice::EventHandler> NewDeviceClient();

  // Start video capturing and try to use the resolution specified in
  // |params|.
  // When capturing starts, the |event_handler| will receive an OnFrameInfo()
  // call informing it of the resolution that was actually picked by the device.
  void AddClient(const VideoCaptureControllerID& id,
                 VideoCaptureControllerEventHandler* event_handler,
                 base::ProcessHandle render_process,
                 const media::VideoCaptureParams& params);

  // Stop video capture. This will take back all buffers held by by
  // |event_handler|, and |event_handler| shouldn't use those buffers any more.
  // Returns the session_id of the stopped client, or
  // kInvalidMediaCaptureSessionId if the indicated client was not registered.
  int RemoveClient(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* event_handler);

  int GetClientCount();

  // API called directly by VideoCaptureManager in case the device is
  // prematurely closed.
  void StopSession(int session_id);

  // Return a buffer previously given in
  // VideoCaptureControllerEventHandler::OnBufferReady.
  void ReturnBuffer(const VideoCaptureControllerID& id,
                    VideoCaptureControllerEventHandler* event_handler,
                    int buffer_id);

 private:
  class VideoCaptureDeviceClient;

  struct ControllerClient;
  typedef std::list<ControllerClient*> ControllerClients;

  // Worker functions on IO thread. Called by the VideoCaptureDeviceClient.
  void DoIncomingCapturedFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& captured_frame,
      base::Time timestamp);
  void DoFrameInfoOnIOThread(
      const media::VideoCaptureCapability& frame_info,
      const scoped_refptr<VideoCaptureBufferPool>& buffer_pool);
  void DoFrameInfoChangedOnIOThread(const media::VideoCaptureCapability& info);
  void DoErrorOnIOThread();
  void DoDeviceStoppedOnIOThread();

  // Send frame info and init buffers to |client|.
  void SendFrameInfoAndBuffers(ControllerClient* client);

  // Find a client of |id| and |handler| in |clients|.
  ControllerClient* FindClient(
      const VideoCaptureControllerID& id,
      VideoCaptureControllerEventHandler* handler,
      const ControllerClients& clients);

  // Find a client of |session_id| in |clients|.
  ControllerClient* FindClient(
      int session_id,
      const ControllerClients& clients);

  // The pool of shared-memory buffers used for capturing.
  scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // All clients served by this controller.
  ControllerClients controller_clients_;

  // The parameter that currently used for the capturing.
  media::VideoCaptureParams current_params_;

  // Tracks the current frame format.
  media::VideoCaptureCapability frame_info_;

  // Takes on only the states 'STARTED' and 'ERROR'. 'ERROR' is an absorbing
  // state which stops the flow of data to clients.
  VideoCaptureState state_;

  base::WeakPtrFactory<VideoCaptureController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
