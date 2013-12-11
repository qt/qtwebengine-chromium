// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp_sender.h"

#include <algorithm>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

static const int kRtcpMaxNackFields = 253;
static const int kRtcpMaxCastLossFields = 100;

RtcpSender::RtcpSender(PacedPacketSender* outgoing_transport,
                       uint32 sending_ssrc,
                       const std::string& c_name)
     : ssrc_(sending_ssrc),
       c_name_(c_name),
       transport_(outgoing_transport) {
  DCHECK_LT(c_name_.length(), kRtcpCnameSize) << "Invalid config";
}

RtcpSender::~RtcpSender() {}

void RtcpSender::SendRtcp(uint32 packet_type_flags,
                          const RtcpSenderInfo* sender_info,
                          const RtcpReportBlock* report_block,
                          uint32 pli_remote_ssrc,
                          const RtcpDlrrReportBlock* dlrr,
                          const RtcpReceiverReferenceTimeReport* rrtr,
                          const RtcpCastMessage* cast_message) {
  std::vector<uint8> packet;
  packet.reserve(kIpPacketSize);
  if (packet_type_flags & kRtcpSr) {
    DCHECK(sender_info) << "Invalid argument";
    BuildSR(*sender_info, report_block, &packet);
    BuildSdec(&packet);
  } else if (packet_type_flags & kRtcpRr) {
    BuildRR(report_block, &packet);
    if (!c_name_.empty()) {
      BuildSdec(&packet);
    }
  }
  if (packet_type_flags & kRtcpPli) {
    BuildPli(pli_remote_ssrc, &packet);
  }
  if (packet_type_flags & kRtcpBye) {
    BuildBye(&packet);
  }
  if (packet_type_flags & kRtcpRpsi) {
    // Implement this for webrtc interop.
    NOTIMPLEMENTED();
  }
  if (packet_type_flags & kRtcpRemb) {
    // Implement this for webrtc interop.
    NOTIMPLEMENTED();
  }
  if (packet_type_flags & kRtcpNack) {
    // Implement this for webrtc interop.
    NOTIMPLEMENTED();
  }
  if (packet_type_flags & kRtcpDlrr) {
    DCHECK(dlrr) << "Invalid argument";
    BuildDlrrRb(dlrr, &packet);
  }
  if (packet_type_flags & kRtcpRrtr) {
    DCHECK(rrtr) << "Invalid argument";
    BuildRrtr(rrtr, &packet);
  }
  if (packet_type_flags & kRtcpCast) {
    DCHECK(cast_message) << "Invalid argument";
    BuildCast(cast_message, &packet);
  }

  if (packet.empty()) return;  // Sanity don't send empty packets.

  transport_->SendRtcpPacket(packet);
}

void RtcpSender::BuildSR(const RtcpSenderInfo& sender_info,
                         const RtcpReportBlock* report_block,
                         std::vector<uint8>* packet) const {
  // Sender report.
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 52, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 52 > kIpPacketSize) return;

  uint16 number_of_rows = (report_block) ? 12 : 6;
  packet->resize(start_size + 28);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 28);
  big_endian_writer.WriteU8(0x80 + (report_block ? 1 : 0));
  big_endian_writer.WriteU8(200);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(sender_info.ntp_seconds);
  big_endian_writer.WriteU32(sender_info.ntp_fraction);
  big_endian_writer.WriteU32(sender_info.rtp_timestamp);
  big_endian_writer.WriteU32(sender_info.send_packet_count);
  big_endian_writer.WriteU32(sender_info.send_octet_count);

  if (report_block) {
    AddReportBlocks(*report_block, packet);  // Adds 24 bytes.
  }
}

void RtcpSender::BuildRR(const RtcpReportBlock* report_block,
                         std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 32, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 32 > kIpPacketSize) return;

  uint16 number_of_rows = (report_block) ? 7 : 1;
  packet->resize(start_size + 8);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 8);
  big_endian_writer.WriteU8(0x80 + (report_block ? 1 : 0));
  big_endian_writer.WriteU8(201);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);

  if (report_block) {
    AddReportBlocks(*report_block, packet);  // Adds 24 bytes.
  }
}

