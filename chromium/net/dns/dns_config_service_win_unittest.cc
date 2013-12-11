// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/win/windows_version.h"
#include "net/dns/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsConfigServiceWinTest, ParseSearchList) {
  const struct TestCase {
    const wchar_t* input;
    const char* output[4];  // NULL-terminated, empty if expected false
  } cases[] = {
    { L"chromium.org", { "chromium.org", NULL } },
    { L"chromium.org,org", { "chromium.org", "org", NULL } },
    // Empty suffixes terminate the list
    { L"crbug.com,com,,org", { "crbug.com", "com", NULL } },
    // IDN are converted to punycode
    { L"\u017c\xf3\u0142ta.pi\u0119\u015b\u0107.pl,pl",
        { "xn--ta-4ja03asj.xn--pi-wla5e0q.pl", "pl", NULL } },
    // Empty search list is invalid
    { L"", { NULL } },
    { L",,", { NULL } },
  };

  std::vector<std::string> actual_output, expected_output;
  for (unsigned i = 0; i < arraysize(cases); ++i) {
    const TestCase& t = cases[i];
    actual_output.clear();
    actual_output.push_back("UNSET");
    expected_output.clear();
    for (const char* const* output = t.output; *output; ++output) {
      expected_output.push_back(*output);
    }
    bool result = internal::ParseSearchList(t.input, &actual_output);
    if (!expected_output.empty()) {
      EXPECT_TRUE(result);
      EXPECT_EQ(expected_output, actual_output);
    } else {
      EXPECT_FALSE(result) << "Unexpected parse success on " << t.input;
    }
  }
}

struct AdapterInfo {
  IFTYPE if_type;
  IF_OPER_STATUS oper_status;
  const WCHAR* dns_suffix;
  std::string dns_server_addresses[4];  // Empty string indicates end.
  int ports[4];
};

scoped_ptr_malloc<IP_ADAPTER_ADDRESSES> CreateAdapterAddresses(
    const AdapterInfo* infos) {
  size_t num_adapters = 0;
  size_t num_addresses = 0;
  for (size_t i = 0; infos[i].if_type; ++i) {
    ++num_adapters;
    for (size_t j = 0; !infos[i].dns_server_addresses[j].empty(); ++j) {
      ++num_addresses;
    }
  }

  size_t heap_size = num_adapters * sizeof(IP_ADAPTER_ADDRESSES) +
                     num_addresses * (sizeof(IP_ADAPTER_DNS_SERVER_ADDRESS) +
                                      sizeof(struct sockaddr_storage));
  scoped_ptr_malloc<IP_ADAPTER_ADDRESSES> heap(
      reinterpret_cast<IP_ADAPTER_ADDRESSES*>(malloc(heap_size)));
  CHECK(heap.get());
  memset(heap.get(), 0, heap_size);

  IP_ADAPTER_ADDRESSES* adapters = heap.get();
  IP_ADAPTER_DNS_SERVER_ADDRESS* addresses =
      reinterpret_cast<IP_ADAPTER_DNS_SERVER_ADDRESS*>(adapters + num_adapters);
  struct sockaddr_storage* storage =
      reinterpret_cast<struct sockaddr_storage*>(addresses + num_addresses);

  for (size_t i = 0; i < num_adapters; ++i) {
    const AdapterInfo& info = infos[i];
    IP_ADAPTER_ADDRESSES* adapter = adapters + i;
    if (i + 1 < num_adapters)
      adapter->Next = adapter + 1;
    adapter->IfType = info.if_type;
    adapter->OperStatus = info.oper_status;
    adapter->DnsSuffix = const_cast<PWCHAR>(info.dns_suffix);
    IP_ADAPTER_DNS_SERVER_ADDRESS* address = NULL;
    for (size_t j = 0; !info.dns_server_addresses[j].empty(); ++j) {
      --num_addresses;
      if (j == 0) {
        address = adapter->FirstDnsServerAddress = addresses + num_addresses;
      } else {
        // Note that |address| is moving backwards.
        address = address->Next = address - 1;
      }
      IPAddressNumber ip;
      CHECK(ParseIPLiteralToNumber(info.dns_server_addresses[j], &ip));
      IPEndPoint ipe(ip, info.ports[j]);
      address->Address.lpSockaddr =
          reinterpret_cast<LPSOCKADDR>(storage + num_addresses);
      socklen_t length = sizeof(struct sockaddr_storage);
      CHECK(ipe.ToSockAddr(address->Address.lpSockaddr, &length));
      address->Address.iSockaddrLength = static_cast<int>(length);
    }
  }

  return heap.Pass();
}

