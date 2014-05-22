// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_host_resolver.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_cache.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// Cache size for the MockCachingHostResolver.
const unsigned kMaxCacheEntries = 100;
// TTL for the successful resolutions. Failures are not cached.
const unsigned kCacheEntryTTLSeconds = 60;

}  // namespace

int ParseAddressList(const std::string& host_list,
                     const std::string& canonical_name,
                     AddressList* addrlist) {
  *addrlist = AddressList();
  std::vector<std::string> addresses;
  base::SplitString(host_list, ',', &addresses);
  addrlist->set_canonical_name(canonical_name);
  for (size_t index = 0; index < addresses.size(); ++index) {
    IPAddressNumber ip_number;
    if (!ParseIPLiteralToNumber(addresses[index], &ip_number)) {
      LOG(WARNING) << "Not a supported IP literal: " << addresses[index];
      return ERR_UNEXPECTED;
    }
    addrlist->push_back(IPEndPoint(ip_number, -1));
  }
  return OK;
}

struct MockHostResolverBase::Request {
  Request(const RequestInfo& req_info,
          AddressList* addr,
          const CompletionCallback& cb)
      : info(req_info), addresses(addr), callback(cb) {}
  RequestInfo info;
  AddressList* addresses;
  CompletionCallback callback;
};

MockHostResolverBase::~MockHostResolverBase() {
  STLDeleteValues(&requests_);
}

int MockHostResolverBase::Resolve(const RequestInfo& info,
                                  RequestPriority priority,
                                  AddressList* addresses,
                                  const CompletionCallback& callback,
                                  RequestHandle* handle,
                                  const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());
  last_request_priority_ = priority;
  num_resolve_++;
  size_t id = next_request_id_++;
  int rv = ResolveFromIPLiteralOrCache(info, addresses);
  if (rv != ERR_DNS_CACHE_MISS) {
    return rv;
  }
  if (synchronous_mode_) {
    return ResolveProc(id, info, addresses);
  }
  // Store the request for asynchronous resolution
  Request* req = new Request(info, addresses, callback);
  requests_[id] = req;
  if (handle)
    *handle = reinterpret_cast<RequestHandle>(id);

  if (!ondemand_mode_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockHostResolverBase::ResolveNow, AsWeakPtr(), id));
  }

  return ERR_IO_PENDING;
}

int MockHostResolverBase::ResolveFromCache(const RequestInfo& info,
                                           AddressList* addresses,
                                           const BoundNetLog& net_log) {
  num_resolve_from_cache_++;
  DCHECK(CalledOnValidThread());
  next_request_id_++;
  int rv = ResolveFromIPLiteralOrCache(info, addresses);
  return rv;
}

void MockHostResolverBase::CancelRequest(RequestHandle handle) {
  DCHECK(CalledOnValidThread());
  size_t id = reinterpret_cast<size_t>(handle);
  RequestMap::iterator it = requests_.find(id);
  if (it != requests_.end()) {
    scoped_ptr<Request> req(it->second);
    requests_.erase(it);
  } else {
    NOTREACHED() << "CancelRequest must NOT be called after request is "
        "complete or canceled.";
  }
}

HostCache* MockHostResolverBase::GetHostCache() {
  return cache_.get();
}

void MockHostResolverBase::ResolveAllPending() {
  DCHECK(CalledOnValidThread());
  DCHECK(ondemand_mode_);
  for (RequestMap::iterator i = requests_.begin(); i != requests_.end(); ++i) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockHostResolverBase::ResolveNow, AsWeakPtr(), i->first));
  }
}

// start id from 1 to distinguish from NULL RequestHandle
MockHostResolverBase::MockHostResolverBase(bool use_caching)
    : last_request_priority_(DEFAULT_PRIORITY),
      synchronous_mode_(false),
      ondemand_mode_(false),
      next_request_id_(1),
      num_resolve_(0),
      num_resolve_from_cache_(0) {
  rules_ = CreateCatchAllHostResolverProc();

  if (use_caching) {
    cache_.reset(new HostCache(kMaxCacheEntries));
  }
}

