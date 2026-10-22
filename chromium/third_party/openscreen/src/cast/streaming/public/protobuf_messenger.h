// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_PUBLIC_PROTOBUF_MESSENGER_H_
#define CAST_STREAMING_PUBLIC_PROTOBUF_MESSENGER_H_

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "google/protobuf/message_lite.h"
#include "platform/base/span.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

// This class handles the common tasks of parsing and serializing protobuf
// messages for Cast Streaming messengers.
template <typename T>
  requires std::is_base_of_v<::google::protobuf::MessageLite, T>
class ProtobufMessenger {
 public:
  using SendMessageCallback = std::function<void(std::vector<uint8_t>)>;
  using ReceiveMessageCallback = std::function<void(std::unique_ptr<T>)>;

  explicit ProtobufMessenger(SendMessageCallback send_message_cb)
      : ProtobufMessenger(std::move(send_message_cb), {}) {}

  ProtobufMessenger(SendMessageCallback send_message_cb,
                    ReceiveMessageCallback receive_message_cb)
      : send_message_cb_(std::move(send_message_cb)),
        receive_message_cb_(std::move(receive_message_cb)) {
    OSP_DCHECK(send_message_cb_);
  }

  ProtobufMessenger(const ProtobufMessenger&) = delete;
  ProtobufMessenger& operator=(const ProtobufMessenger&) = delete;

  ProtobufMessenger(ProtobufMessenger&& other) noexcept = default;
  ProtobufMessenger& operator=(ProtobufMessenger&& other) noexcept = default;

  virtual ~ProtobufMessenger() = default;

  // Distributes an incoming message to the subclass.
  // The `message` should be already base64-decoded.
  void ProcessMessageFromRemote(ByteView message) {
    auto proto = std::make_unique<T>();
    if (proto->ParseFromArray(message.data(), message.size())) {
      OnMessage(std::move(proto));
    } else {
      OSP_DLOG_WARN << "Failed to parse protobuf message from remote";
    }
  }

  // Executes the `send_message_cb_` using `message`.
  void SendMessageToRemote(const T& message) {
    std::vector<uint8_t> serialized(message.ByteSizeLong());
    if (message.SerializeToArray(serialized.data(), serialized.size())) {
      send_message_cb_(std::move(serialized));
    } else {
      OSP_DLOG_WARN << "Failed to serialize protobuf message for remote";
    }
  }

  void SetReceiveMessageCallback(ReceiveMessageCallback receive_message_cb) {
    receive_message_cb_ = std::move(receive_message_cb);
  }

  const ReceiveMessageCallback& receive_message_cb() const {
    return receive_message_cb_;
  }

 protected:
  virtual void OnMessage(std::unique_ptr<T> message) {
    if (receive_message_cb_) {
      receive_message_cb_(std::move(message));
    }
  }

  SendMessageCallback send_message_cb_;
  ReceiveMessageCallback receive_message_cb_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_PUBLIC_PROTOBUF_MESSENGER_H_