void RtcpSender::AddReportBlocks(const RtcpReportBlock& report_block,
                                 std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kIpPacketSize) return;

  packet->resize(start_size + 24);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 24);
  big_endian_writer.WriteU32(report_block.media_ssrc);
  big_endian_writer.WriteU8(report_block.fraction_lost);
  big_endian_writer.WriteU8(report_block.cumulative_lost >> 16);
  big_endian_writer.WriteU8(report_block.cumulative_lost >> 8);
  big_endian_writer.WriteU8(report_block.cumulative_lost);

  // Extended highest seq_no, contain the highest sequence number received.
  big_endian_writer.WriteU32(report_block.extended_high_sequence_number);
  big_endian_writer.WriteU32(report_block.jitter);

  // Last SR timestamp; our NTP time when we received the last report.
  // This is the value that we read from the send report packet not when we
  // received it.
  big_endian_writer.WriteU32(report_block.last_sr);

  // Delay since last received report, time since we received the report.
  big_endian_writer.WriteU32(report_block.delay_since_last_sr);
}

void RtcpSender::BuildSdec(std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size +  12 + c_name_.length(), kIpPacketSize)
      << "Not enough buffer space";
  if (start_size + 12 > kIpPacketSize) return;

  // SDES Source Description.
  packet->resize(start_size + 10);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 10);
  // We always need to add one SDES CNAME.
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(202);

  // Handle SDES length later on.
  uint32 sdes_length_position = start_size + 3;
  big_endian_writer.WriteU16(0);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(1);  // CNAME = 1
  big_endian_writer.WriteU8(static_cast<uint8>(c_name_.length()));

  size_t sdes_length = 10 + c_name_.length();
  packet->insert(packet->end(), c_name_.c_str(),
                 c_name_.c_str() + c_name_.length());

  size_t padding = 0;

  // We must have a zero field even if we have an even multiple of 4 bytes.
  if ((packet->size() % 4) == 0) {
    padding++;
    packet->push_back(0);
  }
  while ((packet->size() % 4) != 0) {
    padding++;
    packet->push_back(0);
  }
  sdes_length += padding;

  // In 32-bit words minus one and we don't count the header.
  uint8 buffer_length = (sdes_length / 4) - 1;
  (*packet)[sdes_length_position] = buffer_length;
}

void RtcpSender::BuildPli(uint32 remote_ssrc,
                          std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 12, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 12 > kIpPacketSize) return;

  packet->resize(start_size + 12);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 12);
  uint8 FMT = 1;  // Picture loss indicator.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(206);
  big_endian_writer.WriteU16(2);  // Used fixed length of 2.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(remote_ssrc);  // Add the remote SSRC.
  TRACE_EVENT_INSTANT2("cast_rtcp", "RtcpSender::PLI", TRACE_EVENT_SCOPE_THREAD,
                       "remote_ssrc", remote_ssrc,
                       "ssrc", ssrc_);
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native Rpsi bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
void RtcpSender::BuildRpsi(const RtcpRpsiMessage* rpsi,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kIpPacketSize) return;

  packet->resize(start_size + 24);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 24);
  uint8 FMT = 3;  // Reference Picture Selection Indication.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(206);

  // Calculate length.
  uint32 bits_required = 7;
  uint8 bytes_required = 1;
  while ((rpsi->picture_id >> bits_required) > 0) {
    bits_required += 7;
    bytes_required++;
  }
  uint8 size = 3;
  if (bytes_required > 6) {
    size = 5;
  } else if (bytes_required > 2) {
    size = 4;
  }
  big_endian_writer.WriteU8(0);
  big_endian_writer.WriteU8(size);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(rpsi->remote_ssrc);

  uint8 padding_bytes = 4 - ((2 + bytes_required) % 4);
  if (padding_bytes == 4) {
    padding_bytes = 0;
  }
  // Add padding length in bits, padding can be 0, 8, 16 or 24.
  big_endian_writer.WriteU8(padding_bytes * 8);
  big_endian_writer.WriteU8(rpsi->payload_type);

  // Add picture ID.
  for (int i = bytes_required - 1; i > 0; i--) {
    big_endian_writer.WriteU8(
        0x80 | static_cast<uint8>(rpsi->picture_id >> (i * 7)));
  }
  // Add last byte of picture ID.
  big_endian_writer.WriteU8(static_cast<uint8>(rpsi->picture_id & 0x7f));

  // Add padding.
  for (int j = 0; j < padding_bytes; ++j) {
    big_endian_writer.WriteU8(0);
  }
}

