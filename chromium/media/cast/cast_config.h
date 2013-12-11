// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_CONFIG_H_
#define MEDIA_CAST_CAST_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

enum RtcpMode {
  kRtcpCompound,  // Compound RTCP mode is described by RFC 4585.
  kRtcpReducedSize,  // Reduced-size RTCP mode is described by RFC 5506.
};

enum VideoCodec {
  kVp8,
  kH264,
  kExternalVideo,
};

enum AudioCodec {
  kOpus,
  kPcm16,
  kExternalAudio,
};

struct AudioSenderConfig {
  AudioSenderConfig();

  uint32 sender_ssrc;
  uint32 incoming_feedback_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  int rtp_history_ms;  // The time RTP packets are stored for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_encoder;
  int frequency;
  int channels;
  int bitrate;
  AudioCodec codec;
};

struct VideoSenderConfig {
  VideoSenderConfig();

  uint32 sender_ssrc;
  uint32 incoming_feedback_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  int rtp_history_ms;  // The time RTP packets are stored for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_encoder;
  int width;  // Incoming frames will be scaled to this size.
  int height;

  float congestion_control_back_off;
  int max_bitrate;
  int min_bitrate;
  int start_bitrate;
  int max_qp;
  int min_qp;
  int max_frame_rate;
  int max_number_of_video_buffers_used;  // Max value depend on codec.
  VideoCodec codec;
  int number_of_cores;
};

struct AudioReceiverConfig {
  AudioReceiverConfig();

  uint32 feedback_ssrc;
  uint32 incoming_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  // The time the receiver is prepared to wait for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_decoder;
  int frequency;
  int channels;
  AudioCodec codec;
};

struct VideoReceiverConfig {
  VideoReceiverConfig();

  uint32 feedback_ssrc;
  uint32 incoming_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  // The time the receiver is prepared to wait for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_decoder;
  int max_frame_rate;

  // Some HW decoders can not run faster than the frame rate, preventing it
  // from catching up after a glitch.
  bool decoder_faster_than_max_frame_rate;
  VideoCodec codec;
};

struct I420VideoPlane {
  int stride;
  int length;
  uint8* data;
};

struct I420VideoFrame {
  int width;
  int height;
  I420VideoPlane y_plane;
  I420VideoPlane u_plane;
  I420VideoPlane v_plane;
};

struct EncodedVideoFrame {
  EncodedVideoFrame();
  ~EncodedVideoFrame();

  VideoCodec codec;
  bool key_frame;
  uint8 frame_id;
  uint8 last_referenced_frame_id;
  std::vector<uint8> data;
};

struct PcmAudioFrame {
  PcmAudioFrame();
  ~PcmAudioFrame();

  int channels;  // Samples in interleaved stereo format. L0, R0, L1 ,R1 ,...
  int frequency;
  std::vector<int16> samples;
};

struct EncodedAudioFrame {
  EncodedAudioFrame();
  ~EncodedAudioFrame();

  AudioCodec codec;
  uint8 frame_id;  // Needed to release the frame. Not used send side.
  int samples;  // Needed send side to advance the RTP timestamp.
                // Not used receive side.
  std::vector<uint8> data;
};

class PacketSender {
 public:
  // All packets to be sent to the network will be delivered via this function.
  virtual bool SendPacket(const uint8* packet, int length) = 0;

  virtual ~PacketSender() {}
};

class PacketReceiver : public base::RefCountedThreadSafe<PacketReceiver> {
 public:
  // All packets received from the network should be delivered via this
  // function.
  virtual void ReceivedPacket(const uint8* packet, int length,
                              const base::Closure callback) = 0;

  virtual ~PacketReceiver() {}
};

class VideoEncoderController {
 public:
  // Inform the encoder about the new target bit rate.
  virtual void SetBitRate(int new_bit_rate) = 0;

  // Inform the encoder to not encode the next frame.
  // Note: this setting is sticky and should last until called with false.
  virtual void SkipNextFrame(bool skip_next_frame) = 0;

  // Inform the encoder to encode the next frame as a key frame.
  virtual void GenerateKeyFrame() = 0;

  // Inform the encoder to only reference frames older or equal to frame_id;
  virtual void LatestFrameIdToReference(uint8 frame_id) = 0;

  // Query the codec about how many frames it has skipped due to slow ACK.
  virtual int NumberOfSkippedFrames() const = 0;

 protected:
  virtual ~VideoEncoderController() {}
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_CONFIG_H_
