// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_CONNECTION_H_
#define PLATFORM_API_TLS_CONNECTION_H_

#include <cstdint>
#include <vector>

#include "platform/api/connection.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/span.h"

namespace openscreen {

class TlsConnection : public Connection {
 public:
  // Get the connected remote address.
  virtual IPEndpoint GetRemoteEndpoint() const = 0;

 protected:
  TlsConnection();
};

}  // namespace openscreen

#endif  // PLATFORM_API_TLS_CONNECTION_H_
