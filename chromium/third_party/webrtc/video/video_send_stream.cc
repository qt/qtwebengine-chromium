/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_send_stream.h"

#include <string>
#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace internal {

// Super simple and temporary overuse logic. This will move to the application
// as soon as the new API allows changing send codec on the fly.
class ResolutionAdaptor : public webrtc::CpuOveruseObserver {
 public:
  ResolutionAdaptor(ViECodec* codec, int channel, size_t width, size_t height)
      : codec_(codec),
        channel_(channel),
        max_width_(width),
        max_height_(height) {}

  virtual ~ResolutionAdaptor() {}

  virtual void OveruseDetected() OVERRIDE {
    VideoCodec codec;
    if (codec_->GetSendCodec(channel_, codec) != 0)
      return;

    if (codec.width / 2 < min_width || codec.height / 2 < min_height)
      return;

    codec.width /= 2;
    codec.height /= 2;
    codec_->SetSendCodec(channel_, codec);
  }

  virtual void NormalUsage() OVERRIDE {
    VideoCodec codec;
    if (codec_->GetSendCodec(channel_, codec) != 0)
      return;

    if (codec.width * 2u > max_width_ || codec.height * 2u > max_height_)
      return;

    codec.width *= 2;
    codec.height *= 2;
    codec_->SetSendCodec(channel_, codec);
  }

 private:
  // Temporary and arbitrary chosen minimum resolution.
  static const size_t min_width = 160;
  static const size_t min_height = 120;

  ViECodec* codec_;
  const int channel_;

  const size_t max_width_;
  const size_t max_height_;
};

