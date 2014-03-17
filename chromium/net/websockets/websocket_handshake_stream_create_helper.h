// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace net {

// Implementation of WebSocketHandshakeStreamBase::CreateHelper. This class is
// used in the implementation of WebSocketStream::CreateAndConnectStream() and
// is not intended to be used by itself.
//
// Holds the information needed to construct a
// WebSocketBasicHandshakeStreamBase.
class NET_EXPORT_PRIVATE WebSocketHandshakeStreamCreateHelper
    : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  explicit WebSocketHandshakeStreamCreateHelper(
      const std::vector<std::string>& requested_subprotocols);

  virtual ~WebSocketHandshakeStreamCreateHelper();

  // WebSocketHandshakeStreamBase::CreateHelper methods

  // Create a WebSocketBasicHandshakeStream.
  virtual WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<ClientSocketHandle> connection,
      bool using_proxy) OVERRIDE;

  // Unimplemented as of November 2013.
  virtual WebSocketHandshakeStreamBase* CreateSpdyStream(
      const base::WeakPtr<SpdySession>& session,
      bool use_relative_url) OVERRIDE;

  // Return the WebSocketHandshakeStreamBase object that we created. In the case
  // where CreateBasicStream() was called more than once, returns the most
  // recent stream, which will be the one on which the handshake succeeded.
  WebSocketHandshakeStreamBase* stream() { return stream_; }

 private:
  const std::vector<std::string> requested_subprotocols_;

  // This is owned by the caller of CreateBaseStream() or
  // CreateSpdyStream(). Both the stream and this object will be destroyed
  // during the destruction of the URLRequest object associated with the
  // handshake.
  WebSocketHandshakeStreamBase* stream_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeStreamCreateHelper);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_
