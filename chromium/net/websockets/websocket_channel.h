// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_CHANNEL_H_
#define NET_WEBSOCKETS_WEBSOCKET_CHANNEL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"  // for WARN_UNUSED_RESULT
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_stream.h"
#include "url/gurl.h"

namespace net {

class BoundNetLog;
class IOBuffer;
class URLRequestContext;

// Transport-independent implementation of WebSockets. Implements protocol
// semantics that do not depend on the underlying transport. Provides the
// interface to the content layer. Some WebSocket concepts are used here without
// definition; please see the RFC at http://tools.ietf.org/html/rfc6455 for
// clarification.
class NET_EXPORT WebSocketChannel {
 public:
  // The type of a WebSocketStream creator callback. Must match the signature of
  // WebSocketStream::CreateAndConnectStream().
  typedef base::Callback<scoped_ptr<WebSocketStreamRequest>(
      const GURL&,
      const std::vector<std::string>&,
      const GURL&,
      URLRequestContext*,
      const BoundNetLog&,
      scoped_ptr<WebSocketStream::ConnectDelegate>)> WebSocketStreamCreator;

  // Creates a new WebSocketChannel in an idle state.
  // SendAddChannelRequest() must be called immediately afterwards to start the
  // connection process.
  WebSocketChannel(scoped_ptr<WebSocketEventInterface> event_interface,
                   URLRequestContext* url_request_context);
  virtual ~WebSocketChannel();

  // Starts the connection process.
  void SendAddChannelRequest(
      const GURL& socket_url,
      const std::vector<std::string>& requested_protocols,
      const GURL& origin);

  // Sends a data frame to the remote side. The frame should usually be no
  // larger than 32KB to prevent the time required to copy the buffers from from
  // unduly delaying other tasks that need to run on the IO thread. This method
  // has a hard limit of 2GB. It is the responsibility of the caller to ensure
  // that they have sufficient send quota to send this data, otherwise the
  // connection will be closed without sending. |fin| indicates the last frame
  // in a message, equivalent to "FIN" as specified in section 5.2 of
  // RFC6455. |data| is the "Payload Data". If |op_code| is kOpCodeText, or it
  // is kOpCodeContinuation and the type the message is Text, then |data| must
  // be a chunk of a valid UTF-8 message, however there is no requirement for
  // |data| to be split on character boundaries.
  void SendFrame(bool fin,
                 WebSocketFrameHeader::OpCode op_code,
                 const std::vector<char>& data);

  // Sends |quota| units of flow control to the remote side. If the underlying
  // transport has a concept of |quota|, then it permits the remote server to
  // send up to |quota| units of data.
  void SendFlowControl(int64 quota);

  // Starts the closing handshake for a client-initiated shutdown of the
  // connection. There is no API to close the connection without a closing
  // handshake, but destroying the WebSocketChannel object while connected will
  // effectively do that. |code| must be in the range 1000-4999. |reason| should
  // be a valid UTF-8 string or empty.
  //
  // This does *not* trigger the event OnClosingHandshake(). The caller should
  // assume that the closing handshake has started and perform the equivalent
  // processing to OnClosingHandshake() if necessary.
  void StartClosingHandshake(uint16 code, const std::string& reason);

  // Starts the connection process, using a specified creator callback rather
  // than the default. This is exposed for testing.
  void SendAddChannelRequestForTesting(
      const GURL& socket_url,
      const std::vector<std::string>& requested_protocols,
      const GURL& origin,
      const WebSocketStreamCreator& creator);

  // The default timout for the closing handshake is a sensible value (see
  // kClosingHandshakeTimeoutSeconds in websocket_channel.cc). However, we can
  // set it to a very small value for testing purposes.
  void SetClosingHandshakeTimeoutForTesting(base::TimeDelta delay);

 private:
  // Methods which return a value of type ChannelState may delete |this|. If the
  // return value is CHANNEL_DELETED, then the caller must return without making
  // any further access to member variables or methods.
  typedef WebSocketEventInterface::ChannelState ChannelState;

