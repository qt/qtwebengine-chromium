// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/logging.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::vector;

namespace net {

// A QuicRandom wrapper that gets a bucket of entropy and distributes it
// bit-by-bit. Replenishes the bucket as needed. Not thread-safe. Expose this
// class if single bit randomness is needed elsewhere.
class QuicRandomBoolSource {
 public:
  // random: Source of entropy. Not owned.
  explicit QuicRandomBoolSource(QuicRandom* random)
      : random_(random),
        bit_bucket_(0),
        bit_mask_(0) {}

  ~QuicRandomBoolSource() {}

  // Returns the next random bit from the bucket.
  bool RandBool() {
    if (bit_mask_ == 0) {
      bit_bucket_ = random_->RandUint64();
      bit_mask_ = 1;
    }
    bool result = ((bit_bucket_ & bit_mask_) != 0);
    bit_mask_ <<= 1;
    return result;
  }

 private:
  // Source of entropy.
  QuicRandom* random_;
  // Stored random bits.
  uint64 bit_bucket_;
  // The next available bit has "1" in the mask. Zero means empty bucket.
  uint64 bit_mask_;

  DISALLOW_COPY_AND_ASSIGN(QuicRandomBoolSource);
};

QuicPacketCreator::QuicPacketCreator(QuicGuid guid,
                                     QuicFramer* framer,
                                     QuicRandom* random_generator,
                                     bool is_server)
    : guid_(guid),
      framer_(framer),
      random_bool_source_(new QuicRandomBoolSource(random_generator)),
      sequence_number_(0),
      fec_group_number_(0),
      is_server_(is_server),
      send_version_in_packet_(!is_server),
      sequence_number_length_(options_.send_sequence_number_length),
      packet_size_(0) {
  framer_->set_fec_builder(this);
}

QuicPacketCreator::~QuicPacketCreator() {
}

void QuicPacketCreator::OnBuiltFecProtectedPayload(
    const QuicPacketHeader& header, StringPiece payload) {
  if (fec_group_.get()) {
    DCHECK_NE(0u, header.fec_group);
    fec_group_->Update(header, payload);
  }
}

bool QuicPacketCreator::ShouldSendFec(bool force_close) const {
  return fec_group_.get() != NULL && fec_group_->NumReceivedPackets() > 0 &&
      (force_close ||
       fec_group_->NumReceivedPackets() >= options_.max_packets_per_fec_group);
}

void QuicPacketCreator::MaybeStartFEC() {
  if (options_.max_packets_per_fec_group > 0 && fec_group_.get() == NULL) {
    DCHECK(queued_frames_.empty());
    // Set the fec group number to the sequence number of the next packet.
    fec_group_number_ = sequence_number() + 1;
    fec_group_.reset(new QuicFecGroup());
  }
}

// Stops serializing version of the protocol in packets sent after this call.
// A packet that is already open might send kQuicVersionSize bytes less than the
// maximum packet size if we stop sending version before it is serialized.
void QuicPacketCreator::StopSendingVersion() {
  DCHECK(send_version_in_packet_);
  send_version_in_packet_ = false;
  if (packet_size_ > 0) {
    DCHECK_LT(kQuicVersionSize, packet_size_);
    packet_size_ -= kQuicVersionSize;
  }
}

void QuicPacketCreator::UpdateSequenceNumberLength(
      QuicPacketSequenceNumber least_packet_awaited_by_peer,
      QuicByteCount bytes_per_second) {
  DCHECK_LE(least_packet_awaited_by_peer, sequence_number_ + 1);
  // Since the packet creator will not change sequence number length mid FEC
  // group, include the size of an FEC group to be safe.
  const QuicPacketSequenceNumber current_delta =
      options_.max_packets_per_fec_group + sequence_number_ + 1
      - least_packet_awaited_by_peer;
  const uint64 congestion_window =
      bytes_per_second / options_.max_packet_length;
  const uint64 delta = max(current_delta, congestion_window);

  options_.send_sequence_number_length =
      QuicFramer::GetMinSequenceNumberLength(delta * 4);
}

bool QuicPacketCreator::HasRoomForStreamFrame(QuicStreamId id,
                                              QuicStreamOffset offset) const {
  return BytesFree() >
      QuicFramer::GetMinStreamFrameSize(framer_->version(), id, offset, true);
}

// static
size_t QuicPacketCreator::StreamFramePacketOverhead(
    QuicVersion version,
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group) {
  return GetPacketHeaderSize(guid_length, include_version,
                             sequence_number_length, is_in_fec_group) +
      // Assumes this is a stream with a single lone packet.
      QuicFramer::GetMinStreamFrameSize(version, 1u, 0u, true);
}

size_t QuicPacketCreator::CreateStreamFrame(QuicStreamId id,
                                            const IOVector& data,
                                            QuicStreamOffset offset,
                                            bool fin,
                                            QuicFrame* frame) {
  DCHECK_GT(options_.max_packet_length,
            StreamFramePacketOverhead(
                framer_->version(), PACKET_8BYTE_GUID, kIncludeVersion,
                PACKET_6BYTE_SEQUENCE_NUMBER, IN_FEC_GROUP));
  if (!HasRoomForStreamFrame(id, offset)) {
    LOG(DFATAL) << "No room for Stream frame, BytesFree: " << BytesFree()
                << " MinStreamFrameSize: "
                << QuicFramer::GetMinStreamFrameSize(
                    framer_->version(), id, offset, true);
  }

  if (data.Empty()) {
    if (!fin) {
      LOG(DFATAL) << "Creating a stream frame with no data or fin.";
    }
    // Create a new packet for the fin, if necessary.
    *frame = QuicFrame(new QuicStreamFrame(id, true, offset, data));
    return 0;
  }

  const size_t free_bytes = BytesFree();
  size_t bytes_consumed = 0;
  const size_t data_size = data.TotalBufferSize();

  // When a STREAM frame is the last frame in a packet, it consumes two fewer
  // bytes of framing overhead.
  // Anytime more data is available than fits in with the extra two bytes,
  // the frame will be the last, and up to two extra bytes are consumed.
  // TODO(ianswett): If QUIC pads, the 1 byte PADDING frame does not fit when
  // 1 byte is available, because then the STREAM frame isn't the last.

  // The minimum frame size(0 bytes of data) if it's not the last frame.
  size_t min_frame_size = QuicFramer::GetMinStreamFrameSize(
      framer_->version(), id, offset, false);
  // Check if it's the last frame in the packet.
  if (data_size + min_frame_size > free_bytes) {
    // The minimum frame size(0 bytes of data) if it is the last frame.
    size_t min_last_frame_size = QuicFramer::GetMinStreamFrameSize(
        framer_->version(), id, offset, true);
    bytes_consumed =
        min<size_t>(free_bytes - min_last_frame_size, data_size);
  } else {
    DCHECK_LT(data_size, BytesFree());
    bytes_consumed = data_size;
  }

  bool set_fin = fin && bytes_consumed == data_size;  // Last frame.
  IOVector frame_data;
  frame_data.AppendIovecAtMostBytes(data.iovec(), data.Size(),
                                    bytes_consumed);
  DCHECK_EQ(frame_data.TotalBufferSize(), bytes_consumed);
  *frame = QuicFrame(new QuicStreamFrame(id, set_fin, offset, frame_data));
  return bytes_consumed;
}

size_t QuicPacketCreator::CreateStreamFrameWithNotifier(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    QuicAckNotifier* notifier,
    QuicFrame* frame) {
  size_t bytes_consumed = CreateStreamFrame(id, data, offset, fin, frame);

  // The frame keeps track of the QuicAckNotifier until it is serialized into
  // a packet. At that point the notifier is informed of the sequence number
  // of the packet that this frame was eventually sent in.
  frame->stream_frame->notifier = notifier;

  return bytes_consumed;
}

SerializedPacket QuicPacketCreator::ReserializeAllFrames(
    const QuicFrames& frames,
    QuicSequenceNumberLength original_length) {
  const QuicSequenceNumberLength start_length = sequence_number_length_;
  const QuicSequenceNumberLength start_options_length =
      options_.send_sequence_number_length;
  const QuicFecGroupNumber start_fec_group = fec_group_number_;
  const size_t start_max_packets_per_fec_group =
      options_.max_packets_per_fec_group;

  // Temporarily set the sequence number length and disable FEC.
  sequence_number_length_ = original_length;
  options_.send_sequence_number_length = original_length;
  fec_group_number_ = 0;
  options_.max_packets_per_fec_group = 0;

  // Serialize the packet and restore the fec and sequence number length state.
  SerializedPacket serialized_packet = SerializeAllFrames(frames);
  sequence_number_length_ = start_length;
  options_.send_sequence_number_length = start_options_length;
  fec_group_number_ = start_fec_group;
  options_.max_packets_per_fec_group = start_max_packets_per_fec_group;

  return serialized_packet;
}

SerializedPacket QuicPacketCreator::SerializeAllFrames(
    const QuicFrames& frames) {
  // TODO(satyamshekhar): Verify that this DCHECK won't fail. What about queued
  // frames from SendStreamData()[send_stream_should_flush_ == false &&
  // data.empty() == true] and retransmit due to RTO.
  DCHECK_EQ(0u, queued_frames_.size());
  if (frames.empty()) {
    LOG(DFATAL) << "Attempt to serialize empty packet";
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    bool success = AddFrame(frames[i], false);
    DCHECK(success);
  }
  SerializedPacket packet = SerializePacket();
  DCHECK(packet.retransmittable_frames == NULL);
  return packet;
}

bool QuicPacketCreator::HasPendingFrames() {
  return !queued_frames_.empty();
}

size_t QuicPacketCreator::BytesFree() const {
  const size_t max_plaintext_size =
      framer_->GetMaxPlaintextSize(options_.max_packet_length);
  DCHECK_GE(max_plaintext_size, PacketSize());

  // If the last frame in the packet is a stream frame, then it can be
  // two bytes smaller than if it were not the last.  So this means that
  // there are two fewer bytes available to the next frame in this case.
  bool has_trailing_stream_frame =
      !queued_frames_.empty() && queued_frames_.back().type == STREAM_FRAME;
  size_t expanded_packet_size = PacketSize() +
      (has_trailing_stream_frame ? kQuicStreamPayloadLengthSize : 0);

  if (expanded_packet_size  >= max_plaintext_size) {
    return 0;
  }
  return max_plaintext_size - expanded_packet_size;
}

size_t QuicPacketCreator::PacketSize() const {
  if (queued_frames_.empty()) {
    // Only adjust the sequence number length when the FEC group is not open,
    // to ensure no packets in a group are too large.
    if (fec_group_.get() == NULL ||
        fec_group_->NumReceivedPackets() == 0) {
      sequence_number_length_ = options_.send_sequence_number_length;
    }
    packet_size_ = GetPacketHeaderSize(options_.send_guid_length,
                                       send_version_in_packet_,
                                       sequence_number_length_,
                                       options_.max_packets_per_fec_group == 0 ?
                                           NOT_IN_FEC_GROUP : IN_FEC_GROUP);
  }
  return packet_size_;
}

bool QuicPacketCreator::AddSavedFrame(const QuicFrame& frame) {
  return AddFrame(frame, true);
}

SerializedPacket QuicPacketCreator::SerializePacket() {
  if (queued_frames_.empty()) {
    LOG(DFATAL) << "Attempt to serialize empty packet";
  }
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, false, false, &header);