TEST(DnsConfigServiceWinTest, ConvertAdapterAddresses) {
  // Check nameservers and connection-specific suffix.
  const struct TestCase {
    AdapterInfo input_adapters[4];        // |if_type| == 0 indicates end.
    std::string expected_nameservers[4];  // Empty string indicates end.
    std::string expected_suffix;
    int expected_ports[4];
  } cases[] = {
    {  // Ignore loopback and inactive adapters.
      {
        { IF_TYPE_SOFTWARE_LOOPBACK, IfOperStatusUp, L"funnyloop",
          { "2.0.0.2" } },
        { IF_TYPE_FASTETHER, IfOperStatusDormant, L"example.com",
          { "1.0.0.1" } },
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org",
          { "10.0.0.10", "2001:FFFF::1111" } },
        { 0 },
      },
      { "10.0.0.10", "2001:FFFF::1111" },
      "chromium.org",
    },
    {  // Respect configured ports.
      {
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org",
        { "10.0.0.10", "2001:FFFF::1111" }, { 1024, 24 } },
        { 0 },
      },
      { "10.0.0.10", "2001:FFFF::1111" },
      "chromium.org",
      { 1024, 24 },
    },
    {  // Use the preferred adapter (first in binding order) and filter
       // stateless DNS discovery addresses.
      {
        { IF_TYPE_SOFTWARE_LOOPBACK, IfOperStatusUp, L"funnyloop",
          { "2.0.0.2" } },
        { IF_TYPE_FASTETHER, IfOperStatusUp, L"example.com",
          { "1.0.0.1", "fec0:0:0:ffff::2", "8.8.8.8" } },
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org",
          { "10.0.0.10", "2001:FFFF::1111" } },
        { 0 },
      },
      { "1.0.0.1", "8.8.8.8" },
      "example.com",
    },
    {  // No usable adapters.
      {
        { IF_TYPE_SOFTWARE_LOOPBACK, IfOperStatusUp, L"localhost",
          { "2.0.0.2" } },
        { IF_TYPE_FASTETHER, IfOperStatusDormant, L"example.com",
          { "1.0.0.1" } },
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org" },
        { 0 },
      },
    },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    const TestCase& t = cases[i];
    internal::DnsSystemSettings settings = {
      CreateAdapterAddresses(t.input_adapters),
      // Default settings for the rest.
    };
    std::vector<IPEndPoint> expected_nameservers;
    for (size_t j = 0; !t.expected_nameservers[j].empty(); ++j) {
      IPAddressNumber ip;
      ASSERT_TRUE(ParseIPLiteralToNumber(t.expected_nameservers[j], &ip));
      int port = t.expected_ports[j];
      if (!port)
        port = dns_protocol::kDefaultPort;
      expected_nameservers.push_back(IPEndPoint(ip, port));
    }

    DnsConfig config;
    internal::ConfigParseWinResult result =
        internal::ConvertSettingsToDnsConfig(settings, &config);
    internal::ConfigParseWinResult expected_result =
        expected_nameservers.empty() ? internal::CONFIG_PARSE_WIN_NO_NAMESERVERS
            : internal::CONFIG_PARSE_WIN_OK;
    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(expected_nameservers, config.nameservers);
    if (result == internal::CONFIG_PARSE_WIN_OK) {
      ASSERT_EQ(1u, config.search.size());
      EXPECT_EQ(t.expected_suffix, config.search[0]);
    }
  }
}

