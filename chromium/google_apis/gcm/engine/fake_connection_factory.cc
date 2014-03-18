// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/fake_connection_factory.h"

#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/socket/stream_socket.h"

namespace gcm {

FakeConnectionFactory::FakeConnectionFactory() {
}

FakeConnectionFactory::~FakeConnectionFactory() {
}

void FakeConnectionFactory::Initialize(
    const BuildLoginRequestCallback& request_builder,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback) {
  request_builder_ = request_builder;
  connection_handler_.reset(new FakeConnectionHandler(read_callback,
                                                      write_callback));
}

ConnectionHandler* FakeConnectionFactory::GetConnectionHandler() const {
  return connection_handler_.get();
}

void FakeConnectionFactory::Connect() {
  mcs_proto::LoginRequest login_request;
  request_builder_.Run(&login_request);
  connection_handler_->Init(login_request, scoped_ptr<net::StreamSocket>());
}

bool FakeConnectionFactory::IsEndpointReachable() const {
  return connection_handler_.get() && connection_handler_->CanSendMessage();
}

base::TimeTicks FakeConnectionFactory::NextRetryAttempt() const {
  return base::TimeTicks();
}

}  // namespace gcm
