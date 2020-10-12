// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_CAST_SOCKET_MESSAGE_PORT_H_
#define CAST_STANDALONE_RECEIVER_CAST_SOCKET_MESSAGE_PORT_H_

#include <memory>
#include <string>
#include <vector>

#include "cast/common/public/cast_socket.h"
#include "cast/streaming/receiver_session.h"
#include "util/weak_ptr.h"

namespace openscreen {
namespace cast {

class CastSocketMessagePort : public MessagePort {
 public:
  CastSocketMessagePort();
  ~CastSocketMessagePort() override;

  void SetSocket(WeakPtr<CastSocket> socket);

  // Returns current socket identifier, or -1 if not connected.
  int GetSocketId();

  // MessagePort overrides.
  void SetClient(MessagePort::Client* client) override;
  void PostMessage(absl::string_view sender_id,
                   absl::string_view message_namespace,
                   absl::string_view message) override;

 private:
  MessagePort::Client* client_ = nullptr;
  WeakPtr<CastSocket> socket_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_CAST_SOCKET_MESSAGE_PORT_H_
