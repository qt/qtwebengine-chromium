// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_v8_tracing.h"

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_cache.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

class ProxyResolverV8TracingTest : public testing::Test {
 public:
  virtual void TearDown() OVERRIDE {
    // Drain any pending messages, which may be left over from cancellation.
    // This way they get reliably run as part of the current test, rather than
    // spilling into the next test's execution.
    base::MessageLoop::current()->RunUntilIdle();
  }
};

scoped_refptr<ProxyResolverScriptData> LoadScriptData(const char* filename) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("proxy_resolver_v8_tracing_unittest");
  path = path.AppendASCII(filename);

  // Try to read the file from disk.
  std::string file_contents;
  bool ok = base::ReadFileToString(path, &file_contents);

  // If we can't load the file from disk, something is misconfigured.
  EXPECT_TRUE(ok) << "Failed to read file: " << path.value();

  // Load the PAC script into the ProxyResolver.
  return ProxyResolverScriptData::FromUTF8(file_contents);
}

void InitResolver(ProxyResolverV8Tracing* resolver, const char* filename) {
  TestCompletionCallback callback;
  int rv =
      resolver->SetPacScript(LoadScriptData(filename), callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
}

class MockErrorObserver : public ProxyResolverErrorObserver {
 public:
  MockErrorObserver() : event_(true, false) {}

  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE {
    {
      base::AutoLock l(lock_);
      output += base::StringPrintf("Error: line %d: %s\n", line_number,
                                   UTF16ToASCII(error).c_str());
    }
    event_.Signal();
  }

  std::string GetOutput() {
    base::AutoLock l(lock_);
    return output;
  }

  void WaitForOutput() {
    event_.Wait();
  }

 private:
  base::Lock lock_;
  std::string output;

  base::WaitableEvent event_;
};

TEST_F(ProxyResolverV8TracingTest, Simple) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  InitResolver(&resolver, "simple.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info, callback.callback(),
      NULL, request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ("foo:99", proxy_info.proxy_server().ToURI());

  EXPECT_EQ(0u, host_resolver.num_resolve());

  // There were no errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- nothing was logged.
  EXPECT_EQ(0u, log.GetSize());
  EXPECT_EQ(0u, request_log.GetSize());
}

TEST_F(ProxyResolverV8TracingTest, JavascriptError) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  InitResolver(&resolver, "error.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://throw-an-error/"), &proxy_info, callback.callback(), NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, callback.WaitForResult());

  EXPECT_EQ(0u, host_resolver.num_resolve());

  EXPECT_EQ("Error: line 5: Uncaught TypeError: Cannot call method 'split' "
            "of null\n", error_observer->GetOutput());

  // Check the NetLogs -- there was 1 alert and 1 javascript error, and they
  // were output to both the global log, and per-request log.
  CapturingNetLog::CapturedEntryList entries_list[2];
  log.GetEntries(&entries_list[0]);
  request_log.GetEntries(&entries_list[1]);

  for (size_t list_i = 0; list_i < arraysize(entries_list); list_i++) {
    const CapturingNetLog::CapturedEntryList& entries = entries_list[list_i];
    EXPECT_EQ(2u, entries.size());
    EXPECT_TRUE(
        LogContainsEvent(entries, 0, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                         NetLog::PHASE_NONE));
    EXPECT_TRUE(
        LogContainsEvent(entries, 1, NetLog::TYPE_PAC_JAVASCRIPT_ERROR,
                         NetLog::PHASE_NONE));

    EXPECT_EQ("{\"message\":\"Prepare to DIE!\"}", entries[0].GetParamsJson());
    EXPECT_EQ("{\"line_number\":5,\"message\":\"Uncaught TypeError: Cannot "
              "call method 'split' of null\"}", entries[1].GetParamsJson());
  }
}

TEST_F(ProxyResolverV8TracingTest, TooManyAlerts) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  InitResolver(&resolver, "too_many_alerts.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"),
      &proxy_info,
      callback.callback(),
      NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  // Iteration1 does a DNS resolve
  // Iteration2 exceeds the alert buffer
  // Iteration3 runs in blocking mode and completes
  EXPECT_EQ("foo:3", proxy_info.proxy_server().ToURI());

  EXPECT_EQ(1u, host_resolver.num_resolve());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- the script generated 50 alerts, which were mirrored
  // to both the global and per-request logs.
  CapturingNetLog::CapturedEntryList entries_list[2];
  log.GetEntries(&entries_list[0]);
  request_log.GetEntries(&entries_list[1]);

  for (size_t list_i = 0; list_i < arraysize(entries_list); list_i++) {
    const CapturingNetLog::CapturedEntryList& entries = entries_list[list_i];
    EXPECT_EQ(50u, entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      ASSERT_TRUE(
          LogContainsEvent(entries, i, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                           NetLog::PHASE_NONE));
    }
  }
}

