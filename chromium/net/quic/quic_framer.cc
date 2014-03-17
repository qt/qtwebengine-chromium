// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_framer.h"

#include "base/containers/hash_tables.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_data_writer.h"

using base::StringPiece;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::numeric_limits;
using std::string;

namespace net {

namespace {

// Mask to select the lowest 48 bits of a sequence number.
const QuicPacketSequenceNumber k6ByteSequenceNumberMask =
    GG_UINT64_C(0x0000FFFFFFFFFFFF);
const QuicPacketSequenceNumber k4ByteSequenceNumberMask =
    GG_UINT64_C(0x00000000FFFFFFFF);
const QuicPacketSequenceNumber k2ByteSequenceNumberMask =
    GG_UINT64_C(0x000000000000FFFF);
const QuicPacketSequenceNumber k1ByteSequenceNumberMask =
    GG_UINT64_C(0x00000000000000FF);

const QuicGuid k1ByteGuidMask = GG_UINT64_C(0x00000000000000FF);
const QuicGuid k4ByteGuidMask = GG_UINT64_C(0x00000000FFFFFFFF);

// Number of bits the sequence number length bits are shifted from the right
// edge of the public header.
const uint8 kPublicHeaderSequenceNumberShift = 4;

// New Frame Types, QUIC v. >= 10:
// There are two interpretations for the Frame Type byte in the QUIC protocol,
// resulting in two Frame Types: Special Frame Types and Regular Frame Types.
//
// Regular Frame Types use the Frame Type byte simply. Currently defined
// Regular Frame Types are:
// Padding            : 0b 00000000 (0x00)
// ResetStream        : 0b 00000001 (0x01)
// ConnectionClose    : 0b 00000010 (0x02)
// GoAway             : 0b 00000011 (0x03)
//
// Special Frame Types encode both a Frame Type and corresponding flags
// all in the Frame Type byte. Currently defined Special Frame Types are:
// Stream             : 0b 1xxxxxxx
// Ack                : 0b 01xxxxxx
// CongestionFeedback : 0b 001xxxxx
//
// Semantics of the flag bits above (the x bits) depends on the frame type.

// Masks to determine if the frame type is a special use
// and for specific special frame types.
const uint8 kQuicFrameTypeSpecialMask = 0xE0;  // 0b 11100000
const uint8 kQuicFrameTypeStreamMask = 0x80;
const uint8 kQuicFrameTypeAckMask = 0x40;
const uint8 kQuicFrameTypeCongestionFeedbackMask = 0x20;

// Stream frame relative shifts and masks for interpreting the stream flags.
// StreamID may be 1, 2, 3, or 4 bytes.
const uint8 kQuicStreamIdShift = 2;
const uint8 kQuicStreamIDLengthMask = 0x03;

// Offset may be 0, 2, 3, 4, 5, 6, 7, 8 bytes.
const uint8 kQuicStreamOffsetShift = 3;
const uint8 kQuicStreamOffsetMask = 0x07;

// Data length may be 0 or 2 bytes.
const uint8 kQuicStreamDataLengthShift = 1;
const uint8 kQuicStreamDataLengthMask = 0x01;

// Fin bit may be set or not.
const uint8 kQuicStreamFinShift = 1;
const uint8 kQuicStreamFinMask = 0x01;

// Sequence number size shift used in AckFrames.
const uint8 kQuicSequenceNumberLengthShift = 2;

// Acks may be truncated.
const uint8 kQuicAckTruncatedShift = 1;
const uint8 kQuicAckTruncatedMask = 0x01;

// Acks may not have any nacks.
const uint8 kQuicHasNacksMask = 0x01;

// Returns the absolute value of the difference between |a| and |b|.
QuicPacketSequenceNumber Delta(QuicPacketSequenceNumber a,
                               QuicPacketSequenceNumber b) {
  // Since these are unsigned numbers, we can't just return abs(a - b)
  if (a < b) {
    return b - a;
  }
  return a - b;
}

QuicPacketSequenceNumber ClosestTo(QuicPacketSequenceNumber target,
                                   QuicPacketSequenceNumber a,
                                   QuicPacketSequenceNumber b) {
  return (Delta(target, a) < Delta(target, b)) ? a : b;
}

}  // namespace

QuicFramer::QuicFramer(const QuicVersionVector& supported_versions,
                       QuicTime creation_time,
                       bool is_server)
    : visitor_(NULL),
      fec_builder_(NULL),
      entropy_calculator_(NULL),
      error_(QUIC_NO_ERROR),
      last_sequence_number_(0),
      last_serialized_guid_(0),
      supported_versions_(supported_versions),
      alternative_decrypter_latch_(false),
      is_server_(is_server),
      creation_time_(creation_time) {
  DCHECK(!supported_versions.empty());
  quic_version_ = supported_versions_[0];
  decrypter_.reset(QuicDecrypter::Create(kNULL));
  encrypter_[ENCRYPTION_NONE].reset(
      QuicEncrypter::Create(kNULL));
}

QuicFramer::~QuicFramer() {}

// static
size_t QuicFramer::GetMinStreamFrameSize(QuicVersion version,
                                         QuicStreamId stream_id,
                                         QuicStreamOffset offset,
                                         bool last_frame_in_packet) {
  return kQuicFrameTypeSize + GetStreamIdSize(stream_id) +
      GetStreamOffsetSize(offset) +
      (last_frame_in_packet ? 0 : kQuicStreamPayloadLengthSize);
}

// static
size_t QuicFramer::GetMinAckFrameSize(
    QuicVersion version,
    QuicSequenceNumberLength sequence_number_length,
    QuicSequenceNumberLength largest_observed_length) {
  return kQuicFrameTypeSize + kQuicEntropyHashSize +
      sequence_number_length + kQuicEntropyHashSize +
      largest_observed_length + kQuicDeltaTimeLargestObservedSize;
}

// static
size_t QuicFramer::GetMinRstStreamFrameSize() {
  return kQuicFrameTypeSize + kQuicMaxStreamIdSize + kQuicErrorCodeSize +
      kQuicErrorDetailsLengthSize;
}

// static
size_t QuicFramer::GetMinConnectionCloseFrameSize() {
  return kQuicFrameTypeSize + kQuicErrorCodeSize + kQuicErrorDetailsLengthSize;
}

// static
size_t QuicFramer::GetMinGoAwayFrameSize() {
  return kQuicFrameTypeSize + kQuicErrorCodeSize + kQuicErrorDetailsLengthSize +
      kQuicMaxStreamIdSize;
}

// static
size_t QuicFramer::GetStreamIdSize(QuicStreamId stream_id) {
  // Sizes are 1 through 4 bytes.
  for (int i = 1; i <= 4; ++i) {
    stream_id >>= 8;
    if (stream_id == 0) {
      return i;
    }
  }
  LOG(DFATAL) << "Failed to determine StreamIDSize.";
  return 4;
}

// static
size_t QuicFramer::GetStreamOffsetSize(QuicStreamOffset offset) {
  // 0 is a special case.
  if (offset == 0) {
    return 0;
  }
  // 2 through 8 are the remaining sizes.
  offset >>= 8;
  for (int i = 2; i <= 8; ++i) {
    offset >>= 8;
    if (offset == 0) {
      return i;
    }
  }
  LOG(DFATAL) << "Failed to determine StreamOffsetSize.";
  return 8;
}

// static
size_t QuicFramer::GetVersionNegotiationPacketSize(size_t number_versions) {
  return kPublicFlagsSize + PACKET_8BYTE_GUID +
      number_versions * kQuicVersionSize;
}

// static
bool QuicFramer::CanTruncate(
    QuicVersion version, const QuicFrame& frame, size_t free_bytes) {
  if ((frame.type == ACK_FRAME || frame.type == CONNECTION_CLOSE_FRAME) &&
      free_bytes >= GetMinAckFrameSize(version,
                                       PACKET_6BYTE_SEQUENCE_NUMBER,
                                       PACKET_6BYTE_SEQUENCE_NUMBER)) {
    return true;
  }
  return false;
}

bool QuicFramer::IsSupportedVersion(const QuicVersion version) const {
  for (size_t i = 0; i < supported_versions_.size(); ++i) {
    if (version == supported_versions_[i]) {
      return true;
    }
  }
  return false;
}

size_t QuicFramer::GetSerializedFrameLength(
    const QuicFrame& frame,
    size_t free_bytes,
    bool first_frame,
    bool last_frame,
    QuicSequenceNumberLength sequence_number_length) {
  if (frame.type == PADDING_FRAME) {
    // PADDING implies end of packet.
    return free_bytes;
  }
  size_t frame_len =
      ComputeFrameLength(frame, last_frame, sequence_number_length);
  if (frame_len > free_bytes) {
    // Only truncate the first frame in a packet, so if subsequent ones go
    // over, stop including more frames.
    if (!first_frame) {
      return 0;
    }
    if (CanTruncate(quic_version_, frame, free_bytes)) {
      // Truncate the frame so the packet will not exceed kMaxPacketSize.
      // Note that we may not use every byte of the writer in this case.
      DVLOG(1) << "Truncating large frame";
      return free_bytes;
    }
  }
  return frame_len;
}

QuicFramer::AckFrameInfo::AckFrameInfo() : max_delta(0) { }

QuicFramer::AckFrameInfo::~AckFrameInfo() { }

QuicPacketEntropyHash QuicFramer::GetPacketEntropyHash(
    const QuicPacketHeader& header) const {
  return header.entropy_flag << (header.packet_sequence_number % 8);
}

// Test only.
SerializedPacket QuicFramer::BuildUnsizedDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames) {
  const size_t max_plaintext_size = GetMaxPlaintextSize(kMaxPacketSize);
  size_t packet_size = GetPacketHeaderSize(header);
  for (size_t i = 0; i < frames.size(); ++i) {
    DCHECK_LE(packet_size, max_plaintext_size);
    bool first_frame = i == 0;
    bool last_frame = i == frames.size() - 1;
    const size_t frame_size = GetSerializedFrameLength(
        frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
        header.public_header.sequence_number_length);
    DCHECK(frame_size);
    packet_size += frame_size;
  }
  return BuildDataPacket(header, frames, packet_size);
}

