// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_generator.h"

#include "base/logging.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;

namespace net {

class QuicAckNotifier;

QuicPacketGenerator::QuicPacketGenerator(DelegateInterface* delegate,
                                         DebugDelegateInterface* debug_delegate,
                                         QuicPacketCreator* creator)
    : delegate_(delegate),
      debug_delegate_(debug_delegate),
      packet_creator_(creator),
      batch_mode_(false),
      should_send_ack_(false),
      should_send_feedback_(false) {
}

QuicPacketGenerator::~QuicPacketGenerator() {
  for (QuicFrames::iterator it = queued_control_frames_.begin();
       it != queued_control_frames_.end(); ++it) {
    switch (it->type) {
      case PADDING_FRAME:
        delete it->padding_frame;
        break;
      case STREAM_FRAME:
        delete it->stream_frame;
        break;
      case ACK_FRAME:
        delete it->ack_frame;
        break;
      case CONGESTION_FEEDBACK_FRAME:
        delete it->congestion_feedback_frame;
        break;
      case RST_STREAM_FRAME:
        delete it->rst_stream_frame;
        break;
      case CONNECTION_CLOSE_FRAME:
        delete it->connection_close_frame;
        break;
      case GOAWAY_FRAME:
        delete it->goaway_frame;
        break;
      case NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot delete type: " << it->type;
    }
  }
}

void QuicPacketGenerator::SetShouldSendAck(bool also_send_feedback) {
  should_send_ack_ = true;
  should_send_feedback_ = also_send_feedback;
  SendQueuedFrames(false);
}

void QuicPacketGenerator::AddControlFrame(const QuicFrame& frame) {
  queued_control_frames_.push_back(frame);
  SendQueuedFrames(false);
}

QuicConsumedData QuicPacketGenerator::ConsumeData(QuicStreamId id,
                                                  const IOVector& data_to_write,
                                                  QuicStreamOffset offset,
                                                  bool fin,
                                                  QuicAckNotifier* notifier) {
  IsHandshake handshake = id == kCryptoStreamId ? IS_HANDSHAKE : NOT_HANDSHAKE;
  // The caller should have flushed pending frames before sending handshake
  // messages.
  DCHECK(handshake == NOT_HANDSHAKE || !HasPendingFrames());
  SendQueuedFrames(false);

  size_t total_bytes_consumed = 0;
  bool fin_consumed = false;

  if (!packet_creator_->HasRoomForStreamFrame(id, offset)) {
    SerializeAndSendPacket();
  }

  IOVector data = data_to_write;
  size_t data_size = data.TotalBufferSize();
  while (delegate_->ShouldGeneratePacket(NOT_RETRANSMISSION,
                                         HAS_RETRANSMITTABLE_DATA, handshake)) {
    QuicFrame frame;
    size_t bytes_consumed;
    if (notifier != NULL) {
      // We want to track which packet this stream frame ends up in.
      bytes_consumed = packet_creator_->CreateStreamFrameWithNotifier(
          id, data, offset + total_bytes_consumed, fin, notifier, &frame);
    } else {
      bytes_consumed = packet_creator_->CreateStreamFrame(
          id, data, offset + total_bytes_consumed, fin, &frame);
    }
    if (!AddFrame(frame)) {
      LOG(DFATAL) << "Failed to add stream frame.";
      // Inability to add a STREAM frame creates an unrecoverable hole in a
      // the stream, so it's best to close the connection.
      delegate_->CloseConnection(QUIC_INTERNAL_ERROR, false);
      return QuicConsumedData(0, false);
    }

    total_bytes_consumed += bytes_consumed;
    fin_consumed = fin && total_bytes_consumed == data_size;
    data.Consume(bytes_consumed);
    DCHECK(data.Empty() || packet_creator_->BytesFree() == 0u);

    // TODO(ianswett): Restore packet reordering.
    if (!InBatchMode() || !packet_creator_->HasRoomForStreamFrame(id, offset)) {
      SerializeAndSendPacket();
    }

    if (data.Empty()) {
      // We're done writing the data. Exit the loop.
      // We don't make this a precondition because we could have 0 bytes of data
      // if we're simply writing a fin.
      break;
    }
  }

  // Ensure the FEC group is closed at the end of this method if not in batch
  // mode.
  if (!InBatchMode() && packet_creator_->ShouldSendFec(true)) {
    SerializedPacket serialized_fec = packet_creator_->SerializeFec();
    DCHECK(serialized_fec.packet);
    delegate_->OnSerializedPacket(serialized_fec);
  }

  DCHECK(InBatchMode() || !packet_creator_->HasPendingFrames());
  return QuicConsumedData(total_bytes_consumed, fin_consumed);
}

bool QuicPacketGenerator::CanSendWithNextPendingFrameAddition() const {
  DCHECK(HasPendingFrames());
  HasRetransmittableData retransmittable =
      (should_send_ack_ || should_send_feedback_) ? NO_RETRANSMITTABLE_DATA
                                                  : HAS_RETRANSMITTABLE_DATA;
  if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
      DCHECK(!queued_control_frames_.empty());  // These are retransmittable.
  }
  return delegate_->ShouldGeneratePacket(NOT_RETRANSMISSION, retransmittable,
                                         NOT_HANDSHAKE);
}

