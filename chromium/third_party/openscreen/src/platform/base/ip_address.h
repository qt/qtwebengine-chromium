// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_IP_ADDRESS_H_
#define PLATFORM_BASE_IP_ADDRESS_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "platform/base/error.h"

namespace openscreen {

class IPAddress {
 public:
  enum class Version {
    kV4,
    kV6,
  };

  static constexpr IPAddress kAnyV4() { return IPAddress{0, 0, 0, 0}; }
  static constexpr IPAddress kAnyV6() {
    return IPAddress{0, 0, 0, 0, 0, 0, 0, 0};
  }
  static constexpr IPAddress kV4LoopbackAddress() {
    return IPAddress{127, 0, 0, 1};
  }
  static constexpr IPAddress kV6LoopbackAddress() {
    return IPAddress{0, 0, 0, 0, 0, 0, 0, 1};
  }
  static constexpr size_t kV4Size = 4;
  static constexpr size_t kV6Size = 16;

  constexpr IPAddress() : version_(Version::kV4), bytes_({}) {}

  // `bytes` contains 4 octets for IPv4, or 8 hextets (16 bytes of big-endian
  // shorts) for IPv6.
  // TODO(jophba): delete once usage is removed in Chromium's network_util.cc.
  inline IPAddress(Version version, const uint8_t* bytes) : version_(version) {
    std::copy_n(bytes, size(), bytes_.begin());
  }

  IPAddress(Version version, std::span<const uint8_t> bytes);

  // IPv4 constructors (IPAddress from 4 octets).
  explicit constexpr IPAddress(std::span<const uint8_t, 4> bytes)
      : version_(Version::kV4),
        bytes_{{bytes[0], bytes[1], bytes[2], bytes[3]}} {}

  constexpr IPAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
      : version_(Version::kV4), bytes_{{b1, b2, b3, b4}} {}

  // IPv6 constructors (IPAddress from 8 hextets).
  explicit constexpr IPAddress(std::span<const uint16_t, 8> hextets)
      : IPAddress(hextets[0],
                  hextets[1],
                  hextets[2],
                  hextets[3],
                  hextets[4],
                  hextets[5],
                  hextets[6],
                  hextets[7]) {}

  constexpr IPAddress(uint16_t h0,
                      uint16_t h1,
                      uint16_t h2,
                      uint16_t h3,
                      uint16_t h4,
                      uint16_t h5,
                      uint16_t h6,
                      uint16_t h7)
      : version_(Version::kV6),
        bytes_{{
            static_cast<uint8_t>(h0 >> 8),
            static_cast<uint8_t>(h0),
            static_cast<uint8_t>(h1 >> 8),
            static_cast<uint8_t>(h1),
            static_cast<uint8_t>(h2 >> 8),
            static_cast<uint8_t>(h2),
            static_cast<uint8_t>(h3 >> 8),
            static_cast<uint8_t>(h3),
            static_cast<uint8_t>(h4 >> 8),
            static_cast<uint8_t>(h4),
            static_cast<uint8_t>(h5 >> 8),
            static_cast<uint8_t>(h5),
            static_cast<uint8_t>(h6 >> 8),
            static_cast<uint8_t>(h6),
            static_cast<uint8_t>(h7 >> 8),
            static_cast<uint8_t>(h7),
        }} {}

  // IPv6 constructor with scope ID.
  explicit constexpr IPAddress(std::span<const uint8_t, 16> bytes,
                               uint32_t scope_id)
      : version_(Version::kV6), scope_id_(scope_id) {
    for (size_t i = 0; i < 16; ++i) {
      bytes_[i] = bytes[i];
    }
  }

  constexpr IPAddress(const IPAddress& o) noexcept = default;
  constexpr IPAddress(IPAddress&& o) noexcept = default;
  ~IPAddress() = default;

  constexpr IPAddress& operator=(const IPAddress& o) noexcept = default;
  constexpr IPAddress& operator=(IPAddress&& o) noexcept = default;

