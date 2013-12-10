// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

class NET_EXPORT_PRIVATE DnsConfigServicePosix : public DnsConfigService {
 public:
  DnsConfigServicePosix();
  virtual ~DnsConfigServicePosix();

 protected:
  // DnsConfigService:
  virtual void ReadNow() OVERRIDE;
  virtual bool StartWatching() OVERRIDE;

 private:
  class Watcher;
  class ConfigReader;
  class HostsReader;

  void OnConfigChanged(bool succeeded);
  void OnHostsChanged(bool succeeded);

  scoped_ptr<Watcher> watcher_;
  scoped_refptr<ConfigReader> config_reader_;
  scoped_refptr<HostsReader> hosts_reader_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};

enum ConfigParsePosixResult {
  CONFIG_PARSE_POSIX_OK = 0,
  CONFIG_PARSE_POSIX_RES_INIT_FAILED,
  CONFIG_PARSE_POSIX_RES_INIT_UNSET,
  CONFIG_PARSE_POSIX_BAD_ADDRESS,
  CONFIG_PARSE_POSIX_BAD_EXT_STRUCT,
  CONFIG_PARSE_POSIX_NULL_ADDRESS,
  CONFIG_PARSE_POSIX_NO_NAMESERVERS,
  CONFIG_PARSE_POSIX_MISSING_OPTIONS,
  CONFIG_PARSE_POSIX_UNHANDLED_OPTIONS,
  CONFIG_PARSE_POSIX_NO_DNSINFO,
  CONFIG_PARSE_POSIX_MAX  // Bounding values for enumeration.
};

// Fills in |dns_config| from |res|.
ConfigParsePosixResult NET_EXPORT_PRIVATE ConvertResStateToDnsConfig(
    const struct __res_state& res, DnsConfig* dns_config);

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
