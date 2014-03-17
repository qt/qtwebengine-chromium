// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_VPX_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"

struct vpx_codec_ctx;
struct vpx_image;

namespace base {
class MessageLoopProxy;
}

namespace media {

// Libvpx video decoder wrapper.
// Note: VpxVideoDecoder accepts only YV12A VP8 content or VP9 content. This is
// done to avoid usurping FFmpeg for all vp8 decoding, because the FFmpeg VP8
// decoder is faster than the libvpx VP8 decoder.
class MEDIA_EXPORT VpxVideoDecoder : public VideoDecoder {
 public:
  explicit VpxVideoDecoder(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);
  virtual ~VpxVideoDecoder();

  // VideoDecoder implementation.
  virtual void Initialize(const VideoDecoderConfig& config,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;
  virtual bool HasAlpha() const OVERRIDE;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true when initialization was successful.
  bool ConfigureDecoder(const VideoDecoderConfig& config);

  void CloseDecoder();

  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer);
  bool VpxDecode(const scoped_refptr<DecoderBuffer>& buffer,
                 scoped_refptr<VideoFrame>* video_frame);

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  void CopyVpxImageTo(const vpx_image* vpx_image,
                      const struct vpx_image* vpx_image_alpha,
                      scoped_refptr<VideoFrame>* video_frame);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<VpxVideoDecoder> weak_factory_;
  base::WeakPtr<VpxVideoDecoder> weak_this_;

  DecoderState state_;

  DecodeCB decode_cb_;
  base::Closure reset_cb_;

  VideoDecoderConfig config_;

  vpx_codec_ctx* vpx_codec_;
  vpx_codec_ctx* vpx_codec_alpha_;

  VideoFramePool frame_pool_;

  DISALLOW_COPY_AND_ASSIGN(VpxVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
