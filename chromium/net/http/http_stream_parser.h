// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_PARSER_H_
#define NET_HTTP_HTTP_STREAM_PARSER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/upload_progress.h"

namespace net {

class ClientSocketHandle;
class DrainableIOBuffer;
class GrowableIOBuffer;
class HttpChunkedDecoder;
struct HttpRequestInfo;
class HttpRequestHeaders;
class HttpResponseInfo;
class IOBuffer;
class IOBufferWithSize;
class SSLCertRequestInfo;
class SSLInfo;
class UploadDataStream;

class NET_EXPORT_PRIVATE HttpStreamParser {
 public:
  // Any data in |read_buffer| will be used before reading from the socket
  // and any data left over after parsing the stream will be put into
  // |read_buffer|.  The left over data will start at offset 0 and the
  // buffer's offset will be set to the first free byte. |read_buffer| may
  // have its capacity changed.
  HttpStreamParser(ClientSocketHandle* connection,
                   const HttpRequestInfo* request,
                   GrowableIOBuffer* read_buffer,
                   const BoundNetLog& net_log);
  virtual ~HttpStreamParser();

  // These functions implement the interface described in HttpStream with
  // some additional functionality
  int SendRequest(const std::string& request_line,
                  const HttpRequestHeaders& headers,
                  HttpResponseInfo* response,
                  const CompletionCallback& callback);

  int ReadResponseHeaders(const CompletionCallback& callback);

  int ReadResponseBody(IOBuffer* buf, int buf_len,
                       const CompletionCallback& callback);

  void Close(bool not_reusable);

  // Returns the progress of uploading. When data is chunked, size is set to
  // zero, but position will not be.
  UploadProgress GetUploadProgress() const;

  HttpResponseInfo* GetResponseInfo();

  bool IsResponseBodyComplete() const;

  bool CanFindEndOfResponse() const;

  bool IsMoreDataBuffered() const;

  bool IsConnectionReused() const;

  void SetConnectionReused();

  bool IsConnectionReusable() const;

  void GetSSLInfo(SSLInfo* ssl_info);

  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);

  // Encodes the given |payload| in the chunked format to |output|.
  // Returns the number of bytes written to |output|. |output_size| should
  // be large enough to store the encoded chunk, which is payload.size() +
  // kChunkHeaderFooterSize. Returns ERR_INVALID_ARGUMENT if |output_size|
  // is not large enough.
  //
  // The output will look like: "HEX\r\n[payload]\r\n"
  // where HEX is a length in hexdecimal (without the "0x" prefix).
  static int EncodeChunk(const base::StringPiece& payload,
                         char* output,
                         size_t output_size);

  // Returns true if request headers and body should be merged (i.e. the
  // sum is small enough and the body is in memory, and not chunked).
  static bool ShouldMergeRequestHeadersAndBody(
      const std::string& request_headers,
      const UploadDataStream* request_body);

  // The number of extra bytes required to encode a chunk.
  static const size_t kChunkHeaderFooterSize;

 private:
  class SeekableIOBuffer;

  // FOO_COMPLETE states implement the second half of potentially asynchronous
  // operations and don't necessarily mean that FOO is complete.
  enum State {
    STATE_NONE,
    STATE_SENDING_HEADERS,
    // If the request comes with a body, either of the following two
    // states will be executed, depending on whether the body is chunked
    // or not.
    STATE_SENDING_BODY,
    STATE_SEND_REQUEST_READING_BODY,
    STATE_REQUEST_SENT,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_BODY_PENDING,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_DONE
  };

  // The number of bytes by which the header buffer is grown when it reaches
  // capacity.
  static const int kHeaderBufInitialSize = 4 * 1024;  // 4K

  // |kMaxHeaderBufSize| is the number of bytes that the response headers can
  // grow to. If the body start is not found within this range of the
  // response, the transaction will fail with ERR_RESPONSE_HEADERS_TOO_BIG.
  // Note: |kMaxHeaderBufSize| should be a multiple of |kHeaderBufInitialSize|.
  static const int kMaxHeaderBufSize = kHeaderBufInitialSize * 64;  // 256K

  // The maximum sane buffer size.
  static const int kMaxBufSize = 2 * 1024 * 1024;  // 2M

  // Handle callbacks.
  void OnIOComplete(int result);

  // Try to make progress sending/receiving the request/response.
  int DoLoop(int result);

  // The implementations of each state of the state machine.
  int DoSendHeaders(int result);
  int DoSendBody(int result);
  int DoSendRequestReadingBody(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);

  // Examines |read_buf_| to find the start and end of the headers. If they are
  // found, parse them with DoParseResponseHeaders().  Return the offset for
  // the end of the headers, or -1 if the complete headers were not found, or
  // with a net::Error if we encountered an error during parsing.
  int ParseResponseHeaders();

  // Parse the headers into response_.  Returns OK on success or a net::Error on
  // failure.
  int DoParseResponseHeaders(int end_of_header_offset);

  // Examine the parsed headers to try to determine the response body size.
  void CalculateResponseBodySize();

  // Current state of the request.
  State io_state_;

  // The request to send.
  const HttpRequestInfo* request_;

  // The request header data.
  scoped_refptr<DrainableIOBuffer> request_headers_;

  // Temporary buffer for reading.
  scoped_refptr<GrowableIOBuffer> read_buf_;

  // Offset of the first unused byte in |read_buf_|.  May be nonzero due to
  // body data in the same packet as header data but is zero when reading
  // headers.
  int read_buf_unused_offset_;

  // The amount beyond |read_buf_unused_offset_| where the status line starts;
  // -1 if not found yet.
  int response_header_start_offset_;

  // The parsed response headers.  Owned by the caller.
  HttpResponseInfo* response_;

  // Indicates the content length.  If this value is less than zero
  // (and chunked_decoder_ is null), then we must read until the server
  // closes the connection.
  int64 response_body_length_;

  // Keep track of the number of response body bytes read so far.
  int64 response_body_read_;

  // Helper if the data is chunked.
  scoped_ptr<HttpChunkedDecoder> chunked_decoder_;

  // Where the caller wants the body data.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // The callback to notify a user that their request or response is
  // complete or there was an error
  CompletionCallback callback_;

  // In the client callback, the client can do anything, including
  // destroying this class, so any pending callback must be issued
  // after everything else is done.  When it is time to issue the client
  // callback, move it from |callback_| to |scheduled_callback_|.
  CompletionCallback scheduled_callback_;

  // The underlying socket.
  ClientSocketHandle* const connection_;

  BoundNetLog net_log_;

  // Callback to be used when doing IO.
  CompletionCallback io_callback_;

  // Buffer used to read the request body from UploadDataStream.
  scoped_refptr<SeekableIOBuffer> request_body_read_buf_;
  // Buffer used to send the request body. This points the same buffer as
  // |request_body_read_buf_| unless the data is chunked.
  scoped_refptr<SeekableIOBuffer> request_body_send_buf_;
  bool sent_last_chunk_;

  base::WeakPtrFactory<HttpStreamParser> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamParser);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_PARSER_H_
