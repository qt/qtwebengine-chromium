// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for client/server reliable streams.

#ifndef NET_QUIC_RELIABLE_QUIC_STREAM_H_
#define NET_QUIC_RELIABLE_QUIC_STREAM_H_

#include <sys/types.h>

#include <list>

#include "base/strings/string_piece.h"
#include "net/base/iovec.h"
#include "net/base/net_export.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_stream_sequencer.h"

namespace net {

namespace test {
class ReliableQuicStreamPeer;
}  // namespace test

class IPEndPoint;
class QuicSession;
class SSLInfo;

class NET_EXPORT_PRIVATE ReliableQuicStream {
 public:
  ReliableQuicStream(QuicStreamId id,
                     QuicSession* session);

  virtual ~ReliableQuicStream();

  bool WillAcceptStreamFrame(const QuicStreamFrame& frame) const;

  // Called when a (potentially duplicate) stream frame has been received
  // for this stream.  Returns false if this frame can not be accepted
  // because there is too much data already buffered.
  virtual bool OnStreamFrame(const QuicStreamFrame& frame);

  // Called when the connection becomes writeable to allow the stream
  // to write any pending data.
  virtual void OnCanWrite();

  // Called by the session just before the stream is deleted.
  virtual void OnClose();

  // Called when we get a stream reset from the peer.
  virtual void OnStreamReset(QuicRstStreamErrorCode error);

  // Called when we get or send a connection close, and should immediately
  // close the stream.  This is not passed through the sequencer,
  // but is handled immediately.
  virtual void OnConnectionClosed(QuicErrorCode error, bool from_peer);

  // Called when the final data has been read.
  virtual void OnFinRead();

  virtual uint32 ProcessRawData(const char* data, uint32 data_len) = 0;

  // Called to reset the stream from this end.
  virtual void Reset(QuicRstStreamErrorCode error);

  // Called to close the entire connection from this end.
  virtual void CloseConnection(QuicErrorCode error);
  virtual void CloseConnectionWithDetails(QuicErrorCode error,
                                          const string& details);

  // Returns the effective priority for the stream.  This value may change
  // during the life of the stream.
  virtual QuicPriority EffectivePriority() const = 0;

  QuicStreamId id() const { return id_; }

  QuicRstStreamErrorCode stream_error() const { return stream_error_; }
  QuicErrorCode connection_error() const { return connection_error_; }

  bool read_side_closed() const { return read_side_closed_; }
  bool write_side_closed() const { return write_side_closed_; }

  uint64 stream_bytes_read() { return stream_bytes_read_; }
  uint64 stream_bytes_written() { return stream_bytes_written_; }

  QuicVersion version();

 protected:
  // Sends as much of 'data' to the connection as the connection will consume,
  // and then buffers any remaining data in queued_data_.
  void WriteOrBufferData(base::StringPiece data, bool fin);

  // Sends as many bytes in the first |count| buffers of |iov| to the connection
  // as the connection will consume.
  // If |ack_notifier_delegate| is provided, then it will be notified once all
  // the ACKs for this write have been received.
  // Returns the number of bytes consumed by the connection.
  QuicConsumedData WritevData(
      const struct iovec* iov,
      int iov_count,
      bool fin,
      QuicAckNotifier::DelegateInterface* ack_notifier_delegate);

  // Close the read side of the socket.  Further frames will not be accepted.
  virtual void CloseReadSide();

  // Close the write side of the socket.  Further writes will fail.
  void CloseWriteSide();

  bool HasBufferedData();

  bool fin_buffered() { return fin_buffered_; }

  QuicSession* session() { return session_; }

  const QuicStreamSequencer* sequencer() const { return &sequencer_; }
  QuicStreamSequencer* sequencer() { return &sequencer_; }

 private:
  friend class test::ReliableQuicStreamPeer;
  friend class QuicStreamUtils;

  std::list<string> queued_data_;

  QuicStreamSequencer sequencer_;
  QuicStreamId id_;
  QuicSession* session_;
  // Bytes read and written refer to payload bytes only: they do not include
  // framing, encryption overhead etc.
  uint64 stream_bytes_read_;
  uint64 stream_bytes_written_;

  // Stream error code received from a RstStreamFrame or error code sent by the
  // visitor or sequencer in the RstStreamFrame.
  QuicRstStreamErrorCode stream_error_;
  // Connection error code due to which the stream was closed. |stream_error_|
  // is set to |QUIC_STREAM_CONNECTION_ERROR| when this happens and consumers
  // should check |connection_error_|.
  QuicErrorCode connection_error_;

  // True if the read side is closed and further frames should be rejected.
  bool read_side_closed_;
  // True if the write side is closed, and further writes should fail.
  bool write_side_closed_;

  bool fin_buffered_;
  bool fin_sent_;

  // True if the session this stream is running under is a server session.
  bool is_server_;

  DISALLOW_COPY_AND_ASSIGN(ReliableQuicStream);
};

}  // namespace net

#endif  // NET_QUIC_RELIABLE_QUIC_STREAM_H_
