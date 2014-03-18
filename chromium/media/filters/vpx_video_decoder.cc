// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/vpx_video_decoder.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

// Include libvpx header files.
// VPX_CODEC_DISABLE_COMPAT excludes parts of the libvpx API that provide
// backwards compatibility for legacy applications using the library.
#define VPX_CODEC_DISABLE_COMPAT 1
extern "C" {
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
}

namespace media {

// Always try to use three threads for video decoding.  There is little reason
// not to since current day CPUs tend to be multi-core and we measured
// performance benefits on older machines such as P4s with hyperthreading.
static const int kDecodeThreads = 2;
static const int kMaxDecodeThreads = 16;

// Returns the number of threads.
static int GetThreadCount(const VideoDecoderConfig& config) {
  // Refer to http://crbug.com/93932 for tsan suppressions on decoding.
  int decode_threads = kDecodeThreads;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if (threads.empty() || !base::StringToInt(threads, &decode_threads)) {
    if (config.codec() == kCodecVP9) {
      // For VP9 decode when using the default thread count, increase the number
      // of decode threads to equal the maximum number of tiles possible for
      // higher resolution streams.
      if (config.coded_size().width() >= 2048)
        decode_threads = 8;
      else if (config.coded_size().width() >= 1024)
        decode_threads = 4;
    }

    return decode_threads;
  }

  decode_threads = std::max(decode_threads, 0);
  decode_threads = std::min(decode_threads, kMaxDecodeThreads);
  return decode_threads;
}

VpxVideoDecoder::VpxVideoDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop),
      weak_factory_(this),
      state_(kUninitialized),
      vpx_codec_(NULL),
      vpx_codec_alpha_(NULL) {
}

VpxVideoDecoder::~VpxVideoDecoder() {
  DCHECK_EQ(kUninitialized, state_);
  CloseDecoder();
}

void VpxVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 const PipelineStatusCB& status_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());
  DCHECK(!config.is_encrypted());
  DCHECK(decode_cb_.is_null());
  DCHECK(reset_cb_.is_null());

  weak_this_ = weak_factory_.GetWeakPtr();

  if (!ConfigureDecoder(config)) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Success!
  config_ = config;
  state_ = kNormal;
  status_cb.Run(PIPELINE_OK);
}

static vpx_codec_ctx* InitializeVpxContext(vpx_codec_ctx* context,
                                           const VideoDecoderConfig& config) {
  context = new vpx_codec_ctx();
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = config.coded_size().width();
  vpx_config.h = config.coded_size().height();
  vpx_config.threads = GetThreadCount(config);

  vpx_codec_err_t status = vpx_codec_dec_init(context,
                                              config.codec() == kCodecVP9 ?
                                                  vpx_codec_vp9_dx() :
                                                  vpx_codec_vp8_dx(),
                                              &vpx_config,
                                              0);
  if (status != VPX_CODEC_OK) {
    LOG(ERROR) << "vpx_codec_dec_init failed, status=" << status;
    delete context;
    return NULL;
  }
  return context;
}

bool VpxVideoDecoder::ConfigureDecoder(const VideoDecoderConfig& config) {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  bool can_handle = false;
  if (config.codec() == kCodecVP9)
    can_handle = true;
  if (!cmd_line->HasSwitch(switches::kDisableVp8AlphaPlayback) &&
      config.codec() == kCodecVP8 && config.format() == VideoFrame::YV12A) {
    can_handle = true;
  }
  if (!can_handle)
    return false;

  CloseDecoder();

  vpx_codec_ = InitializeVpxContext(vpx_codec_, config);
  if (!vpx_codec_)
    return false;

  if (config.format() == VideoFrame::YV12A) {
    vpx_codec_alpha_ = InitializeVpxContext(vpx_codec_alpha_, config);
    if (!vpx_codec_alpha_)
      return false;
  }

  return true;
}

void VpxVideoDecoder::CloseDecoder() {
  if (vpx_codec_) {
    vpx_codec_destroy(vpx_codec_);
    delete vpx_codec_;
    vpx_codec_ = NULL;
  }
  if (vpx_codec_alpha_) {
    vpx_codec_destroy(vpx_codec_alpha_);
    delete vpx_codec_alpha_;
    vpx_codec_alpha_ = NULL;
  }
}

void VpxVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                             const DecodeCB& decode_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!decode_cb.is_null());
  CHECK_NE(state_, kUninitialized);
  CHECK(decode_cb_.is_null()) << "Overlapping decodes are not supported.";

  decode_cb_ = BindToCurrentLoop(decode_cb);

  if (state_ == kError) {
    base::ResetAndReturn(&decode_cb_).Run(kDecodeError, NULL);
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    base::ResetAndReturn(&decode_cb_).Run(kOk, VideoFrame::CreateEOSFrame());
    return;
  }

  DecodeBuffer(buffer);
}

void VpxVideoDecoder::Reset(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(reset_cb_.is_null());
  reset_cb_ = BindToCurrentLoop(closure);

  // Defer the reset if a decode is pending.
  if (!decode_cb_.is_null())
    return;

  DoReset();
}

void VpxVideoDecoder::Stop(const base::Closure& closure) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  base::ScopedClosureRunner runner(BindToCurrentLoop(closure));

  if (state_ == kUninitialized)
    return;

  if (!decode_cb_.is_null()) {
    base::ResetAndReturn(&decode_cb_).Run(kOk, NULL);
    // Reset is pending only when decode is pending.
    if (!reset_cb_.is_null())
      base::ResetAndReturn(&reset_cb_).Run();
  }

  state_ = kUninitialized;
}

bool VpxVideoDecoder::HasAlpha() const {
  return vpx_codec_alpha_ != NULL;
}