VideoSendStream::VideoSendStream(newapi::Transport* transport,
                                 bool overuse_detection,
                                 webrtc::VideoEngine* video_engine,
                                 const VideoSendStream::Config& config,
                                 int base_channel)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.post_encode_callback),
      codec_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      config_(config),
      external_codec_(NULL),
      channel_(-1) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateChannel(channel_, base_channel);
  assert(channel_ != -1);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  assert(rtp_rtcp_ != NULL);

  assert(config_.rtp.ssrcs.size() > 0);
  if (config_.suspend_below_min_bitrate)
    config_.pacing = true;
  rtp_rtcp_->SetTransmissionSmoothingStatus(channel_, config_.pacing);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    if (extension == RtpExtension::kTOffset) {
      if (rtp_rtcp_->SetSendTimestampOffsetStatus(channel_, true, id) != 0)
        abort();
    } else if (extension == RtpExtension::kAbsSendTime) {
      if (rtp_rtcp_->SetSendAbsoluteSendTimeStatus(channel_, true, id) != 0)
        abort();
    } else {
      abort();  // Unsupported extension.
    }
  }

  rtp_rtcp_->SetRembStatus(channel_, true, false);

  // Enable NACK, FEC or both.
  if (config_.rtp.fec.red_payload_type != -1) {
    assert(config_.rtp.fec.ulpfec_payload_type != -1);
    if (config_.rtp.nack.rtp_history_ms > 0) {
      rtp_rtcp_->SetHybridNACKFECStatus(
          channel_,
          true,
          static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    } else {
      rtp_rtcp_->SetFECStatus(
          channel_,
          true,
          static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    }
  } else {
    rtp_rtcp_->SetNACKStatus(channel_, config_.rtp.nack.rtp_history_ms > 0);
  }

  char rtcp_cname[ViERTP_RTCP::KMaxRTCPCNameLength];
  assert(config_.rtp.c_name.length() < ViERTP_RTCP::KMaxRTCPCNameLength);
  strncpy(rtcp_cname, config_.rtp.c_name.c_str(), sizeof(rtcp_cname) - 1);
  rtcp_cname[sizeof(rtcp_cname) - 1] = '\0';

  rtp_rtcp_->SetRTCPCName(channel_, rtcp_cname);

  capture_ = ViECapture::GetInterface(video_engine);
  capture_->AllocateExternalCaptureDevice(capture_id_, external_capture_);
  capture_->ConnectCaptureDevice(capture_id_, channel_);

  network_ = ViENetwork::GetInterface(video_engine);
  assert(network_ != NULL);

  network_->RegisterSendTransport(channel_, transport_adapter_);
  // 28 to match packet overhead in ModuleRtpRtcpImpl.
  network_->SetMTU(channel_,
                   static_cast<unsigned int>(config_.rtp.max_packet_size + 28));

  if (config.encoder) {
    external_codec_ = ViEExternalCodec::GetInterface(video_engine);
    if (external_codec_->RegisterExternalSendCodec(
        channel_, config.codec.plType, config.encoder,
        config.internal_source) != 0) {
      abort();
    }
  }

  codec_ = ViECodec::GetInterface(video_engine);
  if (!SetCodec(config_.codec))
    abort();

  if (overuse_detection) {
    overuse_observer_.reset(
        new ResolutionAdaptor(codec_, channel_, config_.codec.width,
                              config_.codec.height));
    video_engine_base_->RegisterCpuOveruseObserver(channel_,
                                                   overuse_observer_.get());
  }

  image_process_ = ViEImageProcess::GetInterface(video_engine);
  image_process_->RegisterPreEncodeCallback(channel_,
                                            config_.pre_encode_callback);
  if (config_.post_encode_callback) {
    image_process_->RegisterPostEncodeImageCallback(channel_,
                                                    &encoded_frame_proxy_);
  }

  if (config.suspend_below_min_bitrate) {
    codec_->SuspendBelowMinBitrate(channel_);
  }
}

VideoSendStream::~VideoSendStream() {
  image_process_->DeRegisterPreEncodeCallback(channel_);

  network_->DeregisterSendTransport(channel_);

  capture_->DisconnectCaptureDevice(channel_);
  capture_->ReleaseCaptureDevice(capture_id_);

  if (external_codec_) {
    external_codec_->DeRegisterExternalSendCodec(channel_,
                                                 config_.codec.plType);
  }

  video_engine_base_->DeleteChannel(channel_);

  image_process_->Release();
  video_engine_base_->Release();
  capture_->Release();
  codec_->Release();
  if (external_codec_)
    external_codec_->Release();
  network_->Release();
  rtp_rtcp_->Release();
}

void VideoSendStream::PutFrame(const I420VideoFrame& frame) {
  input_frame_.CopyFrame(frame);
  SwapFrame(&input_frame_);
}

void VideoSendStream::SwapFrame(I420VideoFrame* frame) {
  // TODO(pbos): Warn if frame is "too far" into the future, or too old. This
  //             would help detect if frame's being used without NTP.
  //             TO REVIEWER: Is there any good check for this? Should it be
  //             skipped?
  if (frame != &input_frame_)
    input_frame_.SwapFrame(frame);

  // TODO(pbos): Local rendering should not be done on the capture thread.
  if (config_.local_renderer != NULL)
    config_.local_renderer->RenderFrame(input_frame_, 0);

  external_capture_->SwapFrame(&input_frame_);
}

VideoSendStreamInput* VideoSendStream::Input() { return this; }

void VideoSendStream::StartSending() {
  if (video_engine_base_->StartSend(channel_) != 0)
    abort();
  if (video_engine_base_->StartReceive(channel_) != 0)
    abort();
}

void VideoSendStream::StopSending() {
  if (video_engine_base_->StopSend(channel_) != 0)
    abort();
  if (video_engine_base_->StopReceive(channel_) != 0)
    abort();
}

bool VideoSendStream::SetCodec(const VideoCodec& codec) {
  assert(config_.rtp.ssrcs.size() >= codec.numberOfSimulcastStreams);

  CriticalSectionScoped crit(codec_lock_.get());
  if (codec_->SetSendCodec(channel_, codec) != 0)
    return false;

  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    rtp_rtcp_->SetLocalSSRC(channel_,
                            config_.rtp.ssrcs[i],
                            kViEStreamTypeNormal,
                            static_cast<unsigned char>(i));
  }

  config_.codec = codec;
  if (config_.rtp.rtx.ssrcs.empty())
    return true;

  // Set up RTX.
  assert(config_.rtp.rtx.ssrcs.size() == config_.rtp.ssrcs.size());
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    rtp_rtcp_->SetLocalSSRC(channel_,
                            config_.rtp.rtx.ssrcs[i],
                            kViEStreamTypeRtx,
                            static_cast<unsigned char>(i));
  }

  if (config_.rtp.rtx.rtx_payload_type != 0) {
    rtp_rtcp_->SetRtxSendPayloadType(channel_,
                                     config_.rtp.rtx.rtx_payload_type);
  }

  return true;
}

VideoCodec VideoSendStream::GetCodec() {
  CriticalSectionScoped crit(codec_lock_.get());
  return config_.codec;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(
             channel_, packet, static_cast<int>(length)) == 0;
}
}  // namespace internal
}  // namespace webrtc
