// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_server.h"

#include <errno.h>
#include <features.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_socket_utils.h"

#define MMSG_MORE 0

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

const int kEpollFlags = EPOLLIN | EPOLLOUT | EPOLLET;
static const char kSourceAddressTokenSecret[] = "secret";

namespace net {
namespace tools {

QuicServer::QuicServer()
    : port_(0),
      fd_(-1),
      packets_dropped_(0),
      overflow_supported_(false),
      use_recvmmsg_(false),
      crypto_config_(kSourceAddressTokenSecret, QuicRandom::GetInstance()),
      supported_versions_(QuicSupportedVersions()) {
  // Use hardcoded crypto parameters for now.
  config_.SetDefaults();
  config_.set_initial_round_trip_time_us(kMaxInitialRoundTripTimeUs, 0);
  config_.set_server_initial_congestion_window(kMaxInitialWindow,
                                               kDefaultInitialWindow);
  Initialize();
}

QuicServer::QuicServer(const QuicConfig& config,
                       const QuicVersionVector& supported_versions)
    : port_(0),
      fd_(-1),
      packets_dropped_(0),
      overflow_supported_(false),
      use_recvmmsg_(false),
      config_(config),
      crypto_config_(kSourceAddressTokenSecret, QuicRandom::GetInstance()),
      supported_versions_(supported_versions) {
  Initialize();
}

void QuicServer::Initialize() {
#if MMSG_MORE
  use_recvmmsg_ = true;
#endif
  epoll_server_.set_timeout_in_us(50 * 1000);
  // Initialize the in memory cache now.
  QuicInMemoryCache::GetInstance();

  QuicEpollClock clock(&epoll_server_);

  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config_.AddDefaultConfig(
          QuicRandom::GetInstance(), &clock,
          QuicCryptoServerConfig::ConfigOptions()));
}

QuicServer::~QuicServer() {
}

bool QuicServer::Listen(const IPEndPoint& address) {
  port_ = address.port();
  int address_family = address.GetSockAddrFamily();
  fd_ = socket(address_family, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (fd_ < 0) {
    LOG(ERROR) << "CreateSocket() failed: " << strerror(errno);
    return false;
  }

  int rc = QuicSocketUtils::SetGetAddressInfo(fd_, address_family);

  if (rc < 0) {
    LOG(ERROR) << "IP detection not supported" << strerror(errno);
    return false;
  }

  int get_overflow = 1;
  rc = setsockopt(
      fd_, SOL_SOCKET, SO_RXQ_OVFL, &get_overflow, sizeof(get_overflow));

  if (rc < 0) {
    DLOG(WARNING) << "Socket overflow detection not supported";
  } else {
    overflow_supported_ = true;
  }

  // Enable the socket option that allows the local address to be
  // returned if the socket is bound to more than on address.
  int get_local_ip = 1;
  rc = setsockopt(fd_, IPPROTO_IP, IP_PKTINFO,
                  &get_local_ip, sizeof(get_local_ip));
  if (rc == 0 && address_family == AF_INET6) {
    rc = setsockopt(fd_, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                    &get_local_ip, sizeof(get_local_ip));
  }
  if (rc != 0) {
    LOG(ERROR) << "Failed to set required socket options";
    return false;
  }

  sockaddr_storage raw_addr;
  socklen_t raw_addr_len = sizeof(raw_addr);
  CHECK(address.ToSockAddr(reinterpret_cast<sockaddr*>(&raw_addr),
                           &raw_addr_len));
  rc = bind(fd_,
            reinterpret_cast<const sockaddr*>(&raw_addr),
            sizeof(raw_addr));
  if (rc < 0) {
    LOG(ERROR) << "Bind failed: " << strerror(errno);
    return false;
  }

  LOG(INFO) << "Listening on " << address.ToString();
  if (port_ == 0) {
    SockaddrStorage storage;
    IPEndPoint server_address;
    if (getsockname(fd_, storage.addr, &storage.addr_len) != 0 ||
        !server_address.FromSockAddr(storage.addr, storage.addr_len)) {
      LOG(ERROR) << "Unable to get self address.  Error: " << strerror(errno);
      return false;
    }
    port_ = server_address.port();
    LOG(INFO) << "Kernel assigned port is " << port_;
  }

  epoll_server_.RegisterFD(fd_, this, kEpollFlags);
  dispatcher_.reset(new QuicDispatcher(config_, crypto_config_,
                                       supported_versions_,
                                       fd_, &epoll_server_));

  return true;
}

void QuicServer::WaitForEvents() {
  epoll_server_.WaitForEventsAndExecuteCallbacks();
}

void QuicServer::Shutdown() {
  // Before we shut down the epoll server, give all active sessions a chance to
  // notify clients that they're closing.
  dispatcher_->Shutdown();

  close(fd_);
  fd_ = -1;
}

void QuicServer::OnEvent(int fd, EpollEvent* event) {
  DCHECK_EQ(fd, fd_);
  event->out_ready_mask = 0;

  if (event->in_events & EPOLLIN) {
    LOG(ERROR) << "EPOLLIN";
    bool read = true;
    while (read) {
        read = ReadAndDispatchSinglePacket(
            fd_, port_, dispatcher_.get(),
            overflow_supported_ ? &packets_dropped_ : NULL);
    }
  }
  if (event->in_events & EPOLLOUT) {
    bool can_write_more = dispatcher_->OnCanWrite();
    if (can_write_more) {
      event->out_ready_mask |= EPOLLOUT;
    }
  }
  if (event->in_events & EPOLLERR) {
  }
}

/* static */
void QuicServer::MaybeDispatchPacket(QuicDispatcher* dispatcher,
                                     const QuicEncryptedPacket& packet,
                                     const IPEndPoint& server_address,
                                     const IPEndPoint& client_address) {
  QuicGuid guid;
  if (!QuicFramer::ReadGuidFromPacket(packet, &guid)) {
    return;
  }

  bool has_version_flag = QuicFramer::HasVersionFlag(packet);

  dispatcher->ProcessPacket(
      server_address, client_address, guid, has_version_flag, packet);
}

bool QuicServer::ReadAndDispatchSinglePacket(int fd,
                                             int port,
                                             QuicDispatcher* dispatcher,
                                             int* packets_dropped) {
  // Allocate some extra space so we can send an error if the client goes over
  // the limit.
  char buf[2 * kMaxPacketSize];

  IPEndPoint client_address;
  IPAddressNumber server_ip;
  int bytes_read =
      QuicSocketUtils::ReadPacket(fd, buf, arraysize(buf),
                                  packets_dropped,
                                  &server_ip, &client_address);

  if (bytes_read < 0) {
    return false;  // We failed to read.
  }

  QuicEncryptedPacket packet(buf, bytes_read, false);

  IPEndPoint server_address(server_ip, port);
  MaybeDispatchPacket(dispatcher, packet, server_address, client_address);

  return true;
}

}  // namespace tools
}  // namespace net