  // The object passes through a linear progression of states from
  // FRESHLY_CONSTRUCTED to CLOSED, except that the SEND_CLOSED and RECV_CLOSED
  // states may be skipped in case of error.
  enum State {
    FRESHLY_CONSTRUCTED,
    CONNECTING,
    CONNECTED,
    SEND_CLOSED,  // A Close frame has been sent but not received.
    RECV_CLOSED,  // Used briefly between receiving a Close frame and sending
                  // the response. Once the response is sent, the state changes
                  // to CLOSED.
    CLOSE_WAIT,   // The Closing Handshake has completed, but the remote server
                  // has not yet closed the connection.
    CLOSED,       // The Closing Handshake has completed and the connection
                  // has been closed; or the connection is failed.
  };

  // When failing a channel, sometimes it is inappropriate to expose the real
  // reason for failing to the remote server. This enum is used by FailChannel()
  // to select between sending the real status or a "Going Away" status.
  enum ExposeError {
    SEND_REAL_ERROR,
    SEND_GOING_AWAY,
  };

  // Implementation of WebSocketStream::ConnectDelegate for
  // WebSocketChannel. WebSocketChannel does not inherit from
  // WebSocketStream::ConnectDelegate directly to avoid cluttering the public
  // interface with the implementation of those methods, and because the
  // lifetime of a WebSocketChannel is longer than the lifetime of the
  // connection process.
  class ConnectDelegate;

  // Starts the connection process, using the supplied creator callback.
  void SendAddChannelRequestWithSuppliedCreator(
      const GURL& socket_url,
      const std::vector<std::string>& requested_protocols,
      const GURL& origin,
      const WebSocketStreamCreator& creator);

  // Success callback from WebSocketStream::CreateAndConnectStream(). Reports
  // success to the event interface. May delete |this|.
  void OnConnectSuccess(scoped_ptr<WebSocketStream> stream);

  // Failure callback from WebSocketStream::CreateAndConnectStream(). Reports
  // failure to the event interface. May delete |this|.
  void OnConnectFailure(uint16 websocket_error);

  // Returns true if state_ is SEND_CLOSED, CLOSE_WAIT or CLOSED.
  bool InClosingState() const;

  // Calls WebSocketStream::WriteFrames() with the appropriate arguments
  ChannelState WriteFrames() WARN_UNUSED_RESULT;

  // Callback from WebSocketStream::WriteFrames. Sends pending data or adjusts
  // the send quota of the renderer channel as appropriate. |result| is a net
  // error code, usually OK. If |synchronous| is true, then OnWriteDone() is
  // being called from within the WriteFrames() loop and does not need to call
  // WriteFrames() itself.
  ChannelState OnWriteDone(bool synchronous, int result) WARN_UNUSED_RESULT;

  // Calls WebSocketStream::ReadFrames() with the appropriate arguments.
  ChannelState ReadFrames() WARN_UNUSED_RESULT;

  // Callback from WebSocketStream::ReadFrames. Handles any errors and processes
  // the returned chunks appropriately to their type. |result| is a net error
  // code. If |synchronous| is true, then OnReadDone() is being called from
  // within the ReadFrames() loop and does not need to call ReadFrames() itself.
  ChannelState OnReadDone(bool synchronous, int result) WARN_UNUSED_RESULT;

  // Processes a single frame that has been read from the stream.
  ChannelState ProcessFrame(
      scoped_ptr<WebSocketFrame> frame) WARN_UNUSED_RESULT;

  // Handles a frame that the object has received enough of to process. May call
  // |event_interface_| methods, send responses to the server, and change the
  // value of |state_|.
  ChannelState HandleFrame(const WebSocketFrameHeader::OpCode opcode,
                           bool final,
                           const scoped_refptr<IOBuffer>& data_buffer,
                           size_t size) WARN_UNUSED_RESULT;