TEST(DnsConfigServiceWinTest, ConvertSuffixSearch) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    internal::DnsSystemSettings input_settings;
    std::string expected_search[5];
  } cases[] = {
    {  // Policy SearchList override.
      {
        CreateAdapterAddresses(infos),
        { true, L"policy.searchlist.a,policy.searchlist.b" },
        { true, L"tcpip.searchlist.a,tcpip.searchlist.b" },
        { true, L"tcpip.domain" },
        { true, L"primary.dns.suffix" },
      },
      { "policy.searchlist.a", "policy.searchlist.b" },
    },
    {  // User-specified SearchList override.
      {
        CreateAdapterAddresses(infos),
        { false },
        { true, L"tcpip.searchlist.a,tcpip.searchlist.b" },
        { true, L"tcpip.domain" },
        { true, L"primary.dns.suffix" },
      },
      { "tcpip.searchlist.a", "tcpip.searchlist.b" },
    },
    {  // Void SearchList. Using tcpip.domain
      {
        CreateAdapterAddresses(infos),
        { true, L",bad.searchlist,parsed.as.empty" },
        { true, L"tcpip.searchlist,good.but.overridden" },
        { true, L"tcpip.domain" },
        { false },
      },
      { "tcpip.domain", "connection.suffix" },
    },
    {  // Void SearchList. Using primary.dns.suffix
      {
        CreateAdapterAddresses(infos),
        { true, L",bad.searchlist,parsed.as.empty" },
        { true, L"tcpip.searchlist,good.but.overridden" },
        { true, L"tcpip.domain" },
        { true, L"primary.dns.suffix" },
      },
      { "primary.dns.suffix", "connection.suffix" },
    },
    {  // Void SearchList. Using tcpip.domain when primary.dns.suffix is empty
      {
        CreateAdapterAddresses(infos),
        { true, L",bad.searchlist,parsed.as.empty" },
        { true, L"tcpip.searchlist,good.but.overridden" },
        { true, L"tcpip.domain" },
        { true, L"" },
      },
      { "tcpip.domain", "connection.suffix" },
    },
    {  // Void SearchList. Using tcpip.domain when primary.dns.suffix is NULL
      {
        CreateAdapterAddresses(infos),
        { true, L",bad.searchlist,parsed.as.empty" },
        { true, L"tcpip.searchlist,good.but.overridden" },
        { true, L"tcpip.domain" },
        { true },
      },
      { "tcpip.domain", "connection.suffix" },
    },
    {  // No primary suffix. Devolution does not matter.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true },
        { true },
        { { true, 1 }, { true, 2 } },
      },
      { "connection.suffix" },
    },
    {  // Devolution enabled by policy, level by dnscache.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b.c.d.e" },
        { false },
        { { true, 1 }, { false } },   // policy_devolution: enabled, level
        { { true, 0 }, { true, 3 } }, // dnscache_devolution
        { { true, 0 }, { true, 1 } }, // tcpip_devolution
      },
      { "a.b.c.d.e", "connection.suffix", "b.c.d.e", "c.d.e" },
    },
    {  // Devolution enabled by dnscache, level by policy.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b.c.d.e" },
        { true, L"f.g.i.l.j" },
        { { false }, { true, 4 } },
        { { true, 1 }, { false } },
        { { true, 0 }, { true, 3 } },
      },
      { "f.g.i.l.j", "connection.suffix", "g.i.l.j" },
    },
    {  // Devolution enabled by default.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b.c.d.e" },
        { false },
        { { false }, { false } },
        { { false }, { true, 3 } },
        { { false }, { true, 1 } },
      },
      { "a.b.c.d.e", "connection.suffix", "b.c.d.e", "c.d.e" },
    },
    {  // Devolution enabled at level = 2, but nothing to devolve.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b" },
        { false },
        { { false }, { false } },
        { { false }, { true, 2 } },
        { { false }, { true, 2 } },
      },
      { "a.b", "connection.suffix" },
    },
    {  // Devolution disabled when no explicit level.
       // Windows XP and Vista use a default level = 2, but we don't.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b.c.d.e" },
        { false },
        { { true, 1 }, { false } },
        { { true, 1 }, { false } },
        { { true, 1 }, { false } },
      },
      { "a.b.c.d.e", "connection.suffix" },
    },
    {  // Devolution disabled by policy level.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b.c.d.e" },
        { false },
        { { false }, { true, 1 } },
        { { true, 1 }, { true, 3 } },
        { { true, 1 }, { true, 4 } },
      },
      { "a.b.c.d.e", "connection.suffix" },
    },
    {  // Devolution disabled by user setting.
      {
        CreateAdapterAddresses(infos),
        { false },
        { false },
        { true, L"a.b.c.d.e" },
        { false },
        { { false }, { true, 3 } },
        { { false }, { true, 3 } },
        { { true, 0 }, { true, 3 } },
      },
      { "a.b.c.d.e", "connection.suffix" },
    },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    const TestCase& t = cases[i];
    DnsConfig config;
    EXPECT_EQ(internal::CONFIG_PARSE_WIN_OK,
              internal::ConvertSettingsToDnsConfig(t.input_settings, &config));
    std::vector<std::string> expected_search;
    for (size_t j = 0; !t.expected_search[j].empty(); ++j) {
      expected_search.push_back(t.expected_search[j]);
    }
    EXPECT_EQ(expected_search, config.search);
  }
}

