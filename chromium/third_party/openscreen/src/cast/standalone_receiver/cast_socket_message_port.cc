// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_socket_message_port.h"

#include <utility>

#include "cast/common/channel/proto/cast_channel.pb.h"

namespace openscreen {
namespace cast {

CastSocketMessagePort::CastSocketMessagePort() = default;
CastSocketMessagePort::~CastSocketMessagePort() = default;

// NOTE: we assume here that this message port is already the client for
// the passed in socket, so leave the socket's client unchanged. However,
// since sockets should map one to one with receiver sessions, we reset our
// client. The consumer of this message port should call SetClient with the new
// message port client after setting the socket.
void CastSocketMessagePort::SetSocket(WeakPtr<CastSocket> socket) {
  client_ = nullptr;
  socket_ = socket;
}

int CastSocketMessagePort::GetSocketId() {
  return socket_ ? socket_->socket_id() : -1;
}

void CastSocketMessagePort::SetClient(MessagePort::Client* client) {
  client_ = client;
}

void CastSocketMessagePort::PostMessage(absl::string_view sender_id,
                                        absl::string_view message_namespace,
                                        absl::string_view message) {
  ::cast::channel::CastMessage cast_message;
  cast_message.set_source_id(sender_id.data(), sender_id.size());
  cast_message.set_namespace_(message_namespace.data(),
                              message_namespace.size());
  cast_message.set_payload_utf8(message.data(), message.size());

  if (!socket_) {
    client_->OnError(Error::Code::kAlreadyClosed);
    return;
  }
  Error error = socket_->Send(cast_message);
  if (!error.ok()) {
    client_->OnError(error);
  }
}

}  // namespace cast
}  // namespace openscreen
