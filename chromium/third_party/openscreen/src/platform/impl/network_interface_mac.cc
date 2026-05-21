// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

// net/if.h must be included before this.
#include <ifaddrs.h>

#include <algorithm>
#include <string>
#include <vector>

#include "platform/api/network_interface.h"
#include "platform/base/ip_address.h"
#include "platform/base/span.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/scoped_pipe.h"
#include "platform/impl/socket_address_posix.h"
#include "util/osp_logging.h"
#include "util/std_util.h"

namespace openscreen {

namespace {

// Assuming `netmask` consists of 0 to N*8 leftmost bits set followed by all
// unset bits, return the number of leftmost bits set. This also sanity-checks
// that there are no "holes" in the bit pattern, returning 0 if that check
// fails.
uint8_t ToPrefixLength(std::span<const uint8_t> netmask) {
  uint8_t result = 0;
  size_t i = 0;

  // Ensure all of the leftmost bits are set.
  while (i < netmask.size() && netmask[i] == UINT8_C(0xff)) {
    result += 8;
    ++i;
  }

  // Check the intermediate byte, the first that is not 0xFF,
  // e.g. 0b11100000 or 0x00
  if (i < netmask.size() && netmask[i] != UINT8_C(0x00)) {
    uint8_t last_byte = netmask[i];
    // Check the left most bit, bitshifting as we go.
    while (last_byte & UINT8_C(0x80)) {
      ++result;
      last_byte <<= 1;
    }
    OSP_CHECK(last_byte == UINT8_C(0x00));
    ++i;
  }

  // Ensure the rest of the bytes are zeroed out.
  while (i < netmask.size()) {
    OSP_CHECK(netmask[i] == UINT8_C(0x00));
    ++i;
  }

  return result;
}

std::vector<InterfaceInfo> ProcessInterfacesList(ifaddrs* interfaces) {
  // Socket used for querying interface media types.
  const ScopedFd ioctl_socket(socket(AF_INET6, SOCK_DGRAM, 0));

  // Walk the `interfaces` linked list, creating the hierarchical structure.
  std::vector<InterfaceInfo> results;
  for (ifaddrs* cur = interfaces; cur; cur = cur->ifa_next) {
    // Skip: 1) interfaces that are down, 2) interfaces with no address
    // configured.
    if (!(IFF_RUNNING & cur->ifa_flags) || !cur->ifa_addr) {
      continue;
    }

    // Look-up the InterfaceInfo entry by name. Auto-create a new one if none by
    // the current name exists in `results`.
    const std::string name = cur->ifa_name;
    const auto it = std::find_if(
        results.begin(), results.end(),
        [&name](const InterfaceInfo& info) { return info.name == name; });
    InterfaceInfo* interface;
    if (it == results.end()) {
      InterfaceInfo::Type type = InterfaceInfo::Type::kOther;
      // Query for the interface media type and status. If not valid/active,
      // skip further processing. Note that "active" here means the media is
      // connected to the interface, which is different than the interface being
      // up/down.
      ifmediareq ifmr{};
      // `ifmr` is both an input and an output of the `ioctl` method, and since
      // it is zero-initialized it can safely be copied with strncpy() and will
      // always be NUL terminated.
      strncpy(ifmr.ifm_name, name.c_str(), sizeof(ifmr.ifm_name) - 1);
      if (ioctl(ioctl_socket.get(), SIOCGIFMEDIA, &ifmr) >= 0) {
        if (!((ifmr.ifm_status & IFM_AVALID) &&
              (ifmr.ifm_status & IFM_ACTIVE))) {
          continue;  // Skip this interface since it's not valid or active.
        }
        if (ifmr.ifm_current & IFM_IEEE80211) {
          type = InterfaceInfo::Type::kWifi;
        } else if (ifmr.ifm_current & IFM_ETHER) {
          type = InterfaceInfo::Type::kEthernet;
        }
      } else if (cur->ifa_flags & IFF_LOOPBACK) {
        type = InterfaceInfo::Type::kLoopback;
      } else {
        continue;
      }

      // Start with an unknown hardware ethernet address, which should be
      // updated as the linked list is walked.
      const uint8_t kUnknownHardwareAddress[6] = {0, 0, 0, 0, 0, 0};
      results.emplace_back(if_nametoindex(cur->ifa_name),
                           kUnknownHardwareAddress, name, type,
                           // IPSubnets to be filled-in later.
                           std::vector<IPSubnet>());
      interface = &(results.back());
    } else {
      interface = &(*it);
    }

    // Add another address to the list of addresses for the current interface.
    if (cur->ifa_addr->sa_family == AF_LINK) {  // Hardware ethernet address.
      auto* const addr_dl = reinterpret_cast<const sockaddr_dl*>(cur->ifa_addr);
      ByteView address_bytes(reinterpret_cast<uint8_t*>(LLADDR(addr_dl)),
                             addr_dl->sdl_alen);
      interface->hardware_address.assign(address_bytes.begin(),
                                         address_bytes.end());
    } else if (cur->ifa_addr->sa_family == AF_INET6) {  // Ipv6 address.
      struct in6_ifreq ifr = {};
      // `ifr` is both an input and an output of the `ioctl` method, and since
      // it is zero-initialized it can safely be copied with strncpy() and will
      // always be NUL terminated.
      strncpy(ifr.ifr_name, cur->ifa_name, sizeof(ifr.ifr_name) - 1);
      std::copy_n(reinterpret_cast<const uint8_t*>(cur->ifa_addr),
                  cur->ifa_addr->sa_len,
                  reinterpret_cast<uint8_t*>(&ifr.ifr_ifru.ifru_addr));
      if (ioctl(ioctl_socket.get(), SIOCGIFAFLAG_IN6, &ifr) != 0 ||
          ifr.ifr_ifru.ifru_flags & IN6_IFF_DEPRECATED) {
        continue;
      }

      auto* const addr_in6 =
          reinterpret_cast<const sockaddr_in6*>(cur->ifa_addr);
      const IPAddress ip = GetIPAddressFromSockAddr(*addr_in6);
      std::array<uint8_t, IPAddress::kV6Size> netmask_bytes{};
      if (cur->ifa_netmask && cur->ifa_netmask->sa_family == AF_INET6) {
        auto* netmask_in6 =
            reinterpret_cast<const sockaddr_in6*>(cur->ifa_netmask);
        std::copy_n(netmask_in6->sin6_addr.s6_addr, netmask_bytes.size(),
                    netmask_bytes.begin());
      }
      interface->addresses.emplace_back(ip, ToPrefixLength(netmask_bytes));
    } else if (cur->ifa_addr->sa_family == AF_INET) {  // Ipv4 address.
      auto* const addr_in = reinterpret_cast<const sockaddr_in*>(cur->ifa_addr);
      IPAddress ip = GetIPAddressFromSockAddr(*addr_in);
      std::array<uint8_t, IPAddress::kV4Size> netmask_bytes{};
      if (cur->ifa_netmask && cur->ifa_netmask->sa_family == AF_INET) {
        auto* netmask_in =
            reinterpret_cast<const sockaddr_in*>(cur->ifa_netmask);
        std::copy_n(
            reinterpret_cast<const uint8_t*>(&netmask_in->sin_addr.s_addr),
            netmask_bytes.size(), netmask_bytes.begin());
      }
      interface->addresses.emplace_back(ip, ToPrefixLength(netmask_bytes));
    }
  }

  return results;
}

}  // namespace

std::vector<InterfaceInfo> GetNetworkInterfaces() {
  std::vector<InterfaceInfo> results;
  ifaddrs* interfaces;
  if (getifaddrs(&interfaces) == 0) {
    results = ProcessInterfacesList(interfaces);
    freeifaddrs(interfaces);
  }
  return results;
}

}  // namespace openscreen
