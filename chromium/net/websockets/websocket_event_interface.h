// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_
#define NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"  // for WARN_UNUSED_RESULT
#include "net/base/net_export.h"

namespace net {

// Interface for events sent from the network layer to the content layer. These
// events will generally be sent as-is to the renderer process.
class NET_EXPORT WebSocketEventInterface {
 public:
  typedef int WebSocketMessageType;

  // Any event can cause the Channel to be deleted. The Channel needs to avoid
  // doing further processing in this case. It does not need to do cleanup, as
  // cleanup will already have been done as a result of the deletion.
  enum ChannelState {
    CHANNEL_ALIVE,
    CHANNEL_DELETED
  };

  virtual ~WebSocketEventInterface() {}
  // Called in response to an AddChannelRequest. This generally means that a
  // response has been received from the remote server, but the response might
  // have been generated internally. If |fail| is true, the channel cannot be
  // used and should be deleted, returning CHANNEL_DELETED.
  virtual ChannelState OnAddChannelResponse(
      bool fail,
      const std::string& selected_subprotocol) WARN_UNUSED_RESULT = 0;

  // Called when a data frame has been received from the remote host and needs
  // to be forwarded to the renderer process.
  virtual ChannelState OnDataFrame(
      bool fin,
      WebSocketMessageType type,
      const std::vector<char>& data) WARN_UNUSED_RESULT = 0;

  // Called to provide more send quota for this channel to the renderer
  // process. Currently the quota units are always bytes of message body
  // data. In future it might depend on the type of multiplexing in use.
  virtual ChannelState OnFlowControl(int64 quota) WARN_UNUSED_RESULT = 0;

  // Called when the remote server has Started the WebSocket Closing
  // Handshake. The client should not attempt to send any more messages after
  // receiving this message. It will be followed by OnDropChannel() when the
  // closing handshake is complete.
  virtual ChannelState OnClosingHandshake() WARN_UNUSED_RESULT = 0;

  // Called when the channel has been dropped, either due to a network close, a
  // network error, or a protocol error. This may or may not be preceeded by a
  // call to OnClosingHandshake().
  //
  // Warning: Both the |code| and |reason| are passed through to Javascript, so
  // callers must take care not to provide details that could be useful to
  // attackers attempting to use WebSockets to probe networks.
  //
  // The channel should not be used again after OnDropChannel() has been
  // called.
  //
  // This method returns a ChannelState for consistency, but all implementations
  // must delete the Channel and return CHANNEL_DELETED.
  virtual ChannelState OnDropChannel(uint16 code, const std::string& reason)
      WARN_UNUSED_RESULT = 0;

 protected:
  WebSocketEventInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketEventInterface);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_
