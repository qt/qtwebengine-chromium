// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/rpc_messenger.h"

#include <memory>
#include <string>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::cast {

namespace {

std::ostream& operator<<(std::ostream& out, const RpcMessage& message) {
  out << "handle=" << message.handle() << ", proc=" << message.proc();
  switch (message.rpc_oneof_case()) {
    case RpcMessage::kIntegerValue:
      return out << ", integer_value=" << message.integer_value();
    case RpcMessage::kInteger64Value:
      return out << ", integer64_value=" << message.integer64_value();
    case RpcMessage::kDoubleValue:
      return out << ", double_value=" << message.double_value();
    case RpcMessage::kBooleanValue:
      return out << ", boolean_value=" << message.boolean_value();
    case RpcMessage::kStringValue:
      return out << ", string_value=" << message.string_value();
    default:
      return out << ", rpc_oneof=" << message.rpc_oneof_case();
  }

  OSP_NOTREACHED();
}

}  // namespace

constexpr RpcMessenger::Handle RpcMessenger::kInvalidHandle;
constexpr RpcMessenger::Handle RpcMessenger::kAcquireRendererHandle;
constexpr RpcMessenger::Handle RpcMessenger::kAcquireDemuxerHandle;
constexpr RpcMessenger::Handle RpcMessenger::kFirstHandle;

RpcMessenger::~RpcMessenger() {
  receive_callbacks_.clear();
}

RpcMessenger::Handle RpcMessenger::GetUniqueHandle() {
  return next_handle_++;
}

void RpcMessenger::RegisterMessageReceiverCallback(
    RpcMessenger::Handle handle,
    ReceiveMessageCallback callback) {
  OSP_CHECK(receive_callbacks_.find(handle) == receive_callbacks_.end())
      << "must deregister before re-registering";
  receive_callbacks_.emplace_back(handle, std::move(callback));
}

void RpcMessenger::UnregisterMessageReceiverCallback(
    RpcMessenger::Handle handle) {
  receive_callbacks_.erase_key(handle);
}

void RpcMessenger::ProcessMessageFromRemote(
    std::unique_ptr<RpcMessage> message) {
  OnMessage(std::move(message));
}

void RpcMessenger::SendMessageToRemote(const RpcMessage& rpc) {
  OSP_VLOG << "Sending RPC message: " << rpc;
  ProtobufMessenger<RpcMessage>::SendMessageToRemote(rpc);
}

bool RpcMessenger::IsRegisteredForTesting(RpcMessenger::Handle handle) {
  return receive_callbacks_.find(handle) != receive_callbacks_.end();
}

WeakPtr<RpcMessenger> RpcMessenger::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RpcMessenger::OnMessage(std::unique_ptr<RpcMessage> message) {
  const auto entry = receive_callbacks_.find(message->handle());
  if (entry == receive_callbacks_.end()) {
    OSP_VLOG << "Dropping message due to unregistered handle: "
             << message->handle();
    return;
  }
  entry->second(std::move(message));
}

}  // namespace openscreen::cast