  MaybeAddPadding();

  size_t max_plaintext_size =
      framer_->GetMaxPlaintextSize(options_.max_packet_length);
  DCHECK_GE(max_plaintext_size, packet_size_);
  // ACK and CONNECTION_CLOSE Frames will be truncated only if they're
  // the first frame in the packet.  If truncation is to occur, then
  // GetSerializedFrameLength will have returned all bytes free.
  bool possibly_truncated =
      packet_size_ != max_plaintext_size ||
      queued_frames_.size() != 1 ||
      (queued_frames_.back().type == ACK_FRAME ||
       queued_frames_.back().type == CONNECTION_CLOSE_FRAME);
  SerializedPacket serialized =
      framer_->BuildDataPacket(header, queued_frames_, packet_size_);
  if (!serialized.packet) {
    LOG(DFATAL) << "Failed to serialize " << queued_frames_.size()
                << " frames.";
  }
  // Because of possible truncation, we can't be confident that our
  // packet size calculation worked correctly.
  if (!possibly_truncated)
    DCHECK_EQ(packet_size_, serialized.packet->length());

  packet_size_ = 0;
  queued_frames_.clear();
  serialized.retransmittable_frames = queued_retransmittable_frames_.release();
  return serialized;
}

SerializedPacket QuicPacketCreator::SerializeFec() {
  DCHECK_LT(0u, fec_group_->NumReceivedPackets());
  DCHECK_EQ(0u, queued_frames_.size());
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, true,
                   fec_group_->entropy_parity(), &header);
  QuicFecData fec_data;
  fec_data.fec_group = fec_group_->min_protected_packet();
  fec_data.redundancy = fec_group_->payload_parity();
  SerializedPacket serialized = framer_->BuildFecPacket(header, fec_data);
  fec_group_.reset(NULL);
  fec_group_number_ = 0;
  packet_size_ = 0;
  if (!serialized.packet) {
    LOG(DFATAL) << "Failed to serialize fec packet for group:"
                << fec_data.fec_group;
  }
  DCHECK_GE(options_.max_packet_length, serialized.packet->length());
  return serialized;
}