void VpxVideoDecoder::DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK_NE(state_, kError);
  DCHECK(reset_cb_.is_null());
  DCHECK(!decode_cb_.is_null());
  DCHECK(buffer);

  // Transition to kDecodeFinished on the first end of stream buffer.
  if (state_ == kNormal && buffer->end_of_stream()) {
    state_ = kDecodeFinished;
    base::ResetAndReturn(&decode_cb_).Run(kOk, VideoFrame::CreateEOSFrame());
    return;
  }

  scoped_refptr<VideoFrame> video_frame;
  if (!VpxDecode(buffer, &video_frame)) {
    state_ = kError;
    base::ResetAndReturn(&decode_cb_).Run(kDecodeError, NULL);
    return;
  }

  // If we didn't get a frame we need more data.
  if (!video_frame.get()) {
    base::ResetAndReturn(&decode_cb_).Run(kNotEnoughData, NULL);
    return;
  }

  base::ResetAndReturn(&decode_cb_).Run(kOk, video_frame);
}

bool VpxVideoDecoder::VpxDecode(const scoped_refptr<DecoderBuffer>& buffer,
                                scoped_refptr<VideoFrame>* video_frame) {
  DCHECK(video_frame);
  DCHECK(!buffer->end_of_stream());

  // Pass |buffer| to libvpx.
  int64 timestamp = buffer->timestamp().InMicroseconds();
  void* user_priv = reinterpret_cast<void*>(&timestamp);
  vpx_codec_err_t status = vpx_codec_decode(vpx_codec_,
                                            buffer->data(),
                                            buffer->data_size(),
                                            user_priv,
                                            0);
  if (status != VPX_CODEC_OK) {
    LOG(ERROR) << "vpx_codec_decode() failed, status=" << status;
    return false;
  }

  // Gets pointer to decoded data.
  vpx_codec_iter_t iter = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(vpx_codec_, &iter);
  if (!vpx_image) {
    *video_frame = NULL;
    return true;
  }

  if (vpx_image->user_priv != reinterpret_cast<void*>(&timestamp)) {
    LOG(ERROR) << "Invalid output timestamp.";
    return false;
  }

  const vpx_image_t* vpx_image_alpha = NULL;
  if (vpx_codec_alpha_ && buffer->side_data_size() >= 8) {
    // Pass alpha data to libvpx.
    int64 timestamp_alpha = buffer->timestamp().InMicroseconds();
    void* user_priv_alpha = reinterpret_cast<void*>(&timestamp_alpha);

    // First 8 bytes of side data is side_data_id in big endian.
    const uint64 side_data_id = base::NetToHost64(
        *(reinterpret_cast<const uint64*>(buffer->side_data())));
    if (side_data_id == 1) {
      status = vpx_codec_decode(vpx_codec_alpha_,
                                buffer->side_data() + 8,
                                buffer->side_data_size() - 8,
                                user_priv_alpha,
                                0);

      if (status != VPX_CODEC_OK) {
        LOG(ERROR) << "vpx_codec_decode() failed on alpha, status=" << status;
        return false;
      }

      // Gets pointer to decoded data.
      vpx_codec_iter_t iter_alpha = NULL;
      vpx_image_alpha = vpx_codec_get_frame(vpx_codec_alpha_, &iter_alpha);
      if (!vpx_image_alpha) {
        *video_frame = NULL;
        return true;
      }

      if (vpx_image_alpha->user_priv !=
          reinterpret_cast<void*>(&timestamp_alpha)) {
        LOG(ERROR) << "Invalid output timestamp on alpha.";
        return false;
      }
    }
  }

  CopyVpxImageTo(vpx_image, vpx_image_alpha, video_frame);
  (*video_frame)->SetTimestamp(base::TimeDelta::FromMicroseconds(timestamp));
  return true;
}

void VpxVideoDecoder::DoReset() {
  DCHECK(decode_cb_.is_null());

  state_ = kNormal;
  reset_cb_.Run();
  reset_cb_.Reset();
}

void VpxVideoDecoder::CopyVpxImageTo(const vpx_image* vpx_image,
                                     const struct vpx_image* vpx_image_alpha,
                                     scoped_refptr<VideoFrame>* video_frame) {
  CHECK(vpx_image);
  CHECK(vpx_image->fmt == VPX_IMG_FMT_I420 ||
        vpx_image->fmt == VPX_IMG_FMT_YV12);

  gfx::Size size(vpx_image->d_w, vpx_image->d_h);

  *video_frame = frame_pool_.CreateFrame(
      vpx_codec_alpha_ ? VideoFrame::YV12A : VideoFrame::YV12,
      size,
      gfx::Rect(size),
      config_.natural_size(),
      kNoTimestamp());

  CopyYPlane(vpx_image->planes[VPX_PLANE_Y],
             vpx_image->stride[VPX_PLANE_Y],
             vpx_image->d_h,
             video_frame->get());
  CopyUPlane(vpx_image->planes[VPX_PLANE_U],
             vpx_image->stride[VPX_PLANE_U],
             (vpx_image->d_h + 1) / 2,
             video_frame->get());
  CopyVPlane(vpx_image->planes[VPX_PLANE_V],
             vpx_image->stride[VPX_PLANE_V],
             (vpx_image->d_h + 1) / 2,
             video_frame->get());
  if (!vpx_codec_alpha_)
    return;
  if (!vpx_image_alpha) {
    MakeOpaqueAPlane(
        vpx_image->stride[VPX_PLANE_Y], vpx_image->d_h, video_frame->get());
    return;
  }
  CopyAPlane(vpx_image_alpha->planes[VPX_PLANE_Y],
             vpx_image->stride[VPX_PLANE_Y],
             vpx_image->d_h,
             video_frame->get());
}

}  // namespace media