TEST(DnsConfigServiceWinTest, AppendToMultiLabelName) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  // The default setting was true pre-Vista.
  bool default_value = (base::win::GetVersion() < base::win::VERSION_VISTA);

  const struct TestCase {
    internal::DnsSystemSettings::RegDword input;
    bool expected_output;
  } cases[] = {
    { { true, 0 }, false },
    { { true, 1 }, true },
    { { false, 0 }, default_value },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    const TestCase& t = cases[i];
    internal::DnsSystemSettings settings = {
      CreateAdapterAddresses(infos),
      { false }, { false }, { false }, { false },
      { { false }, { false } },
      { { false }, { false } },
      { { false }, { false } },
      t.input,
    };
    DnsConfig config;
    EXPECT_EQ(internal::CONFIG_PARSE_WIN_OK,
              internal::ConvertSettingsToDnsConfig(settings, &config));
    EXPECT_EQ(t.expected_output, config.append_to_multi_label_name);
  }
}

// Setting have_name_resolution_policy_table should set unhandled_options.
TEST(DnsConfigServiceWinTest, HaveNRPT) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    bool have_nrpt;
    bool unhandled_options;
    internal::ConfigParseWinResult result;
  } cases[] = {
    { false, false, internal::CONFIG_PARSE_WIN_OK },
    { true, true, internal::CONFIG_PARSE_WIN_UNHANDLED_OPTIONS },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    const TestCase& t = cases[i];
    internal::DnsSystemSettings settings = {
      CreateAdapterAddresses(infos),
      { false }, { false }, { false }, { false },
      { { false }, { false } },
      { { false }, { false } },
      { { false }, { false } },
      { false },
      t.have_nrpt,
    };
    DnsConfig config;
    EXPECT_EQ(t.result,
              internal::ConvertSettingsToDnsConfig(settings, &config));
    EXPECT_EQ(t.unhandled_options, config.unhandled_options);
    EXPECT_EQ(t.have_nrpt, config.use_local_ipv6);
  }
}


}  // namespace

}  // namespace net

