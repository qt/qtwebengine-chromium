// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/logging.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_socket_utils.h"
#include "net/tools/quic/quic_spdy_client_stream.h"

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

namespace net {
namespace tools {

const int kEpollFlags = EPOLLIN | EPOLLOUT | EPOLLET;

QuicClient::QuicClient(IPEndPoint server_address,
                       const string& server_hostname,
                       const QuicVersionVector& supported_versions,
                       bool print_response)
    : server_address_(server_address),
      server_hostname_(server_hostname),
      local_port_(0),
      fd_(-1),
      helper_(CreateQuicConnectionHelper()),
      initialized_(false),
      packets_dropped_(0),
      overflow_supported_(false),
      supported_versions_(supported_versions),
      print_response_(print_response) {
  config_.SetDefaults();
}

QuicClient::QuicClient(IPEndPoint server_address,
                       const string& server_hostname,
                       const QuicConfig& config,
                       const QuicVersionVector& supported_versions)
    : server_address_(server_address),
      server_hostname_(server_hostname),
      config_(config),
      local_port_(0),
      fd_(-1),
      helper_(CreateQuicConnectionHelper()),
      initialized_(false),
      packets_dropped_(0),
      overflow_supported_(false),
      supported_versions_(supported_versions),
      print_response_(false) {
}

QuicClient::~QuicClient() {
  if (connected()) {
    session()->connection()->SendConnectionClosePacket(
        QUIC_PEER_GOING_AWAY, "");
  }
}

bool QuicClient::Initialize() {
  DCHECK(!initialized_);

  epoll_server_.set_timeout_in_us(50 * 1000);
  crypto_config_.SetDefaults();

  int address_family = server_address_.GetSockAddrFamily();
  fd_ = socket(address_family, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
  if (fd_ < 0) {
    LOG(ERROR) << "CreateSocket() failed: " << strerror(errno);
    return false;
  }

  int get_overflow = 1;
  int rc = setsockopt(fd_, SOL_SOCKET, SO_RXQ_OVFL, &get_overflow,
                      sizeof(get_overflow));
  if (rc < 0) {
    DLOG(WARNING) << "Socket overflow detection not supported";
  } else {
    overflow_supported_ = true;
  }

  int get_local_ip = 1;
  if (address_family == AF_INET) {
    rc = setsockopt(fd_, IPPROTO_IP, IP_PKTINFO,
                    &get_local_ip, sizeof(get_local_ip));
  } else {
    rc =  setsockopt(fd_, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                     &get_local_ip, sizeof(get_local_ip));
  }

  if (rc < 0) {
    LOG(ERROR) << "IP detection not supported" << strerror(errno);
    return false;
  }

  if (bind_to_address_.size() != 0) {
    client_address_ = IPEndPoint(bind_to_address_, local_port_);
  } else if (address_family == AF_INET) {
    IPAddressNumber any4;
    CHECK(net::ParseIPLiteralToNumber("0.0.0.0", &any4));
    client_address_ = IPEndPoint(any4, local_port_);
  } else {
    IPAddressNumber any6;
    CHECK(net::ParseIPLiteralToNumber("::", &any6));
    client_address_ = IPEndPoint(any6, local_port_);
  }

  sockaddr_storage raw_addr;
  socklen_t raw_addr_len = sizeof(raw_addr);
  CHECK(client_address_.ToSockAddr(reinterpret_cast<sockaddr*>(&raw_addr),
                           &raw_addr_len));
  rc = bind(fd_,
            reinterpret_cast<const sockaddr*>(&raw_addr),
            sizeof(raw_addr));
  if (rc < 0) {
    LOG(ERROR) << "Bind failed: " << strerror(errno);
    return false;
  }

  SockaddrStorage storage;
  if (getsockname(fd_, storage.addr, &storage.addr_len) != 0 ||
      !client_address_.FromSockAddr(storage.addr, storage.addr_len)) {
    LOG(ERROR) << "Unable to get self address.  Error: " << strerror(errno);
  }

  epoll_server_.RegisterFD(fd_, this, kEpollFlags);
  initialized_ = true;
  return true;
}

bool QuicClient::Connect() {
  if (!StartConnect()) {
    return false;
  }
  while (EncryptionBeingEstablished()) {
    WaitForEvents();
  }
  return session_->connection()->connected();
}

bool QuicClient::StartConnect() {
  DCHECK(!connected() && initialized_);

  QuicPacketWriter* writer = CreateQuicPacketWriter();
  if (writer_.get() != writer) {
    writer_.reset(writer);
  }

  session_.reset(new QuicClientSession(
      server_hostname_,
      config_,
      new QuicConnection(GenerateGuid(), server_address_, helper_.get(),
                         writer_.get(), false, supported_versions_),
      &crypto_config_));
  return session_->CryptoConnect();
}

bool QuicClient::EncryptionBeingEstablished() {
  return !session_->IsEncryptionEstablished() &&
      session_->connection()->connected();
}

void QuicClient::Disconnect() {
  DCHECK(initialized_);

  if (connected()) {
    session()->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
  }
  epoll_server_.UnregisterFD(fd_);
  close(fd_);
  fd_ = -1;
  initialized_ = false;
}

void QuicClient::SendRequestsAndWaitForResponse(
    const CommandLine::StringVector& args) {
  for (size_t i = 0; i < args.size(); ++i) {
    BalsaHeaders headers;
    headers.SetRequestFirstlineFromStringPieces("GET", args[i], "HTTP/1.1");
    QuicSpdyClientStream* stream = CreateReliableClientStream();
    stream->SendRequest(headers, "", true);
    stream->set_visitor(this);
  }

  while (WaitForEvents()) { }
}

QuicSpdyClientStream* QuicClient::CreateReliableClientStream() {
  if (!connected()) {
    return NULL;
  }

  return session_->CreateOutgoingDataStream();
}

void QuicClient::WaitForStreamToClose(QuicStreamId id) {
  DCHECK(connected());

  while (!session_->IsClosedStream(id)) {
    epoll_server_.WaitForEventsAndExecuteCallbacks();
  }
}

void QuicClient::WaitForCryptoHandshakeConfirmed() {
  DCHECK(connected());

  while (!session_->IsCryptoHandshakeConfirmed()) {
    epoll_server_.WaitForEventsAndExecuteCallbacks();
  }
}

bool QuicClient::WaitForEvents() {
  DCHECK(connected());

  epoll_server_.WaitForEventsAndExecuteCallbacks();
  return session_->num_active_requests() != 0;
}

void QuicClient::OnEvent(int fd, EpollEvent* event) {
  DCHECK_EQ(fd, fd_);

  if (event->in_events & EPOLLIN) {
    while (connected() && ReadAndProcessPacket()) {
    }
  }
  if (connected() && (event->in_events & EPOLLOUT)) {
    session_->connection()->OnCanWrite();
  }
  if (event->in_events & EPOLLERR) {
    DLOG(INFO) << "Epollerr";
  }
}

void QuicClient::OnClose(QuicDataStream* stream) {
  if (!print_response_) {
    return;
  }

  QuicSpdyClientStream* client_stream =
      static_cast<QuicSpdyClientStream*>(stream);
  const BalsaHeaders& headers = client_stream->headers();
  printf("%s\n", headers.first_line().as_string().c_str());
  for (BalsaHeaders::const_header_lines_iterator i =
           headers.header_lines_begin();
       i != headers.header_lines_end(); ++i) {
    printf("%s: %s\n", i->first.as_string().c_str(),
           i->second.as_string().c_str());
  }
  printf("%s\n", client_stream->data().c_str());
}

QuicPacketCreator::Options* QuicClient::options() {
  if (session() == NULL) {
    return NULL;
  }
  return session_->options();
}

bool QuicClient::connected() const {
  return session_.get() && session_->connection() &&
      session_->connection()->connected();
}

QuicGuid QuicClient::GenerateGuid() {
  return QuicRandom::GetInstance()->RandUint64();
}

QuicEpollConnectionHelper* QuicClient::CreateQuicConnectionHelper() {
  return new QuicEpollConnectionHelper(&epoll_server_);
}

QuicPacketWriter* QuicClient::CreateQuicPacketWriter() {
  return new QuicDefaultPacketWriter(fd_);
}

bool QuicClient::ReadAndProcessPacket() {
  // Allocate some extra space so we can send an error if the server goes over
  // the limit.
  char buf[2 * kMaxPacketSize];

  IPEndPoint server_address;
  IPAddressNumber client_ip;

  int bytes_read = QuicSocketUtils::ReadPacket(
      fd_, buf, arraysize(buf), overflow_supported_ ? &packets_dropped_ : NULL,
      &client_ip, &server_address);

  if (bytes_read < 0) {
    return false;
  }

  QuicEncryptedPacket packet(buf, bytes_read, false);
  QuicGuid our_guid = session_->connection()->guid();
  QuicGuid packet_guid;

  if (!QuicFramer::ReadGuidFromPacket(packet, &packet_guid)) {
    DLOG(INFO) << "Could not read GUID from packet";
    return true;
  }
  if (packet_guid != our_guid) {
    DLOG(INFO) << "Ignoring packet from unexpected GUID: "
               << packet_guid << " instead of " << our_guid;
    return true;
  }

  IPEndPoint client_address(client_ip, client_address_.port());
  session_->connection()->ProcessUdpPacket(
      client_address, server_address, packet);
  return true;
}

}  // namespace tools
}  // namespace net
