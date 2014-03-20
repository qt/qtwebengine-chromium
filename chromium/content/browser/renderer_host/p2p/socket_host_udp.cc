// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_udp.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/p2p/socket_host_throttler.h"
#include "content/common/p2p_messages.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_sender.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace {

// UDP packets cannot be bigger than 64k.
const int kReadBufferSize = 65536;
// Socket receive buffer size.
const int kRecvSocketBufferSize = 65536;  // 64K

// Defines set of transient errors. These errors are ignored when we get them
// from sendto() or recvfrom() calls.
//
// net::ERR_OUT_OF_MEMORY
//
// This is caused by ENOBUFS which means the buffer of the network interface
// is full.
//
// net::ERR_CONNECTION_RESET
//
// This is caused by WSAENETRESET or WSAECONNRESET which means the
// last send resulted in an "ICMP Port Unreachable" message.
bool IsTransientError(int error) {
  return error == net::ERR_ADDRESS_UNREACHABLE ||
         error == net::ERR_ADDRESS_INVALID ||
         error == net::ERR_ACCESS_DENIED ||
         error == net::ERR_CONNECTION_RESET ||
         error == net::ERR_OUT_OF_MEMORY;
}

bool AllowUDPWithoutSTUN() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableP2PSocketSTUNFilter);
}

}  // namespace

namespace content {

P2PSocketHostUdp::PendingPacket::PendingPacket(
    const net::IPEndPoint& to,
    const std::vector<char>& content,
    net::DiffServCodePoint dscp_,
    uint64 id)
    : to(to),
      data(new net::IOBuffer(content.size())),
      size(content.size()),
      dscp(dscp_),
      id(id) {
  memcpy(data->data(), &content[0], size);
}

P2PSocketHostUdp::PendingPacket::~PendingPacket() {
}

P2PSocketHostUdp::P2PSocketHostUdp(IPC::Sender* message_sender,
                                   int id,
                                   P2PMessageThrottler* throttler)
    : P2PSocketHost(message_sender, id),
      socket_(new net::UDPServerSocket(NULL, net::NetLog::Source())),
      send_pending_(false),
      last_dscp_(net::DSCP_CS0),
      throttler_(throttler) {
}

P2PSocketHostUdp::~P2PSocketHostUdp() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostUdp::Init(const net::IPEndPoint& local_address,
                            const net::IPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  int result = socket_->Listen(local_address);
  if (result < 0) {
    LOG(ERROR) << "bind() failed: " << result;
    OnError();
    return false;
  }

  // Setting recv socket buffer size.
  if (!socket_->SetReceiveBufferSize(kRecvSocketBufferSize)) {
    LOG(WARNING) << "Failed to set socket receive buffer size to "
                 << kRecvSocketBufferSize;
  }

  net::IPEndPoint address;
  result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketHostUdp::Init(): unable to get local address: "
               << result;
    OnError();
    return false;
  }
  VLOG(1) << "Local address: " << address.ToString();

  state_ = STATE_OPEN;

  message_sender_->Send(new P2PMsg_OnSocketCreated(id_, address));

  recv_buffer_ = new net::IOBuffer(kReadBufferSize);
  DoRead();

  return true;
}

void P2PSocketHostUdp::OnError() {
  socket_.reset();
  send_queue_.clear();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN)
    message_sender_->Send(new P2PMsg_OnError(id_));

  state_ = STATE_ERROR;
}

void P2PSocketHostUdp::DoRead() {
  int result;
  do {
    result = socket_->RecvFrom(
        recv_buffer_.get(),
        kReadBufferSize,
        &recv_address_,
        base::Bind(&P2PSocketHostUdp::OnRecv, base::Unretained(this)));
    if (result == net::ERR_IO_PENDING)
      return;
    HandleReadResult(result);
  } while (state_ == STATE_OPEN);
}

void P2PSocketHostUdp::OnRecv(int result) {
  HandleReadResult(result);
  if (state_ == STATE_OPEN) {
    DoRead();
  }
}

void P2PSocketHostUdp::HandleReadResult(int result) {
  DCHECK_EQ(STATE_OPEN, state_);

  if (result > 0) {
    std::vector<char> data(recv_buffer_->data(), recv_buffer_->data() + result);

    if (!ContainsKey(connected_peers_, recv_address_)) {
      P2PSocketHost::StunMessageType type;
      bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
      if ((stun && IsRequestOrResponse(type)) || AllowUDPWithoutSTUN()) {
        connected_peers_.insert(recv_address_);
      } else if (!stun || type == STUN_DATA_INDICATION) {
        LOG(ERROR) << "Received unexpected data packet from "
                   << recv_address_.ToString()
                   << " before STUN binding is finished.";
        return;
      }
    }

    message_sender_->Send(new P2PMsg_OnDataReceived(
        id_, recv_address_, data, base::TimeTicks::Now()));
  } else if (result < 0 && !IsTransientError(result)) {
    LOG(ERROR) << "Error when reading from UDP socket: " << result;
    OnError();
  }
}

