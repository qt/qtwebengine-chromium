// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_encoder.h"

#include "base/bind.h"
#include "base/logging.h"

namespace media {
namespace cast {

VideoEncoder::VideoEncoder(scoped_refptr<CastThread> cast_thread,
                           const VideoSenderConfig& video_config,
                           uint8 max_unacked_frames)
    : video_config_(video_config),
      cast_thread_(cast_thread),
      skip_next_frame_(false),
      skip_count_(0) {
  if (video_config.codec == kVp8) {
    vp8_encoder_.reset(new Vp8Encoder(video_config, max_unacked_frames));
  } else {
    DCHECK(false) << "Invalid config";  // Codec not supported.
  }

  dynamic_config_.key_frame_requested = false;
  dynamic_config_.latest_frame_id_to_reference = kStartFrameId;
  dynamic_config_.bit_rate = video_config.start_bitrate;
}

VideoEncoder::~VideoEncoder() {}

bool VideoEncoder::EncodeVideoFrame(
    const I420VideoFrame* video_frame,
    const base::TimeTicks& capture_time,
    const FrameEncodedCallback& frame_encoded_callback,
    const base::Closure frame_release_callback) {
  if (video_config_.codec != kVp8) return false;

  if (skip_next_frame_) {
    ++skip_count_;
    VLOG(1) << "Skip encoding frame";
    return false;
  }

  cast_thread_->PostTask(CastThread::VIDEO_ENCODER, FROM_HERE,
      base::Bind(&VideoEncoder::EncodeVideoFrameEncoderThread, this,
          video_frame, capture_time, dynamic_config_, frame_encoded_callback,
          frame_release_callback));

  dynamic_config_.key_frame_requested = false;
  return true;
}

void VideoEncoder::EncodeVideoFrameEncoderThread(
    const I420VideoFrame* video_frame,
    const base::TimeTicks& capture_time,
    const CodecDynamicConfig& dynamic_config,
    const FrameEncodedCallback& frame_encoded_callback,
    const base::Closure frame_release_callback) {
  if (dynamic_config.key_frame_requested) {
    vp8_encoder_->GenerateKeyFrame();
  }
  vp8_encoder_->LatestFrameIdToReference(
      dynamic_config.latest_frame_id_to_reference);
  vp8_encoder_->UpdateRates(dynamic_config.bit_rate);

  scoped_ptr<EncodedVideoFrame> encoded_frame(new EncodedVideoFrame());
  bool retval = vp8_encoder_->Encode(*video_frame, encoded_frame.get());

  // We are done with the video frame release it.
  cast_thread_->PostTask(CastThread::MAIN, FROM_HERE, frame_release_callback);

  if (!retval) {
    VLOG(1) << "Encoding failed";
    return;
  }
  if (encoded_frame->data.size() <= 0) {
    VLOG(1) << "Encoding resulted in an empty frame";
    return;
  }
  cast_thread_->PostTask(CastThread::MAIN, FROM_HERE,
      base::Bind(frame_encoded_callback,
          base::Passed(&encoded_frame), capture_time));
}

// Inform the encoder about the new target bit rate.
void VideoEncoder::SetBitRate(int new_bit_rate) OVERRIDE {
  dynamic_config_.bit_rate = new_bit_rate;
}

// Inform the encoder to not encode the next frame.
void VideoEncoder::SkipNextFrame(bool skip_next_frame) OVERRIDE {
  skip_next_frame_ = skip_next_frame;
}

// Inform the encoder to encode the next frame as a key frame.
void VideoEncoder::GenerateKeyFrame() OVERRIDE {
  dynamic_config_.key_frame_requested = true;
}

// Inform the encoder to only reference frames older or equal to frame_id;
void VideoEncoder::LatestFrameIdToReference(uint8 frame_id) OVERRIDE {
  dynamic_config_.latest_frame_id_to_reference = frame_id;
}

int VideoEncoder::NumberOfSkippedFrames() const OVERRIDE {
  return skip_count_;
}

}  //  namespace cast
}  //  namespace media
