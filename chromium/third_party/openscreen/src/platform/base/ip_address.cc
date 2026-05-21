// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/ip_address.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <net/if.h>
#endif

namespace openscreen {

IPAddress::IPAddress(Version version, std::span<const uint8_t> bytes)
    : version_(version) {
  assert(bytes.size() >= size());
  std::copy_n(bytes.begin(), size(), bytes_.begin());
}

bool IPAddress::operator==(const IPAddress& o) const {
  return version_ == o.version_ &&
         std::equal(bytes_.begin(), bytes_.begin() + size(),
                    o.bytes_.begin()) &&
         scope_id_ == o.scope_id_;
}

bool IPAddress::operator!=(const IPAddress& o) const {
  return !(*this == o);
}

IPAddress::operator bool() const {
  return std::any_of(bytes_.begin(), bytes_.begin() + size(),
                     [](uint8_t byte) { return byte; });
}

void IPAddress::CopyTo(std::span<uint8_t> bytes) const {
  assert(bytes.size() >= size());
  std::copy_n(bytes_.begin(), size(), bytes.begin());
}

bool IPAddress::IsLinkLocal() const {
  if (!IsV6()) {
    return false;
  }
  // Link-local addresses start with fe80::/10
  return (bytes_[0] == 0xfe) && ((bytes_[1] & 0xc0) == 0x80);
}

namespace {

ErrorOr<IPAddress> ParseV4(std::string_view s) {
  uint8_t octets[4];
  for (int i = 0; i < 4; ++i) {
    if (i > 0) {
      if (s.empty() || s.front() != '.') {
        return Error::Code::kInvalidIPV4Address;
      }
      s.remove_prefix(1);
    }
    const auto result =
        std::from_chars(s.data(), s.data() + s.size(), octets[i]);
    if (result.ec != std::errc()) {
      return Error::Code::kInvalidIPV4Address;
    }
    s.remove_prefix(result.ptr - s.data());
  }

  if (!s.empty()) {
    return Error::Code::kInvalidIPV4Address;
  }

  return IPAddress(octets[0], octets[1], octets[2], octets[3]);
}

// Returns the zero-expansion of a double-colon in `s` if `s` is a
// well-formatted IPv6 address. If `s` is ill-formatted, returns *any* string
// that is ill-formatted.
std::string ExpandIPv6DoubleColon(std::string_view s) {
  constexpr std::string_view kDoubleColon = "::";
  const size_t double_colon_position = s.find(kDoubleColon);
  if (double_colon_position == std::string::npos) {
    return std::string(s);  // Nothing to expand.
  }
  if (double_colon_position != s.rfind(kDoubleColon)) {
    return {};  // More than one occurrence of double colons is illegal.
  }

  std::ostringstream expanded;
  const int num_single_colons = std::count(s.begin(), s.end(), ':') - 2;
  int num_zero_groups_to_insert = 8 - num_single_colons;
  if (double_colon_position != 0) {
    // abcd:0123:4567::f000:1
    // ^^^^^^^^^^^^^^^
    expanded << s.substr(0, double_colon_position + 1);
    --num_zero_groups_to_insert;
  }
  if (double_colon_position != (s.size() - 2)) {
    --num_zero_groups_to_insert;
  }
  while (--num_zero_groups_to_insert > 0) {
    expanded << "0:";
  }
  expanded << '0';
  if (double_colon_position != (s.size() - 2)) {
    // abcd:0123:4567::f000:1
    //                ^^^^^^^
    expanded << s.substr(double_colon_position + 1);
  }
  return expanded.str();
}

}  // namespace

ErrorOr<IPAddress> ParseV6(std::string_view s) {
  std::string_view address_part = s;
  uint32_t scope_id = 0;

  // Handle link-local addresses with scope ID, e.g., fe80::1%eth0
  const size_t scope_pos = s.find('%');
  if (scope_pos != std::string::npos) {
    address_part = s.substr(0, scope_pos);
    std::string_view scope_name = s.substr(scope_pos + 1);
#if BUILDFLAG(IS_POSIX)
    scope_id = if_nametoindex(std::string(scope_name).c_str());
#endif
    if (scope_id == 0) {
      // If if_nametoindex failed or is not available, try parsing as a number.
      unsigned int parsed_id = 0;
      const auto result = std::from_chars(
          scope_name.data(), scope_name.data() + scope_name.size(), parsed_id);

      if (result.ec == std::errc() &&
          result.ptr == scope_name.data() + scope_name.size() &&
          parsed_id > 0) {
        scope_id = parsed_id;
      }
    }

    if (scope_id == 0) {
      return Error::Code::kInvalidIPV6Address;
    }
  }

  const std::string scan_input = ExpandIPv6DoubleColon(address_part);
  std::string_view scan_view(scan_input);
  uint16_t hextets[8];

  for (int i = 0; i < 8; ++i) {
    if (i > 0) {
      if (scan_view.empty() || scan_view.front() != ':') {
        return Error::Code::kInvalidIPV6Address;
      }
      scan_view.remove_prefix(1);
    }
    const auto result = std::from_chars(
        scan_view.data(), scan_view.data() + scan_view.size(), hextets[i], 16);
    if (result.ec != std::errc()) {
      return Error::Code::kInvalidIPV6Address;
    }
    scan_view.remove_prefix(result.ptr - scan_view.data());
  }

  if (!scan_view.empty()) {
    return Error::Code::kInvalidIPV6Address;
  }

  IPAddress address(hextets);
  if (scope_id != 0) {
    if (!address.IsLinkLocal()) {
      return Error::Code::kInvalidIPV6Address;
    }
    address.scope_id_ = scope_id;
  }
  return address;
}

// static
ErrorOr<IPAddress> IPAddress::Parse(std::string_view s) {
  ErrorOr<IPAddress> v4 = ParseV4(s);

  return v4 ? std::move(v4) : ParseV6(s);
}

// static
const IPEndpoint IPEndpoint::kAnyV4() {
  return IPEndpoint{};
}

// static
const IPEndpoint IPEndpoint::kAnyV6() {
  return IPEndpoint{IPAddress::kAnyV6(), 0};
}

IPEndpoint::operator bool() const {
  return address || port;
}

// static
ErrorOr<IPEndpoint> IPEndpoint::Parse(std::string_view s) {
  // Look for the colon that separates the IP address from the port number. Note
  // that this check also guards against the case where `s` is the empty string.
  const auto colon_pos = s.rfind(':');
  if (colon_pos == std::string::npos) {
    return Error(Error::Code::kParseError, "missing colon separator");
  }
  // The colon cannot be the first nor the last character in `s` because that
  // would mean there is no address part or port part.
  if (colon_pos == 0) {
    return Error(Error::Code::kParseError, "missing address before colon");
  }
  if (colon_pos == (s.size() - 1)) {
    return Error(Error::Code::kParseError, "missing port after colon");
  }

  ErrorOr<IPAddress> address(Error::Code::kParseError);
  if (s[0] == '[' && s[colon_pos - 1] == ']') {
    // [abcd:beef:1:1::2600]:8080
    // ^^^^^^^^^^^^^^^^^^^^^
    address = ParseV6(s.substr(1, colon_pos - 2));
  } else {
    // 127.0.0.1:22
    // ^^^^^^^^^
    address = ParseV4(s.substr(0, colon_pos));
  }
  if (address.is_error()) {
    return Error(Error::Code::kParseError, "invalid address part");
  }

  const std::string_view port_part = s.substr(colon_pos + 1);
  int port;
  const auto result = std::from_chars(
      port_part.data(), port_part.data() + port_part.size(), port);
  if (result.ec != std::errc() ||
      result.ptr != port_part.data() + port_part.size() || port < 0 ||
      port > std::numeric_limits<uint16_t>::max()) {
    return Error(Error::Code::kParseError, "invalid port part");
  }

  return IPEndpoint{address.value(), static_cast<uint16_t>(port)};
}

bool operator==(const IPEndpoint& a, const IPEndpoint& b) {
  return (a.address == b.address) && (a.port == b.port);
}

bool operator!=(const IPEndpoint& a, const IPEndpoint& b) {
  return !(a == b);
}

bool IPAddress::operator<(const IPAddress& other) const {
  if (version() != other.version()) {
    return version() < other.version();
  }

  if (IsV4()) {
    return memcmp(bytes_.data(), other.bytes_.data(), 4) < 0;
  } else {
    const int cmp = memcmp(bytes_.data(), other.bytes_.data(), 16);
    if (cmp != 0) {
      return cmp < 0;
    }
    return scope_id_ < other.scope_id_;
  }
}

bool operator<(const IPEndpoint& a, const IPEndpoint& b) {
  if (a.address != b.address) {
    return a.address < b.address;
  }

  return a.port < b.port;
}

std::ostream& operator<<(std::ostream& out, const IPAddress& address) {
  char separator;
  size_t values_per_separator;
  int value_width;
  if (address.IsV4()) {
    out << std::dec;
    separator = '.';
    values_per_separator = 1;
    value_width = 0;
  } else if (address.IsV6()) {
    out << std::hex << std::setfill('0') << std::right;
    separator = ':';
    values_per_separator = 2;
    value_width = 2;
  }
  std::span<const uint8_t> bytes = address.bytes();
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i > 0 && (i % values_per_separator == 0)) {
      out << separator;
    }
    out << std::setw(value_width) << static_cast<int>(bytes[i]);
  }
  if (address.IsLinkLocal() && address.GetScopeId() != 0) {
#if BUILDFLAG(IS_POSIX)
    char ifname[IF_NAMESIZE];
    if (if_indextoname(address.GetScopeId(), ifname)) {
      out << '%' << ifname;
    } else {
      out << '%' << address.GetScopeId();
    }
#else
    out << '%' << address.GetScopeId();
#endif
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const IPEndpoint& endpoint) {
  if (endpoint.address.IsV6()) {
    out << '[';
  }
  out << endpoint.address;
  if (endpoint.address.IsV6()) {
    out << ']';
  }
  return out << ':' << std::dec << static_cast<int>(endpoint.port);
}

std::string IPEndpoint::ToString() const {
  std::ostringstream name;
  name << *this;
  return name.str();
}

}  // namespace openscreen
