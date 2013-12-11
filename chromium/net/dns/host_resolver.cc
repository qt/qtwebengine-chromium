// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_impl.h"

namespace net {

namespace {

// Maximum of 6 concurrent resolver threads (excluding retries).
// Some routers (or resolvers) appear to start to provide host-not-found if
// too many simultaneous resolutions are pending.  This number needs to be
// further optimized, but 8 is what FF currently does. We found some routers
// that limit this to 6, so we're temporarily holding it at that level.
const size_t kDefaultMaxProcTasks = 6u;

// When configuring from field trial, do not allow
const size_t kSaneMaxProcTasks = 20u;

PrioritizedDispatcher::Limits GetDispatcherLimits(
    const HostResolver::Options& options) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES,
                                       options.max_concurrent_resolves);

  // If not using default, do not use the field trial.
  if (limits.total_jobs != HostResolver::kDefaultParallelism)
    return limits;

  // Default, without trial is no reserved slots.
  limits.total_jobs = kDefaultMaxProcTasks;

  // Parallelism is determined by the field trial.
  std::string group = base::FieldTrialList::FindFullName(
      "HostResolverDispatch");

  if (group.empty())
    return limits;

  // The format of the group name is a list of non-negative integers separated
  // by ':'. Each of the elements in the list corresponds to an element in
  // |reserved_slots|, except the last one which is the |total_jobs|.

  std::vector<std::string> group_parts;
  base::SplitString(group, ':', &group_parts);
  if (group_parts.size() != NUM_PRIORITIES + 1) {
    NOTREACHED();
    return limits;
  }

  std::vector<size_t> parsed(group_parts.size());
  size_t total_reserved_slots = 0;

  for (size_t i = 0; i < group_parts.size(); ++i) {
    if (!base::StringToSizeT(group_parts[i], &parsed[i])) {
      NOTREACHED();
      return limits;
    }
  }

  size_t total_jobs = parsed.back();
  parsed.pop_back();
  for (size_t i = 0; i < parsed.size(); ++i) {
    total_reserved_slots += parsed[i];
  }

  // There must be some unreserved slots available for the all priorities.
  if (total_reserved_slots > total_jobs ||
      (total_reserved_slots == total_jobs && parsed[MINIMUM_PRIORITY] == 0)) {
    NOTREACHED();
    return limits;
  }

  limits.total_jobs = total_jobs;
  limits.reserved_slots = parsed;
  return limits;
}

}  // namespace

HostResolver::Options::Options()
    : max_concurrent_resolves(kDefaultParallelism),
      max_retry_attempts(kDefaultRetryAttempts),
      enable_caching(true) {
}

HostResolver::RequestInfo::RequestInfo(const HostPortPair& host_port_pair)
    : host_port_pair_(host_port_pair),
      address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      host_resolver_flags_(0),
      allow_cached_response_(true),
      is_speculative_(false) {}

HostResolver::~HostResolver() {
}

AddressFamily HostResolver::GetDefaultAddressFamily() const {
  return ADDRESS_FAMILY_UNSPECIFIED;
}

void HostResolver::SetDnsClientEnabled(bool enabled) {
}

HostCache* HostResolver::GetHostCache() {
  return NULL;
}

base::Value* HostResolver::GetDnsConfigAsValue() const {
  return NULL;
}

// static
scoped_ptr<HostResolver>
HostResolver::CreateSystemResolver(const Options& options, NetLog* net_log) {
  scoped_ptr<HostCache> cache;
  if (options.enable_caching)
    cache = HostCache::CreateDefaultCache();
  return scoped_ptr<HostResolver>(new HostResolverImpl(
      cache.Pass(),
      GetDispatcherLimits(options),
      HostResolverImpl::ProcTaskParams(NULL, options.max_retry_attempts),
      net_log));
}

// static
scoped_ptr<HostResolver>
HostResolver::CreateDefaultResolver(NetLog* net_log) {
  return CreateSystemResolver(Options(), net_log);
}

HostResolver::HostResolver() {
}

}  // namespace net
