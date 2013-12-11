// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_CONFIG_H_
#define CAST_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_CONFIG_H_

#include "media/cast/cast_config.h"
#include "media/cast/rtp_common/rtp_defines.h"

namespace media {
namespace cast {

struct RtpPacketizerConfig {
  RtpPacketizerConfig() {
    ssrc = 0;
    max_payload_length = kIpPacketSize - 28;   // Default is IP-v4/UDP.
    audio = false;
    frequency = 8000;
    payload_type = -1;
    sequence_number = 0;
    rtp_timestamp = 0;
  }

  // General.
  bool audio;
  int payload_type;
  uint16 max_payload_length;
  uint16 sequence_number;
  uint32 rtp_timestamp;
  int frequency;

  // SSRC.
  unsigned int ssrc;

  // Video.
  VideoCodec video_codec;

  // Audio.
  uint8 channels;
  AudioCodec audio_codec;
};

}  // namespace cast
}  // namespace media

#endif  // CAST_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_CONFIG_H_
