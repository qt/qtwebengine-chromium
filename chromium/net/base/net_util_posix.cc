// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif
#include <net/if.h>
#include <netinet/in.h>

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {

bool FileURLToFilePath(const GURL& url, base::FilePath* path) {
  *path = base::FilePath();
  std::string& file_path_str = const_cast<std::string&>(path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

  // Firefox seems to ignore the "host" of a file url if there is one. That is,
  // file://foo/bar.txt maps to /bar.txt.
  // TODO(dhg): This should probably take into account UNCs which could
  // include a hostname other than localhost or blank
  std::string old_path = url.path();

  if (old_path.empty())
    return false;

  // GURL stores strings as percent-encoded 8-bit, this will undo if possible.
  old_path = UnescapeURLComponent(old_path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // Collapse multiple path slashes into a single path slash.
  std::string new_path;
  do {
    new_path = old_path;
    ReplaceSubstringsAfterOffset(&new_path, 0, "//", "/");
    old_path.swap(new_path);
  } while (new_path != old_path);

  file_path_str.assign(old_path);

  return !file_path_str.empty();
}

bool GetNetworkList(NetworkInterfaceList* networks) {
#if defined(OS_ANDROID)
  std::string network_list = android::GetNetworkList();
  base::StringTokenizer network_interfaces(network_list, "\n");
  while (network_interfaces.GetNext()) {
    std::string network_item = network_interfaces.token();
    base::StringTokenizer network_tokenizer(network_item, "\t");
    CHECK(network_tokenizer.GetNext());
    std::string name = network_tokenizer.token();

    CHECK(network_tokenizer.GetNext());
    std::string interface_address = network_tokenizer.token();
    IPAddressNumber address;
    size_t network_prefix = 0;
    CHECK(ParseCIDRBlock(network_tokenizer.token(),
                         &address,
                         &network_prefix));

    CHECK(network_tokenizer.GetNext());
    uint32 index = 0;
    CHECK(base::StringToUint(network_tokenizer.token(), &index));

    networks->push_back(
        NetworkInterface(name, index, address, network_prefix));
  }
  return true;
#else
  // getifaddrs() may require IO operations.
  base::ThreadRestrictions::AssertIOAllowed();

  ifaddrs *interfaces;
  if (getifaddrs(&interfaces) < 0) {
    PLOG(ERROR) << "getifaddrs";
    return false;
  }

  // Enumerate the addresses assigned to network interfaces which are up.
  for (ifaddrs *interface = interfaces;
       interface != NULL;
       interface = interface->ifa_next) {
    // Skip loopback interfaces, and ones which are down.
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    // Skip interfaces with no address configured.
    struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;

    // Skip unspecified addresses (i.e. made of zeroes) and loopback addresses
    // configured on non-loopback interfaces.
    int addr_size = 0;
    if (addr->sa_family == AF_INET6) {
      struct sockaddr_in6* addr_in6 =
          reinterpret_cast<struct sockaddr_in6*>(addr);
      struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      addr_size = sizeof(*addr_in6);
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) ||
          IN6_IS_ADDR_UNSPECIFIED(sin6_addr)) {
        continue;
      }
    } else if (addr->sa_family == AF_INET) {
      struct sockaddr_in* addr_in =
          reinterpret_cast<struct sockaddr_in*>(addr);
      addr_size = sizeof(*addr_in);
      if (addr_in->sin_addr.s_addr == INADDR_LOOPBACK ||
          addr_in->sin_addr.s_addr == 0) {
        continue;
      }
    } else {
      // Skip non-IP addresses.
      continue;
    }

    IPEndPoint address;
    std::string name = interface->ifa_name;
    if (address.FromSockAddr(addr, addr_size)) {
      uint8 net_mask = 0;
      if (interface->ifa_netmask) {
        IPEndPoint netmask;
        if (netmask.FromSockAddr(interface->ifa_netmask, addr_size)) {
          net_mask = MaskPrefixLength(netmask.address());
        }
      }

      networks->push_back(
          NetworkInterface(name, if_nametoindex(name.c_str()),
                           address.address(), net_mask));
    }
  }

  freeifaddrs(interfaces);

  return true;
#endif
}

WifiPHYLayerProtocol GetWifiPHYLayerProtocol() {
  return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

}  // namespace net
