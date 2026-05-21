// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_utils.h"

#include <stdint.h>

#include "quiche/common/quiche_ip_address.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

quiche::QuicheIpAddress ToQuicheIpAddress(const IPAddress& address) {
  if (address.IsV4()) {
    std::span<const uint8_t> bytes = address.bytes();
    const uint32_t address_32 =
        (bytes[3] << 24) + (bytes[2] << 16) + (bytes[1] << 8) + (bytes[0]);
    const in_addr result = {address_32};
    static_assert(sizeof(result) == IPAddress::kV4Size,
                  "Address size mismatch");
    return quiche::QuicheIpAddress(result);
  }

  if (address.IsV6()) {
    in6_addr result;
    address.CopyTo(std::span<uint8_t>(result.s6_addr, 16));
    static_assert(sizeof(result) == IPAddress::kV6Size,
                  "Address size mismatch");
    return quiche::QuicheIpAddress(result);
  }

  return quiche::QuicheIpAddress();
}

quic::QuicSocketAddress ToQuicSocketAddress(const IPEndpoint& endpoint) {
  return quic::QuicSocketAddress(ToQuicheIpAddress(endpoint.address),
                                 endpoint.port);
}

IPEndpoint ToIPEndpoint(const quic::QuicSocketAddress& address) {
  auto result = IPEndpoint::Parse(address.ToString());
  return result.value(IPEndpoint{});
}

}  // namespace openscreen::osp
