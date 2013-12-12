// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mapped_host_resolver.h"

#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string FirstAddress(const AddressList& address_list) {
  if (address_list.empty())
    return std::string();
  return address_list.front().ToString();
}

TEST(MappedHostResolverTest, Inclusion) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  scoped_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddSimulatedFailure("*google.com");
  resolver_impl->rules()->AddRule("baz.com", "192.168.1.5");
  resolver_impl->rules()->AddRule("foo.com", "192.168.1.8");
  resolver_impl->rules()->AddRule("proxy", "192.168.1.11");

  // Create a remapped resolver that uses |resolver_impl|.
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl.PassAs<HostResolver>()));

  int rv;
  AddressList address_list;

  // Try resolving "www.google.com:80". There are no mappings yet, so this
  // hits |resolver_impl| and fails.
  TestCompletionCallback callback;
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);

  // Remap *.google.com to baz.com.
  EXPECT_TRUE(resolver->AddRuleFromString("map *.google.com baz.com"));

  // Try resolving "www.google.com:80". Should be remapped to "baz.com:80".
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.5:80", FirstAddress(address_list));

  // Try resolving "foo.com:77". This will NOT be remapped, so result
  // is "foo.com:77".
  rv = resolver->Resolve(HostResolver::RequestInfo(HostPortPair("foo.com", 77)),
                         DEFAULT_PRIORITY,
                         &address_list,
                         callback.callback(),
                         NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.8:77", FirstAddress(address_list));

  // Remap "*.org" to "proxy:99".
  EXPECT_TRUE(resolver->AddRuleFromString("Map *.org proxy:99"));

  // Try resolving "chromium.org:61". Should be remapped to "proxy:99".
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("chromium.org", 61)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.11:99", FirstAddress(address_list));
}

// Tests that exclusions are respected.
TEST(MappedHostResolverTest, Exclusion) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  scoped_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("baz", "192.168.1.5");
  resolver_impl->rules()->AddRule("www.google.com", "192.168.1.3");

  // Create a remapped resolver that uses |resolver_impl|.
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl.PassAs<HostResolver>()));

  int rv;
  AddressList address_list;
  TestCompletionCallback callback;

  // Remap "*.com" to "baz".
  EXPECT_TRUE(resolver->AddRuleFromString("map *.com baz"));

  // Add an exclusion for "*.google.com".
  EXPECT_TRUE(resolver->AddRuleFromString("EXCLUDE *.google.com"));

  // Try resolving "www.google.com". Should not be remapped due to exclusion).
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.3:80", FirstAddress(address_list));

  // Try resolving "chrome.com:80". Should be remapped to "baz:80".
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("chrome.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.5:80", FirstAddress(address_list));
}

TEST(MappedHostResolverTest, SetRulesFromString) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  scoped_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("baz", "192.168.1.7");
  resolver_impl->rules()->AddRule("bar", "192.168.1.9");

  // Create a remapped resolver that uses |resolver_impl|.
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl.PassAs<HostResolver>()));

  int rv;
  AddressList address_list;
  TestCompletionCallback callback;

  // Remap "*.com" to "baz", and *.net to "bar:60".
  resolver->SetRulesFromString("map *.com baz , map *.net bar:60");

  // Try resolving "www.google.com". Should be remapped to "baz".
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.7:80", FirstAddress(address_list));

  // Try resolving "chrome.net:80". Should be remapped to "bar:60".
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("chrome.net", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.9:60", FirstAddress(address_list));
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST(MappedHostResolverTest, ParseInvalidRules) {
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(scoped_ptr<HostResolver>()));

  EXPECT_FALSE(resolver->AddRuleFromString("xyz"));
  EXPECT_FALSE(resolver->AddRuleFromString(std::string()));
  EXPECT_FALSE(resolver->AddRuleFromString(" "));
  EXPECT_FALSE(resolver->AddRuleFromString("EXCLUDE"));
  EXPECT_FALSE(resolver->AddRuleFromString("EXCLUDE foo bar"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE x"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE x :10"));
}

// Test mapping hostnames to resolving failures.
TEST(MappedHostResolverTest, MapToError) {
  scoped_ptr<MockHostResolver> resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("*", "192.168.1.5");

  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl.PassAs<HostResolver>()));

  int rv;
  AddressList address_list;

  // Remap *.google.com to resolving failures.
  EXPECT_TRUE(resolver->AddRuleFromString("MAP *.google.com ~NOTFOUND"));

  // Try resolving www.google.com --> Should give an error.
  TestCompletionCallback callback1;
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.google.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback1.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);

  // Try resolving www.foo.com --> Should succeed.
  TestCompletionCallback callback2;
  rv = resolver->Resolve(
      HostResolver::RequestInfo(HostPortPair("www.foo.com", 80)),
      DEFAULT_PRIORITY,
      &address_list,
      callback2.callback(),
      NULL,
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.5:80", FirstAddress(address_list));
}

}  // namespace

}  // namespace net