// Verify that buffered alerts cannot grow unboundedly, even when the message is
// empty string.
TEST_F(ProxyResolverV8TracingTest, TooManyEmptyAlerts) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  InitResolver(&resolver, "too_many_empty_alerts.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"),
      &proxy_info,
      callback.callback(),
      NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ("foo:3", proxy_info.proxy_server().ToURI());

  EXPECT_EQ(1u, host_resolver.num_resolve());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- the script generated 50 alerts, which were mirrored
  // to both the global and per-request logs.
  CapturingNetLog::CapturedEntryList entries_list[2];
  log.GetEntries(&entries_list[0]);
  request_log.GetEntries(&entries_list[1]);

  for (size_t list_i = 0; list_i < arraysize(entries_list); list_i++) {
    const CapturingNetLog::CapturedEntryList& entries = entries_list[list_i];
    EXPECT_EQ(1000u, entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      ASSERT_TRUE(
          LogContainsEvent(entries, i, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                           NetLog::PHASE_NONE));
    }
  }
}

// This test runs a PAC script that issues a sequence of DNS resolves. The test
// verifies the final result, and that the underlying DNS resolver received
// the correct set of queries.
TEST_F(ProxyResolverV8TracingTest, Dns) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRuleForAddressFamily(
      "host1", ADDRESS_FAMILY_IPV4, "166.155.144.44");
  host_resolver.rules()
      ->AddIPLiteralRule("host1", "::1,192.168.1.1", std::string());
  host_resolver.rules()->AddSimulatedFailure("host2");
  host_resolver.rules()->AddRule("host3", "166.155.144.33");
  host_resolver.rules()->AddRule("host5", "166.155.144.55");
  host_resolver.rules()->AddSimulatedFailure("host6");
  host_resolver.rules()->AddRuleForAddressFamily(
      "*", ADDRESS_FAMILY_IPV4, "122.133.144.155");
  host_resolver.rules()->AddRule("*", "133.122.100.200");

  InitResolver(&resolver, "dns.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"),
      &proxy_info,
      callback.callback(),
      NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  // The test does 13 DNS resolution, however only 7 of them are unique.
  EXPECT_EQ(7u, host_resolver.num_resolve());

  const char* kExpectedResult =
    "122.133.144.155-"  // myIpAddress()
    "null-"  // dnsResolve('')
    "__1_192.168.1.1-"  // dnsResolveEx('host1')
    "null-"  // dnsResolve('host2')
    "166.155.144.33-"  // dnsResolve('host3')
    "122.133.144.155-"  // myIpAddress()
    "166.155.144.33-"  // dnsResolve('host3')
    "__1_192.168.1.1-"  // dnsResolveEx('host1')
    "122.133.144.155-"  // myIpAddress()
    "null-"  // dnsResolve('host2')
    "-"  // dnsResolveEx('host6')
    "133.122.100.200-"  // myIpAddressEx()
    "166.155.144.44"  // dnsResolve('host1')
    ":99";

  EXPECT_EQ(kExpectedResult, proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- the script generated 1 alert, mirrored to both
  // the per-request and global logs.
  CapturingNetLog::CapturedEntryList entries_list[2];
  log.GetEntries(&entries_list[0]);
  request_log.GetEntries(&entries_list[1]);

  for (size_t list_i = 0; list_i < arraysize(entries_list); list_i++) {
    const CapturingNetLog::CapturedEntryList& entries = entries_list[list_i];
    EXPECT_EQ(1u, entries.size());
    EXPECT_TRUE(
        LogContainsEvent(entries, 0, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                         NetLog::PHASE_NONE));
    EXPECT_EQ("{\"message\":\"iteration: 7\"}", entries[0].GetParamsJson());
  }
}