  // Low-level method to send a single frame. Used for both data and control
  // frames. Either sends the frame immediately or buffers it to be scheduled
  // when the current write finishes. |fin| and |op_code| are defined as for
  // SendFrame() above, except that |op_code| may also be a control frame
  // opcode.
  ChannelState SendIOBuffer(bool fin,
                            WebSocketFrameHeader::OpCode op_code,
                            const scoped_refptr<IOBuffer>& buffer,
                            size_t size) WARN_UNUSED_RESULT;

  // Performs the "Fail the WebSocket Connection" operation as defined in
  // RFC6455. The supplied code and reason are sent back to the renderer in an
  // OnDropChannel message. If state_ is CONNECTED then a Close message is sent
  // to the remote host. If |expose| is SEND_REAL_ERROR then the remote host is
  // given the same status code passed to the renderer; otherwise it is sent a
  // fixed "Going Away" code.  Closes the stream_ and sets state_ to CLOSED.
  // FailChannel() always returns CHANNEL_DELETED. It is not valid to access any
  // member variables or methods after calling FailChannel().
  ChannelState FailChannel(ExposeError expose,
                           uint16 code,
                           const std::string& reason) WARN_UNUSED_RESULT;

  // Sends a Close frame to Start the WebSocket Closing Handshake, or to respond
  // to a Close frame from the server. As a special case, setting |code| to
  // kWebSocketErrorNoStatusReceived will create a Close frame with no payload;
  // this is symmetric with the behaviour of ParseClose.
  ChannelState SendClose(uint16 code,
                         const std::string& reason) WARN_UNUSED_RESULT;

  // Parses a Close frame. If no status code is supplied, then |code| is set to
  // 1005 (No status code) with empty |reason|. If the supplied code is
  // outside the valid range, then 1002 (Protocol error) is set instead. If the
  // reason text is not valid UTF-8, then |reason| is set to an empty string
  // instead.
  void ParseClose(const scoped_refptr<IOBuffer>& buffer,
                  size_t size,
                  uint16* code,
                  std::string* reason);

  // Called if the closing handshake times out. Closes the connection and
  // informs the |event_interface_| if appropriate.
  void CloseTimeout();

  // The URL of the remote server.
  GURL socket_url_;

  // The object receiving events.
  const scoped_ptr<WebSocketEventInterface> event_interface_;

  // The URLRequestContext to pass to the WebSocketStream creator.
  URLRequestContext* const url_request_context_;

  // The WebSocketStream on which to send and receive data.
  scoped_ptr<WebSocketStream> stream_;

  // A data structure containing a vector of frames to be sent and the total
  // number of bytes contained in the vector.
  class SendBuffer;
  // Data that is currently pending write, or NULL if no write is pending.
  scoped_ptr<SendBuffer> data_being_sent_;
  // Data that is queued up to write after the current write completes.
  // Only non-NULL when such data actually exists.
  scoped_ptr<SendBuffer> data_to_send_next_;

  // Destination for the current call to WebSocketStream::ReadFrames
  ScopedVector<WebSocketFrame> read_frames_;

  // Handle to an in-progress WebSocketStream creation request. Only non-NULL
  // during the connection process.
  scoped_ptr<WebSocketStreamRequest> stream_request_;

  // If the renderer's send quota reaches this level, it is sent a quota
  // refresh. "quota units" are currently bytes. TODO(ricea): Update the
  // definition of quota units when necessary.
  int send_quota_low_water_mark_;
  // The level the quota is refreshed to when it reaches the low_water_mark
  // (quota units).
  int send_quota_high_water_mark_;
  // The current amount of quota that the renderer has available for sending
  // on this logical channel (quota units).
  int current_send_quota_;

  // Timer for the closing handshake.
  base::OneShotTimer<WebSocketChannel> timer_;

  // Timeout for the closing handshake.
  base::TimeDelta timeout_;

  // Storage for the status code and reason from the time the Close frame
  // arrives until the connection is closed and they are passed to
  // OnDropChannel().
  uint16 closing_code_;
  std::string closing_reason_;

  // The current state of the channel. Mainly used for sanity checking, but also
  // used to track the close state.
  State state_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketChannel);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_CHANNEL_H_