SerializedPacket QuicPacketCreator::SerializeConnectionClose(
    QuicConnectionCloseFrame* close_frame) {
  QuicFrames frames;
  frames.push_back(QuicFrame(close_frame));
  return SerializeAllFrames(frames);
}

QuicEncryptedPacket* QuicPacketCreator::SerializeVersionNegotiationPacket(
    const QuicVersionVector& supported_versions) {
  DCHECK(is_server_);
  QuicPacketPublicHeader header;
  header.guid = guid_;
  header.reset_flag = false;
  header.version_flag = true;
  header.versions = supported_versions;
  QuicEncryptedPacket* encrypted =
      framer_->BuildVersionNegotiationPacket(header, supported_versions);
  DCHECK(encrypted);
  DCHECK_GE(options_.max_packet_length, encrypted->length());
  return encrypted;
}

void QuicPacketCreator::FillPacketHeader(QuicFecGroupNumber fec_group,
                                         bool fec_flag,
                                         bool fec_entropy_flag,
                                         QuicPacketHeader* header) {
  header->public_header.guid = guid_;
  header->public_header.reset_flag = false;
  header->public_header.version_flag = send_version_in_packet_;
  header->fec_flag = fec_flag;
  header->packet_sequence_number = ++sequence_number_;
  header->public_header.sequence_number_length = sequence_number_length_;

  bool entropy_flag;
  if (fec_flag) {
    // FEC packets don't have an entropy of their own. Entropy flag for FEC
    // packets is the XOR of entropy of previous packets.
    entropy_flag = fec_entropy_flag;
  } else {
    entropy_flag = random_bool_source_->RandBool();
  }
  header->entropy_flag = entropy_flag;
  header->is_in_fec_group = fec_group == 0 ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
  header->fec_group = fec_group;
}

