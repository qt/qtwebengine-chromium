// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp_sender/rtp_packetizer/rtp_packetizer.h"

#include "base/logging.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

static const uint16 kCommonRtpHeaderLength = 12;
static const uint16 kCastRtpHeaderLength = 7;
static const uint8 kCastKeyFrameBitMask = 0x80;
static const uint8 kCastReferenceFrameIdBitMask = 0x40;

RtpPacketizer::RtpPacketizer(PacedPacketSender* transport,
                             PacketStorage* packet_storage,
                             RtpPacketizerConfig rtp_packetizer_config)
    : config_(rtp_packetizer_config),
      transport_(transport),
      packet_storage_(packet_storage),
      sequence_number_(config_.sequence_number),
      rtp_timestamp_(config_.rtp_timestamp),
      packet_id_(0),
      send_packets_count_(0),
      send_octet_count_(0) {
  DCHECK(transport) << "Invalid argument";
}

RtpPacketizer::~RtpPacketizer() {}

void RtpPacketizer::IncomingEncodedVideoFrame(
    const EncodedVideoFrame* video_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(!config_.audio) << "Invalid state";
  if (config_.audio) return;

  // Timestamp is in 90 KHz for video.
  rtp_timestamp_ = GetVideoRtpTimestamp(capture_time);
  time_last_sent_rtp_timestamp_ = capture_time;

  Cast(video_frame->key_frame,
       video_frame->frame_id,
       video_frame->last_referenced_frame_id,
       rtp_timestamp_,
       video_frame->data);
}

void RtpPacketizer::IncomingEncodedAudioFrame(
    const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(config_.audio) << "Invalid state";
  if (!config_.audio) return;

  rtp_timestamp_ += audio_frame->samples;  // Timestamp is in samples for audio.
  time_last_sent_rtp_timestamp_ = recorded_time;
  Cast(true, audio_frame->frame_id, 0, rtp_timestamp_, audio_frame->data);
}

uint16 RtpPacketizer::NextSequenceNumber() {
  ++sequence_number_;
  return sequence_number_ - 1;
}

bool RtpPacketizer::LastSentTimestamp(base::TimeTicks* time_sent,
                                      uint32* rtp_timestamp) const {
  if (time_last_sent_rtp_timestamp_.is_null()) return false;

  *time_sent = time_last_sent_rtp_timestamp_;
  *rtp_timestamp = rtp_timestamp_;
  return true;
}

// TODO(mikhal): Switch to pass data with a const_ref.
void RtpPacketizer::Cast(bool is_key,
                         uint32 frame_id,
                         uint32 reference_frame_id,
                         uint32 timestamp,
                         const std::string& data) {
  uint16 rtp_header_length = kCommonRtpHeaderLength + kCastRtpHeaderLength;
  uint16 max_length = config_.max_payload_length - rtp_header_length - 1;

  // Split the payload evenly (round number up).
  size_t num_packets = (data.size() + max_length) / max_length;
  size_t payload_length = (data.size() + num_packets) / num_packets;
  DCHECK_LE(payload_length, max_length) << "Invalid argument";

  PacketList packets;

  size_t remaining_size = data.size();
  std::string::const_iterator data_iter = data.begin();
  while (remaining_size > 0) {
    Packet packet;

    if (remaining_size < payload_length) {
      payload_length = remaining_size;
    }
    remaining_size -= payload_length;
    BuildCommonRTPheader(&packet, remaining_size == 0, timestamp);

    // Build Cast header.
    packet.push_back(
        (is_key ? kCastKeyFrameBitMask : 0) | kCastReferenceFrameIdBitMask);
    packet.push_back(frame_id);
    size_t start_size = packet.size();
    packet.resize(start_size + 4);
    net::BigEndianWriter big_endian_writer(&(packet[start_size]), 4);
    big_endian_writer.WriteU16(packet_id_);
    big_endian_writer.WriteU16(static_cast<uint16>(num_packets - 1));
    packet.push_back(static_cast<uint8>(reference_frame_id));

    // Copy payload data.
    packet.insert(packet.end(), data_iter, data_iter + payload_length);

    // Store packet.
    packet_storage_->StorePacket(frame_id, packet_id_, &packet);
    ++packet_id_;
    data_iter += payload_length;

    // Update stats.
    ++send_packets_count_;
    send_octet_count_ += payload_length;
    packets.push_back(packet);
  }
  DCHECK(packet_id_ == num_packets) << "Invalid state";

  // Send to network.
  transport_->SendPackets(packets);

  // Prepare for next frame.
  packet_id_ = 0;
}

void RtpPacketizer::BuildCommonRTPheader(
    Packet* packet, bool marker_bit, uint32 time_stamp) {
  packet->push_back(0x80);
  packet->push_back(static_cast<uint8>(config_.payload_type) |
                    (marker_bit ? kRtpMarkerBitMask : 0));
  size_t start_size = packet->size();
  packet->resize(start_size + 10);
  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 10);
  big_endian_writer.WriteU16(sequence_number_);
  big_endian_writer.WriteU32(time_stamp);
  big_endian_writer.WriteU32(config_.ssrc);
  ++sequence_number_;
}

}  // namespace cast
}  // namespace media
