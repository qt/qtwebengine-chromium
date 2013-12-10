// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"

#include "base/lazy_instance.h"
#include "net/quic/quic_crypto_client_stream.h"

using std::string;

namespace net {

MockCryptoClientStreamFactory::MockCryptoClientStreamFactory()
    : handshake_mode_(MockCryptoClientStream::CONFIRM_HANDSHAKE),
      last_stream_(NULL) {
}

QuicCryptoClientStream*
MockCryptoClientStreamFactory::CreateQuicCryptoClientStream(
    const string& server_hostname,
    QuicSession* session,
    QuicCryptoClientConfig* crypto_config) {
  last_stream_ = new MockCryptoClientStream(server_hostname, session,
                                            crypto_config, handshake_mode_);
  return last_stream_;
}

}  // namespace net