bool QuicPacketCreator::ShouldRetransmit(const QuicFrame& frame) {
  return frame.type != ACK_FRAME && frame.type != CONGESTION_FEEDBACK_FRAME &&
      frame.type != PADDING_FRAME;
}

bool QuicPacketCreator::AddFrame(const QuicFrame& frame,
                                 bool save_retransmittable_frames) {
  size_t frame_len = framer_->GetSerializedFrameLength(
      frame, BytesFree(), queued_frames_.empty(), true,
      options()->send_sequence_number_length);
  if (frame_len == 0) {
    return false;
  }
  DCHECK_LT(0u, packet_size_);
  MaybeStartFEC();
  packet_size_ += frame_len;
  // If the last frame in the packet was a stream frame, then once we add the
  // new frame it's serialization will be two bytes larger.
  if (!queued_frames_.empty() && queued_frames_.back().type == STREAM_FRAME) {
    packet_size_ += kQuicStreamPayloadLengthSize;
  }
  if (save_retransmittable_frames && ShouldRetransmit(frame)) {
    if (queued_retransmittable_frames_.get() == NULL) {
      queued_retransmittable_frames_.reset(new RetransmittableFrames());
    }
    if (frame.type == STREAM_FRAME) {
      queued_frames_.push_back(
          queued_retransmittable_frames_->AddStreamFrame(frame.stream_frame));
    } else {
      queued_frames_.push_back(
          queued_retransmittable_frames_->AddNonStreamFrame(frame));
    }
  } else {
    queued_frames_.push_back(frame);
  }
  return true;
}

void QuicPacketCreator::MaybeAddPadding() {
  if (BytesFree() == 0) {
    // Don't pad full packets.
    return;
  }

  // If any of the frames in the current packet are on the crypto stream
  // then they contain handshake messagses, and we should pad them.
  bool is_handshake = false;
  for (size_t i = 0; i < queued_frames_.size(); ++i) {
    if (queued_frames_[i].type == STREAM_FRAME &&
        queued_frames_[i].stream_frame->stream_id == kCryptoStreamId) {
      is_handshake = true;
      break;
    }
  }
  if (!is_handshake) {
    return;
  }

  QuicPaddingFrame padding;
  bool success = AddFrame(QuicFrame(&padding), false);
  DCHECK(success);
}

}  // namespace net
