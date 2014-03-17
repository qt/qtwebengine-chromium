// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_LIST_H_
#define NET_PROXY_PROXY_LIST_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_retry_info.h"

namespace base {
class ListValue;
class TimeDelta;
}

namespace net {

class ProxyServer;

// This class is used to hold a list of proxies returned by GetProxyForUrl or
// manually configured. It handles proxy fallback if multiple servers are
// specified.
class NET_EXPORT_PRIVATE ProxyList {
 public:
  ProxyList();
  ~ProxyList();

  // Initializes the proxy list to a string containing one or more proxy servers
  // delimited by a semicolon.
  void Set(const std::string& proxy_uri_list);

  // Set the proxy list to a single entry, |proxy_server|.
  void SetSingleProxyServer(const ProxyServer& proxy_server);

  // Append a single proxy server to the end of the proxy list.
  void AddProxyServer(const ProxyServer& proxy_server);

  // De-prioritizes the proxies that we have cached as not working, by moving
  // them to the end of the fallback list.
  void DeprioritizeBadProxies(const ProxyRetryInfoMap& proxy_retry_info);

  // Returns true if this proxy list contains at least one proxy that is
  // not currently present in |proxy_retry_info|.
  bool HasUntriedProxies(const ProxyRetryInfoMap& proxy_retry_info) const;

  // Delete any entry which doesn't have one of the specified proxy schemes.
  // |scheme_bit_field| is a bunch of ProxyServer::Scheme bitwise ORed together.
  void RemoveProxiesWithoutScheme(int scheme_bit_field);

  // Clear the proxy list.
  void Clear();

  // Returns true if there is nothing left in the ProxyList.
  bool IsEmpty() const;

  // Returns the number of proxy servers in this list.
  size_t size() const;

  // Returns true if |*this| lists the same proxies as |other|.
  bool Equals(const ProxyList& other) const;

  // Returns the first proxy server in the list. It is only valid to call
  // this if !IsEmpty().
  const ProxyServer& Get() const;

  // Sets the list by parsing the pac result |pac_string|.
  // Some examples for |pac_string|:
  //   "DIRECT"
  //   "PROXY foopy1"
  //   "PROXY foopy1; SOCKS4 foopy2:1188"
  // Does a best-effort parse, and silently discards any errors.
  void SetFromPacString(const std::string& pac_string);

  // Returns a PAC-style semicolon-separated list of valid proxy servers.
  // For example: "PROXY xxx.xxx.xxx.xxx:xx; SOCKS yyy.yyy.yyy:yy".
  std::string ToPacString() const;

  // Returns a serialized value for the list. The caller takes ownership of it.
  base::ListValue* ToValue() const;

  // Marks the current proxy server as bad and deletes it from the list.  The
  // list of known bad proxies is given by proxy_retry_info.  Returns true if
  // there is another server available in the list.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info,
                const BoundNetLog& net_log);

  // Updates |proxy_retry_info| to indicate that the first proxy in the list
  // is bad. This is distinct from Fallback(), above, to allow updating proxy
  // retry information without modifying a given transction's proxy list. Will
  // retry after |retry_delay| if positive, and will use the default proxy retry
  // duration otherwise. Additionally updates |proxy_retry_info| with
  // |another_proxy_to_bypass| if non-empty.
  void UpdateRetryInfoOnFallback(
      ProxyRetryInfoMap* proxy_retry_info,
      base::TimeDelta retry_delay,
      const ProxyServer& another_proxy_to_bypass,
      const BoundNetLog& net_log) const;

 private:
  // Updates |proxy_retry_info| to indicate that the proxy in |proxies_| with
  // the URI of |proxy_key| is bad. The |proxy_key| must start with the scheme
  // (only if https) followed by the host and  then an explicit port. For
  // example, if the proxy origin is https://proxy.chromium.org:443/ the key is
  // https://proxy.chrome.org:443 whereas if the origin is
  // http://proxy.chrome.org/, the key is proxy.chrome.org:80.
  void AddProxyToRetryList(ProxyRetryInfoMap* proxy_retry_info,
                           base::TimeDelta retry_delay,
                           const std::string& proxy_key,
                           const BoundNetLog& net_log) const;

  // List of proxies.
  std::vector<ProxyServer> proxies_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_LIST_H_