void RtcpSender::BuildRemb(const RtcpRembMessage* remb,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  size_t remb_size = 20 + 4 * remb->remb_ssrcs.size();
  DCHECK_LT(start_size + remb_size, kIpPacketSize)
      << "Not enough buffer space";
  if (start_size + remb_size > kIpPacketSize) return;

  packet->resize(start_size + remb_size);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), remb_size);

  // Add application layer feedback.
  uint8 FMT = 15;
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(206);
  big_endian_writer.WriteU8(0);
  big_endian_writer.WriteU8(remb->remb_ssrcs.size() + 4);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(0);  // Remote SSRC must be 0.
  big_endian_writer.WriteU32(kRemb);
  big_endian_writer.WriteU8(remb->remb_ssrcs.size());

  // 6 bit exponent and a 18 bit mantissa.
  uint8 bitrate_exponent;
  uint32 bitrate_mantissa;
  BitrateToRembExponentBitrate(remb->remb_bitrate,
                               &bitrate_exponent,
                               &bitrate_mantissa);

  big_endian_writer.WriteU8(static_cast<uint8>((bitrate_exponent << 2) +
      ((bitrate_mantissa >> 16) & 0x03)));
  big_endian_writer.WriteU8(static_cast<uint8>(bitrate_mantissa >> 8));
  big_endian_writer.WriteU8(static_cast<uint8>(bitrate_mantissa));

  std::list<uint32>::const_iterator it = remb->remb_ssrcs.begin();
  for (; it != remb->remb_ssrcs.end(); ++it) {
    big_endian_writer.WriteU32(*it);
  }
  TRACE_COUNTER_ID1("cast_rtcp", "RtcpSender::RembBitrate", ssrc_,
                    remb->remb_bitrate);
}

void RtcpSender::BuildNack(const RtcpNackMessage* nack,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 16, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 16 > kIpPacketSize) return;

  packet->resize(start_size + 16);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 16);

  uint8 FMT = 1;
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(205);
  big_endian_writer.WriteU8(0);
  int nack_size_pos = start_size + 3;
  big_endian_writer.WriteU8(3);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(nack->remote_ssrc);  // Add the remote SSRC.

  // Build NACK bitmasks and write them to the Rtcp message.
  // The nack list should be sorted and not contain duplicates.
  int number_of_nack_fields = 0;
  int max_number_of_nack_fields =
      std::min<int>(kRtcpMaxNackFields, (kIpPacketSize - packet->size()) / 4);

  std::list<uint16>::const_iterator it = nack->nack_list.begin();
  while (it != nack->nack_list.end() &&
         number_of_nack_fields < max_number_of_nack_fields) {
    uint16 nack_sequence_number = *it;
    uint16 bitmask = 0;
    ++it;
    while (it != nack->nack_list.end()) {
      int shift = static_cast<uint16>(*it - nack_sequence_number) - 1;
      if (shift >= 0 && shift <= 15) {
        bitmask |= (1 << shift);
        ++it;
      } else {
        break;
      }
    }
    // Write the sequence number and the bitmask to the packet.
    start_size = packet->size();
    DCHECK_LT(start_size + 4, kIpPacketSize) << "Not enough buffer space";
    if (start_size + 4 > kIpPacketSize) return;

    packet->resize(start_size + 4);
    net::BigEndianWriter big_endian_nack_writer(&((*packet)[start_size]), 4);
    big_endian_nack_writer.WriteU16(nack_sequence_number);
    big_endian_nack_writer.WriteU16(bitmask);
    number_of_nack_fields++;
  }
  (*packet)[nack_size_pos] = static_cast<uint8>(2 + number_of_nack_fields);
  TRACE_COUNTER_ID1("cast_rtcp", "RtcpSender::NACK", ssrc_,
                    nack->nack_list.size());
}

void RtcpSender::BuildBye(std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 8, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 8 > kIpPacketSize) return;

  packet->resize(start_size + 8);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 8);
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(203);
  big_endian_writer.WriteU16(1);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
}