// This test runs a PAC script that does "myIpAddress()" followed by
// "dnsResolve()". This requires 2 restarts. However once the HostResolver's
// cache is warmed, subsequent calls should take 0 restarts.
TEST_F(ProxyResolverV8TracingTest, DnsChecksCache) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("foopy", "166.155.144.11");
  host_resolver.rules()->AddRule("*", "122.133.144.155");

  InitResolver(&resolver, "simple_dns.js");

  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foopy/req1"),
      &proxy_info,
      callback1.callback(),
      NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback1.WaitForResult());

  // The test does 2 DNS resolutions.
  EXPECT_EQ(2u, host_resolver.num_resolve());

  // The first request took 2 restarts, hence on g_iteration=3.
  EXPECT_EQ("166.155.144.11:3", proxy_info.proxy_server().ToURI());

  rv = resolver.GetProxyForURL(
      GURL("http://foopy/req2"),
      &proxy_info,
      callback2.callback(),
      NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback2.WaitForResult());

  EXPECT_EQ(4u, host_resolver.num_resolve());

  // This time no restarts were required, so g_iteration incremented by 1.
  EXPECT_EQ("166.155.144.11:4", proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  EXPECT_EQ(0u, log.GetSize());
  EXPECT_EQ(0u, request_log.GetSize());
}

