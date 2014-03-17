// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class BoundNetLog;
class URLRequestContext;
struct WebSocketFrame;

// WebSocketStreamRequest is the caller's handle to the process of creation of a
// WebSocketStream. Deleting the object before the OnSuccess or OnFailure
// callbacks are called will cancel the request (and neither callback will be
// called). After OnSuccess or OnFailure have been called, this object may be
// safely deleted without side-effects.
class NET_EXPORT_PRIVATE WebSocketStreamRequest {
 public:
  virtual ~WebSocketStreamRequest();
};

// WebSocketStream is a transport-agnostic interface for reading and writing
// WebSocket frames. This class provides an abstraction for WebSocket streams
// based on various transport layers, such as normal WebSocket connections
// (WebSocket protocol upgraded from HTTP handshake), SPDY transports, or
// WebSocket connections with multiplexing extension. Subtypes of
// WebSocketStream are responsible for managing the underlying transport
// appropriately.
//
// All functions except Close() can be asynchronous. If an operation cannot
// be finished synchronously, the function returns ERR_IO_PENDING, and
// |callback| will be called when the operation is finished. Non-null |callback|
// must be provided to these functions.

class NET_EXPORT_PRIVATE WebSocketStream {
 public:
  // A concrete object derived from ConnectDelegate is supplied by the caller to
  // CreateAndConnectStream() to receive the result of the connection.
  class NET_EXPORT_PRIVATE ConnectDelegate {
   public:
    virtual ~ConnectDelegate();
    // Called on successful connection. The parameter is an object derived from
    // WebSocketStream.
    virtual void OnSuccess(scoped_ptr<WebSocketStream> stream) = 0;

    // Called on failure to connect. The parameter is either one of the values
    // defined in net::WebSocketError, or an error defined by some WebSocket
    // extension protocol that we implement.
    virtual void OnFailure(unsigned short websocket_error) = 0;
  };

  // Create and connect a WebSocketStream of an appropriate type. The actual
  // concrete type returned depends on whether multiplexing or SPDY are being
  // used to communicate with the remote server. If the handshake completed
  // successfully, then connect_delegate->OnSuccess() is called with a
  // WebSocketStream instance. If it failed, then connect_delegate->OnFailure()
  // is called with a WebSocket result code corresponding to the error. Deleting
  // the returned WebSocketStreamRequest object will cancel the connection, in
  // which case the |connect_delegate| object that the caller passed will be
  // deleted without any of its methods being called. Unless cancellation is
  // required, the caller should keep the WebSocketStreamRequest object alive
  // until connect_delegate->OnSuccess() or OnFailure() have been called, then
  // it is safe to delete.
  static scoped_ptr<WebSocketStreamRequest> CreateAndConnectStream(
      const GURL& socket_url,
      const std::vector<std::string>& requested_subprotocols,
      const GURL& origin,
      URLRequestContext* url_request_context,
      const BoundNetLog& net_log,
      scoped_ptr<ConnectDelegate> connect_delegate);

  // Derived classes must make sure Close() is called when the stream is not
  // closed on destruction.
  virtual ~WebSocketStream();

  // Reads WebSocket frame data. This operation finishes when new frame data
  // becomes available.
  //
  // |frames| remains owned by the caller and must be valid until the
  // operation completes or Close() is called. |frames| must be empty on
  // calling.
  //
  // This function should not be called while the previous call of ReadFrames()
  // is still pending.
  //
  // Returns net::OK or one of the net::ERR_* codes.
  //
  // frames->size() >= 1 if the result is OK.
  //
  // Only frames with complete header information are inserted into |frames|. If
  // the currently available bytes of a new frame do not form a complete frame
  // header, then the implementation will buffer them until all the fields in
  // the WebSocketFrameHeader object can be filled. If ReadFrames() is freshly
  // called in this situation, it will return ERR_IO_PENDING exactly as if no
  // data was available.
  //
  // Original frame boundaries are not preserved. In particular, if only part of
  // a frame is available, then the frame will be split, and the available data
  // will be returned immediately.
  //
  // When the socket is closed on the remote side, this method will return
  // ERR_CONNECTION_CLOSED. It will not return OK with an empty vector.
  //
  // If the connection is closed in the middle of receiving an incomplete frame,
  // ReadFrames may discard the incomplete frame. Since the renderer will
  // discard any incomplete messages when the connection is closed, this makes
  // no difference to the overall semantics.
  //
  // Implementations of ReadFrames() must be able to handle being deleted during
  // the execution of callback.Run(). In practice this means that the method
  // calling callback.Run() (and any calling methods in the same object) must
  // return immediately without any further method calls or access to member
  // variables. Implementors should write test(s) for this case.
  virtual int ReadFrames(ScopedVector<WebSocketFrame>* frames,
                         const CompletionCallback& callback) = 0;

  // Writes WebSocket frame data.
  //
  // |frames| must be valid until the operation completes or Close() is called.
  //
  // This function must not be called while a previous call of WriteFrames() is
  // still pending.
  //
  // This method will only return OK if all frames were written completely.
  // Otherwise it will return an appropriate net error code.
  //
  // The callback implementation is permitted to delete this
  // object. Implementations of WriteFrames() should be robust against
  // this. This generally means returning to the event loop immediately after
  // calling the callback.
  virtual int WriteFrames(ScopedVector<WebSocketFrame>* frames,
                          const CompletionCallback& callback) = 0;

  // Closes the stream. All pending I/O operations (if any) are cancelled
  // at this point, so |frames| can be freed.
  virtual void Close() = 0;

  // The subprotocol that was negotiated for the stream. If no protocol was
  // negotiated, then the empty string is returned.
  virtual std::string GetSubProtocol() const = 0;

  // The extensions that were negotiated for the stream. Since WebSocketStreams
  // can be layered, this may be different from what this particular
  // WebSocketStream implements. The primary purpose of this accessor is to make
  // the data available to Javascript. The format of the string is identical to
  // the contents of the Sec-WebSocket-Extensions header supplied by the server,
  // with some canonicalisations applied (leading and trailing whitespace
  // removed, multiple headers concatenated into one comma-separated list). See
  // RFC6455 section 9.1 for the exact format specification. If no
  // extensions were negotiated, the empty string is returned.
  virtual std::string GetExtensions() const = 0;

 protected:
  WebSocketStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStream);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
