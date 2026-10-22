// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/receiver/public/receiver_socket_factory.h"

#include "platform/api/tls_connection.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

ReceiverSocketFactory::Client::~Client() = default;

ReceiverSocketFactory::ReceiverSocketFactory(Client& client,
                                             CastSocket::Client& socket_client)
    : client_(client), socket_client_(socket_client) {}

ReceiverSocketFactory::~ReceiverSocketFactory() = default;

void ReceiverSocketFactory::OnAccepted(
    TlsConnectionFactory* factory,
    std::vector<uint8_t> der_x509_peer_cert,
    std::unique_ptr<TlsConnection> connection) {
  CreateSocket(std::move(connection));
}

void ReceiverSocketFactory::OnConnected(
    TlsConnectionFactory* factory,
    std::vector<uint8_t> der_x509_peer_cert,
    std::unique_ptr<TlsConnection> connection) {
  OSP_LOG_FATAL << "This factory is accept-only";
}

void ReceiverSocketFactory::OnConnectionFailed(
    TlsConnectionFactory* factory,
    const IPEndpoint& remote_address) {
  client_.OnError(this, Error(Error::Code::kConnectionFailed,
                              "Accepting connection failed."));
}

void ReceiverSocketFactory::OnError(TlsConnectionFactory* factory,
                                    const Error& error) {
  client_.OnError(this, error);
}

void ReceiverSocketFactory::CreateSocket(
    std::unique_ptr<Connection> connection) {
  IPEndpoint endpoint = connection->GetRemoteEndpoint();
  auto socket =
      std::make_unique<CastSocket>(std::move(connection), &socket_client_);
  client_.OnConnected(this, endpoint, std::move(socket));
}

}  // namespace openscreen::cast
