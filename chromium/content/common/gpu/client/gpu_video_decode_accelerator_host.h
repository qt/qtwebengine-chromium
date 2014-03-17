// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/size.h"

namespace content {
class GpuChannelHost;

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public IPC::Listener,
      public media::VideoDecodeAccelerator,
      public CommandBufferProxyImpl::DeletionObserver,
      public base::NonThreadSafe {
 public:
  // |channel| is used to send IPC messages to GPU process.
  GpuVideoDecodeAcceleratorHost(GpuChannelHost* channel,
                                int32 decoder_route_id,
                                media::VideoDecodeAccelerator::Client* client,
                                CommandBufferProxyImpl* impl);

  // IPC::Listener implementation.
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(media::VideoCodecProfile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

  // CommandBufferProxyImpl::DeletionObserver implemetnation.
  virtual void OnWillDeleteImpl() OVERRIDE;

 private:
  // Only Destroy() should be deleting |this|.
  virtual ~GpuVideoDecodeAcceleratorHost();

  void Send(IPC::Message* message);

  void OnBitstreamBufferProcessed(int32 bitstream_buffer_id);
  void OnProvidePictureBuffer(uint32 num_requested_buffers,
                              const gfx::Size& dimensions,
                              uint32 texture_target);
  void OnDismissPictureBuffer(int32 picture_buffer_id);
  void OnPictureReady(int32 picture_buffer_id, int32 bitstream_buffer_id);
  void OnFlushDone();
  void OnResetDone();
  void OnErrorNotification(uint32 error);

  // Sends IPC messages to the Gpu process.
  GpuChannelHost* channel_;

  // Route ID for the associated decoder in the GPU process.
  int32 decoder_route_id_;

  // Reference to the client that will receive callbacks from the decoder.
  media::VideoDecodeAccelerator::Client* client_;

  // Unowned reference to the CommandBufferProxyImpl that created us.
  CommandBufferProxyImpl* impl_;

  // Requested dimensions of the buffer, from ProvidePictureBuffers().
  gfx::Size picture_buffer_dimensions_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
