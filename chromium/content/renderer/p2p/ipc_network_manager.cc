// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_network_manager.h"

#include "base/bind.h"
#include "base/sys_byteorder.h"
#include "net/base/net_util.h"

namespace content {

IpcNetworkManager::IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher)
    : socket_dispatcher_(socket_dispatcher),
      start_count_(0),
      network_list_received_(false),
      weak_factory_(this) {
  socket_dispatcher_->AddNetworkListObserver(this);
}

IpcNetworkManager::~IpcNetworkManager() {
  DCHECK(!start_count_);
  socket_dispatcher_->RemoveNetworkListObserver(this);
}

void IpcNetworkManager::StartUpdating() {
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&IpcNetworkManager::SendNetworksChangedSignal,
                   weak_factory_.GetWeakPtr()));
  }
  ++start_count_;
}

void IpcNetworkManager::StopUpdating() {
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void IpcNetworkManager::OnNetworkListChanged(
    const net::NetworkInterfaceList& list) {

  // Update flag if network list received for the first time.
  if (!network_list_received_)
    network_list_received_ = true;

  // Note: 32 and 64 are the arbitrary(kind of) prefix length used to
  // differentiate IPv4 and IPv6 addresses.
  // talk_base::Network uses these prefix_length to compare network
  // interfaces discovered.
  std::vector<talk_base::Network*> networks;
  for (net::NetworkInterfaceList::const_iterator it = list.begin();
       it != list.end(); it++) {
    if (it->address.size() == net::kIPv4AddressSize) {
      uint32 address;
      memcpy(&address, &it->address[0], sizeof(uint32));
      address = talk_base::NetworkToHost32(address);
      talk_base::Network* network = new talk_base::Network(
          it->name, it->name, talk_base::IPAddress(address), 32);
      network->AddIP(talk_base::IPAddress(address));
      networks.push_back(network);
    } else if (it->address.size() == net::kIPv6AddressSize) {
      in6_addr address;
      memcpy(&address, &it->address[0], sizeof(in6_addr));
      talk_base::IPAddress ip6_addr(address);
      if (!talk_base::IPIsPrivate(ip6_addr)) {
        talk_base::Network* network = new talk_base::Network(
            it->name, it->name, ip6_addr, 64);
        network->AddIP(ip6_addr);
        networks.push_back(network);
      }
    }
  }

  bool changed = false;
  MergeNetworkList(networks, &changed);
  if (changed)
    SignalNetworksChanged();
}

void IpcNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
