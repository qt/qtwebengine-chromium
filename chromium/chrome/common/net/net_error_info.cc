// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/common/net/net_error_info.h"

namespace chrome_common_net {

const char kDnsProbeErrorDomain[] = "dnsprobe";

const char* DnsProbeStatusToString(int status) {
  switch (status) {
  case DNS_PROBE_POSSIBLE:
    return "DNS_PROBE_POSSIBLE";
  case DNS_PROBE_NOT_RUN:
    return "DNS_PROBE_NOT_RUN";
  case DNS_PROBE_STARTED:
    return "DNS_PROBE_STARTED";
  case DNS_PROBE_FINISHED_INCONCLUSIVE:
    return "DNS_PROBE_FINISHED_INCONCLUSIVE";
  case DNS_PROBE_FINISHED_NO_INTERNET:
    return "DNS_PROBE_FINISHED_NO_INTERNET";
  case DNS_PROBE_FINISHED_BAD_CONFIG:
    return "DNS_PROBE_FINISHED_BAD_CONFIG";
  case DNS_PROBE_FINISHED_NXDOMAIN:
    return "DNS_PROBE_FINISHED_NXDOMAIN";
  default:
    NOTREACHED();
    return "";
  }
}

bool DnsProbeStatusIsFinished(DnsProbeStatus status) {
  return status >= DNS_PROBE_FINISHED_INCONCLUSIVE &&
         status < DNS_PROBE_MAX;
}

}  // namespace chrome_common_net