void QuicPacketGenerator::SendQueuedFrames(bool flush) {
  // Only add pending frames if we are SURE we can then send the whole packet.
  while (HasPendingFrames() &&
         (flush || CanSendWithNextPendingFrameAddition())) {
    if (!AddNextPendingFrame()) {
      // Packet was full, so serialize and send it.
      SerializeAndSendPacket();
    }
  }

  if (!InBatchMode() || flush) {
    if (packet_creator_->HasPendingFrames()) {
      SerializeAndSendPacket();
    }

    // Ensure the FEC group is closed at the end of this method unless other
    // writes are pending.
    if (packet_creator_->ShouldSendFec(true)) {
      SerializedPacket serialized_fec = packet_creator_->SerializeFec();
      DCHECK(serialized_fec.packet);
      delegate_->OnSerializedPacket(serialized_fec);
    }
  }
}

bool QuicPacketGenerator::InBatchMode() {
  return batch_mode_;
}

void QuicPacketGenerator::StartBatchOperations() {
  batch_mode_ = true;
}

void QuicPacketGenerator::FinishBatchOperations() {
  batch_mode_ = false;
  SendQueuedFrames(false);
}

void QuicPacketGenerator::FlushAllQueuedFrames() {
  SendQueuedFrames(true);
}

bool QuicPacketGenerator::HasQueuedFrames() const {
  return packet_creator_->HasPendingFrames() || HasPendingFrames();
}

bool QuicPacketGenerator::HasPendingFrames() const {
  return should_send_ack_ || should_send_feedback_ ||
      !queued_control_frames_.empty();
}

bool QuicPacketGenerator::AddNextPendingFrame() {
  if (should_send_ack_) {
    pending_ack_frame_.reset(delegate_->CreateAckFrame());
    // If we can't this add the frame now, then we still need to do so later.
    should_send_ack_ = !AddFrame(QuicFrame(pending_ack_frame_.get()));
    // Return success if we have cleared out this flag (i.e., added the frame).
    // If we still need to send, then the frame is full, and we have failed.
    return !should_send_ack_;
  }

  if (should_send_feedback_) {
    pending_feedback_frame_.reset(delegate_->CreateFeedbackFrame());
    // If we can't this add the frame now, then we still need to do so later.
    should_send_feedback_ = !AddFrame(QuicFrame(pending_feedback_frame_.get()));
    // Return success if we have cleared out this flag (i.e., added the frame).
    // If we still need to send, then the frame is full, and we have failed.
    return !should_send_feedback_;
  }

  if (queued_control_frames_.empty()) {
    LOG(DFATAL) << "AddNextPendingFrame called with no queued control frames.";
  }
  if (!AddFrame(queued_control_frames_.back())) {
    // Packet was full.
    return false;
  }
  queued_control_frames_.pop_back();
  return true;
}

bool QuicPacketGenerator::AddFrame(const QuicFrame& frame) {
  bool success = packet_creator_->AddSavedFrame(frame);
  if (success && debug_delegate_) {
    debug_delegate_->OnFrameAddedToPacket(frame);
  }
  return success;
}

void QuicPacketGenerator::SerializeAndSendPacket() {
  SerializedPacket serialized_packet = packet_creator_->SerializePacket();
  DCHECK(serialized_packet.packet);
  delegate_->OnSerializedPacket(serialized_packet);

  if (packet_creator_->ShouldSendFec(false)) {
    SerializedPacket serialized_fec = packet_creator_->SerializeFec();
    DCHECK(serialized_fec.packet);
    delegate_->OnSerializedPacket(serialized_fec);
  }
}

}  // namespace net
