/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(pbos): Move Config from common.h to here.

#ifndef WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_CONFIG_H_
#define WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_CONFIG_H_

#include <string>
#include <vector>

#include "webrtc/typedefs.h"

namespace webrtc {

struct RtpStatistics {
  RtpStatistics()
      : ssrc(0),
        fraction_loss(0),
        cumulative_loss(0),
        extended_max_sequence_number(0) {}
  uint32_t ssrc;
  int fraction_loss;
  int cumulative_loss;
  int extended_max_sequence_number;
  std::string c_name;
};

// Settings for NACK, see RFC 4585 for details.
struct NackConfig {
  NackConfig() : rtp_history_ms(0) {}
  // Send side: the time RTP packets are stored for retransmissions.
  // Receive side: the time the receiver is prepared to wait for
  // retransmissions.
  // Set to '0' to disable.
  int rtp_history_ms;
};

// Settings for forward error correction, see RFC 5109 for details. Set the
// payload types to '-1' to disable.
struct FecConfig {
  FecConfig() : ulpfec_payload_type(-1), red_payload_type(-1) {}
  // Payload type used for ULPFEC packets.
  int ulpfec_payload_type;

  // Payload type used for RED packets.
  int red_payload_type;
};

// Settings for RTP retransmission payload format, see RFC 4588 for details.
struct RtxConfig {
  RtxConfig() : rtx_payload_type(0), video_payload_type(0) {}
  // SSRCs to use for the RTX streams.
  std::vector<uint32_t> ssrcs;

  // Payload type to use for the RTX stream.
  int rtx_payload_type;

  // Original video payload this RTX stream is used for.
  int video_payload_type;
};

// RTP header extension to use for the video stream, see RFC 5285.
struct RtpExtension {
  static const char* kTOffset;
  static const char* kAbsSendTime;
  RtpExtension(const char* name, int id) : name(name), id(id) {}
  // TODO(mflodman) Add API to query supported extensions.
  std::string name;
  int id;
};
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_NEW_INCLUDE_CONFIG_H_