// This test runs a weird PAC script that was designed to defeat the DNS tracing
// optimization. The proxy resolver should detect the inconsistency and
// fall-back to synchronous mode execution.
TEST_F(ProxyResolverV8TracingTest, FallBackToSynchronous1) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("host1", "166.155.144.11");
  host_resolver.rules()->AddRule("crazy4", "133.199.111.4");
  host_resolver.rules()->AddRule("*", "122.133.144.155");

  InitResolver(&resolver, "global_sideffects1.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info, callback.callback(), NULL,
      request_log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  // The script itself only does 2 DNS resolves per execution, however it
  // constructs the hostname using a global counter which changes on each
  // invocation.
  EXPECT_EQ(3u, host_resolver.num_resolve());

  EXPECT_EQ("166.155.144.11-133.199.111.4:100",
            proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- the script generated 1 alert, mirrored to both
  // the per-request and global logs.
  CapturingNetLog::CapturedEntryList entries_list[2];
  log.GetEntries(&entries_list[0]);
  request_log.GetEntries(&entries_list[1]);

  for (size_t list_i = 0; list_i < arraysize(entries_list); list_i++) {
    const CapturingNetLog::CapturedEntryList& entries = entries_list[list_i];
    EXPECT_EQ(1u, entries.size());
    EXPECT_TRUE(
        LogContainsEvent(entries, 0, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                         NetLog::PHASE_NONE));
    EXPECT_EQ("{\"message\":\"iteration: 4\"}", entries[0].GetParamsJson());
  }
}

// This test runs a weird PAC script that was designed to defeat the DNS tracing
// optimization. The proxy resolver should detect the inconsistency and
// fall-back to synchronous mode execution.
TEST_F(ProxyResolverV8TracingTest, FallBackToSynchronous2) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("host1", "166.155.144.11");
  host_resolver.rules()->AddRule("host2", "166.155.144.22");
  host_resolver.rules()->AddRule("host3", "166.155.144.33");
  host_resolver.rules()->AddRule("host4", "166.155.144.44");
  host_resolver.rules()->AddRule("*", "122.133.144.155");

  InitResolver(&resolver, "global_sideffects2.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info, callback.callback(), NULL,
      request_log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ(3u, host_resolver.num_resolve());

  EXPECT_EQ("166.155.144.44:100", proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- nothing was logged.
  EXPECT_EQ(0u, log.GetSize());
  EXPECT_EQ(0u, request_log.GetSize());
}

// This test runs a weird PAC script that yields a never ending sequence
// of DNS resolves when restarting. Running it will hit the maximum
// DNS resolves per request limit (20) after which every DNS resolve will
// fail.
TEST_F(ProxyResolverV8TracingTest, InfiniteDNSSequence) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("host*", "166.155.144.11");
  host_resolver.rules()->AddRule("*", "122.133.144.155");

  InitResolver(&resolver, "global_sideffects3.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info, callback.callback(), NULL,
      request_log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ(20u, host_resolver.num_resolve());

  EXPECT_EQ(
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "166.155.144.11-166.155.144.11-166.155.144.11-166.155.144.11-"
      "null:21", proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- 1 alert was logged.
  EXPECT_EQ(1u, log.GetSize());
  EXPECT_EQ(1u, request_log.GetSize());
}

// This test runs a weird PAC script that yields a never ending sequence
// of DNS resolves when restarting. Running it will hit the maximum
// DNS resolves per request limit (20) after which every DNS resolve will
// fail.
TEST_F(ProxyResolverV8TracingTest, InfiniteDNSSequence2) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("host*", "166.155.144.11");
  host_resolver.rules()->AddRule("*", "122.133.144.155");

  InitResolver(&resolver, "global_sideffects4.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info, callback.callback(), NULL,
      request_log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ(20u, host_resolver.num_resolve());

  EXPECT_EQ("null21:34", proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  // Check the NetLogs -- 1 alert was logged.
  EXPECT_EQ(1u, log.GetSize());
  EXPECT_EQ(1u, request_log.GetSize());
}

void DnsDuringInitHelper(bool synchronous_host_resolver) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  host_resolver.set_synchronous_mode(synchronous_host_resolver);
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("host1", "91.13.12.1");
  host_resolver.rules()->AddRule("host2", "91.13.12.2");

  InitResolver(&resolver, "dns_during_init.js");

  // Initialization did 2 dnsResolves.
  EXPECT_EQ(2u, host_resolver.num_resolve());

  host_resolver.rules()->ClearRules();
  host_resolver.GetHostCache()->clear();

  host_resolver.rules()->AddRule("host1", "145.88.13.3");
  host_resolver.rules()->AddRule("host2", "137.89.8.45");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info, callback.callback(), NULL,
      request_log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  // Fetched host1 and host2 again, since the ones done during initialization
  // should not have been cached.
  EXPECT_EQ(4u, host_resolver.num_resolve());

  EXPECT_EQ("91.13.12.1-91.13.12.2-145.88.13.3-137.89.8.45:99",
            proxy_info.proxy_server().ToURI());

  // Check the NetLogs -- the script generated 2 alerts during initialization.
  EXPECT_EQ(0u, request_log.GetSize());
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  ASSERT_EQ(2u, entries.size());
  EXPECT_TRUE(
      LogContainsEvent(entries, 0, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                       NetLog::PHASE_NONE));
  EXPECT_TRUE(
      LogContainsEvent(entries, 1, NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                       NetLog::PHASE_NONE));

  EXPECT_EQ("{\"message\":\"Watsup\"}", entries[0].GetParamsJson());
  EXPECT_EQ("{\"message\":\"Watsup2\"}", entries[1].GetParamsJson());
}

// Tests a PAC script which does DNS resolves during initialization.
TEST_F(ProxyResolverV8TracingTest, DnsDuringInit) {
  // Test with both both a host resolver that always completes asynchronously,
  // and then again with one that completes synchronously.
  DnsDuringInitHelper(false);
  DnsDuringInitHelper(true);
}

void CrashCallback(int) {
  // Be extra sure that if the callback ever gets invoked, the test will fail.
  CHECK(false);
}

// Start some requests, cancel them all, and then destroy the resolver.
// Note the execution order for this test can vary. Since multiple
// threads are involved, the cancellation may be received a different
// times.
TEST_F(ProxyResolverV8TracingTest, CancelAll) {
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  host_resolver.rules()->AddSimulatedFailure("*");

  InitResolver(&resolver, "dns.js");

  const size_t kNumRequests = 5;
  ProxyInfo proxy_info[kNumRequests];
  ProxyResolver::RequestHandle request[kNumRequests];

  for (size_t i = 0; i < kNumRequests; ++i) {
    int rv = resolver.GetProxyForURL(
        GURL("http://foo/"), &proxy_info[i],
        base::Bind(&CrashCallback), &request[i], BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  for (size_t i = 0; i < kNumRequests; ++i) {
    resolver.CancelRequest(request[i]);
  }
}

// Note the execution order for this test can vary. Since multiple
// threads are involved, the cancellation may be received a different
// times.
TEST_F(ProxyResolverV8TracingTest, CancelSome) {
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  host_resolver.rules()->AddSimulatedFailure("*");

  InitResolver(&resolver, "dns.js");

  ProxyInfo proxy_info1;
  ProxyInfo proxy_info2;
  ProxyResolver::RequestHandle request1;
  ProxyResolver::RequestHandle request2;
  TestCompletionCallback callback;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info1,
      base::Bind(&CrashCallback), &request1, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info2,
      callback.callback(), &request2, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  resolver.CancelRequest(request1);

  EXPECT_EQ(OK, callback.WaitForResult());
}

// Cancel a request after it has finished running on the worker thread, and has
// posted a task the completion task back to origin thread.
TEST_F(ProxyResolverV8TracingTest, CancelWhilePendingCompletionTask) {
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  host_resolver.rules()->AddSimulatedFailure("*");

  InitResolver(&resolver, "error.js");

  ProxyInfo proxy_info1;
  ProxyInfo proxy_info2;
  ProxyInfo proxy_info3;
  ProxyResolver::RequestHandle request1;
  ProxyResolver::RequestHandle request2;
  ProxyResolver::RequestHandle request3;
  TestCompletionCallback callback;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info1,
      base::Bind(&CrashCallback), &request1, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = resolver.GetProxyForURL(
      GURL("http://throw-an-error/"), &proxy_info2,
      callback.callback(), &request2, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait until the first request has finished running on the worker thread.
  // (The second request will output an error).
  error_observer->WaitForOutput();

  // Cancel the first request, while it has a pending completion task on
  // the origin thread.
  resolver.CancelRequest(request1);

  EXPECT_EQ(ERR_PAC_SCRIPT_FAILED, callback.WaitForResult());

  // Start another request, to make sure it is able to complete.
  rv = resolver.GetProxyForURL(
      GURL("http://i-have-no-idea-what-im-doing/"), &proxy_info3,
      callback.callback(), &request3, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ("i-approve-this-message:42",
            proxy_info3.proxy_server().ToURI());
}

// This implementation of HostResolver allows blocking until a resolve request
// has been received. The resolve requests it receives will never be completed.
class BlockableHostResolver : public HostResolver {
 public:
  BlockableHostResolver()
      : num_cancelled_requests_(0), waiting_for_resolve_(false) {}

  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) OVERRIDE {
    EXPECT_FALSE(callback.is_null());
    EXPECT_TRUE(out_req);

    if (!action_.is_null())
      action_.Run();

    // Indicate to the caller that a request was received.
    EXPECT_TRUE(waiting_for_resolve_);
    base::MessageLoop::current()->Quit();

    // This line is intentionally after action_.Run(), since one of the
    // tests does a cancellation inside of Resolve(), and it is more
    // interesting if *out_req hasn't been written yet at that point.
    *out_req = reinterpret_cast<RequestHandle*>(1);  // Magic value.

    // Return ERR_IO_PENDING as this request will NEVER be completed.
    // Expectation is for the caller to later cancel the request.
    return ERR_IO_PENDING;
  }

  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const BoundNetLog& net_log) OVERRIDE {
    NOTREACHED();
    return ERR_DNS_CACHE_MISS;
  }

  virtual void CancelRequest(RequestHandle req) OVERRIDE {
    EXPECT_EQ(reinterpret_cast<RequestHandle*>(1), req);
    num_cancelled_requests_++;
  }

  void SetAction(const base::Callback<void(void)>& action) {
    action_ = action;
  }

  // Waits until Resolve() has been called.
  void WaitUntilRequestIsReceived() {
    waiting_for_resolve_ = true;
    base::MessageLoop::current()->Run();
    DCHECK(waiting_for_resolve_);
    waiting_for_resolve_ = false;
  }

  int num_cancelled_requests() const {
    return num_cancelled_requests_;
  }

 private:
  int num_cancelled_requests_;
  bool waiting_for_resolve_;
  base::Callback<void(void)> action_;
};

// This cancellation test exercises a more predictable cancellation codepath --
// when the request has an outstanding DNS request in flight.
TEST_F(ProxyResolverV8TracingTest, CancelWhileOutstandingNonBlockingDns) {
  BlockableHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  InitResolver(&resolver, "dns.js");

  ProxyInfo proxy_info1;
  ProxyInfo proxy_info2;
  ProxyResolver::RequestHandle request1;
  ProxyResolver::RequestHandle request2;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/req1"), &proxy_info1,
      base::Bind(&CrashCallback), &request1, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);

  host_resolver.WaitUntilRequestIsReceived();

  rv = resolver.GetProxyForURL(
      GURL("http://foo/req2"), &proxy_info2,
      base::Bind(&CrashCallback), &request2, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);

  host_resolver.WaitUntilRequestIsReceived();

  resolver.CancelRequest(request1);
  resolver.CancelRequest(request2);

  EXPECT_EQ(2, host_resolver.num_cancelled_requests());

  // After leaving this scope, the ProxyResolver is destroyed.
  // This should not cause any problems, as the outstanding work
  // should have been cancelled.
}

void CancelRequestAndPause(ProxyResolverV8Tracing* resolver,
                           ProxyResolver::RequestHandle request) {
  resolver->CancelRequest(request);

  // Sleep for a little bit. This makes it more likely for the worker
  // thread to have returned from its call, and serves as a regression
  // test for http://crbug.com/173373.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(30));
}

// In non-blocking mode, the worker thread actually does block for
// a short time to see if the result is in the DNS cache. Test
// cancellation while the worker thread is waiting on this event.
TEST_F(ProxyResolverV8TracingTest, CancelWhileBlockedInNonBlockingDns) {
  BlockableHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  InitResolver(&resolver, "dns.js");

  ProxyInfo proxy_info;
  ProxyResolver::RequestHandle request;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info,
      base::Bind(&CrashCallback), &request, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);

  host_resolver.SetAction(
      base::Bind(CancelRequestAndPause, &resolver, request));

  host_resolver.WaitUntilRequestIsReceived();

  // At this point the host resolver ran Resolve(), and should have cancelled
  // the request.

  EXPECT_EQ(1, host_resolver.num_cancelled_requests());
}

// Cancel the request while there is a pending DNS request, however before
// the request is sent to the host resolver.
TEST_F(ProxyResolverV8TracingTest, CancelWhileBlockedInNonBlockingDns2) {
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  InitResolver(&resolver, "dns.js");

  ProxyInfo proxy_info;
  ProxyResolver::RequestHandle request;

  int rv = resolver.GetProxyForURL(
      GURL("http://foo/"), &proxy_info,
      base::Bind(&CrashCallback), &request, BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait a bit, so the DNS task has hopefully been posted. The test will
  // work whatever the delay is here, but it is most useful if the delay
  // is large enough to allow a task to be posted back.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
  resolver.CancelRequest(request);

  EXPECT_EQ(0u, host_resolver.num_resolve());
}

TEST_F(ProxyResolverV8TracingTest, CancelSetPacWhileOutstandingBlockingDns) {
  BlockableHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;

  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, NULL);

  int rv =
      resolver.SetPacScript(LoadScriptData("dns_during_init.js"),
      base::Bind(&CrashCallback));
  EXPECT_EQ(ERR_IO_PENDING, rv);

  host_resolver.WaitUntilRequestIsReceived();

  resolver.CancelSetPacScript();
  EXPECT_EQ(1, host_resolver.num_cancelled_requests());
}

// This tests that the execution of a PAC script is terminated when the DNS
// dependencies are missing. If the test fails, then it will hang.
TEST_F(ProxyResolverV8TracingTest, Terminate) {
  CapturingNetLog log;
  CapturingBoundNetLog request_log;
  MockCachingHostResolver host_resolver;
  MockErrorObserver* error_observer = new MockErrorObserver;
  ProxyResolverV8Tracing resolver(&host_resolver, error_observer, &log);

  host_resolver.rules()->AddRule("host1", "182.111.0.222");
  host_resolver.rules()->AddRule("host2", "111.33.44.55");

  InitResolver(&resolver, "terminate.js");

  TestCompletionCallback callback;
  ProxyInfo proxy_info;

  int rv = resolver.GetProxyForURL(
      GURL("http://foopy/req1"),
      &proxy_info,
      callback.callback(),
      NULL,
      request_log.bound());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  // The test does 2 DNS resolutions.
  EXPECT_EQ(2u, host_resolver.num_resolve());

  EXPECT_EQ("foopy:3", proxy_info.proxy_server().ToURI());

  // No errors.
  EXPECT_EQ("", error_observer->GetOutput());

  EXPECT_EQ(0u, log.GetSize());
  EXPECT_EQ(0u, request_log.GetSize());
}

// Tests that multiple instances of ProxyResolverV8Tracing can coexist and run
// correctly at the same time. This is relevant because at the moment (time
// this test was written) each ProxyResolverV8Tracing creates its own thread to
// run V8 on, however each thread is operating on the same v8::Isolate.
TEST_F(ProxyResolverV8TracingTest, MultipleResolvers) {
  // ------------------------
  // Setup resolver0
  // ------------------------
  MockHostResolver host_resolver0;
  host_resolver0.rules()->AddRuleForAddressFamily(
      "host1", ADDRESS_FAMILY_IPV4, "166.155.144.44");
  host_resolver0.rules()
      ->AddIPLiteralRule("host1", "::1,192.168.1.1", std::string());
  host_resolver0.rules()->AddSimulatedFailure("host2");
  host_resolver0.rules()->AddRule("host3", "166.155.144.33");
  host_resolver0.rules()->AddRule("host5", "166.155.144.55");
  host_resolver0.rules()->AddSimulatedFailure("host6");
  host_resolver0.rules()->AddRuleForAddressFamily(
      "*", ADDRESS_FAMILY_IPV4, "122.133.144.155");
  host_resolver0.rules()->AddRule("*", "133.122.100.200");
  ProxyResolverV8Tracing resolver0(
      &host_resolver0, new MockErrorObserver, NULL);
  InitResolver(&resolver0, "dns.js");

  // ------------------------
  // Setup resolver1
  // ------------------------
  ProxyResolverV8Tracing resolver1(
      &host_resolver0, new MockErrorObserver, NULL);
  InitResolver(&resolver1, "dns.js");

  // ------------------------
  // Setup resolver2
  // ------------------------
  ProxyResolverV8Tracing resolver2(
      &host_resolver0, new MockErrorObserver, NULL);
  InitResolver(&resolver2, "simple.js");

  // ------------------------
  // Setup resolver3
  // ------------------------
  MockHostResolver host_resolver3;
  host_resolver3.rules()->AddRule("foo", "166.155.144.33");
  ProxyResolverV8Tracing resolver3(
      &host_resolver3, new MockErrorObserver, NULL);
  InitResolver(&resolver3, "simple_dns.js");

  // ------------------------
  // Queue up work for each resolver (which will be running in parallel).
  // ------------------------

  ProxyResolverV8Tracing* resolver[] = {
    &resolver0, &resolver1, &resolver2, &resolver3,
  };

  const size_t kNumResolvers = arraysize(resolver);
  const size_t kNumIterations = 20;
  const size_t kNumResults = kNumResolvers * kNumIterations;
  TestCompletionCallback callback[kNumResults];
  ProxyInfo proxy_info[kNumResults];

  for (size_t i = 0; i < kNumResults; ++i) {
    size_t resolver_i = i % kNumResolvers;
    int rv = resolver[resolver_i]->GetProxyForURL(
        GURL("http://foo/"), &proxy_info[i], callback[i].callback(), NULL,
        BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  // ------------------------
  // Verify all of the results.
  // ------------------------

  const char* kExpectedForDnsJs =
    "122.133.144.155-"  // myIpAddress()
    "null-"  // dnsResolve('')
    "__1_192.168.1.1-"  // dnsResolveEx('host1')
    "null-"  // dnsResolve('host2')
    "166.155.144.33-"  // dnsResolve('host3')
    "122.133.144.155-"  // myIpAddress()
    "166.155.144.33-"  // dnsResolve('host3')
    "__1_192.168.1.1-"  // dnsResolveEx('host1')
    "122.133.144.155-"  // myIpAddress()
    "null-"  // dnsResolve('host2')
    "-"  // dnsResolveEx('host6')
    "133.122.100.200-"  // myIpAddressEx()
    "166.155.144.44"  // dnsResolve('host1')
    ":99";

  for (size_t i = 0; i < kNumResults; ++i) {
    size_t resolver_i = i % kNumResolvers;
    EXPECT_EQ(OK, callback[i].WaitForResult());

    std::string proxy_uri = proxy_info[i].proxy_server().ToURI();

    if (resolver_i == 0 || resolver_i == 1) {
      EXPECT_EQ(kExpectedForDnsJs, proxy_uri);
    } else if (resolver_i == 2) {
      EXPECT_EQ("foo:99", proxy_uri);
    } else if (resolver_i == 3) {
      EXPECT_EQ("166.155.144.33:",
                proxy_uri.substr(0, proxy_uri.find(':') + 1));
    } else {
      NOTREACHED();
    }
  }
}

}  // namespace

}  // namespace net
