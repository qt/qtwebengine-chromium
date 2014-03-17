// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
#define MEDIA_FILTERS_GPU_VIDEO_DECODER_H_

#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/video/video_decode_accelerator.h"

template <class T> class scoped_refptr;

namespace base {
class MessageLoopProxy;
class SharedMemory;
}

namespace media {

class DecoderBuffer;
class GpuVideoAcceleratorFactories;
class MediaLog;

// GPU-accelerated video decoder implementation.  Relies on
// AcceleratedVideoDecoderMsg_Decode and friends.
class MEDIA_EXPORT GpuVideoDecoder
    : public VideoDecoder,
      public VideoDecodeAccelerator::Client {
 public:
  // The message loop of |factories| will be saved to |gvd_loop_proxy_|.
  explicit GpuVideoDecoder(
      const scoped_refptr<GpuVideoAcceleratorFactories>& factories,
      const scoped_refptr<MediaLog>& media_log);

  // VideoDecoder implementation.
  virtual void Initialize(const VideoDecoderConfig& config,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;
  virtual bool HasAlpha() const OVERRIDE;
  virtual bool NeedsBitstreamConversion() const OVERRIDE;
  virtual bool CanReadWithoutStalling() const OVERRIDE;

  // VideoDecodeAccelerator::Client implementation.
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void ProvidePictureBuffers(uint32 count,
                                     const gfx::Size& size,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;

 protected:
  virtual ~GpuVideoDecoder();

 private:
  enum State {
    kNormal,
    kDrainingDecoder,
    kDecoderDrained,
    kError
  };

  // A shared memory segment and its allocated size.
  struct SHMBuffer {
    SHMBuffer(base::SharedMemory* m, size_t s);
    ~SHMBuffer();
    base::SharedMemory* shm;
    size_t size;
  };

  // A SHMBuffer and the DecoderBuffer its data came from.
  struct BufferPair {
    BufferPair(SHMBuffer* s, const scoped_refptr<DecoderBuffer>& b);
    ~BufferPair();
    SHMBuffer* shm_buffer;
    scoped_refptr<DecoderBuffer> buffer;
  };

  typedef std::map<int32, PictureBuffer> PictureBufferMap;

  // Return true if more decode work can be piled on to the VDA.
  bool CanMoreDecodeWorkBeDone();

  // Enqueue a frame for later delivery (or drop it on the floor if a
  // vda->Reset() is in progress) and trigger out-of-line delivery of the oldest
  // ready frame to the client if there is a pending read.  A NULL |frame|
  // merely triggers delivery, and requires the ready_video_frames_ queue not be
  // empty.
  void EnqueueFrameAndTriggerFrameDelivery(
      const scoped_refptr<VideoFrame>& frame);

  // Indicate the picture buffer can be reused by the decoder.
  void ReusePictureBuffer(int64 picture_buffer_id, uint32 sync_point);

  void RecordBufferData(
      const BitstreamBuffer& bitstream_buffer, const DecoderBuffer& buffer);
  void GetBufferData(int32 id, base::TimeDelta* timetamp,
                     gfx::Rect* visible_rect, gfx::Size* natural_size);

  void DestroyVDA();

  // Request a shared-memory segment of at least |min_size| bytes.  Will
  // allocate as necessary.  Caller does not own returned pointer.
  SHMBuffer* GetSHM(size_t min_size);

  // Return a shared-memory segment to the available pool.
  void PutSHM(SHMBuffer* shm_buffer);

  // Destroy all PictureBuffers in |buffers|, and delete their textures.
  void DestroyPictureBuffers(PictureBufferMap* buffers);

  bool needs_bitstream_conversion_;

  // Message loop which this class and |factories_| run on.
  scoped_refptr<base::MessageLoopProxy> gvd_loop_proxy_;
  base::WeakPtrFactory<GpuVideoDecoder> weak_factory_;
  base::WeakPtr<GpuVideoDecoder> weak_this_;

  scoped_refptr<GpuVideoAcceleratorFactories> factories_;

  // Populated during Initialize() (on success) and unchanged until an error
  // occurs.
  scoped_ptr<VideoDecodeAccelerator> vda_;

  // Callbacks that are !is_null() only during their respective operation being
  // asynchronously executed.
  DecodeCB pending_decode_cb_;
  base::Closure pending_reset_cb_;

  State state_;

  VideoDecoderConfig config_;

  // Shared-memory buffer pool.  Since allocating SHM segments requires a
  // round-trip to the browser process, we keep allocation out of the
  // steady-state of the decoder.
  std::vector<SHMBuffer*> available_shm_segments_;

  scoped_refptr<MediaLog> media_log_;

  std::map<int32, BufferPair> bitstream_buffers_in_decoder_;
  PictureBufferMap assigned_picture_buffers_;
  PictureBufferMap dismissed_picture_buffers_;
  // PictureBuffers given to us by VDA via PictureReady, which we sent forward
  // as VideoFrames to be rendered via decode_cb_, and which will be returned
  // to us via ReusePictureBuffer.
  std::set<int32> picture_buffers_at_display_;

  // The texture target used for decoded pictures.
  uint32 decoder_texture_target_;

  struct BufferData {
    BufferData(int32 bbid, base::TimeDelta ts, const gfx::Rect& visible_rect,
               const gfx::Size& natural_size);
    ~BufferData();
    int32 bitstream_buffer_id;
    base::TimeDelta timestamp;
    gfx::Rect visible_rect;
    gfx::Size natural_size;
  };
  std::list<BufferData> input_buffer_data_;

  // picture_buffer_id and the frame wrapping the corresponding Picture, for
  // frames that have been decoded but haven't been requested by a Decode() yet.
  std::list<scoped_refptr<VideoFrame> > ready_video_frames_;
  int32 next_picture_buffer_id_;
  int32 next_bitstream_buffer_id_;

  // Set during ProvidePictureBuffers(), used for checking and implementing
  // HasAvailableOutputFrames().
  int available_pictures_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_GPU_VIDEO_DECODER_H_