void P2PSocketHostUdp::Send(const net::IPEndPoint& to,
                            const std::vector<char>& data,
                            net::DiffServCodePoint dscp,
                            uint64 packet_id) {
  if (!socket_) {
    // The Send message may be sent after the an OnError message was
    // sent by hasn't been processed the renderer.
    return;
  }

  if (!ContainsKey(connected_peers_, to) && !AllowUDPWithoutSTUN()) {
    P2PSocketHost::StunMessageType type = P2PSocketHost::StunMessageType();
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Page tried to send a data packet to " << to.ToString()
                 << " before STUN binding is finished.";
      OnError();
      return;
    }

    if (throttler_->DropNextPacket(data.size())) {
      VLOG(0) << "STUN message is dropped due to high volume.";
      // Do not reset socket.
      return;
    }
  }

  if (send_pending_) {
    send_queue_.push_back(PendingPacket(to, data, dscp, packet_id));
  } else {
    PendingPacket packet(to, data, dscp, packet_id);
    DoSend(packet);
  }
}

void P2PSocketHostUdp::DoSend(const PendingPacket& packet) {
  TRACE_EVENT_ASYNC_STEP_INTO1("p2p", "Send", packet.id, "UdpAsyncSendTo",
                               "size", packet.size);
  // Don't try to set DSCP in following conditions,
  // 1. If the outgoing packet is set to DSCP_NO_CHANGE
  // 2. If no change in DSCP value from last packet
  // 3. If there is any error in setting DSCP on socket.
  if (packet.dscp != net::DSCP_NO_CHANGE &&
      last_dscp_ != packet.dscp && last_dscp_ != net::DSCP_NO_CHANGE) {
    int result = socket_->SetDiffServCodePoint(packet.dscp);
    if (result == net::OK) {
      last_dscp_ = packet.dscp;
    } else if (!IsTransientError(result) && last_dscp_ != net::DSCP_CS0) {
      // We receieved a non-transient error, and it seems we have
      // not changed the DSCP in the past, disable DSCP as it unlikely
      // to work in the future.
      last_dscp_ = net::DSCP_NO_CHANGE;
    }
  }
  int result = socket_->SendTo(
      packet.data.get(),
      packet.size,
      packet.to,
      base::Bind(&P2PSocketHostUdp::OnSend, base::Unretained(this), packet.id));

  // sendto() may return an error, e.g. if we've received an ICMP Destination
  // Unreachable message. When this happens try sending the same packet again,
  // and just drop it if it fails again.
  if (IsTransientError(result)) {
    result = socket_->SendTo(
        packet.data.get(),
        packet.size,
        packet.to,
        base::Bind(&P2PSocketHostUdp::OnSend, base::Unretained(this),
                   packet.id));
  }

  if (result == net::ERR_IO_PENDING) {
    send_pending_ = true;
  } else {
    HandleSendResult(packet.id, result);
  }
}

void P2PSocketHostUdp::OnSend(uint64 packet_id, int result) {
  DCHECK(send_pending_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  send_pending_ = false;

  HandleSendResult(packet_id, result);

  // Send next packets if we have them waiting in the buffer.
  while (state_ == STATE_OPEN && !send_queue_.empty() && !send_pending_) {
    DoSend(send_queue_.front());
    send_queue_.pop_front();
  }
}

void P2PSocketHostUdp::HandleSendResult(uint64 packet_id, int result) {
  TRACE_EVENT_ASYNC_END1("p2p", "Send", packet_id,
                         "result", result);
  if (result > 0) {
    message_sender_->Send(new P2PMsg_OnSendComplete(id_));
  } else if (IsTransientError(result)) {
    VLOG(0) << "sendto() has failed twice returning a "
        " transient error. Dropping the packet.";
  } else if (result < 0) {
    LOG(ERROR) << "Error when sending data in UDP socket: " << result;
    OnError();
  }
}

P2PSocketHost* P2PSocketHostUdp::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  NOTREACHED();
  OnError();
  return NULL;
}

}  // namespace content
