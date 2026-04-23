// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/statistics_common.h"

#include "util/osp_logging.h"

namespace openscreen::cast {

// static
StatisticsEvent::Type StatisticsEvent::FromWireType(WireType wire_type) {
  switch (wire_type) {
    case WireType::kAudioAckSent:
    case WireType::kVideoAckSent:
    case WireType::kUnifiedAckSent:
      return Type::kFrameAckSent;

    case WireType::kAudioPlayoutDelay:
    case WireType::kVideoRenderDelay:
    case WireType::kUnifiedRenderDelay:
      return Type::kFramePlayedOut;

    case WireType::kAudioFrameDecoded:
    case WireType::kVideoFrameDecoded:
    case WireType::kUnifiedFrameDecoded:
      return Type::kFrameDecoded;

    case WireType::kAudioPacketReceived:
    case WireType::kVideoPacketReceived:
    case WireType::kUnifiedPacketReceived:
      return Type::kPacketReceived;

    default:
      OSP_VLOG << "Unexpected RTCP log message received: "
               << static_cast<int>(wire_type);
      return Type::kUnknown;
  }
}

// static
StatisticsEvent::WireType StatisticsEvent::ToWireType(Type type) {
  switch (type) {
    case Type::kUnknown:
      return WireType::kUnknown;

    case Type::kFrameAckSent:
      return WireType::kUnifiedAckSent;

    case Type::kFramePlayedOut:
      return WireType::kUnifiedRenderDelay;

    case Type::kFrameDecoded:
      return WireType::kUnifiedFrameDecoded;

    case Type::kPacketReceived:
      return WireType::kUnifiedPacketReceived;

    default:
      OSP_VLOG << "Unknown RTCP log message event type: "
               << static_cast<int>(type);
      return WireType::kUnknown;
  }
}

// static
StatisticsEvent::MediaType StatisticsEvent::ToMediaType(StreamType type) {
  switch (type) {
    case StreamType::kUnknown:
      return MediaType::kUnknown;
    case StreamType::kAudio:
      return MediaType::kAudio;
    case StreamType::kVideo:
      return MediaType::kVideo;
  }

  OSP_NOTREACHED();
}

StatisticsEvent::StatisticsEvent(const StatisticsEvent& other) = default;
StatisticsEvent::StatisticsEvent(StatisticsEvent&& other) noexcept = default;
StatisticsEvent& StatisticsEvent::operator=(const StatisticsEvent& other) =
    default;
StatisticsEvent& StatisticsEvent::operator=(StatisticsEvent&& other) = default;

bool StatisticsEvent::operator==(const StatisticsEvent& other) const {
  return frame_id == other.frame_id && type == other.type &&
         media_type == other.media_type &&
         rtp_timestamp == other.rtp_timestamp && size == other.size &&
         timestamp == other.timestamp &&
         received_timestamp == other.received_timestamp;
}

FrameEvent::FrameEvent(const FrameEvent& other) = default;
FrameEvent::FrameEvent(FrameEvent&& other) noexcept = default;
FrameEvent& FrameEvent::operator=(const FrameEvent& other) = default;
FrameEvent& FrameEvent::operator=(FrameEvent&& other) = default;

bool FrameEvent::operator==(const FrameEvent& other) const {
  return StatisticsEvent::operator==(other) && width == other.width &&
         height == other.height && delay_delta == other.delay_delta &&
         key_frame == other.key_frame && target_bitrate == other.target_bitrate;
}

PacketEvent::PacketEvent(const PacketEvent& other) = default;
PacketEvent::PacketEvent(PacketEvent&& other) noexcept = default;
PacketEvent& PacketEvent::operator=(const PacketEvent& other) = default;
PacketEvent& PacketEvent::operator=(PacketEvent&& other) = default;

bool PacketEvent::operator==(const PacketEvent& other) const {
  return StatisticsEvent::operator==(other) && packet_id == other.packet_id &&
         max_packet_id == other.max_packet_id;
}

}  // namespace openscreen::cast
