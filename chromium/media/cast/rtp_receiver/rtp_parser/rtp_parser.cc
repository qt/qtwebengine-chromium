// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_receiver/rtp_parser/rtp_parser.h"

#include "base/logging.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

static const int kRtpCommonHeaderLength = 12;
static const int kRtpCastHeaderLength = 7;
static const uint8 kCastKeyFrameBitMask = 0x80;
static const uint8 kCastReferenceFrameIdBitMask = 0x40;

RtpParser::RtpParser(RtpData* incoming_payload_callback,
                     const RtpParserConfig parser_config)
    : data_callback_(incoming_payload_callback),
      parser_config_(parser_config) {
}

RtpParser::~RtpParser() {
}

bool RtpParser::ParsePacket(const uint8* packet, int length,
                            RtpCastHeader* rtp_header) {
  if (length == 0) return false;
  // Get RTP general header.
  if (!ParseCommon(packet, length, rtp_header)) return false;
  if (rtp_header->webrtc.header.payloadType == parser_config_.payload_type &&
    rtp_header->webrtc.header.ssrc == parser_config_.ssrc) {
    return ParseCast(packet + kRtpCommonHeaderLength,
        length - kRtpCommonHeaderLength, rtp_header);
  }
  // Not a valid payload type / ssrc combination.
  return false;
}

bool RtpParser::ParseCommon(const uint8* packet,
                            int length,
                            RtpCastHeader* rtp_header) {
  if (length < kRtpCommonHeaderLength) return false;
  uint8 version = packet[0] >> 6;
  if (version != 2) return false;
  uint8 cc = packet[0] & 0x0f;
  bool marker = ((packet[1] & 0x80) != 0);
  int payload_type = packet[1] & 0x7f;

  uint16 sequence_number;
  uint32 rtp_timestamp, ssrc;
  net::BigEndianReader big_endian_reader(packet + 2, 80);
  big_endian_reader.ReadU16(&sequence_number);
  big_endian_reader.ReadU32(&rtp_timestamp);
  big_endian_reader.ReadU32(&ssrc);

  rtp_header->webrtc.header.markerBit      = marker;
  rtp_header->webrtc.header.payloadType    = payload_type;
  rtp_header->webrtc.header.sequenceNumber = sequence_number;
  rtp_header->webrtc.header.timestamp      = rtp_timestamp;
  rtp_header->webrtc.header.ssrc           = ssrc;
  rtp_header->webrtc.header.numCSRCs       = cc;

  uint8 csrc_octs = cc * 4;
  rtp_header->webrtc.type.Audio.numEnergy = rtp_header->webrtc.header.numCSRCs;
  rtp_header->webrtc.header.headerLength = kRtpCommonHeaderLength + csrc_octs;
  rtp_header->webrtc.type.Audio.isCNG = false;
  rtp_header->webrtc.type.Audio.channel = parser_config_.audio_channels;
  return true;
}

bool RtpParser::ParseCast(const uint8* packet,
                          int length,
                          RtpCastHeader* rtp_header) {
  if (length < kRtpCastHeaderLength) return false;
  // Extract header.
  const uint8* data_ptr = packet;
  int data_length = length;
  rtp_header->is_key_frame = (data_ptr[0] & kCastKeyFrameBitMask);
  rtp_header->is_reference = (data_ptr[0] & kCastReferenceFrameIdBitMask);
  rtp_header->frame_id = data_ptr[1];

  net::BigEndianReader big_endian_reader(data_ptr + 2, 32);
  big_endian_reader.ReadU16(&rtp_header->packet_id);
  big_endian_reader.ReadU16(&rtp_header->max_packet_id);

  if (rtp_header->is_reference) {
    rtp_header->reference_frame_id = data_ptr[6];
    data_ptr += kRtpCastHeaderLength;
    data_length -= kRtpCastHeaderLength;
  } else {
    data_ptr += kRtpCastHeaderLength - 1;
    data_length -= kRtpCastHeaderLength - 1;
  }

  if (rtp_header->max_packet_id < rtp_header->packet_id) {
    return false;
  }
  data_callback_->OnReceivedPayloadData(data_ptr, data_length, rtp_header);
  return true;
}

}  // namespace cast
}  // namespace media