SerializedPacket QuicFramer::BuildDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames,
    size_t packet_size) {
  QuicDataWriter writer(packet_size);
  const SerializedPacket kNoPacket(
      0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL);
  if (!AppendPacketHeader(header, &writer)) {
    return kNoPacket;
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    const QuicFrame& frame = frames[i];

    const bool last_frame_in_packet = i == (frames.size() - 1);
    if (!AppendTypeByte(frame, last_frame_in_packet, &writer)) {
      return kNoPacket;
    }

    switch (frame.type) {
      case PADDING_FRAME:
        writer.WritePadding();
        break;
      case STREAM_FRAME:
        if (!AppendStreamFramePayload(
            *frame.stream_frame, last_frame_in_packet, &writer)) {
          return kNoPacket;
        }
        break;
      case ACK_FRAME:
        if (!AppendAckFramePayloadAndTypeByte(
                header, *frame.ack_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case CONGESTION_FEEDBACK_FRAME:
        if (!AppendQuicCongestionFeedbackFramePayload(
                *frame.congestion_feedback_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case RST_STREAM_FRAME:
        if (!AppendRstStreamFramePayload(*frame.rst_stream_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!AppendConnectionCloseFramePayload(
                *frame.connection_close_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case GOAWAY_FRAME:
        if (!AppendGoAwayFramePayload(*frame.goaway_frame, &writer)) {
          return kNoPacket;
        }
        break;
      default:
        RaiseError(QUIC_INVALID_FRAME_DATA);
        return kNoPacket;
    }
  }

  // Save the length before writing, because take clears it.
  const size_t len = writer.length();
  // Less than or equal because truncated acks end up with max_plaintex_size
  // length, even though they're typically slightly shorter.
  DCHECK_LE(len, packet_size);
  QuicPacket* packet = QuicPacket::NewDataPacket(
      writer.take(), len, true, header.public_header.guid_length,
      header.public_header.version_flag,
      header.public_header.sequence_number_length);

  if (fec_builder_) {
    fec_builder_->OnBuiltFecProtectedPayload(header,
                                             packet->FecProtectedData());
  }

  return SerializedPacket(header.packet_sequence_number,
                          header.public_header.sequence_number_length, packet,
                          GetPacketEntropyHash(header), NULL);
}

SerializedPacket QuicFramer::BuildFecPacket(const QuicPacketHeader& header,
                                            const QuicFecData& fec) {
  DCHECK_EQ(IN_FEC_GROUP, header.is_in_fec_group);
  DCHECK_NE(0u, header.fec_group);
  size_t len = GetPacketHeaderSize(header);
  len += fec.redundancy.length();

  QuicDataWriter writer(len);
  const SerializedPacket kNoPacket(
      0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL);
  if (!AppendPacketHeader(header, &writer)) {
    return kNoPacket;
  }

  if (!writer.WriteBytes(fec.redundancy.data(), fec.redundancy.length())) {
    return kNoPacket;
  }

  return SerializedPacket(
      header.packet_sequence_number,
      header.public_header.sequence_number_length,
      QuicPacket::NewFecPacket(writer.take(), len, true,
                               header.public_header.guid_length,
                               header.public_header.version_flag,
                               header.public_header.sequence_number_length),
      GetPacketEntropyHash(header), NULL);
}

// static
QuicEncryptedPacket* QuicFramer::BuildPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  DCHECK(packet.public_header.reset_flag);
  size_t len = GetPublicResetPacketSize();
  QuicDataWriter writer(len);

  uint8 flags = static_cast<uint8>(PACKET_PUBLIC_FLAGS_RST |
                                   PACKET_PUBLIC_FLAGS_8BYTE_GUID |
                                   PACKET_PUBLIC_FLAGS_6BYTE_SEQUENCE);
  if (!writer.WriteUInt8(flags)) {
    return NULL;
  }

  if (!writer.WriteUInt64(packet.public_header.guid)) {
    return NULL;
  }

  if (!writer.WriteUInt64(packet.nonce_proof)) {
    return NULL;
  }

  if (!AppendPacketSequenceNumber(PACKET_6BYTE_SEQUENCE_NUMBER,
                                  packet.rejected_sequence_number,
                                  &writer)) {
    return NULL;
  }

  return new QuicEncryptedPacket(writer.take(), len, true);
}

QuicEncryptedPacket* QuicFramer::BuildVersionNegotiationPacket(
    const QuicPacketPublicHeader& header,
    const QuicVersionVector& supported_versions) {
  DCHECK(header.version_flag);
  size_t len = GetVersionNegotiationPacketSize(supported_versions.size());
  QuicDataWriter writer(len);

  uint8 flags = static_cast<uint8>(PACKET_PUBLIC_FLAGS_VERSION |
                                   PACKET_PUBLIC_FLAGS_8BYTE_GUID |
                                   PACKET_PUBLIC_FLAGS_6BYTE_SEQUENCE);
  if (!writer.WriteUInt8(flags)) {
    return NULL;
  }

  if (!writer.WriteUInt64(header.guid)) {
    return NULL;
  }

  for (size_t i = 0; i < supported_versions.size(); ++i) {
    if (!writer.WriteUInt32(QuicVersionToQuicTag(supported_versions[i]))) {
      return NULL;
    }
  }

  return new QuicEncryptedPacket(writer.take(), len, true);
}

bool QuicFramer::ProcessPacket(const QuicEncryptedPacket& packet) {
  DCHECK(!reader_.get());
  reader_.reset(new QuicDataReader(packet.data(), packet.length()));

  visitor_->OnPacket();

  // First parse the public header.
  QuicPacketPublicHeader public_header;
  if (!ProcessPublicHeader(&public_header)) {
    DLOG(WARNING) << "Unable to process public header.";
    DCHECK_NE("", detailed_error_);
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (is_server_ && public_header.version_flag &&
      public_header.versions[0] != quic_version_) {
    if (!visitor_->OnProtocolVersionMismatch(public_header.versions[0])) {
      reader_.reset(NULL);
      return true;
    }
  }

  bool rv;
  if (!is_server_ && public_header.version_flag) {
    rv = ProcessVersionNegotiationPacket(&public_header);
  } else if (public_header.reset_flag) {
    rv = ProcessPublicResetPacket(public_header);
  } else {
    rv = ProcessDataPacket(public_header, packet);
  }

  reader_.reset(NULL);
  return rv;
}

bool QuicFramer::ProcessVersionNegotiationPacket(
    QuicPacketPublicHeader* public_header) {
  DCHECK(!is_server_);
  // Try reading at least once to raise error if the packet is invalid.
  do {
    QuicTag version;
    if (!reader_->ReadBytes(&version, kQuicVersionSize)) {
      set_detailed_error("Unable to read supported version in negotiation.");
      return RaiseError(QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
    }
    public_header->versions.push_back(QuicTagToQuicVersion(version));
  } while (!reader_->IsDoneReading());

  visitor_->OnVersionNegotiationPacket(*public_header);
  return true;
}

bool QuicFramer::ProcessDataPacket(
    const QuicPacketPublicHeader& public_header,
    const QuicEncryptedPacket& packet) {
  QuicPacketHeader header(public_header);
  if (!ProcessPacketHeader(&header, packet)) {
    DLOG(WARNING) << "Unable to process data packet header.";
    return false;
  }

  if (!visitor_->OnPacketHeader(header)) {
    // The visitor suppresses further processing of the packet.
    return true;
  }

  if (packet.length() > kMaxPacketSize) {
    DLOG(WARNING) << "Packet too large: " << packet.length();
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  // Handle the payload.
  if (!header.fec_flag) {
    if (header.is_in_fec_group == IN_FEC_GROUP) {
      StringPiece payload = reader_->PeekRemainingPayload();
      visitor_->OnFecProtectedPayload(payload);
    }
    if (!ProcessFrameData(header)) {
      DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
      DLOG(WARNING) << "Unable to process frame data.";
      return false;
    }
  } else {
    QuicFecData fec_data;
    fec_data.fec_group = header.fec_group;
    fec_data.redundancy = reader_->ReadRemainingPayload();
    visitor_->OnFecData(fec_data);
  }

  visitor_->OnPacketComplete();
  return true;
}

bool QuicFramer::ProcessPublicResetPacket(
    const QuicPacketPublicHeader& public_header) {
  QuicPublicResetPacket packet(public_header);
  if (!reader_->ReadUInt64(&packet.nonce_proof)) {
    set_detailed_error("Unable to read nonce proof.");
    return RaiseError(QUIC_INVALID_PUBLIC_RST_PACKET);
  }
  // TODO(satyamshekhar): validate nonce to protect against DoS.

  if (!reader_->ReadUInt48(&packet.rejected_sequence_number)) {
    set_detailed_error("Unable to read rejected sequence number.");
    return RaiseError(QUIC_INVALID_PUBLIC_RST_PACKET);
  }
  visitor_->OnPublicResetPacket(packet);
  return true;
}

bool QuicFramer::ProcessRevivedPacket(QuicPacketHeader* header,
                                      StringPiece payload) {
  DCHECK(!reader_.get());

  visitor_->OnRevivedPacket();

  header->entropy_hash = GetPacketEntropyHash(*header);

  if (!visitor_->OnPacketHeader(*header)) {
    return true;
  }

  if (payload.length() > kMaxPacketSize) {
    set_detailed_error("Revived packet too large.");
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  reader_.reset(new QuicDataReader(payload.data(), payload.length()));
  if (!ProcessFrameData(*header)) {
    DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
    DLOG(WARNING) << "Unable to process frame data.";
    return false;
  }

  visitor_->OnPacketComplete();
  reader_.reset(NULL);
  return true;
}

bool QuicFramer::AppendPacketHeader(const QuicPacketHeader& header,
                                    QuicDataWriter* writer) {
  DCHECK(header.fec_group > 0 || header.is_in_fec_group == NOT_IN_FEC_GROUP);
  uint8 public_flags = 0;
  if (header.public_header.reset_flag) {
    public_flags |= PACKET_PUBLIC_FLAGS_RST;
  }
  if (header.public_header.version_flag) {
    public_flags |= PACKET_PUBLIC_FLAGS_VERSION;
  }

  public_flags |=
      GetSequenceNumberFlags(header.public_header.sequence_number_length)
          << kPublicHeaderSequenceNumberShift;

  switch (header.public_header.guid_length) {
    case PACKET_0BYTE_GUID:
      if (!writer->WriteUInt8(public_flags | PACKET_PUBLIC_FLAGS_0BYTE_GUID)) {
        return false;
      }
      break;
    case PACKET_1BYTE_GUID:
      if (!writer->WriteUInt8(public_flags | PACKET_PUBLIC_FLAGS_1BYTE_GUID)) {
         return false;
      }
      if (!writer->WriteUInt8(header.public_header.guid & k1ByteGuidMask)) {
        return false;
      }
      break;
    case PACKET_4BYTE_GUID:
      if (!writer->WriteUInt8(public_flags | PACKET_PUBLIC_FLAGS_4BYTE_GUID)) {
         return false;
      }
      if (!writer->WriteUInt32(header.public_header.guid & k4ByteGuidMask)) {
        return false;
      }
      break;
    case PACKET_8BYTE_GUID:
      if (!writer->WriteUInt8(public_flags | PACKET_PUBLIC_FLAGS_8BYTE_GUID)) {
        return false;
      }
      if (!writer->WriteUInt64(header.public_header.guid)) {
        return false;
      }
      break;
  }
  last_serialized_guid_ = header.public_header.guid;

  if (header.public_header.version_flag) {
    DCHECK(!is_server_);
    writer->WriteUInt32(QuicVersionToQuicTag(quic_version_));
  }

  if (!AppendPacketSequenceNumber(header.public_header.sequence_number_length,
                                  header.packet_sequence_number, writer)) {
    return false;
  }

  uint8 private_flags = 0;
  if (header.entropy_flag) {
    private_flags |= PACKET_PRIVATE_FLAGS_ENTROPY;
  }
  if (header.is_in_fec_group == IN_FEC_GROUP) {
    private_flags |= PACKET_PRIVATE_FLAGS_FEC_GROUP;
  }
  if (header.fec_flag) {
    private_flags |= PACKET_PRIVATE_FLAGS_FEC;
  }
  if (!writer->WriteUInt8(private_flags)) {
    return false;
  }

  // The FEC group number is the sequence number of the first fec
  // protected packet, or 0 if this packet is not protected.
  if (header.is_in_fec_group == IN_FEC_GROUP) {
    DCHECK_GE(header.packet_sequence_number, header.fec_group);
    DCHECK_GT(255u, header.packet_sequence_number - header.fec_group);
    // Offset from the current packet sequence number to the first fec
    // protected packet.
    uint8 first_fec_protected_packet_offset =
        header.packet_sequence_number - header.fec_group;
    if (!writer->WriteBytes(&first_fec_protected_packet_offset, 1)) {
      return false;
    }
  }

  return true;
}

QuicPacketSequenceNumber QuicFramer::CalculatePacketSequenceNumberFromWire(
    QuicSequenceNumberLength sequence_number_length,
    QuicPacketSequenceNumber packet_sequence_number) const {
  // The new sequence number might have wrapped to the next epoch, or
  // it might have reverse wrapped to the previous epoch, or it might
  // remain in the same epoch.  Select the sequence number closest to the
  // next expected sequence number, the previous sequence number plus 1.

  // epoch_delta is the delta between epochs the sequence number was serialized
  // with, so the correct value is likely the same epoch as the last sequence
  // number or an adjacent epoch.
  const QuicPacketSequenceNumber epoch_delta =
      GG_UINT64_C(1) << (8 * sequence_number_length);
  QuicPacketSequenceNumber next_sequence_number = last_sequence_number_ + 1;
  QuicPacketSequenceNumber epoch = last_sequence_number_ & ~(epoch_delta - 1);
  QuicPacketSequenceNumber prev_epoch = epoch - epoch_delta;
  QuicPacketSequenceNumber next_epoch = epoch + epoch_delta;

  return ClosestTo(next_sequence_number,
                   epoch + packet_sequence_number,
                   ClosestTo(next_sequence_number,
                             prev_epoch + packet_sequence_number,
                             next_epoch + packet_sequence_number));
}

bool QuicFramer::ProcessPublicHeader(
    QuicPacketPublicHeader* public_header) {
  uint8 public_flags;
  if (!reader_->ReadBytes(&public_flags, 1)) {
    set_detailed_error("Unable to read public flags.");
    return false;
  }

  public_header->reset_flag = (public_flags & PACKET_PUBLIC_FLAGS_RST) != 0;
  public_header->version_flag =
      (public_flags & PACKET_PUBLIC_FLAGS_VERSION) != 0;

  if (!public_header->version_flag && public_flags > PACKET_PUBLIC_FLAGS_MAX) {
    set_detailed_error("Illegal public flags value.");
    return false;
  }

  if (public_header->reset_flag && public_header->version_flag) {
    set_detailed_error("Got version flag in reset packet");
    return false;
  }

  switch (public_flags & PACKET_PUBLIC_FLAGS_8BYTE_GUID) {
    case PACKET_PUBLIC_FLAGS_8BYTE_GUID:
      if (!reader_->ReadUInt64(&public_header->guid)) {
        set_detailed_error("Unable to read GUID.");
        return false;
      }
      public_header->guid_length = PACKET_8BYTE_GUID;
      break;
    case PACKET_PUBLIC_FLAGS_4BYTE_GUID:
      // If the guid is truncated, expect to read the last serialized guid.
      if (!reader_->ReadBytes(&public_header->guid, PACKET_4BYTE_GUID)) {
        set_detailed_error("Unable to read GUID.");
        return false;
      }
      if ((public_header->guid & k4ByteGuidMask) !=
          (last_serialized_guid_ & k4ByteGuidMask)) {
        set_detailed_error(
            "Truncated 4 byte GUID does not match previous guid.");
        return false;
      }
      public_header->guid_length = PACKET_4BYTE_GUID;
      public_header->guid = last_serialized_guid_;
      break;
    case PACKET_PUBLIC_FLAGS_1BYTE_GUID:
      if (!reader_->ReadBytes(&public_header->guid, PACKET_1BYTE_GUID)) {
        set_detailed_error("Unable to read GUID.");
        return false;
      }
      if ((public_header->guid & k1ByteGuidMask) !=
          (last_serialized_guid_ & k1ByteGuidMask)) {
        set_detailed_error(
            "Truncated 1 byte GUID does not match previous guid.");
        return false;
      }
      public_header->guid_length = PACKET_1BYTE_GUID;
      public_header->guid = last_serialized_guid_;
      break;
    case PACKET_PUBLIC_FLAGS_0BYTE_GUID:
      public_header->guid_length = PACKET_0BYTE_GUID;
      public_header->guid = last_serialized_guid_;
      break;
  }

  public_header->sequence_number_length =
      ReadSequenceNumberLength(
          public_flags >> kPublicHeaderSequenceNumberShift);

  // Read the version only if the packet is from the client.
  // version flag from the server means version negotiation packet.
  if (public_header->version_flag && is_server_) {
    QuicTag version_tag;
    if (!reader_->ReadUInt32(&version_tag)) {
      set_detailed_error("Unable to read protocol version.");
      return false;
    }

    // If the version from the new packet is the same as the version of this
    // framer, then the public flags should be set to something we understand.
    // If not, this raises an error.
    QuicVersion version = QuicTagToQuicVersion(version_tag);
    if (version == quic_version_ && public_flags > PACKET_PUBLIC_FLAGS_MAX) {
      set_detailed_error("Illegal public flags value.");
      return false;
    }
    public_header->versions.push_back(version);
  }
  return true;
}

// static
bool QuicFramer::ReadGuidFromPacket(const QuicEncryptedPacket& packet,
                                    QuicGuid* guid) {
  QuicDataReader reader(packet.data(), packet.length());
  uint8 public_flags;
  if (!reader.ReadBytes(&public_flags, 1)) {
    return false;
  }
  // Ensure it's an 8 byte guid.
  if ((public_flags & PACKET_PUBLIC_FLAGS_8BYTE_GUID) !=
          PACKET_PUBLIC_FLAGS_8BYTE_GUID) {
    return false;
  }

  return reader.ReadUInt64(guid);
}

// static
QuicSequenceNumberLength QuicFramer::ReadSequenceNumberLength(uint8 flags) {
  switch (flags & PACKET_FLAGS_6BYTE_SEQUENCE) {
    case PACKET_FLAGS_6BYTE_SEQUENCE:
      return PACKET_6BYTE_SEQUENCE_NUMBER;
    case PACKET_FLAGS_4BYTE_SEQUENCE:
      return PACKET_4BYTE_SEQUENCE_NUMBER;
    case PACKET_FLAGS_2BYTE_SEQUENCE:
      return PACKET_2BYTE_SEQUENCE_NUMBER;
    case PACKET_FLAGS_1BYTE_SEQUENCE:
      return PACKET_1BYTE_SEQUENCE_NUMBER;
    default:
      LOG(DFATAL) << "Unreachable case statement.";
      return PACKET_6BYTE_SEQUENCE_NUMBER;
  }
}

// static
QuicSequenceNumberLength QuicFramer::GetMinSequenceNumberLength(
    QuicPacketSequenceNumber sequence_number) {
  if (sequence_number < 1 << (PACKET_1BYTE_SEQUENCE_NUMBER * 8)) {
    return PACKET_1BYTE_SEQUENCE_NUMBER;
  } else if (sequence_number < 1 << (PACKET_2BYTE_SEQUENCE_NUMBER * 8)) {
    return PACKET_2BYTE_SEQUENCE_NUMBER;
  } else if (sequence_number <
             GG_UINT64_C(1) << (PACKET_4BYTE_SEQUENCE_NUMBER * 8)) {
    return PACKET_4BYTE_SEQUENCE_NUMBER;
  } else {
    return PACKET_6BYTE_SEQUENCE_NUMBER;
  }
}

// static
uint8 QuicFramer::GetSequenceNumberFlags(
    QuicSequenceNumberLength sequence_number_length) {
  switch (sequence_number_length) {
    case PACKET_1BYTE_SEQUENCE_NUMBER:
      return PACKET_FLAGS_1BYTE_SEQUENCE;
    case PACKET_2BYTE_SEQUENCE_NUMBER:
      return PACKET_FLAGS_2BYTE_SEQUENCE;
    case PACKET_4BYTE_SEQUENCE_NUMBER:
      return PACKET_FLAGS_4BYTE_SEQUENCE;
    case PACKET_6BYTE_SEQUENCE_NUMBER:
      return PACKET_FLAGS_6BYTE_SEQUENCE;
    default:
      LOG(DFATAL) << "Unreachable case statement.";
      return PACKET_FLAGS_6BYTE_SEQUENCE;
  }
}

// static
QuicFramer::AckFrameInfo QuicFramer::GetAckFrameInfo(
    const QuicAckFrame& frame) {
  const ReceivedPacketInfo& received_info = frame.received_info;

  AckFrameInfo ack_info;
  if (!received_info.missing_packets.empty()) {
    DCHECK_GE(received_info.largest_observed,
              *received_info.missing_packets.rbegin());
    size_t cur_range_length = 0;
    SequenceNumberSet::const_iterator iter =
        received_info.missing_packets.begin();
    QuicPacketSequenceNumber last_missing = *iter;
    ++iter;
    for (; iter != received_info.missing_packets.end(); ++iter) {
      if (cur_range_length != numeric_limits<uint8>::max() &&
          *iter == (last_missing + 1)) {
        ++cur_range_length;
      } else {
        ack_info.nack_ranges[last_missing - cur_range_length]
            = cur_range_length;
        cur_range_length = 0;
      }
      ack_info.max_delta = max(ack_info.max_delta, *iter - last_missing);
      last_missing = *iter;
    }
    // Include the last nack range.
    ack_info.nack_ranges[last_missing - cur_range_length] = cur_range_length;
    // Include the range to the largest observed.
    ack_info.max_delta = max(ack_info.max_delta,
                             received_info.largest_observed - last_missing);
  }
  return ack_info;
}

bool QuicFramer::ProcessPacketHeader(
    QuicPacketHeader* header,
    const QuicEncryptedPacket& packet) {
  if (!ProcessPacketSequenceNumber(header->public_header.sequence_number_length,
                                   &header->packet_sequence_number)) {
    set_detailed_error("Unable to read sequence number.");
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (header->packet_sequence_number == 0u) {
    set_detailed_error("Packet sequence numbers cannot be 0.");
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (!visitor_->OnUnauthenticatedHeader(*header)) {
    return false;
  }

  if (!DecryptPayload(*header, packet)) {
    set_detailed_error("Unable to decrypt payload.");
    return RaiseError(QUIC_DECRYPTION_FAILURE);
  }

  uint8 private_flags;
  if (!reader_->ReadBytes(&private_flags, 1)) {
    set_detailed_error("Unable to read private flags.");
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (private_flags > PACKET_PRIVATE_FLAGS_MAX) {
    set_detailed_error("Illegal private flags value.");
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  header->entropy_flag = (private_flags & PACKET_PRIVATE_FLAGS_ENTROPY) != 0;
  header->fec_flag = (private_flags & PACKET_PRIVATE_FLAGS_FEC) != 0;

  if ((private_flags & PACKET_PRIVATE_FLAGS_FEC_GROUP) != 0) {
    header->is_in_fec_group = IN_FEC_GROUP;
    uint8 first_fec_protected_packet_offset;
    if (!reader_->ReadBytes(&first_fec_protected_packet_offset, 1)) {
      set_detailed_error("Unable to read first fec protected packet offset.");
      return RaiseError(QUIC_INVALID_PACKET_HEADER);
    }
    if (first_fec_protected_packet_offset >= header->packet_sequence_number) {
      set_detailed_error("First fec protected packet offset must be less "
                         "than the sequence number.");
      return RaiseError(QUIC_INVALID_PACKET_HEADER);
    }
    header->fec_group =
        header->packet_sequence_number - first_fec_protected_packet_offset;
  }

  header->entropy_hash = GetPacketEntropyHash(*header);
  // Set the last sequence number after we have decrypted the packet
  // so we are confident is not attacker controlled.
  last_sequence_number_ = header->packet_sequence_number;
  return true;
}

bool QuicFramer::ProcessPacketSequenceNumber(
    QuicSequenceNumberLength sequence_number_length,
    QuicPacketSequenceNumber* sequence_number) {
  QuicPacketSequenceNumber wire_sequence_number = 0u;
  if (!reader_->ReadBytes(&wire_sequence_number, sequence_number_length)) {
    return false;
  }

  // TODO(ianswett): Explore the usefulness of trying multiple sequence numbers
  // in case the first guess is incorrect.
  *sequence_number =
      CalculatePacketSequenceNumberFromWire(sequence_number_length,
                                            wire_sequence_number);
  return true;
}

bool QuicFramer::ProcessFrameData(const QuicPacketHeader& header) {
  if (reader_->IsDoneReading()) {
    set_detailed_error("Packet has no frames.");
    return RaiseError(QUIC_MISSING_PAYLOAD);
  }
  while (!reader_->IsDoneReading()) {
    uint8 frame_type;
    if (!reader_->ReadBytes(&frame_type, 1)) {
      set_detailed_error("Unable to read frame type.");
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }

    if (frame_type & kQuicFrameTypeSpecialMask) {
      // Stream Frame
      if (frame_type & kQuicFrameTypeStreamMask) {
        QuicStreamFrame frame;
        if (!ProcessStreamFrame(frame_type, &frame)) {
          return RaiseError(QUIC_INVALID_STREAM_DATA);
        }
        if (!visitor_->OnStreamFrame(frame)) {
          DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      // Ack Frame
      if (frame_type & kQuicFrameTypeAckMask) {
        QuicAckFrame frame;
        if (!ProcessAckFrame(header, frame_type, &frame)) {
          return RaiseError(QUIC_INVALID_ACK_DATA);
        }
        if (!visitor_->OnAckFrame(frame)) {
          DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      // Congestion Feedback Frame
      if (frame_type & kQuicFrameTypeCongestionFeedbackMask) {
        QuicCongestionFeedbackFrame frame;
        if (!ProcessQuicCongestionFeedbackFrame(&frame)) {
          return RaiseError(QUIC_INVALID_CONGESTION_FEEDBACK_DATA);
        }
        if (!visitor_->OnCongestionFeedbackFrame(frame)) {
          DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      // This was a special frame type that did not match any
      // of the known ones. Error.
      set_detailed_error("Illegal frame type.");
      DLOG(WARNING) << "Illegal frame type: "
                    << static_cast<int>(frame_type);
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }

    switch (frame_type) {
      case PADDING_FRAME:
        // We're done with the packet.
        return true;

      case RST_STREAM_FRAME: {
        QuicRstStreamFrame frame;
        if (!ProcessRstStreamFrame(&frame)) {
          return RaiseError(QUIC_INVALID_RST_STREAM_DATA);
        }
        if (!visitor_->OnRstStreamFrame(frame)) {
          DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case CONNECTION_CLOSE_FRAME: {
        QuicConnectionCloseFrame frame;
        if (!ProcessConnectionCloseFrame(&frame)) {
          return RaiseError(QUIC_INVALID_CONNECTION_CLOSE_DATA);
        }

        if (!visitor_->OnConnectionCloseFrame(frame)) {
          DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case GOAWAY_FRAME: {
        QuicGoAwayFrame goaway_frame;
        if (!ProcessGoAwayFrame(&goaway_frame)) {
          return RaiseError(QUIC_INVALID_GOAWAY_DATA);
        }
        if (!visitor_->OnGoAwayFrame(goaway_frame)) {
          DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      default:
        set_detailed_error("Illegal frame type.");
        DLOG(WARNING) << "Illegal frame type: "
                      << static_cast<int>(frame_type);
        return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
  }

  return true;
}

bool QuicFramer::ProcessStreamFrame(uint8 frame_type,
                                    QuicStreamFrame* frame) {
  uint8 stream_flags = frame_type;

  stream_flags &= ~kQuicFrameTypeStreamMask;

  // Read from right to left: StreamID, Offset, Data Length, Fin.
  const uint8 stream_id_length = (stream_flags & kQuicStreamIDLengthMask) + 1;
  stream_flags >>= kQuicStreamIdShift;

  uint8 offset_length = (stream_flags & kQuicStreamOffsetMask);
  // There is no encoding for 1 byte, only 0 and 2 through 8.
  if (offset_length > 0) {
    offset_length += 1;
  }
  stream_flags >>= kQuicStreamOffsetShift;

  bool has_data_length =
      (stream_flags & kQuicStreamDataLengthMask) == kQuicStreamDataLengthMask;
  stream_flags >>= kQuicStreamDataLengthShift;

  frame->fin = (stream_flags & kQuicStreamFinMask) == kQuicStreamFinShift;

  frame->stream_id = 0;
  if (!reader_->ReadBytes(&frame->stream_id, stream_id_length)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  frame->offset = 0;
  if (!reader_->ReadBytes(&frame->offset, offset_length)) {
    set_detailed_error("Unable to read offset.");
    return false;
  }

  StringPiece frame_data;
  if (has_data_length) {
    if (!reader_->ReadStringPiece16(&frame_data)) {
      set_detailed_error("Unable to read frame data.");
      return false;
    }
  } else {
    if (!reader_->ReadStringPiece(&frame_data, reader_->BytesRemaining())) {
      set_detailed_error("Unable to read frame data.");
      return false;
    }
  }
  // Point frame to the right data.
  frame->data.Clear();
  if (!frame_data.empty()) {
    frame->data.Append(const_cast<char*>(frame_data.data()), frame_data.size());
  }

  return true;
}

bool QuicFramer::ProcessAckFrame(const QuicPacketHeader& header,
                                 uint8 frame_type,
                                 QuicAckFrame* frame) {
  if (!ProcessSentInfo(header, &frame->sent_info)) {
    return false;
  }
  if (!ProcessReceivedInfo(frame_type, &frame->received_info)) {
    return false;
  }
  return true;
}

bool QuicFramer::ProcessReceivedInfo(uint8 frame_type,
                                     ReceivedPacketInfo* received_info) {
  // Determine the three lengths from the frame type: largest observed length,
  // missing sequence number length, and missing range length.
  const QuicSequenceNumberLength missing_sequence_number_length =
      ReadSequenceNumberLength(frame_type);
  frame_type >>= kQuicSequenceNumberLengthShift;
  const QuicSequenceNumberLength largest_observed_sequence_number_length =
      ReadSequenceNumberLength(frame_type);
  frame_type >>= kQuicSequenceNumberLengthShift;
  received_info->is_truncated = frame_type & kQuicAckTruncatedMask;
  frame_type >>= kQuicAckTruncatedShift;
  bool has_nacks = frame_type & kQuicHasNacksMask;

  if (!reader_->ReadBytes(&received_info->entropy_hash, 1)) {
    set_detailed_error("Unable to read entropy hash for received packets.");
    return false;
  }

  if (!reader_->ReadBytes(&received_info->largest_observed,
                          largest_observed_sequence_number_length)) {
    set_detailed_error("Unable to read largest observed.");
    return false;
  }

  uint64 delta_time_largest_observed_us;
  if (!reader_->ReadUFloat16(&delta_time_largest_observed_us)) {
    set_detailed_error("Unable to read delta time largest observed.");
    return false;
  }

  if (delta_time_largest_observed_us == kUFloat16MaxValue) {
    received_info->delta_time_largest_observed = QuicTime::Delta::Infinite();
  } else {
    received_info->delta_time_largest_observed =
        QuicTime::Delta::FromMicroseconds(delta_time_largest_observed_us);
  }

  if (!has_nacks) {
    return true;
  }

  uint8 num_missing_ranges;
  if (!reader_->ReadBytes(&num_missing_ranges, 1)) {
    set_detailed_error("Unable to read num missing packet ranges.");
    return false;
  }

  QuicPacketSequenceNumber last_sequence_number =
      received_info->largest_observed;
  for (size_t i = 0; i < num_missing_ranges; ++i) {
    QuicPacketSequenceNumber missing_delta = 0;
    if (!reader_->ReadBytes(&missing_delta, missing_sequence_number_length)) {
      set_detailed_error("Unable to read missing sequence number delta.");
      return false;
    }
    last_sequence_number -= missing_delta;
    QuicPacketSequenceNumber range_length = 0;
    if (!reader_->ReadBytes(&range_length, PACKET_1BYTE_SEQUENCE_NUMBER)) {
      set_detailed_error("Unable to read missing sequence number range.");
      return false;
    }
    for (size_t i = 0; i <= range_length; ++i) {
      received_info->missing_packets.insert(last_sequence_number - i);
    }
    // Subtract an extra 1 to ensure ranges are represented efficiently and
    // can't overlap by 1 sequence number.  This allows a missing_delta of 0
    // to represent an adjacent nack range.
    last_sequence_number -= (range_length + 1);
  }

  return true;
}

bool QuicFramer::ProcessSentInfo(const QuicPacketHeader& header,
                                 SentPacketInfo* sent_info) {
  if (!reader_->ReadBytes(&sent_info->entropy_hash, 1)) {
    set_detailed_error("Unable to read entropy hash for sent packets.");
    return false;
  }

  QuicPacketSequenceNumber least_unacked_delta = 0;
  if (!reader_->ReadBytes(&least_unacked_delta,
                          header.public_header.sequence_number_length)) {
    set_detailed_error("Unable to read least unacked delta.");
    return false;
  }
  DCHECK_GE(header.packet_sequence_number, least_unacked_delta);
  sent_info->least_unacked =
      header.packet_sequence_number - least_unacked_delta;

  return true;
}

bool QuicFramer::ProcessQuicCongestionFeedbackFrame(
    QuicCongestionFeedbackFrame* frame) {
  uint8 feedback_type;
  if (!reader_->ReadBytes(&feedback_type, 1)) {
    set_detailed_error("Unable to read congestion feedback type.");
    return false;
  }
  frame->type =
      static_cast<CongestionFeedbackType>(feedback_type);

  switch (frame->type) {
    case kInterArrival: {
      CongestionFeedbackMessageInterArrival* inter_arrival =
          &frame->inter_arrival;
      if (!reader_->ReadUInt16(
              &inter_arrival->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      uint8 num_received_packets;
      if (!reader_->ReadBytes(&num_received_packets, 1)) {
        set_detailed_error("Unable to read num received packets.");
        return false;
      }

      if (num_received_packets > 0u) {
        uint64 smallest_received;
        if (!ProcessPacketSequenceNumber(PACKET_6BYTE_SEQUENCE_NUMBER,
                                         &smallest_received)) {
          set_detailed_error("Unable to read smallest received.");
          return false;
        }

        uint64 time_received_us;
        if (!reader_->ReadUInt64(&time_received_us)) {
          set_detailed_error("Unable to read time received.");
          return false;
        }
        QuicTime time_received = creation_time_.Add(
            QuicTime::Delta::FromMicroseconds(time_received_us));

        inter_arrival->received_packet_times.insert(
            make_pair(smallest_received, time_received));

        for (uint8 i = 0; i < num_received_packets - 1; ++i) {
          uint16 sequence_delta;
          if (!reader_->ReadUInt16(&sequence_delta)) {
            set_detailed_error(
                "Unable to read sequence delta in received packets.");
            return false;
          }

          int32 time_delta_us;
          if (!reader_->ReadBytes(&time_delta_us, sizeof(time_delta_us))) {
            set_detailed_error(
                "Unable to read time delta in received packets.");
            return false;
          }
          QuicPacketSequenceNumber packet = smallest_received + sequence_delta;
          inter_arrival->received_packet_times.insert(
              make_pair(packet, time_received.Add(
                  QuicTime::Delta::FromMicroseconds(time_delta_us))));
        }
      }
      break;
    }
    case kFixRate: {
      uint32 bitrate = 0;
      if (!reader_->ReadUInt32(&bitrate)) {
        set_detailed_error("Unable to read bitrate.");
        return false;
      }
      frame->fix_rate.bitrate = QuicBandwidth::FromBytesPerSecond(bitrate);
      break;
    }
    case kTCP: {
      CongestionFeedbackMessageTCP* tcp = &frame->tcp;
      if (!reader_->ReadUInt16(&tcp->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      // TODO(ianswett): Remove receive window, since it's constant.
      uint16 receive_window = 0;
      if (!reader_->ReadUInt16(&receive_window)) {
        set_detailed_error("Unable to read receive window.");
        return false;
      }
      // Simple bit packing, don't send the 4 least significant bits.
      tcp->receive_window = static_cast<QuicByteCount>(receive_window) << 4;
      break;
    }
    default:
      set_detailed_error("Illegal congestion feedback type.");
      DLOG(WARNING) << "Illegal congestion feedback type: "
                    << frame->type;
      return RaiseError(QUIC_INVALID_FRAME_DATA);
  }

  return true;
}

bool QuicFramer::ProcessRstStreamFrame(QuicRstStreamFrame* frame) {
  if (!reader_->ReadUInt32(&frame->stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read rst stream error code.");
    return false;
  }

  if (error_code >= QUIC_STREAM_LAST_ERROR ||
      error_code < QUIC_STREAM_NO_ERROR) {
    set_detailed_error("Invalid rst stream error code.");
    return false;
  }

  frame->error_code = static_cast<QuicRstStreamErrorCode>(error_code);

  StringPiece error_details;
  if (!reader_->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read rst stream error details.");
    return false;
  }
  frame->error_details = error_details.as_string();

  return true;
}

bool QuicFramer::ProcessConnectionCloseFrame(QuicConnectionCloseFrame* frame) {
  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read connection close error code.");
    return false;
  }

  if (error_code >= QUIC_LAST_ERROR ||
         error_code < QUIC_NO_ERROR) {
    set_detailed_error("Invalid error code.");
    return false;
  }

  frame->error_code = static_cast<QuicErrorCode>(error_code);

  StringPiece error_details;
  if (!reader_->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read connection close error details.");
    return false;
  }
  frame->error_details = error_details.as_string();

  return true;
}

bool QuicFramer::ProcessGoAwayFrame(QuicGoAwayFrame* frame) {
  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read go away error code.");
    return false;
  }
  frame->error_code = static_cast<QuicErrorCode>(error_code);

  if (error_code >= QUIC_LAST_ERROR ||
      error_code < QUIC_NO_ERROR) {
    set_detailed_error("Invalid error code.");
    return false;
  }

  uint32 stream_id;
  if (!reader_->ReadUInt32(&stream_id)) {
    set_detailed_error("Unable to read last good stream id.");
    return false;
  }
  frame->last_good_stream_id = static_cast<QuicStreamId>(stream_id);

  StringPiece reason_phrase;
  if (!reader_->ReadStringPiece16(&reason_phrase)) {
    set_detailed_error("Unable to read goaway reason.");
    return false;
  }
  frame->reason_phrase = reason_phrase.as_string();

  return true;
}

// static
StringPiece QuicFramer::GetAssociatedDataFromEncryptedPacket(
    const QuicEncryptedPacket& encrypted,
    QuicGuidLength guid_length,
    bool includes_version,
    QuicSequenceNumberLength sequence_number_length) {
  return StringPiece(encrypted.data() + kStartOfHashData,
                     GetStartOfEncryptedData(
                         guid_length, includes_version, sequence_number_length)
                     - kStartOfHashData);
}

void QuicFramer::SetDecrypter(QuicDecrypter* decrypter) {
  DCHECK(alternative_decrypter_.get() == NULL);
  decrypter_.reset(decrypter);
}

void QuicFramer::SetAlternativeDecrypter(QuicDecrypter* decrypter,
                                         bool latch_once_used) {
  alternative_decrypter_.reset(decrypter);
  alternative_decrypter_latch_ = latch_once_used;
}

const QuicDecrypter* QuicFramer::decrypter() const {
  return decrypter_.get();
}

const QuicDecrypter* QuicFramer::alternative_decrypter() const {
  return alternative_decrypter_.get();
}

void QuicFramer::SetEncrypter(EncryptionLevel level,
                              QuicEncrypter* encrypter) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, NUM_ENCRYPTION_LEVELS);
  encrypter_[level].reset(encrypter);
}

const QuicEncrypter* QuicFramer::encrypter(EncryptionLevel level) const {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, NUM_ENCRYPTION_LEVELS);
  DCHECK(encrypter_[level].get() != NULL);
  return encrypter_[level].get();
}

void QuicFramer::SwapCryptersForTest(QuicFramer* other) {
  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; i++) {
    encrypter_[i].swap(other->encrypter_[i]);
  }
  decrypter_.swap(other->decrypter_);
  alternative_decrypter_.swap(other->alternative_decrypter_);

  const bool other_latch = other->alternative_decrypter_latch_;
  other->alternative_decrypter_latch_ = alternative_decrypter_latch_;
  alternative_decrypter_latch_ = other_latch;
}

QuicEncryptedPacket* QuicFramer::EncryptPacket(
    EncryptionLevel level,
    QuicPacketSequenceNumber packet_sequence_number,
    const QuicPacket& packet) {
  DCHECK(encrypter_[level].get() != NULL);

  scoped_ptr<QuicData> out(encrypter_[level]->EncryptPacket(
      packet_sequence_number, packet.AssociatedData(), packet.Plaintext()));
  if (out.get() == NULL) {
    RaiseError(QUIC_ENCRYPTION_FAILURE);
    return NULL;
  }
  StringPiece header_data = packet.BeforePlaintext();
  size_t len =  header_data.length() + out->length();
  char* buffer = new char[len];
  // TODO(rch): eliminate this buffer copy by passing in a buffer to Encrypt().
  memcpy(buffer, header_data.data(), header_data.length());
  memcpy(buffer + header_data.length(), out->data(), out->length());
  return new QuicEncryptedPacket(buffer, len, true);
}

size_t QuicFramer::GetMaxPlaintextSize(size_t ciphertext_size) {
  // In order to keep the code simple, we don't have the current encryption
  // level to hand. Both the NullEncrypter and AES-GCM have a tag length of 12.
  size_t min_plaintext_size = ciphertext_size;

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; i++) {
    if (encrypter_[i].get() != NULL) {
      size_t size = encrypter_[i]->GetMaxPlaintextSize(ciphertext_size);
      if (size < min_plaintext_size) {
        min_plaintext_size = size;
      }
    }
  }

  return min_plaintext_size;
}

bool QuicFramer::DecryptPayload(const QuicPacketHeader& header,
                                const QuicEncryptedPacket& packet) {
  StringPiece encrypted;
  if (!reader_->ReadStringPiece(&encrypted, reader_->BytesRemaining())) {
    return false;
  }
  DCHECK(decrypter_.get() != NULL);
  decrypted_.reset(decrypter_->DecryptPacket(
      header.packet_sequence_number,
      GetAssociatedDataFromEncryptedPacket(
          packet,
          header.public_header.guid_length,
          header.public_header.version_flag,
          header.public_header.sequence_number_length),
      encrypted));
  if  (decrypted_.get() == NULL && alternative_decrypter_.get() != NULL) {
    decrypted_.reset(alternative_decrypter_->DecryptPacket(
        header.packet_sequence_number,
        GetAssociatedDataFromEncryptedPacket(
            packet,
            header.public_header.guid_length,
            header.public_header.version_flag,
            header.public_header.sequence_number_length),
        encrypted));
    if (decrypted_.get() != NULL) {
      if (alternative_decrypter_latch_) {
        // Switch to the alternative decrypter and latch so that we cannot
        // switch back.
        decrypter_.reset(alternative_decrypter_.release());
      } else {
        // Switch the alternative decrypter so that we use it first next time.
        decrypter_.swap(alternative_decrypter_);
      }
    }
  }

  if  (decrypted_.get() == NULL) {
    return false;
  }

  reader_.reset(new QuicDataReader(decrypted_->data(), decrypted_->length()));
  return true;
}

size_t QuicFramer::GetAckFrameSize(
    const QuicAckFrame& ack,
    QuicSequenceNumberLength sequence_number_length) {
  AckFrameInfo ack_info = GetAckFrameInfo(ack);
  QuicSequenceNumberLength largest_observed_length =
      GetMinSequenceNumberLength(ack.received_info.largest_observed);
  QuicSequenceNumberLength missing_sequence_number_length =
      GetMinSequenceNumberLength(ack_info.max_delta);

  return GetMinAckFrameSize(quic_version_,
                            sequence_number_length,
                            largest_observed_length) +
      (ack_info.nack_ranges.empty() ? 0 : kNumberOfMissingPacketsSize) +
       ack_info.nack_ranges.size() *
           (missing_sequence_number_length + PACKET_1BYTE_SEQUENCE_NUMBER);
}

size_t QuicFramer::ComputeFrameLength(
    const QuicFrame& frame,
    bool last_frame_in_packet,
    QuicSequenceNumberLength sequence_number_length) {
  switch (frame.type) {
    case STREAM_FRAME:
      return GetMinStreamFrameSize(quic_version_,
                                   frame.stream_frame->stream_id,
                                   frame.stream_frame->offset,
                                   last_frame_in_packet) +
          frame.stream_frame->data.TotalBufferSize();
    case ACK_FRAME: {
      return GetAckFrameSize(*frame.ack_frame, sequence_number_length);
    }
    case CONGESTION_FEEDBACK_FRAME: {
      size_t len = kQuicFrameTypeSize;
      const QuicCongestionFeedbackFrame& congestion_feedback =
          *frame.congestion_feedback_frame;
      len += 1;  // Congestion feedback type.

      switch (congestion_feedback.type) {
        case kInterArrival: {
          const CongestionFeedbackMessageInterArrival& inter_arrival =
              congestion_feedback.inter_arrival;
          len += 2;
          len += 1;  // Number received packets.
          if (inter_arrival.received_packet_times.size() > 0) {
            len += PACKET_6BYTE_SEQUENCE_NUMBER;  // Smallest received.
            len += 8;  // Time.
            // 2 bytes per sequence number delta plus 4 bytes per delta time.
            len += PACKET_6BYTE_SEQUENCE_NUMBER *
                (inter_arrival.received_packet_times.size() - 1);
          }
          break;
        }
        case kFixRate:
          len += 4;
          break;
        case kTCP:
          len += 4;
          break;
        default:
          set_detailed_error("Illegal feedback type.");
          DVLOG(1) << "Illegal feedback type: " << congestion_feedback.type;
          break;
      }
      return len;
    }
    case RST_STREAM_FRAME:
      return GetMinRstStreamFrameSize() +
          frame.rst_stream_frame->error_details.size();
    case CONNECTION_CLOSE_FRAME:
      return GetMinConnectionCloseFrameSize() +
          frame.connection_close_frame->error_details.size();
    case GOAWAY_FRAME:
      return GetMinGoAwayFrameSize() + frame.goaway_frame->reason_phrase.size();
    case PADDING_FRAME:
      DCHECK(false);
      return 0;
    case NUM_FRAME_TYPES:
      DCHECK(false);
      return 0;
  }

  // Not reachable, but some Chrome compilers can't figure that out.  *sigh*
  DCHECK(false);
  return 0;
}

bool QuicFramer::AppendTypeByte(const QuicFrame& frame,
                                bool last_frame_in_packet,
                                QuicDataWriter* writer) {
  uint8 type_byte = 0;
  switch (frame.type) {
    case STREAM_FRAME: {
      if (frame.stream_frame == NULL) {
        LOG(DFATAL) << "Failed to append STREAM frame with no stream_frame.";
      }
      // Fin bit.
      type_byte |= frame.stream_frame->fin ? kQuicStreamFinMask : 0;

      // Data Length bit.
      type_byte <<= kQuicStreamDataLengthShift;
      type_byte |= last_frame_in_packet ? 0 : kQuicStreamDataLengthMask;

      // Offset 3 bits.
      type_byte <<= kQuicStreamOffsetShift;
      const size_t offset_len = GetStreamOffsetSize(frame.stream_frame->offset);
      if (offset_len > 0) {
        type_byte |= offset_len - 1;
      }

      // stream id 2 bits.
      type_byte <<= kQuicStreamIdShift;
      type_byte |= GetStreamIdSize(frame.stream_frame->stream_id) - 1;
      type_byte |= kQuicFrameTypeStreamMask;  // Set Stream Frame Type to 1.
      break;
    }
    case ACK_FRAME:
      return true;
    case CONGESTION_FEEDBACK_FRAME: {
      // TODO(ianswett): Use extra 5 bits in the congestion feedback framing.
      type_byte = kQuicFrameTypeCongestionFeedbackMask;
      break;
    }
    default:
      type_byte = frame.type;
      break;
  }

  return writer->WriteUInt8(type_byte);
}

// static
bool QuicFramer::AppendPacketSequenceNumber(
    QuicSequenceNumberLength sequence_number_length,
    QuicPacketSequenceNumber packet_sequence_number,
    QuicDataWriter* writer) {
  // Ensure the entire sequence number can be written.
  if (writer->capacity() - writer->length() <
      static_cast<size_t>(sequence_number_length)) {
    return false;
  }
  switch (sequence_number_length) {
    case PACKET_1BYTE_SEQUENCE_NUMBER:
      return writer->WriteUInt8(
          packet_sequence_number & k1ByteSequenceNumberMask);
      break;
    case PACKET_2BYTE_SEQUENCE_NUMBER:
      return writer->WriteUInt16(
          packet_sequence_number & k2ByteSequenceNumberMask);
      break;
    case PACKET_4BYTE_SEQUENCE_NUMBER:
      return writer->WriteUInt32(
          packet_sequence_number & k4ByteSequenceNumberMask);
      break;
    case PACKET_6BYTE_SEQUENCE_NUMBER:
      return writer->WriteUInt48(
          packet_sequence_number & k6ByteSequenceNumberMask);
      break;
    default:
      NOTREACHED() << "sequence_number_length: " << sequence_number_length;
      return false;
  }
}

bool QuicFramer::AppendStreamFramePayload(
    const QuicStreamFrame& frame,
    bool last_frame_in_packet,
    QuicDataWriter* writer) {
  if (!writer->WriteBytes(&frame.stream_id, GetStreamIdSize(frame.stream_id))) {
    return false;
  }
  if (!writer->WriteBytes(&frame.offset, GetStreamOffsetSize(frame.offset))) {
    return false;
  }
  if (!last_frame_in_packet) {
    if (!writer->WriteUInt16(frame.data.TotalBufferSize())) {
      return false;
    }
  }

  if (!writer->WriteIOVector(frame.data)) {
    return false;
  }
  return true;
}

// static
bool QuicFramer::HasVersionFlag(const QuicEncryptedPacket& packet) {
  return packet.length() > 0 &&
      (packet.data()[0] & PACKET_PUBLIC_FLAGS_VERSION) != 0;
}

// static
QuicPacketSequenceNumber QuicFramer::CalculateLargestObserved(
    const SequenceNumberSet& missing_packets,
    SequenceNumberSet::const_iterator largest_written) {
  SequenceNumberSet::const_iterator it = largest_written;
  QuicPacketSequenceNumber previous_missing = *it;
  ++it;

  // See if the next thing is a gap in the missing packets: if it's a
  // non-missing packet we can return it.
  if (it != missing_packets.end() && previous_missing + 1 != *it) {
    return *it - 1;
  }

  // Otherwise return the largest missing packet, as indirectly observed.
  return *largest_written;
}

void QuicFramer::set_version(const QuicVersion version) {
  DCHECK(IsSupportedVersion(version));
  quic_version_ = version;
}

bool QuicFramer::AppendAckFramePayloadAndTypeByte(
    const QuicPacketHeader& header,
    const QuicAckFrame& frame,
    QuicDataWriter* writer) {
  AckFrameInfo ack_info = GetAckFrameInfo(frame);
  QuicPacketSequenceNumber ack_largest_observed =
      frame.received_info.largest_observed;
  QuicSequenceNumberLength largest_observed_length =
      GetMinSequenceNumberLength(ack_largest_observed);
  QuicSequenceNumberLength missing_sequence_number_length =
      GetMinSequenceNumberLength(ack_info.max_delta);
  // Determine whether we need to truncate ranges.
  size_t available_range_bytes = writer->capacity() - writer->length() -
      GetMinAckFrameSize(quic_version_,
                         header.public_header.sequence_number_length,
                         largest_observed_length);
  size_t max_num_ranges = available_range_bytes /
      (missing_sequence_number_length + PACKET_1BYTE_SEQUENCE_NUMBER);
  max_num_ranges =
      min(static_cast<size_t>(numeric_limits<uint8>::max()), max_num_ranges);
  bool truncated = ack_info.nack_ranges.size() > max_num_ranges;
  DVLOG_IF(1, truncated) << "Truncating ack from "
                         << ack_info.nack_ranges.size() << " ranges to "
                         << max_num_ranges;

  // Write out the type byte by setting the low order bits and doing shifts
  // to make room for the next bit flags to be set.
  // Whether there are any nacks.
  uint8 type_byte = ack_info.nack_ranges.empty() ? 0 : kQuicHasNacksMask;

  // truncating bit.
  type_byte <<= kQuicAckTruncatedShift;
  type_byte |= truncated ? kQuicAckTruncatedMask : 0;

  // Largest observed sequence number length.
  type_byte <<= kQuicSequenceNumberLengthShift;
  type_byte |= GetSequenceNumberFlags(largest_observed_length);

  // Missing sequence number length.
  type_byte <<= kQuicSequenceNumberLengthShift;
  type_byte |= GetSequenceNumberFlags(missing_sequence_number_length);

  type_byte |= kQuicFrameTypeAckMask;

  if (!writer->WriteUInt8(type_byte)) {
    return false;
  }

  // TODO(satyamshekhar): Decide how often we really should send this
  // entropy_hash update.
  if (!writer->WriteUInt8(frame.sent_info.entropy_hash)) {
    return false;
  }

  DCHECK_GE(header.packet_sequence_number, frame.sent_info.least_unacked);
  const QuicPacketSequenceNumber least_unacked_delta =
      header.packet_sequence_number - frame.sent_info.least_unacked;
  if (!AppendPacketSequenceNumber(header.public_header.sequence_number_length,
                                  least_unacked_delta, writer)) {
    return false;
  }

  const ReceivedPacketInfo& received_info = frame.received_info;
  QuicPacketEntropyHash ack_entropy_hash = received_info.entropy_hash;
  NackRangeMap::reverse_iterator ack_iter = ack_info.nack_ranges.rbegin();
  if (truncated) {
    // Skip the nack ranges which the truncated ack won't include and set
    // a correct largest observed for the truncated ack.
    for (size_t i = 1; i < (ack_info.nack_ranges.size() - max_num_ranges);
         ++i) {
      ++ack_iter;
    }
    // If the last range is followed by acks, include them.
    // If the last range is followed by another range, specify the end of the
    // range as the largest_observed.
    ack_largest_observed = ack_iter->first - 1;
    // Also update the entropy so it matches the largest observed.
    ack_entropy_hash = entropy_calculator_->EntropyHash(ack_largest_observed);
    ++ack_iter;
  }

  if (!writer->WriteUInt8(ack_entropy_hash)) {
    return false;
  }

  if (!AppendPacketSequenceNumber(largest_observed_length,
                                  ack_largest_observed, writer)) {
    return false;
  }

  uint64 delta_time_largest_observed_us = kUFloat16MaxValue;
  if (!received_info.delta_time_largest_observed.IsInfinite()) {
    DCHECK_LE(0u,
              frame.received_info.delta_time_largest_observed.ToMicroseconds());
    delta_time_largest_observed_us =
        received_info.delta_time_largest_observed.ToMicroseconds();
  }

  if (!writer->WriteUFloat16(delta_time_largest_observed_us)) {
    return false;
  }

  if (ack_info.nack_ranges.empty()) {
    return true;
  }

  const uint8 num_missing_ranges =
      min(ack_info.nack_ranges.size(), max_num_ranges);
  if (!writer->WriteBytes(&num_missing_ranges, 1)) {
    return false;
  }

  int num_ranges_written = 0;
  QuicPacketSequenceNumber last_sequence_written = ack_largest_observed;
  for (; ack_iter != ack_info.nack_ranges.rend(); ++ack_iter) {
    // Calculate the delta to the last number in the range.
    QuicPacketSequenceNumber missing_delta =
        last_sequence_written - (ack_iter->first + ack_iter->second);
    if (!AppendPacketSequenceNumber(missing_sequence_number_length,
                                    missing_delta, writer)) {
      return false;
    }
    if (!AppendPacketSequenceNumber(PACKET_1BYTE_SEQUENCE_NUMBER,
                                    ack_iter->second, writer)) {
      return false;
    }
    // Subtract 1 so a missing_delta of 0 means an adjacent range.
    last_sequence_written = ack_iter->first - 1;
    ++num_ranges_written;
  }

  DCHECK_EQ(num_missing_ranges, num_ranges_written);
  return true;
}

bool QuicFramer::AppendQuicCongestionFeedbackFramePayload(
    const QuicCongestionFeedbackFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteBytes(&frame.type, 1)) {
    return false;
  }

  switch (frame.type) {
    case kInterArrival: {
      const CongestionFeedbackMessageInterArrival& inter_arrival =
          frame.inter_arrival;
      if (!writer->WriteUInt16(
              inter_arrival.accumulated_number_of_lost_packets)) {
        return false;
      }
      DCHECK_GE(numeric_limits<uint8>::max(),
                inter_arrival.received_packet_times.size());
      if (inter_arrival.received_packet_times.size() >
          numeric_limits<uint8>::max()) {
        return false;
      }
      // TODO(ianswett): Make num_received_packets a varint.
      uint8 num_received_packets =
          inter_arrival.received_packet_times.size();
      if (!writer->WriteBytes(&num_received_packets, 1)) {
        return false;
      }
      if (num_received_packets > 0) {
        TimeMap::const_iterator it =
            inter_arrival.received_packet_times.begin();

        QuicPacketSequenceNumber lowest_sequence = it->first;
        if (!AppendPacketSequenceNumber(PACKET_6BYTE_SEQUENCE_NUMBER,
                                        lowest_sequence, writer)) {
          return false;
        }

        QuicTime lowest_time = it->second;
        if (!writer->WriteUInt64(
                lowest_time.Subtract(creation_time_).ToMicroseconds())) {
          return false;
        }

        for (++it; it != inter_arrival.received_packet_times.end(); ++it) {
          QuicPacketSequenceNumber sequence_delta = it->first - lowest_sequence;
          DCHECK_GE(numeric_limits<uint16>::max(), sequence_delta);
          if (sequence_delta > numeric_limits<uint16>::max()) {
            return false;
          }
          if (!writer->WriteUInt16(static_cast<uint16>(sequence_delta))) {
            return false;
          }

          int32 time_delta_us =
              it->second.Subtract(lowest_time).ToMicroseconds();
          if (!writer->WriteBytes(&time_delta_us, sizeof(time_delta_us))) {
            return false;
          }
        }
      }
      break;
    }
    case kFixRate: {
      const CongestionFeedbackMessageFixRate& fix_rate =
          frame.fix_rate;
      if (!writer->WriteUInt32(fix_rate.bitrate.ToBytesPerSecond())) {
        return false;
      }
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = frame.tcp;
      DCHECK_LE(tcp.receive_window, 1u << 20);
      // Simple bit packing, don't send the 4 least significant bits.
      uint16 receive_window = static_cast<uint16>(tcp.receive_window >> 4);
      if (!writer->WriteUInt16(tcp.accumulated_number_of_lost_packets)) {
        return false;
      }
      if (!writer->WriteUInt16(receive_window)) {
        return false;
      }
      break;
    }
    default:
      return false;
  }

  return true;
}

bool QuicFramer::AppendRstStreamFramePayload(
        const QuicRstStreamFrame& frame,
        QuicDataWriter* writer) {
  if (!writer->WriteUInt32(frame.stream_id)) {
    return false;
  }

  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }

  if (!writer->WriteStringPiece16(frame.error_details)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendConnectionCloseFramePayload(
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  if (!writer->WriteStringPiece16(frame.error_details)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendGoAwayFramePayload(const QuicGoAwayFrame& frame,
                                          QuicDataWriter* writer) {
  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  uint32 stream_id = static_cast<uint32>(frame.last_good_stream_id);
  if (!writer->WriteUInt32(stream_id)) {
    return false;
  }
  if (!writer->WriteStringPiece16(frame.reason_phrase)) {
    return false;
  }
  return true;
}

bool QuicFramer::RaiseError(QuicErrorCode error) {
  DVLOG(1) << detailed_error_;
  set_error(error);
  visitor_->OnError(this);
  reader_.reset(NULL);
  return false;
}

}  // namespace net
