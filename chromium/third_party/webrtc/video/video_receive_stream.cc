/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_receive_stream.h"

#include <assert.h>
#include <stdlib.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {
namespace internal {

VideoReceiveStream::VideoReceiveStream(webrtc::VideoEngine* video_engine,
                                       const VideoReceiveStream::Config& config,
                                       newapi::Transport* transport,
                                       webrtc::VoiceEngine* voice_engine,
                                       int base_channel)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.pre_decode_callback),
      config_(config),
      channel_(-1) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateReceiveChannel(channel_, base_channel);
  assert(channel_ != -1);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  assert(rtp_rtcp_ != NULL);

  // TODO(pbos): This is not fine grained enough...
  rtp_rtcp_->SetNACKStatus(channel_, config_.rtp.nack.rtp_history_ms > 0);
  rtp_rtcp_->SetKeyFrameRequestMethod(channel_, kViEKeyFrameRequestPliRtcp);
  switch (config_.rtp.rtcp_mode) {
    case newapi::kRtcpCompound:
      rtp_rtcp_->SetRTCPStatus(channel_, kRtcpCompound_RFC4585);
      break;
    case newapi::kRtcpReducedSize:
      rtp_rtcp_->SetRTCPStatus(channel_, kRtcpNonCompound_RFC5506);
      break;
  }

  assert(config_.rtp.remote_ssrc != 0);
  assert(config_.rtp.local_ssrc != 0);
  assert(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);

  rtp_rtcp_->SetLocalSSRC(channel_, config_.rtp.local_ssrc);
  rtp_rtcp_->SetRembStatus(channel_, false, config_.rtp.remb);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    if (extension == RtpExtension::kTOffset) {
      if (rtp_rtcp_->SetReceiveTimestampOffsetStatus(channel_, true, id) != 0)
        abort();
    } else if (extension == RtpExtension::kAbsSendTime) {
      if (rtp_rtcp_->SetReceiveAbsoluteSendTimeStatus(channel_, true, id) != 0)
        abort();
    } else {
      abort();  // Unsupported extension.
    }
  }

  network_ = ViENetwork::GetInterface(video_engine);
  assert(network_ != NULL);

  network_->RegisterSendTransport(channel_, transport_adapter_);

  codec_ = ViECodec::GetInterface(video_engine);

  for (size_t i = 0; i < config_.codecs.size(); ++i) {
    if (codec_->SetReceiveCodec(channel_, config_.codecs[i]) != 0) {
      // TODO(pbos): Abort gracefully, this can be a runtime error.
      //             Factor out to an Init() method.
      abort();
    }
  }

  external_codec_ = ViEExternalCodec::GetInterface(video_engine);
  for (size_t i = 0; i < config_.external_decoders.size(); ++i) {
    ExternalVideoDecoder* decoder = &config_.external_decoders[i];
    if (external_codec_->RegisterExternalReceiveCodec(
            channel_,
            decoder->payload_type,
            decoder->decoder,
            decoder->renderer,
            decoder->expected_delay_ms) !=
        0) {
      // TODO(pbos): Abort gracefully? Can this be a runtime error?
      abort();
    }
  }

  render_ = ViERender::GetInterface(video_engine);
  assert(render_ != NULL);

  render_->AddRenderCallback(channel_, this);

  if (voice_engine) {
    video_engine_base_->SetVoiceEngine(voice_engine);
    video_engine_base_->ConnectAudioChannel(channel_, config_.audio_channel_id);
  }

  image_process_ = ViEImageProcess::GetInterface(video_engine);
  if (config.pre_decode_callback) {
    image_process_->RegisterPreDecodeImageCallback(channel_,
                                                   &encoded_frame_proxy_);
  }
  image_process_->RegisterPreRenderCallback(channel_,
                                            config_.pre_render_callback);

  clock_ = Clock::GetRealTimeClock();
}

VideoReceiveStream::~VideoReceiveStream() {
  image_process_->DeRegisterPreRenderCallback(channel_);
  image_process_->DeRegisterPreDecodeCallback(channel_);

  render_->RemoveRenderer(channel_);

  for (size_t i = 0; i < config_.external_decoders.size(); ++i) {
    external_codec_->DeRegisterExternalReceiveCodec(
        channel_, config_.external_decoders[i].payload_type);
  }

  network_->DeregisterSendTransport(channel_);

  video_engine_base_->SetVoiceEngine(NULL);
  image_process_->Release();
  video_engine_base_->Release();
  external_codec_->Release();
  codec_->Release();
  network_->Release();
  render_->Release();
  rtp_rtcp_->Release();
}

void VideoReceiveStream::StartReceiving() {
  if (render_->StartRender(channel_) != 0)
    abort();
  if (video_engine_base_->StartReceive(channel_) != 0)
    abort();
}

void VideoReceiveStream::StopReceiving() {
  if (render_->StopRender(channel_) != 0)
    abort();
  if (video_engine_base_->StopReceive(channel_) != 0)
    abort();
}

void VideoReceiveStream::GetCurrentReceiveCodec(VideoCodec* receive_codec) {
  // TODO(pbos): Implement
}

bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(
             channel_, packet, static_cast<int>(length)) == 0;
}

bool VideoReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTPPacket(
             channel_, packet, static_cast<int>(length),
             PacketTime()) == 0;
}

int32_t VideoReceiveStream::RenderFrame(const uint32_t stream_id,
                                        I420VideoFrame& video_frame) {
  if (config_.renderer == NULL)
    return 0;

  config_.renderer->RenderFrame(
      video_frame, video_frame.render_time_ms() - clock_->TimeInMilliseconds());
  return 0;
}
}  // namespace internal
}  // namespace webrtc