int MockHostResolverBase::ResolveFromIPLiteralOrCache(const RequestInfo& info,
                                                      AddressList* addresses) {
  IPAddressNumber ip;
  if (ParseIPLiteralToNumber(info.hostname(), &ip)) {
    *addresses = AddressList::CreateFromIPAddress(ip, info.port());
    if (info.host_resolver_flags() & HOST_RESOLVER_CANONNAME)
      addresses->SetDefaultCanonicalName();
    return OK;
  }
  int rv = ERR_DNS_CACHE_MISS;
  if (cache_.get() && info.allow_cached_response()) {
    HostCache::Key key(info.hostname(),
                       info.address_family(),
                       info.host_resolver_flags());
    const HostCache::Entry* entry = cache_->Lookup(key, base::TimeTicks::Now());
    if (entry) {
      rv = entry->error;
      if (rv == OK)
        *addresses = AddressList::CopyWithPort(entry->addrlist, info.port());
    }
  }
  return rv;
}

int MockHostResolverBase::ResolveProc(size_t id,
                                      const RequestInfo& info,
                                      AddressList* addresses) {
  AddressList addr;
  int rv = rules_->Resolve(info.hostname(),
                           info.address_family(),
                           info.host_resolver_flags(),
                           &addr,
                           NULL);
  if (cache_.get()) {
    HostCache::Key key(info.hostname(),
                       info.address_family(),
                       info.host_resolver_flags());
    // Storing a failure with TTL 0 so that it overwrites previous value.
    base::TimeDelta ttl;
    if (rv == OK)
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);
    cache_->Set(key, HostCache::Entry(rv, addr), base::TimeTicks::Now(), ttl);
  }
  if (rv == OK)
    *addresses = AddressList::CopyWithPort(addr, info.port());
  return rv;
}

void MockHostResolverBase::ResolveNow(size_t id) {
  RequestMap::iterator it = requests_.find(id);
  if (it == requests_.end())
    return;  // was canceled

  scoped_ptr<Request> req(it->second);
  requests_.erase(it);
  int rv = ResolveProc(id, req->info, req->addresses);
  if (!req->callback.is_null())
    req->callback.Run(rv);
}

//-----------------------------------------------------------------------------

struct RuleBasedHostResolverProc::Rule {
  enum ResolverType {
    kResolverTypeFail,
    kResolverTypeSystem,
    kResolverTypeIPLiteral,
  };

  ResolverType resolver_type;
  std::string host_pattern;
  AddressFamily address_family;
  HostResolverFlags host_resolver_flags;
  std::string replacement;
  std::string canonical_name;
  int latency_ms;  // In milliseconds.

  Rule(ResolverType resolver_type,
       const std::string& host_pattern,
       AddressFamily address_family,
       HostResolverFlags host_resolver_flags,
       const std::string& replacement,
       const std::string& canonical_name,
       int latency_ms)
      : resolver_type(resolver_type),
        host_pattern(host_pattern),
        address_family(address_family),
        host_resolver_flags(host_resolver_flags),
        replacement(replacement),
        canonical_name(canonical_name),
        latency_ms(latency_ms) {}
};

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous)
    : HostResolverProc(previous) {
}

void RuleBasedHostResolverProc::AddRule(const std::string& host_pattern,
                                        const std::string& replacement) {
  AddRuleForAddressFamily(host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
                          replacement);
}

void RuleBasedHostResolverProc::AddRuleForAddressFamily(
    const std::string& host_pattern,
    AddressFamily address_family,
    const std::string& replacement) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem,
            host_pattern,
            address_family,
            flags,
            replacement,
            std::string(),
            0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRule(
    const std::string& host_pattern,
    const std::string& ip_literal,
    const std::string& canonical_name) {
  // Literals are always resolved to themselves by HostResolverImpl,
  // consequently we do not support remapping them.
  IPAddressNumber ip_number;
  DCHECK(!ParseIPLiteralToNumber(host_pattern, &ip_number));
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  if (!canonical_name.empty())
    flags |= HOST_RESOLVER_CANONNAME;
  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal, canonical_name,
            0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem,
            host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED,
            flags,
            replacement,
            std::string(),
            latency_ms);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem,
            host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED,
            flags,
            std::string(),
            std::string(),
            0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeFail,
            host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED,
            flags,
            std::string(),
            std::string(),
            0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::ClearRules() {
  rules_.clear();
}

