// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/socket_dispatcher.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/child/child_process.h"
#include "content/common/p2p_messages.h"
#include "content/renderer/p2p/host_address_request.h"
#include "content/renderer/p2p/network_list_observer.h"
#include "content/renderer/p2p/socket_client_impl.h"
#include "content/renderer/render_view_impl.h"

namespace content {

P2PSocketDispatcher::P2PSocketDispatcher(
    base::MessageLoopProxy* ipc_message_loop)
    : message_loop_(ipc_message_loop),
      network_notifications_started_(false),
      network_list_observers_(
          new ObserverListThreadSafe<NetworkListObserver>()),
      channel_(NULL) {
}

P2PSocketDispatcher::~P2PSocketDispatcher() {
  network_list_observers_->AssertEmpty();
  for (IDMap<P2PSocketClientImpl>::iterator i(&clients_); !i.IsAtEnd();
       i.Advance()) {
    i.GetCurrentValue()->Detach();
  }
}

void P2PSocketDispatcher::AddNetworkListObserver(
    NetworkListObserver* network_list_observer) {
  network_list_observers_->AddObserver(network_list_observer);
  network_notifications_started_ = true;
  SendP2PMessage(new P2PHostMsg_StartNetworkNotifications());
}

void P2PSocketDispatcher::RemoveNetworkListObserver(
    NetworkListObserver* network_list_observer) {
  network_list_observers_->RemoveObserver(network_list_observer);
}

void P2PSocketDispatcher::Send(IPC::Message* message) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!channel_) {
    DLOG(WARNING) << "P2PSocketDispatcher::Send() - Channel closed.";
    delete message;
    return;
  }

  channel_->Send(message);
}

bool P2PSocketDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(P2PSocketDispatcher, message)
    IPC_MESSAGE_HANDLER(P2PMsg_NetworkListChanged, OnNetworkListChanged)
    IPC_MESSAGE_HANDLER(P2PMsg_GetHostAddressResult, OnGetHostAddressResult)
    IPC_MESSAGE_HANDLER(P2PMsg_OnSocketCreated, OnSocketCreated)
    IPC_MESSAGE_HANDLER(P2PMsg_OnIncomingTcpConnection, OnIncomingTcpConnection)
    IPC_MESSAGE_HANDLER(P2PMsg_OnSendComplete, OnSendComplete)
    IPC_MESSAGE_HANDLER(P2PMsg_OnError, OnError)
    IPC_MESSAGE_HANDLER(P2PMsg_OnDataReceived, OnDataReceived)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void P2PSocketDispatcher::OnFilterAdded(IPC::Channel* channel) {
  DVLOG(1) << "P2PSocketDispatcher::OnFilterAdded()";
  channel_ = channel;
}

void P2PSocketDispatcher::OnFilterRemoved() {
  channel_ = NULL;
}

void P2PSocketDispatcher::OnChannelClosing() {
  channel_ = NULL;
}

base::MessageLoopProxy* P2PSocketDispatcher::message_loop() {
  return message_loop_.get();
}

int P2PSocketDispatcher::RegisterClient(P2PSocketClientImpl* client) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return clients_.Add(client);
}

void P2PSocketDispatcher::UnregisterClient(int id) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  clients_.Remove(id);
}

void P2PSocketDispatcher::SendP2PMessage(IPC::Message* msg) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&P2PSocketDispatcher::Send,
                                       this, msg));
    return;
  }
  Send(msg);
}

int P2PSocketDispatcher::RegisterHostAddressRequest(
    P2PHostAddressRequest* request) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return host_address_requests_.Add(request);
}

void P2PSocketDispatcher::UnregisterHostAddressRequest(int id) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  host_address_requests_.Remove(id);
}

void P2PSocketDispatcher::OnNetworkListChanged(
    const net::NetworkInterfaceList& networks) {
  network_list_observers_->Notify(
      &NetworkListObserver::OnNetworkListChanged, networks);
}

void P2PSocketDispatcher::OnGetHostAddressResult(
    int32 request_id,
    const net::IPAddressNumber& address) {
  P2PHostAddressRequest* request = host_address_requests_.Lookup(request_id);
  if (!request) {
    VLOG(1) << "Received P2P message for socket that doesn't exist.";
    return;
  }

  request->OnResponse(address);
}

void P2PSocketDispatcher::OnSocketCreated(
    int socket_id, const net::IPEndPoint& address) {
  P2PSocketClientImpl* client = GetClient(socket_id);
  if (client) {
    client->OnSocketCreated(address);
  }
}

void P2PSocketDispatcher::OnIncomingTcpConnection(
    int socket_id, const net::IPEndPoint& address) {
  P2PSocketClientImpl* client = GetClient(socket_id);
  if (client) {
    client->OnIncomingTcpConnection(address);
  }
}

void P2PSocketDispatcher::OnSendComplete(int socket_id) {
  P2PSocketClientImpl* client = GetClient(socket_id);
  if (client) {
    client->OnSendComplete();
  }
}

void P2PSocketDispatcher::OnError(int socket_id) {
  P2PSocketClientImpl* client = GetClient(socket_id);
  if (client) {
    client->OnError();
  }
}

void P2PSocketDispatcher::OnDataReceived(
    int socket_id, const net::IPEndPoint& address,
    const std::vector<char>& data,
    const base::TimeTicks& timestamp) {
  P2PSocketClientImpl* client = GetClient(socket_id);
  if (client) {
    client->OnDataReceived(address, data, timestamp);
  }
}

P2PSocketClientImpl* P2PSocketDispatcher::GetClient(int socket_id) {
  P2PSocketClientImpl* client = clients_.Lookup(socket_id);
  if (client == NULL) {
    // This may happen if the socket was closed, but the browser side
    // hasn't processed the close message by the time it sends the
    // message to the renderer.
    VLOG(1) << "Received P2P message for socket that doesn't exist.";
    return NULL;
  }

  return client;
}

}  // namespace content
