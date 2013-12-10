// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SPDY_CLIENT_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_SPDY_CLIENT_STREAM_H_

#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/tools/quic/quic_reliable_client_stream.h"

namespace net {

class BalsaHeaders;

namespace tools {

class QuicClientSession;

// All this does right now is send an SPDY request, and aggregate the
// SPDY response.
class QuicSpdyClientStream : public QuicReliableClientStream {
 public:
  QuicSpdyClientStream(QuicStreamId id, QuicClientSession* session);
  virtual ~QuicSpdyClientStream();

  // ReliableQuicStream implementation called by the session when there's
  // data for us.
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE;

  virtual void TerminateFromPeer(bool half_close) OVERRIDE;

  virtual ssize_t SendRequest(const BalsaHeaders& headers,
                              base::StringPiece body,
                              bool fin) OVERRIDE;

  // While the server's set_priority shouldn't be called externally, the creator
  // of client-side streams should be able to set the priority.
  using QuicReliableClientStream::set_priority;

 private:
  int ParseResponseHeaders();

  scoped_refptr<GrowableIOBuffer> read_buf_;
  bool response_headers_received_;
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SPDY_CLIENT_STREAM_H_