int RuleBasedHostResolverProc::Resolve(const std::string& host,
                                       AddressFamily address_family,
                                       HostResolverFlags host_resolver_flags,
                                       AddressList* addrlist,
                                       int* os_error) {
  RuleList::iterator r;
  for (r = rules_.begin(); r != rules_.end(); ++r) {
    bool matches_address_family =
        r->address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        r->address_family == address_family;
    // Ignore HOST_RESOLVER_SYSTEM_ONLY, since it should have no impact on
    // whether a rule matches.
    HostResolverFlags flags = host_resolver_flags & ~HOST_RESOLVER_SYSTEM_ONLY;
    // Flags match if all of the bitflags in host_resolver_flags are enabled
    // in the rule's host_resolver_flags. However, the rule may have additional
    // flags specified, in which case the flags should still be considered a
    // match.
    bool matches_flags = (r->host_resolver_flags & flags) == flags;
    if (matches_flags && matches_address_family &&
        MatchPattern(host, r->host_pattern)) {
      if (r->latency_ms != 0) {
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(r->latency_ms));
      }

      // Remap to a new host.
      const std::string& effective_host =
          r->replacement.empty() ? host : r->replacement;

      // Apply the resolving function to the remapped hostname.
      switch (r->resolver_type) {
        case Rule::kResolverTypeFail:
          return ERR_NAME_NOT_RESOLVED;
        case Rule::kResolverTypeSystem:
#if defined(OS_WIN)
          net::EnsureWinsockInit();
#endif
          return SystemHostResolverCall(effective_host,
                                        address_family,
                                        host_resolver_flags,
                                        addrlist, os_error);
        case Rule::kResolverTypeIPLiteral:
          return ParseAddressList(effective_host,
                                  r->canonical_name,
                                  addrlist);
        default:
          NOTREACHED();
          return ERR_UNEXPECTED;
      }
    }
  }
  return ResolveUsingPrevious(host, address_family,
                              host_resolver_flags, addrlist, os_error);
}

RuleBasedHostResolverProc::~RuleBasedHostResolverProc() {
}

RuleBasedHostResolverProc* CreateCatchAllHostResolverProc() {
  RuleBasedHostResolverProc* catchall = new RuleBasedHostResolverProc(NULL);
  catchall->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  // Next add a rules-based layer the use controls.
  return new RuleBasedHostResolverProc(catchall);
}

//-----------------------------------------------------------------------------

int HangingHostResolver::Resolve(const RequestInfo& info,
                                 RequestPriority priority,
                                 AddressList* addresses,
                                 const CompletionCallback& callback,
                                 RequestHandle* out_req,
                                 const BoundNetLog& net_log) {
  return ERR_IO_PENDING;
}

int HangingHostResolver::ResolveFromCache(const RequestInfo& info,
                                          AddressList* addresses,
                                          const BoundNetLog& net_log) {
  return ERR_DNS_CACHE_MISS;
}

//-----------------------------------------------------------------------------

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc() {}

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc(
    HostResolverProc* proc) {
  Init(proc);
}

ScopedDefaultHostResolverProc::~ScopedDefaultHostResolverProc() {
  HostResolverProc* old_proc =
      HostResolverProc::SetDefault(previous_proc_.get());
  // The lifetimes of multiple instances must be nested.
  CHECK_EQ(old_proc, current_proc_);
}

void ScopedDefaultHostResolverProc::Init(HostResolverProc* proc) {
  current_proc_ = proc;
  previous_proc_ = HostResolverProc::SetDefault(current_proc_.get());
  current_proc_->SetLastProc(previous_proc_.get());
}

}  // namespace net
