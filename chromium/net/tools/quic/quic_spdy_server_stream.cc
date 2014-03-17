// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_server_stream.h"

#include "base/memory/singleton.h"
#include "net/quic/quic_session.h"
#include "net/spdy/spdy_framer.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/spdy_utils.h"

using base::StringPiece;
using std::string;

namespace net {
namespace tools {

static const size_t kHeaderBufInitialSize = 4096;

QuicSpdyServerStream::QuicSpdyServerStream(QuicStreamId id,
                                           QuicSession* session)
    : QuicDataStream(id, session),
      read_buf_(new GrowableIOBuffer()),
      request_headers_received_(false) {
}

QuicSpdyServerStream::~QuicSpdyServerStream() {
}

uint32 QuicSpdyServerStream::ProcessData(const char* data, uint32 length) {
  uint32 total_bytes_processed = 0;

  // Are we still reading the request headers.
  if (!request_headers_received_) {
    // Grow the read buffer if necessary.
    if (read_buf_->RemainingCapacity() < (int)length) {
      read_buf_->SetCapacity(read_buf_->capacity() + kHeaderBufInitialSize);
    }
    memcpy(read_buf_->data(), data, length);
    read_buf_->set_offset(read_buf_->offset() + length);
    ParseRequestHeaders();
  } else {
    body_.append(data + total_bytes_processed, length - total_bytes_processed);
  }
  return length;
}

void QuicSpdyServerStream::OnFinRead() {
  ReliableQuicStream::OnFinRead();
  if (write_side_closed() || fin_buffered()) {
    return;
  }

  if (!request_headers_received_) {
    SendErrorResponse();  // We're not done reading headers.
  } else if ((headers_.content_length_status() ==
             BalsaHeadersEnums::VALID_CONTENT_LENGTH) &&
             body_.size() != headers_.content_length()) {
    SendErrorResponse();  // Invalid content length
  } else {
    SendResponse();
  }
}

int QuicSpdyServerStream::ParseRequestHeaders() {
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  SpdyFramer framer(SPDY3);
  SpdyHeaderBlock headers;
  char* data = read_buf_->StartOfBuffer();
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                               &headers);
  if (len == 0) {
    return -1;
  }

  if (!SpdyUtils::FillBalsaRequestHeaders(headers, &headers_)) {
    SendErrorResponse();
    return -1;
  }

  size_t delta = read_buf_len - len;
  if (delta > 0) {
    body_.append(data + len, delta);
  }

  request_headers_received_ = true;
  return len;
}

void QuicSpdyServerStream::SendResponse() {
  // Find response in cache. If not found, send error response.
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(headers_);
  if (response == NULL) {
    SendErrorResponse();
    return;
  }

  DLOG(INFO) << "Sending response for stream " << id();
  SendHeadersAndBody(response->headers(), response->body());
}

void QuicSpdyServerStream::SendErrorResponse() {
  DLOG(INFO) << "Sending error response for stream " << id();
  BalsaHeaders headers;
  headers.SetResponseFirstlineFromStringPieces(
      "HTTP/1.1", "500", "Server Error");
  headers.ReplaceOrAppendHeader("content-length", "3");
  SendHeadersAndBody(headers, "bad");
}

void QuicSpdyServerStream:: SendHeadersAndBody(
    const BalsaHeaders& response_headers,
    StringPiece body) {
  // We only support SPDY and HTTP, and neither handles bidirectional streaming.
  if (!read_side_closed()) {
    CloseReadSide();
  }

  SpdyHeaderBlock header_block =
      SpdyUtils::ResponseHeadersToSpdyHeaders(response_headers);

  string headers_string =
      session()->compressor()->CompressHeaders(header_block);
    WriteOrBufferData(headers_string, body.empty());

  if (!body.empty()) {
    WriteOrBufferData(body, true);
  }
}

}  // namespace tools
}  // namespace net
