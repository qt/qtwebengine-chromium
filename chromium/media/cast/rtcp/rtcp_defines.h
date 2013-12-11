// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_DEFINES_H_
#define MEDIA_CAST_RTCP_RTCP_DEFINES_H_

#include <list>
#include <map>
#include <set>

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

class RtcpCastMessage {
 public:
  explicit RtcpCastMessage(uint32 media_ssrc);
  ~RtcpCastMessage();

  uint32 media_ssrc_;
  uint8 ack_frame_id_;
  MissingFramesAndPacketsMap missing_frames_and_packets_;
};

struct RtcpSenderInfo {
  // First three members are used for lipsync.
  // First two members are used for rtt.
  uint32 ntp_seconds;
  uint32 ntp_fraction;
  uint32 rtp_timestamp;
  uint32 send_packet_count;
  uint32 send_octet_count;
};

struct RtcpReportBlock {
  uint32 remote_ssrc;  // SSRC of sender of this report.
  uint32 media_ssrc;  // SSRC of the RTP packet sender.
  uint8 fraction_lost;
  uint32 cumulative_lost;  // 24 bits valid.
  uint32 extended_high_sequence_number;
  uint32 jitter;
  uint32 last_sr;
  uint32 delay_since_last_sr;
};

struct RtcpRpsiMessage {
  uint32 remote_ssrc;
  uint8 payload_type;
  uint64 picture_id;
};

class RtcpNackMessage {
 public:
  RtcpNackMessage();
  ~RtcpNackMessage();

  uint32 remote_ssrc;
  std::list<uint16> nack_list;
};

class RtcpRembMessage {
 public:
  RtcpRembMessage();
  ~RtcpRembMessage();

  uint32 remb_bitrate;
  std::list<uint32> remb_ssrcs;
};

struct RtcpReceiverReferenceTimeReport {
  uint32 remote_ssrc;
  uint32 ntp_seconds;
  uint32 ntp_fraction;
};

struct RtcpDlrrReportBlock {
  uint32 last_rr;
  uint32 delay_since_last_rr;
};

inline bool operator==(RtcpReportBlock lhs, RtcpReportBlock rhs) {
  return lhs.remote_ssrc == rhs.remote_ssrc &&
      lhs.media_ssrc == rhs.media_ssrc &&
      lhs.fraction_lost == rhs.fraction_lost &&
      lhs.cumulative_lost == rhs.cumulative_lost &&
      lhs.extended_high_sequence_number == rhs.extended_high_sequence_number &&
      lhs.jitter == rhs.jitter &&
      lhs.last_sr == rhs.last_sr &&
      lhs.delay_since_last_sr == rhs.delay_since_last_sr;
}

inline bool operator==(RtcpSenderInfo lhs, RtcpSenderInfo rhs) {
  return lhs.ntp_seconds == rhs.ntp_seconds &&
      lhs.ntp_fraction == rhs.ntp_fraction &&
      lhs.rtp_timestamp == rhs.rtp_timestamp &&
      lhs.send_packet_count == rhs.send_packet_count &&
      lhs.send_octet_count == rhs.send_octet_count;
}

inline bool operator==(RtcpReceiverReferenceTimeReport lhs,
                       RtcpReceiverReferenceTimeReport rhs) {
  return lhs.remote_ssrc == rhs.remote_ssrc &&
      lhs.ntp_seconds == rhs.ntp_seconds &&
      lhs.ntp_fraction == rhs.ntp_fraction;
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_DEFINES_H_