  bool operator==(const IPAddress& o) const;
  bool operator!=(const IPAddress& o) const;

  // IP address comparison rules are based on the following two principles:
  // 1. newer versions are greater, e.g. IPv6 > IPv4
  // 2. higher numerical values are greater, e.g. 192.168.0.1 > 10.0.0.1
  bool operator<(const IPAddress& other) const;
  bool operator>(const IPAddress& other) const { return other < *this; }
  bool operator<=(const IPAddress& other) const { return !(other < *this); }
  bool operator>=(const IPAddress& other) const { return !(*this < other); }
  explicit operator bool() const;

  Version version() const { return version_; }
  size_t size() const { return (version_ == Version::kV4) ? kV4Size : kV6Size; }
  bool IsV4() const { return version_ == Version::kV4; }
  bool IsV6() const { return version_ == Version::kV6; }

  // Returns true if the address is an IPv6 link-local address.
  bool IsLinkLocal() const;

  // Returns the scope ID for link-local IPv6 addresses. Returns 0 for
  // non-link-local addresses.
  uint32_t GetScopeId() const { return scope_id_; }

  // These methods assume `x` is the appropriate size, but due to various
  // callers' casting needs we can't check them like the constructors above.
  // Callers should instead make any necessary checks themselves.
  void CopyTo(std::span<uint8_t> bytes) const;

  // TODO(jophba): delete once usage is removed in Chromium's network_util.cc.
  inline void CopyToV4(uint8_t* x) const { CopyTo(std::span(x, kV4Size)); }
  inline void CopyToV6(uint8_t* x) const { CopyTo(std::span(x, kV6Size)); }

  // In some instances, we want direct access to the underlying byte storage,
  // in order to avoid making multiple copies.
  std::span<const uint8_t> bytes() const {
    return {bytes_.data(), (version_ == Version::kV4) ? kV4Size : kV6Size};
  }

  // Parses a text representation of an IPv4 address (e.g. "192.168.0.1") or an
  // IPv6 address (e.g. "abcd::1234").
  static ErrorOr<IPAddress> Parse(std::string_view s);

 private:
  friend ErrorOr<IPAddress> ParseV6(std::string_view s);

  Version version_;
  std::array<uint8_t, 16> bytes_;
  uint32_t scope_id_ = 0;
};

struct IPEndpoint {
 public:
  IPAddress address;
  uint16_t port = 0;

  // Used with various socket types to indicate "any" address.
  static const IPEndpoint kAnyV4();
  static const IPEndpoint kAnyV6();
  explicit operator bool() const;

  // Parses a text representation of an IPv4/IPv6 address and port (e.g.
  // "192.168.0.1:8080" or "[abcd::1234]:8080").
  static ErrorOr<IPEndpoint> Parse(std::string_view s);

  std::string ToString() const;
};

bool operator==(const IPEndpoint& a, const IPEndpoint& b);
bool operator!=(const IPEndpoint& a, const IPEndpoint& b);

bool operator<(const IPEndpoint& a, const IPEndpoint& b);
inline bool operator>(const IPEndpoint& a, const IPEndpoint& b) {
  return b < a;
}
inline bool operator<=(const IPEndpoint& a, const IPEndpoint& b) {
  return !(a > b);
}
inline bool operator>=(const IPEndpoint& a, const IPEndpoint& b) {
  return !(a < b);
}

// Outputs a string of the form:
//      123.234.34.56
//   or fe80:0000:0000:0000:1234:5678:9abc:def0
std::ostream& operator<<(std::ostream& out, const IPAddress& address);

// Outputs a string of the form:
//      123.234.34.56:443
//   or [fe80:0000:0000:0000:1234:5678:9abc:def0]:8080
std::ostream& operator<<(std::ostream& out, const IPEndpoint& endpoint);

}  // namespace openscreen

#endif  // PLATFORM_BASE_IP_ADDRESS_H_
