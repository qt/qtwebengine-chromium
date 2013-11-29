// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS_CLIENT_SOCKET_H_
#define NET_SOCKET_SOCKS_CLIENT_SOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/host_resolver.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/socket/stream_socket.h"

namespace net {

class ClientSocketHandle;
class BoundNetLog;

// The SOCKS client socket implementation
class NET_EXPORT_PRIVATE SOCKSClientSocket : public StreamSocket {
 public:
  // Takes ownership of the |transport_socket|, which should already be
  // connected by the time Connect() is called.
  //
  // |req_info| contains the hostname and port to which the socket above will
  // communicate to via the socks layer. For testing the referrer is optional.
  SOCKSClientSocket(ClientSocketHandle* transport_socket,
                    const HostResolver::RequestInfo& req_info,
                    HostResolver* host_resolver);

  // Deprecated constructor (http://crbug.com/37810) that takes a StreamSocket.
  SOCKSClientSocket(StreamSocket* transport_socket,
                    const HostResolver::RequestInfo& req_info,
                    HostResolver* host_resolver);

  // On destruction Disconnect() is called.
  virtual ~SOCKSClientSocket();

  // StreamSocket implementation.

  // Does the SOCKS handshake and completes the protocol.
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual bool WasNpnNegotiated() const OVERRIDE;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE;

  // Socket implementation.
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE;

  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SOCKSClientSocketTest, CompleteHandshake);
  FRIEND_TEST_ALL_PREFIXES(SOCKSClientSocketTest, SOCKS4AFailedDNS);
  FRIEND_TEST_ALL_PREFIXES(SOCKSClientSocketTest, SOCKS4AIfDomainInIPv6);

  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_HANDSHAKE_WRITE,
    STATE_HANDSHAKE_WRITE_COMPLETE,
    STATE_HANDSHAKE_READ,
    STATE_HANDSHAKE_READ_COMPLETE,
    STATE_NONE,
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoHandshakeRead();
  int DoHandshakeReadComplete(int result);
  int DoHandshakeWrite();
  int DoHandshakeWriteComplete(int result);

  const std::string BuildHandshakeWriteBuffer() const;

  // Stores the underlying socket.
  scoped_ptr<ClientSocketHandle> transport_;

  State next_state_;

  // Stores the callback to the layer above, called on completing Connect().
  CompletionCallback user_callback_;

  // This IOBuffer is used by the class to read and write
  // SOCKS handshake data. The length contains the expected size to
  // read or write.
  scoped_refptr<IOBuffer> handshake_buf_;

  // While writing, this buffer stores the complete write handshake data.
  // While reading, it stores the handshake information received so far.
  std::string buffer_;

  // This becomes true when the SOCKS handshake has completed and the
  // overlying connection is free to communicate.
  bool completed_handshake_;

  // These contain the bytes sent / received by the SOCKS handshake.
  size_t bytes_sent_;
  size_t bytes_received_;

  // Used to resolve the hostname to which the SOCKS proxy will connect.
  SingleRequestHostResolver host_resolver_;
  AddressList addresses_;
  HostResolver::RequestInfo host_request_info_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS_CLIENT_SOCKET_H_
