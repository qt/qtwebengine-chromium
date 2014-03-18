/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_

#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video/encoded_frame_callback_adapter.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class VideoEngine;
class ViEBase;
class ViECapture;
class ViECodec;
class ViEExternalCapture;
class ViEExternalCodec;
class ViEImageProcess;
class ViENetwork;
class ViERTP_RTCP;

namespace internal {

class ResolutionAdaptor;

class VideoSendStream : public webrtc::VideoSendStream,
                        public VideoSendStreamInput {
 public:
  VideoSendStream(newapi::Transport* transport,
                  bool overuse_detection,
                  webrtc::VideoEngine* video_engine,
                  const VideoSendStream::Config& config,
                  int base_channel);

  virtual ~VideoSendStream();

  virtual void PutFrame(const I420VideoFrame& frame) OVERRIDE;

  virtual void SwapFrame(I420VideoFrame* frame) OVERRIDE;

  virtual VideoSendStreamInput* Input() OVERRIDE;

  virtual void StartSending() OVERRIDE;

  virtual void StopSending() OVERRIDE;

  virtual bool SetCodec(const VideoCodec& codec) OVERRIDE;
  virtual VideoCodec GetCodec() OVERRIDE;

 public:
  bool DeliverRtcp(const uint8_t* packet, size_t length);

 private:
  I420VideoFrame input_frame_;
  TransportAdapter transport_adapter_;
  EncodedFrameCallbackAdapter encoded_frame_proxy_;
  scoped_ptr<CriticalSectionWrapper> codec_lock_;
  VideoSendStream::Config config_;

  ViEBase* video_engine_base_;
  ViECapture* capture_;
  ViECodec* codec_;
  ViEExternalCapture* external_capture_;
  ViEExternalCodec* external_codec_;
  ViENetwork* network_;
  ViERTP_RTCP* rtp_rtcp_;
  ViEImageProcess* image_process_;

  int channel_;
  int capture_id_;
  scoped_ptr<ResolutionAdaptor> overuse_observer_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
