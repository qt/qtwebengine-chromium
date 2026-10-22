// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_PUBLIC_RPC_MESSENGER_H_
#define CAST_STREAMING_PUBLIC_RPC_MESSENGER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cast/streaming/public/protobuf_messenger.h"
#include "cast/streaming/remoting.pb.h"
#include "util/flat_map.h"
#include "util/weak_ptr.h"

namespace openscreen::cast {

// Processes incoming and outgoing RPC messages and links them to desired
// components on both end points. For outgoing messages, the messenger
// must send an RPC message with associated handle value. On the messagee side,
// the message is sent to a pre-registered component using that handle.
// Before RPC communication starts, both sides need to negotiate the handle
// value in the existing RPC communication channel using the special handles
// |kAcquire*Handle|.
class RpcMessenger final : public ProtobufMessenger<RpcMessage> {
 public:
  using Handle = int;
  using ReceiveMessageCallback =
      std::function<void(std::unique_ptr<RpcMessage>)>;

  using ProtobufMessenger<RpcMessage>::ProtobufMessenger;
  ~RpcMessenger() override;

  // Get unique handle value for RPC message handles.
  Handle GetUniqueHandle();

  // Register a component to receive messages via the given
  // ReceiveMessageCallback. `handle` is a unique handle value provided by a
  // prior call to GetUniqueHandle() and is used to reference the component in
  // the RPC messages. The receiver can then use it to direct an RPC message
  // back to a specific component.
  void RegisterMessageReceiverCallback(Handle handle,
                                       ReceiveMessageCallback callback);

  // Allows components to unregister in order to stop receiving message.
  void UnregisterMessageReceiverCallback(Handle handle);

  using ProtobufMessenger<RpcMessage>::ProcessMessageFromRemote;

  // This overload distributes an already-deserialized message to the
  // registered component.
  void ProcessMessageFromRemote(std::unique_ptr<RpcMessage> message);

  // Executes the `send_message_cb_` using `rpc`.
  void SendMessageToRemote(const RpcMessage& rpc);

  // Checks if the handle is registered for receiving messages. Test-only.
  bool IsRegisteredForTesting(Handle handle);

  // Weak pointer creator.
  WeakPtr<RpcMessenger> GetWeakPtr();

  // Predefined invalid handle value for RPC message.
  static constexpr Handle kInvalidHandle = -1;

  // Predefined handle values for RPC messages related to initialization (before
  // the receiver handle(s) are known).
  static constexpr Handle kAcquireRendererHandle = 0;
  static constexpr Handle kAcquireDemuxerHandle = 1;

  // The first handle to return from GetUniqueHandle().
  static constexpr Handle kFirstHandle = 100;

 protected:
  void OnMessage(std::unique_ptr<RpcMessage> message) override;

 private:
  // Next unique handle to return from GetUniqueHandle().
  Handle next_handle_ = kFirstHandle;

  // Maps of handle values to associated MessageReceivers.
  FlatMap<Handle, ReceiveMessageCallback> receive_callbacks_;

  WeakPtrFactory<RpcMessenger> weak_factory_{this};
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_PUBLIC_RPC_MESSENGER_H_
