// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_P2P_SOCKET_CLIENT_H_
#define CONTENT_PUBLIC_RENDERER_P2P_SOCKET_CLIENT_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/common/p2p_socket_type.h"
#include "net/base/ip_endpoint.h"

namespace content {

class P2PSocketClientDelegate;

// P2P socket that routes all calls over IPC.
// Note that while ref-counting is thread-safe, all methods must be
// called on the same thread.
class CONTENT_EXPORT P2PSocketClient :
      public base::RefCountedThreadSafe<P2PSocketClient> {
 public:
  // Create a new P2PSocketClient() of the specified |type| and connected to
  // the specified |address|. |address| matters only when |type| is set to
  // P2P_SOCKET_TCP_CLIENT. The methods on the returned socket may only be
  // called on the same thread that created it.
  static scoped_refptr<P2PSocketClient> Create(
      P2PSocketType type,
      const net::IPEndPoint& local_address,
      const net::IPEndPoint& remote_address,
      P2PSocketClientDelegate* delegate);

  P2PSocketClient() {}

  // Send the |data| to the |address|.
  virtual void Send(const net::IPEndPoint& address,
                    const std::vector<char>& data) = 0;

  // Send the |data| to the |address| using Differentiated Services Code Point
  // |dscp|.
  virtual void SendWithDscp(const net::IPEndPoint& address,
                            const std::vector<char>& data,
                            net::DiffServCodePoint dscp) = 0;

  // Must be called before the socket is destroyed.
  virtual void Close() = 0;

  virtual int GetSocketID() const = 0;
  virtual void SetDelegate(P2PSocketClientDelegate* delegate) = 0;

 protected:
  virtual ~P2PSocketClient() {}

 private:
  // Calls destructor.
  friend class base::RefCountedThreadSafe<P2PSocketClient>;
};
}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_P2P_SOCKET_CLIENT_H_
