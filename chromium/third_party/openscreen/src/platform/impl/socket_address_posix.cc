// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_address_posix.h"

#include <algorithm>
#include <vector>

#include "util/osp_logging.h"

namespace openscreen {

SocketAddressPosix::SocketAddressPosix(const struct sockaddr& address) {
  if (address.sa_family == AF_INET) {
    std::copy_n(reinterpret_cast<const uint8_t*>(&address),
                sizeof(struct sockaddr_in),
                reinterpret_cast<uint8_t*>(&internal_address_.v4));
    RecomputeEndpoint(IPAddress::Version::kV4);
  } else if (address.sa_family == AF_INET6) {
    std::copy_n(reinterpret_cast<const uint8_t*>(&address),
                sizeof(struct sockaddr_in6),
                reinterpret_cast<uint8_t*>(&internal_address_.v6));
    RecomputeEndpoint(IPAddress::Version::kV6);
  } else {
    // Not IPv4 or IPv6.
    OSP_NOTREACHED();
  }
}

SocketAddressPosix::SocketAddressPosix(const IPEndpoint& endpoint)
    : endpoint_(endpoint) {
  if (endpoint.address.IsV4()) {
    internal_address_.v4 = ToSockAddrIn(endpoint);
  } else {
    internal_address_.v6 = ToSockAddrIn6(endpoint);
  }
}

struct sockaddr* SocketAddressPosix::address() {
  switch (version()) {
    case IPAddress::Version::kV4:
      return reinterpret_cast<struct sockaddr*>(&internal_address_.v4);
    case IPAddress::Version::kV6:
      return reinterpret_cast<struct sockaddr*>(&internal_address_.v6);
    default:
      OSP_NOTREACHED();
  }
}

const struct sockaddr* SocketAddressPosix::address() const {
  switch (version()) {
    case IPAddress::Version::kV4:
      return reinterpret_cast<const struct sockaddr*>(&internal_address_.v4);
    case IPAddress::Version::kV6:
      return reinterpret_cast<const struct sockaddr*>(&internal_address_.v6);
    default:
      OSP_NOTREACHED();
  }
}

socklen_t SocketAddressPosix::size() const {
  switch (version()) {
    case IPAddress::Version::kV4:
      return sizeof(struct sockaddr_in);
    case IPAddress::Version::kV6:
      return sizeof(struct sockaddr_in6);
    default:
      OSP_NOTREACHED();
  }
}

void SocketAddressPosix::RecomputeEndpoint() {
  RecomputeEndpoint(endpoint_.address.version());
}

void SocketAddressPosix::RecomputeEndpoint(IPAddress::Version version) {
  switch (version) {
    case IPAddress::Version::kV4:
      endpoint_.address = GetIPAddressFromSockAddr(internal_address_.v4);
      endpoint_.port = ntohs(internal_address_.v4.sin_port);
      break;
    case IPAddress::Version::kV6:
      endpoint_.address = GetIPAddressFromSockAddr(internal_address_.v6);
      endpoint_.port = ntohs(internal_address_.v6.sin6_port);
      break;
  }
}

IPAddress GetIPAddressFromSockAddr(const struct sockaddr_in& sa) {
  static_assert(IPAddress::kV4Size == sizeof(sa.sin_addr.s_addr),
                "IPv4 address size mismatch.");
  return IPAddress(
      IPAddress::Version::kV4,
      std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(&sa.sin_addr.s_addr), 4));
}

IPAddress GetIPAddressFromSockAddr(const struct sockaddr_in6& sa) {
  return IPAddress(std::span<const uint8_t, 16>(sa.sin6_addr.s6_addr, 16),
                   sa.sin6_scope_id);
}

struct sockaddr_in ToSockAddrIn(const IPEndpoint& endpoint) {
  OSP_CHECK(endpoint.address.IsV4());
  struct sockaddr_in out{};
  out.sin_family = AF_INET;
  out.sin_port = htons(endpoint.port);
  endpoint.address.CopyTo(
      std::span<uint8_t>(reinterpret_cast<uint8_t*>(&out.sin_addr.s_addr), 4));
  return out;
}

struct sockaddr_in6 ToSockAddrIn6(const IPEndpoint& endpoint) {
  OSP_CHECK(endpoint.address.IsV6());
  struct sockaddr_in6 out{};
  out.sin6_family = AF_INET6;
  out.sin6_flowinfo = 0;
  out.sin6_scope_id = 0;
  if (endpoint.address.IsLinkLocal() && endpoint.address.GetScopeId() != 0) {
    out.sin6_scope_id = endpoint.address.GetScopeId();
  }
  out.sin6_port = htons(endpoint.port);
  endpoint.address.CopyTo(
      std::span<uint8_t>(reinterpret_cast<uint8_t*>(&out.sin6_addr), 16));
  return out;
}

}  // namespace openscreen
