// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/reliable_quic_stream.h"

#include "net/quic/quic_session.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/spdy/write_blocked_list.h"

using base::StringPiece;
using std::min;

namespace net {

namespace {

// This is somewhat arbitrary.  It's possible, but unlikely, we will either fail
// to set a priority client-side, or cancel a stream before stripping the
// priority from the wire server-side.  In either case, start out with a
// priority in the middle.
QuicPriority kDefaultPriority = 3;

// Appends bytes from data into partial_data_buffer.  Once partial_data_buffer
// reaches 4 bytes, copies the data into 'result' and clears
// partial_data_buffer.
// Returns the number of bytes consumed.
uint32 StripUint32(const char* data, uint32 data_len,
                   string* partial_data_buffer,
                   uint32* result) {
  DCHECK_GT(4u, partial_data_buffer->length());
  size_t missing_size = 4 - partial_data_buffer->length();
  if (data_len < missing_size) {
    StringPiece(data, data_len).AppendToString(partial_data_buffer);
    return data_len;
  }
  StringPiece(data, missing_size).AppendToString(partial_data_buffer);
  DCHECK_EQ(4u, partial_data_buffer->length());
  memcpy(result, partial_data_buffer->data(), 4);
  partial_data_buffer->clear();
  return missing_size;
}

}  // namespace

ReliableQuicStream::ReliableQuicStream(QuicStreamId id,
                                       QuicSession* session)
    : sequencer_(this),
      id_(id),
      session_(session),
      visitor_(NULL),
      stream_bytes_read_(0),
      stream_bytes_written_(0),
      headers_decompressed_(false),
      priority_(kDefaultPriority),
      headers_id_(0),
      decompression_failed_(false),
      stream_error_(QUIC_STREAM_NO_ERROR),
      connection_error_(QUIC_NO_ERROR),
      read_side_closed_(false),
      write_side_closed_(false),
      priority_parsed_(false),
      fin_buffered_(false),
      fin_sent_(false) {
}

ReliableQuicStream::~ReliableQuicStream() {
}

bool ReliableQuicStream::WillAcceptStreamFrame(
    const QuicStreamFrame& frame) const {
  if (read_side_closed_) {
    return true;
  }
  if (frame.stream_id != id_) {
    LOG(ERROR) << "Error!";
    return false;
  }
  return sequencer_.WillAcceptStreamFrame(frame);
}

bool ReliableQuicStream::OnStreamFrame(const QuicStreamFrame& frame) {
  DCHECK_EQ(frame.stream_id, id_);
  if (read_side_closed_) {
    DLOG(INFO) << "Ignoring frame " << frame.stream_id;
    // We don't want to be reading: blackhole the data.
    return true;
  }
  // Note: This count include duplicate data received.
  stream_bytes_read_ += frame.data.length();

  bool accepted = sequencer_.OnStreamFrame(frame);

  return accepted;
}

void ReliableQuicStream::OnStreamReset(QuicRstStreamErrorCode error) {
  stream_error_ = error;
  TerminateFromPeer(false);  // Full close.
}

void ReliableQuicStream::ConnectionClose(QuicErrorCode error, bool from_peer) {
  if (read_side_closed_ && write_side_closed_) {
    return;
  }
  if (error != QUIC_NO_ERROR) {
    stream_error_ = QUIC_STREAM_CONNECTION_ERROR;
    connection_error_ = error;
  }

  if (from_peer) {
    TerminateFromPeer(false);
  } else {
    CloseWriteSide();
    CloseReadSide();
  }
}

void ReliableQuicStream::TerminateFromPeer(bool half_close) {
  if (!half_close) {
    CloseWriteSide();
  }
  CloseReadSide();
}

void ReliableQuicStream::Close(QuicRstStreamErrorCode error) {
  stream_error_ = error;
  if (error != QUIC_STREAM_NO_ERROR)  {
    // Sending a RstStream results in calling CloseStream.
    session()->SendRstStream(id(), error);
  } else {
    session_->CloseStream(id());
  }
}

size_t ReliableQuicStream::Readv(const struct iovec* iov, size_t iov_len) {
  if (headers_decompressed_ && decompressed_headers_.empty()) {
    return sequencer_.Readv(iov, iov_len);
  }
  size_t bytes_consumed = 0;
  size_t iov_index = 0;
  while (iov_index < iov_len &&
         decompressed_headers_.length() > bytes_consumed) {
    size_t bytes_to_read = min(iov[iov_index].iov_len,
                               decompressed_headers_.length() - bytes_consumed);
    char* iov_ptr = static_cast<char*>(iov[iov_index].iov_base);
    memcpy(iov_ptr,
           decompressed_headers_.data() + bytes_consumed, bytes_to_read);
    bytes_consumed += bytes_to_read;
    ++iov_index;
  }
  decompressed_headers_.erase(0, bytes_consumed);
  return bytes_consumed;
}

int ReliableQuicStream::GetReadableRegions(iovec* iov, size_t iov_len) {
  if (headers_decompressed_ && decompressed_headers_.empty()) {
    return sequencer_.GetReadableRegions(iov, iov_len);
  }
  if (iov_len == 0) {
    return 0;
  }
  iov[0].iov_base = static_cast<void*>(
      const_cast<char*>(decompressed_headers_.data()));
  iov[0].iov_len = decompressed_headers_.length();
  return 1;
}

bool ReliableQuicStream::IsHalfClosed() const {
  if (!headers_decompressed_ || !decompressed_headers_.empty()) {
    return false;
  }
  return sequencer_.IsHalfClosed();
}

bool ReliableQuicStream::HasBytesToRead() const {
  return !decompressed_headers_.empty() || sequencer_.HasBytesToRead();
}

const IPEndPoint& ReliableQuicStream::GetPeerAddress() const {
  return session_->peer_address();
}

QuicSpdyCompressor* ReliableQuicStream::compressor() {
  return session_->compressor();
}

bool ReliableQuicStream::GetSSLInfo(SSLInfo* ssl_info) {
  return session_->GetSSLInfo(ssl_info);
}

QuicConsumedData ReliableQuicStream::WriteData(StringPiece data, bool fin) {
  DCHECK(data.size() > 0 || fin);
  return WriteOrBuffer(data, fin);
}


void ReliableQuicStream::set_priority(QuicPriority priority) {
  DCHECK_EQ(0u, stream_bytes_written_);
  priority_ = priority;
}

QuicConsumedData ReliableQuicStream::WriteOrBuffer(StringPiece data, bool fin) {
  DCHECK(!fin_buffered_);

  QuicConsumedData consumed_data(0, false);
  fin_buffered_ = fin;

  if (queued_data_.empty()) {
    consumed_data = WriteDataInternal(string(data.data(), data.length()), fin);
    DCHECK_LE(consumed_data.bytes_consumed, data.length());
  }

  // If there's unconsumed data or an unconsumed fin, queue it.
  if (consumed_data.bytes_consumed < data.length() ||
      (fin && !consumed_data.fin_consumed)) {
    queued_data_.push_back(
        string(data.data() + consumed_data.bytes_consumed,
               data.length() - consumed_data.bytes_consumed));
  }

  return QuicConsumedData(data.size(), true);
}

void ReliableQuicStream::OnCanWrite() {
  bool fin = false;
  while (!queued_data_.empty()) {
    const string& data = queued_data_.front();
    if (queued_data_.size() == 1 && fin_buffered_) {
      fin = true;
    }
    QuicConsumedData consumed_data = WriteDataInternal(data, fin);
    if (consumed_data.bytes_consumed == data.size() &&
        fin == consumed_data.fin_consumed) {
      queued_data_.pop_front();
    } else {
      queued_data_.front().erase(0, consumed_data.bytes_consumed);
      break;
    }
  }
}

QuicConsumedData ReliableQuicStream::WriteDataInternal(
    StringPiece data, bool fin) {
  struct iovec iov = {const_cast<char*>(data.data()),
                      static_cast<size_t>(data.size())};
  return WritevDataInternal(&iov, 1, fin);
}

QuicConsumedData ReliableQuicStream::WritevDataInternal(const struct iovec* iov,
                                                        int iov_count,
                                                        bool fin) {
  if (write_side_closed_) {
    DLOG(ERROR) << "Attempt to write when the write side is closed";
    return QuicConsumedData(0, false);
  }

  size_t write_length = 0u;
  for (int i = 0; i < iov_count; ++i) {
    write_length += iov[i].iov_len;
  }
  QuicConsumedData consumed_data =
      session()->WritevData(id(), iov, iov_count, stream_bytes_written_, fin);
  stream_bytes_written_ += consumed_data.bytes_consumed;
  if (consumed_data.bytes_consumed == write_length) {
    if (fin && consumed_data.fin_consumed) {
      fin_sent_ = true;
      CloseWriteSide();
    } else if (fin && !consumed_data.fin_consumed) {
      session_->MarkWriteBlocked(id(), EffectivePriority());
    }
  } else {
    session_->MarkWriteBlocked(id(), EffectivePriority());
  }
  return consumed_data;
}

QuicPriority ReliableQuicStream::EffectivePriority() const {
  return priority();
}

void ReliableQuicStream::CloseReadSide() {
  if (read_side_closed_) {
    return;
  }
  DLOG(INFO) << "Done reading from stream " << id();

  read_side_closed_ = true;
  if (write_side_closed_) {
    DLOG(INFO) << "Closing stream: " << id();
    session_->CloseStream(id());
  }
}

uint32 ReliableQuicStream::ProcessRawData(const char* data, uint32 data_len) {
  DCHECK_NE(0u, data_len);
  if (id() == kCryptoStreamId) {
    // The crypto stream does not use compression.
    return ProcessData(data, data_len);
  }

  uint32 total_bytes_consumed = 0;
  if (headers_id_ == 0u) {
    total_bytes_consumed += StripPriorityAndHeaderId(data, data_len);
    data += total_bytes_consumed;
    data_len -= total_bytes_consumed;
    if (data_len == 0 || !session_->connection()->connected()) {
      return total_bytes_consumed;
    }
  }
  DCHECK_NE(0u, headers_id_);

  // Once the headers are finished, we simply pass the data through.
  if (headers_decompressed_) {
    // Some buffered header data remains.
    if (!decompressed_headers_.empty()) {
      ProcessHeaderData();
    }
    if (decompressed_headers_.empty()) {
      DVLOG(1) << "Delegating procesing to ProcessData";
      total_bytes_consumed += ProcessData(data, data_len);
    }
    return total_bytes_consumed;
  }

  QuicHeaderId current_header_id =
      session_->decompressor()->current_header_id();
  // Ensure that this header id looks sane.
  if (headers_id_ < current_header_id ||
      headers_id_ > kMaxHeaderIdDelta + current_header_id) {
    DVLOG(1) << "Invalid headers for stream: " << id()
             << " header_id: " << headers_id_
             << " current_header_id: " << current_header_id;
    session_->connection()->SendConnectionClose(QUIC_INVALID_HEADER_ID);
    return total_bytes_consumed;
  }

  // If we are head-of-line blocked on decompression, then back up.
  if (current_header_id != headers_id_) {
    session_->MarkDecompressionBlocked(headers_id_, id());
    DVLOG(1) << "Unable to decompress header data for stream: " << id()
             << " header_id: " << headers_id_;
    return total_bytes_consumed;
  }

  // Decompressed data will be delivered to decompressed_headers_.
  size_t bytes_consumed = session_->decompressor()->DecompressData(
      StringPiece(data, data_len), this);
  DCHECK_NE(0u, bytes_consumed);
  if (bytes_consumed > data_len) {
    DCHECK(false) << "DecompressData returned illegal value";
    OnDecompressionError();
    return total_bytes_consumed;
  }
  total_bytes_consumed += bytes_consumed;
  data += bytes_consumed;
  data_len -= bytes_consumed;

  if (decompression_failed_) {
    // The session will have been closed in OnDecompressionError.
    return total_bytes_consumed;
  }

  // Headers are complete if the decompressor has moved on to the
  // next stream.
  headers_decompressed_ =
      session_->decompressor()->current_header_id() != headers_id_;
  if (!headers_decompressed_) {
    DCHECK_EQ(0u, data_len);
  }

  ProcessHeaderData();

  if (!headers_decompressed_ || !decompressed_headers_.empty()) {
    return total_bytes_consumed;
  }

  // We have processed all of the decompressed data but we might
  // have some more raw data to process.
  if (data_len > 0) {
    total_bytes_consumed += ProcessData(data, data_len);
  }

  // The sequencer will push any additional buffered frames if this data
  // has been completely consumed.
  return total_bytes_consumed;
}

uint32 ReliableQuicStream::ProcessHeaderData() {
  if (decompressed_headers_.empty()) {
    return 0;
  }

  size_t bytes_processed = ProcessData(decompressed_headers_.data(),
                                       decompressed_headers_.length());
  if (bytes_processed == decompressed_headers_.length()) {
    decompressed_headers_.clear();
  } else {
    decompressed_headers_ = decompressed_headers_.erase(0, bytes_processed);
  }
  return bytes_processed;
}

void ReliableQuicStream::OnDecompressorAvailable() {
  DCHECK_EQ(headers_id_,
            session_->decompressor()->current_header_id());
  DCHECK(!headers_decompressed_);
  DCHECK(!decompression_failed_);
  DCHECK_EQ(0u, decompressed_headers_.length());

  while (!headers_decompressed_) {
    struct iovec iovec;
    if (sequencer_.GetReadableRegions(&iovec, 1) == 0) {
      return;
    }

    size_t bytes_consumed = session_->decompressor()->DecompressData(
        StringPiece(static_cast<char*>(iovec.iov_base),
                    iovec.iov_len),
        this);
    DCHECK_LE(bytes_consumed, iovec.iov_len);
    if (decompression_failed_) {
      return;
    }
    sequencer_.MarkConsumed(bytes_consumed);

    headers_decompressed_ =
        session_->decompressor()->current_header_id() != headers_id_;
  }

  // Either the headers are complete, or the all data as been consumed.
  ProcessHeaderData();  // Unprocessed headers remain in decompressed_headers_.
  if (IsHalfClosed()) {
    TerminateFromPeer(true);
  } else if (headers_decompressed_ && decompressed_headers_.empty()) {
    sequencer_.FlushBufferedFrames();
  }
}

bool ReliableQuicStream::OnDecompressedData(StringPiece data) {
  data.AppendToString(&decompressed_headers_);
  return true;
}

void ReliableQuicStream::OnDecompressionError() {
  DCHECK(!decompression_failed_);
  decompression_failed_ = true;
  session_->connection()->SendConnectionClose(QUIC_DECOMPRESSION_FAILURE);
}


void ReliableQuicStream::CloseWriteSide() {
  if (write_side_closed_) {
    return;
  }
  DLOG(INFO) << "Done writing to stream " << id();

  write_side_closed_ = true;
  if (read_side_closed_) {
    DLOG(INFO) << "Closing stream: " << id();
    session_->CloseStream(id());
  }
}

bool ReliableQuicStream::HasBufferedData() {
  return !queued_data_.empty();
}

void ReliableQuicStream::OnClose() {
  CloseReadSide();
  CloseWriteSide();

  if (visitor_) {
    Visitor* visitor = visitor_;
    // Calling Visitor::OnClose() may result the destruction of the visitor,
    // so we need to ensure we don't call it again.
    visitor_ = NULL;
    visitor->OnClose(this);
  }
}

uint32 ReliableQuicStream::StripPriorityAndHeaderId(
    const char* data, uint32 data_len) {
  uint32 total_bytes_parsed = 0;

  if (!priority_parsed_ &&
      session_->connection()->version() >= QUIC_VERSION_9 &&
      session_->connection()->is_server()) {
    QuicPriority temporary_priority = priority_;
    total_bytes_parsed = StripUint32(
        data, data_len, &headers_id_and_priority_buffer_, &temporary_priority);
    if (total_bytes_parsed > 0 && headers_id_and_priority_buffer_.size() == 0) {
      priority_parsed_ = true;
      // Spdy priorities are inverted, so the highest numerical value is the
      // lowest legal priority.
      if (temporary_priority > static_cast<QuicPriority>(kLowestPriority)) {
        session_->connection()->SendConnectionClose(QUIC_INVALID_PRIORITY);
        return 0;
      }
      priority_ = temporary_priority;
    }
    data += total_bytes_parsed;
    data_len -= total_bytes_parsed;
  }
  if (data_len > 0 && headers_id_ == 0u) {
    // The headers ID has not yet been read.  Strip it from the beginning of
    // the data stream.
    total_bytes_parsed += StripUint32(
        data, data_len, &headers_id_and_priority_buffer_, &headers_id_);
  }
  return total_bytes_parsed;
}

}  // namespace net