/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|reserved |   PT=XR=207   |             length            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                              SSRC                             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     BT=5      |   reserved    |         block length          |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |                 SSRC_1 (SSRC of first receiver)               | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  |                         last RR (LRR)                         |   1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   delay since last RR (DLRR)                  |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
void RtcpSender::BuildDlrrRb(const RtcpDlrrReportBlock* dlrr,
                             std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kIpPacketSize) return;

  packet->resize(start_size + 24);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 24);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(207);
  big_endian_writer.WriteU16(5);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(5);  // Add block type.
  big_endian_writer.WriteU8(0);  // Add reserved.
  big_endian_writer.WriteU16(3);  // Block length.
  big_endian_writer.WriteU32(ssrc_);  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(dlrr->last_rr);
  big_endian_writer.WriteU32(dlrr->delay_since_last_rr);
}

void RtcpSender::BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kIpPacketSize) return;

  packet->resize(start_size + 20);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 20);

  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(207);
  big_endian_writer.WriteU16(4);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(4);  // Add block type.
  big_endian_writer.WriteU8(0);  // Add reserved.
  big_endian_writer.WriteU16(2);  // Block length.

  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(rrtr->ntp_seconds);
  big_endian_writer.WriteU32(rrtr->ntp_fraction);
}

void RtcpSender::BuildCast(const RtcpCastMessage* cast,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kIpPacketSize) return;

  packet->resize(start_size + 20);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 20);
  uint8 FMT = 15;  // Application layer feedback.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(206);
  big_endian_writer.WriteU8(0);
  int cast_size_pos = start_size + 3;  // Save length position.
  big_endian_writer.WriteU8(4);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(cast->media_ssrc_);  // Remote SSRC.
  big_endian_writer.WriteU32(kCast);
  big_endian_writer.WriteU8(cast->ack_frame_id_);
  int cast_loss_field_pos = start_size + 17;  // Save loss field position.
  big_endian_writer.WriteU8(0);  // Overwritten with number_of_loss_fields.
  big_endian_writer.WriteU8(0);  // Reserved.
  big_endian_writer.WriteU8(0);  // Reserved.

  int number_of_loss_fields = 0;
  int max_number_of_loss_fields = std::min<int>(kRtcpMaxCastLossFields,
      (kIpPacketSize - packet->size()) / 4);

  std::map<uint8, std::set<uint16> >::const_iterator frame_it =
      cast->missing_frames_and_packets_.begin();

  for (; frame_it != cast->missing_frames_and_packets_.end() &&
      number_of_loss_fields < max_number_of_loss_fields; ++frame_it) {
    // Iterate through all frames with missing packets.
    if (frame_it->second.empty()) {
      // Special case all packets in a frame is missing.
      start_size = packet->size();
      packet->resize(start_size + 4);
      net::BigEndianWriter big_endian_nack_writer(&((*packet)[start_size]), 4);
      big_endian_nack_writer.WriteU8(frame_it->first);
      big_endian_nack_writer.WriteU16(kRtcpCastAllPacketsLost);
      big_endian_nack_writer.WriteU8(0);
      ++number_of_loss_fields;
    } else {
      std::set<uint16>::const_iterator packet_it = frame_it->second.begin();
      while (packet_it != frame_it->second.end()) {
        uint16 packet_id = *packet_it;

        start_size = packet->size();
        packet->resize(start_size + 4);
        net::BigEndianWriter big_endian_nack_writer(
            &((*packet)[start_size]), 4);

        // Write frame and packet id to buffer before calculating bitmask.
        big_endian_nack_writer.WriteU8(frame_it->first);
        big_endian_nack_writer.WriteU16(packet_id);

        uint8 bitmask = 0;
        ++packet_it;
        while (packet_it != frame_it->second.end()) {
          int shift = static_cast<uint8>(*packet_it - packet_id) - 1;
          if (shift >= 0 && shift <= 7) {
            bitmask |= (1 << shift);
            ++packet_it;
          } else {
            break;
          }
        }
        big_endian_nack_writer.WriteU8(bitmask);
        ++number_of_loss_fields;
      }
    }
  }
  (*packet)[cast_size_pos] = static_cast<uint8>(4 + number_of_loss_fields);
  (*packet)[cast_loss_field_pos] = static_cast<uint8>(number_of_loss_fields);

  // Frames with missing packets.
  TRACE_COUNTER_ID1("cast_rtcp", "RtcpSender::CastNACK", ssrc_,
                    cast->missing_frames_and_packets_.size());
}

}  // namespace cast
}  // namespace media
