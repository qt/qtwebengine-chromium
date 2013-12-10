// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SOCKET_PERMISSION_REQUEST_H_
#define CONTENT_PUBLIC_COMMON_SOCKET_PERMISSION_REQUEST_H_

#include <string>

namespace content {

// This module provides helper types for checking socket permission.

struct SocketPermissionRequest {
  enum OperationType {
    NONE = 0,
    TCP_CONNECT,
    TCP_LISTEN,
    UDP_BIND,
    UDP_SEND_TO,
    UDP_MULTICAST_MEMBERSHIP,
    RESOLVE_HOST,
    RESOLVE_PROXY,
    NETWORK_STATE
  };

  SocketPermissionRequest(OperationType type,
                          const std::string& host,
                          int port)
    : type(type),
      host(host),
      port(port) {
  }

  OperationType type;
  std::string host;
  int port;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SOCKET_PERMISSION_REQUEST_H_
