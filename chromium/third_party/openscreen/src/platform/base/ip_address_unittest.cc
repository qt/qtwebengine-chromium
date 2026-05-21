// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/ip_address.h"

#include <format>

#include "build/build_config.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"

namespace openscreen {

#if BUILDFLAG(IS_MAC)
constexpr char kLoopbackInterface[] = "lo0";
#else
constexpr char kLoopbackInterface[] = "lo";
#endif

using ::testing::ElementsAreArray;

TEST(IPAddressTest, V4Constructors) {
  IPAddress address1(std::array<uint8_t, 4>{1, 2, 3, 4});
  EXPECT_THAT(address1.bytes(), ElementsAreArray({1, 2, 3, 4}));

  uint8_t x[] = {4, 3, 2, 1};
  IPAddress address2(x);
  EXPECT_THAT(address2.bytes(), ElementsAreArray(x));

  const auto b = address2.bytes();
  const uint8_t raw_bytes[4]{b[0], b[1], b[2], b[3]};
  EXPECT_THAT(raw_bytes, ElementsAreArray(x));

  IPAddress address3(IPAddress::Version::kV4, x);
  EXPECT_THAT(address3.bytes(), ElementsAreArray(x));

  IPAddress address4(6, 5, 7, 9);
  EXPECT_THAT(address4.bytes(), ElementsAreArray({6, 5, 7, 9}));

  IPAddress address5(address4);
  EXPECT_THAT(address5.bytes(), ElementsAreArray({6, 5, 7, 9}));

  address5 = address1;
  EXPECT_THAT(address5.bytes(), ElementsAreArray({1, 2, 3, 4}));
}

TEST(IPAddressTest, V4ComparisonAndBoolean) {
  IPAddress address1;
  EXPECT_EQ(address1, address1);
  EXPECT_FALSE(address1);

  uint8_t x[] = {4, 3, 2, 1};
  IPAddress address2(x);
  EXPECT_NE(address1, address2);
  EXPECT_TRUE(address2);

  IPAddress address3(x);
  EXPECT_EQ(address2, address3);
  EXPECT_TRUE(address3);

  address2 = address1;
  EXPECT_EQ(address1, address2);
  EXPECT_FALSE(address2);
}

TEST(IPAddressTest, V4Parse) {
  ErrorOr<IPAddress> address = IPAddress::Parse("192.168.0.1");
  ASSERT_TRUE(address);
  EXPECT_THAT(address.value().bytes(), ElementsAreArray({192, 168, 0, 1}));
}

TEST(IPAddressTest, V4ParseFailures) {
  EXPECT_FALSE(IPAddress::Parse("192..0.1"))
      << "empty value should fail to parse";
  EXPECT_FALSE(IPAddress::Parse(".192.168.0.1"))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse(".192.168.1"))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("..192.168.0.1"))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("..192.1"))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.168.0.1."))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.168.1."))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.168.1.."))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.168.."))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.x3.0.1"))
      << "non-digit character should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.3.1"))
      << "too few values should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("192.3.2.0.1"))
      << "too many values should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("1920.3.2.1"))
      << "value > 255 should fail to parse";
}

TEST(IPAddressTest, V6Constructors) {
  IPAddress address1(std::array<uint16_t, 8>{0x0102, 0x0304, 0x0506, 0x0708,
                                             0x090a, 0x0b0c, 0x0d0e, 0x0f10});
  EXPECT_THAT(address1.bytes(), ElementsAreArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                                  11, 12, 13, 14, 15, 16}));

  const uint8_t x[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  const uint16_t hextets[] = {0x0102, 0x0304, 0x0506, 0x0708,
                              0x090a, 0x0b0c, 0x0d0e, 0x0f10};
  IPAddress address2(hextets);
  EXPECT_THAT(address2.bytes(), ElementsAreArray(x));

  IPAddress address3(IPAddress::Version::kV6, x);
  EXPECT_THAT(address3.bytes(), ElementsAreArray(x));

  IPAddress address4(0x100f, 0x0e0d, 0x0c0b, 0x0a09, 0x0807, 0x0605, 0x0403,
                     0x0201);
  EXPECT_THAT(address4.bytes(), ElementsAreArray({16, 15, 14, 13, 12, 11, 10, 9,
                                                  8, 7, 6, 5, 4, 3, 2, 1}));

  IPAddress address5(address4);
  EXPECT_THAT(address5.bytes(), ElementsAreArray({16, 15, 14, 13, 12, 11, 10, 9,
                                                  8, 7, 6, 5, 4, 3, 2, 1}));
}

TEST(IPAddressTest, V6ComparisonAndBoolean) {
  IPAddress address1;
  EXPECT_EQ(address1, address1);
  EXPECT_FALSE(address1);

  uint8_t x[] = {16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  IPAddress address2(IPAddress::Version::kV6, x);
  EXPECT_NE(address1, address2);
  EXPECT_TRUE(address2);

  IPAddress address3(IPAddress::Version::kV6, x);
  EXPECT_EQ(address2, address3);
  EXPECT_TRUE(address3);

  address2 = address1;
  EXPECT_EQ(address1, address2);
  EXPECT_FALSE(address2);
}

TEST(IPAddressTest, V6ParseBasic) {
  ErrorOr<IPAddress> address =
      IPAddress::Parse("abcd:ef01:2345:6789:9876:5432:10FE:DBCA");
  ASSERT_TRUE(address);
  EXPECT_THAT(
      address.value().bytes(),
      ElementsAreArray({0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0x98,
                        0x76, 0x54, 0x32, 0x10, 0xfe, 0xdb, 0xca}));
}

TEST(IPAddressTest, V6ParseDoubleColon) {
  ErrorOr<IPAddress> address1 =
      IPAddress::Parse("abcd:ef01:2345:6789:9876:5432::dbca");
  ASSERT_TRUE(address1);
  EXPECT_THAT(
      address1.value().bytes(),
      ElementsAreArray({0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0x98,
                        0x76, 0x54, 0x32, 0x00, 0x00, 0xdb, 0xca}));
  ErrorOr<IPAddress> address2 = IPAddress::Parse("abcd::10fe:dbca");
  ASSERT_TRUE(address2);
  EXPECT_THAT(
      address2.value().bytes(),
      ElementsAreArray({0xab, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x10, 0xfe, 0xdb, 0xca}));

  ErrorOr<IPAddress> address3 = IPAddress::Parse("::10fe:dbca");
  ASSERT_TRUE(address3);
  EXPECT_THAT(
      address3.value().bytes(),
      ElementsAreArray({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x10, 0xfe, 0xdb, 0xca}));

  ErrorOr<IPAddress> address4 = IPAddress::Parse("10fe:dbca::");
  ASSERT_TRUE(address4);
  EXPECT_THAT(
      address4.value().bytes(),
      ElementsAreArray({0x10, 0xfe, 0xdb, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST(IPAddressTest, V6SmallValues) {
  ErrorOr<IPAddress> address1 = IPAddress::Parse("::");
  ASSERT_TRUE(address1);
  EXPECT_THAT(
      address1.value().bytes(),
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));

  ErrorOr<IPAddress> address2 = IPAddress::Parse("::1");
  ASSERT_TRUE(address2);
  EXPECT_THAT(
      address2.value().bytes(),
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));

  ErrorOr<IPAddress> address3 = IPAddress::Parse("::2:1");
  ASSERT_TRUE(address3);
  EXPECT_THAT(
      address3.value().bytes(),
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01}));
}

TEST(IPAddressTest, V6ParseFailures) {
  EXPECT_FALSE(IPAddress::Parse(":abcd::dbca"))
      << "leading colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("abcd::dbca:"))
      << "trailing colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("abxd::1234"))
      << "non-hex digit should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("abcd:1234"))
      << "too few values should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("a:b:c:d:e:f:0:1:2:3:4:5:6:7:8:9:a"))
      << "too many values should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("1:2:3:4:5:6:7::8"))
      << "too many values around double-colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("1:2:3:4:5:6:7:8::"))
      << "too many values before double-colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("::1:2:3:4:5:6:7:8"))
      << "too many values after double-colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("abcd1::dbca"))
      << "value > 0xffff should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("::abcd::dbca"))
      << "multiple double colon should fail to parse";

  EXPECT_FALSE(IPAddress::Parse(":::abcd::dbca"))
      << "leading triple colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("abcd:::dbca"))
      << "triple colon should fail to parse";
  EXPECT_FALSE(IPAddress::Parse("abcd:dbca:::"))
      << "trailing triple colon should fail to parse";
}

TEST(IPAddressTest, V6ParseThreeDigitValue) {
  ErrorOr<IPAddress> address = IPAddress::Parse("::123");
  ASSERT_TRUE(address);
  EXPECT_THAT(
      address.value().bytes(),
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x23}));
}

TEST(IPAddressTest, IPEndpointBoolOperator) {
  IPEndpoint endpoint;
  ASSERT_FALSE((endpoint));
  ASSERT_TRUE((IPEndpoint{{192, 168, 0, 1}, 80}));
  ASSERT_TRUE((IPEndpoint{{192, 168, 0, 1}, 0}));
  ASSERT_TRUE((IPEndpoint{{}, 80}));
}

TEST(IPAddressTest, IPEndpointParse) {
  IPEndpoint expected{IPAddress(std::array<uint8_t, 4>{{1, 2, 3, 4}}), 5678};
  ErrorOr<IPEndpoint> result = IPEndpoint::Parse("1.2.3.4:5678");
  ASSERT_TRUE(result.is_value()) << result.error();
  EXPECT_EQ(expected, result.value());

  expected = IPEndpoint{
      IPAddress(std::array<uint16_t, 8>{{0xabcd, 0, 0, 0, 0, 0, 0, 1}}), 99};
  result = IPEndpoint::Parse("[abcd::1]:99");
  ASSERT_TRUE(result.is_value()) << result.error();
  EXPECT_EQ(expected, result.value());

  expected = IPEndpoint{
      IPAddress(std::array<uint16_t, 8>{{0, 0, 0, 0, 0, 0, 0, 0}}), 5791};
  result = IPEndpoint::Parse("[::]:5791");
  ASSERT_TRUE(result.is_value()) << result.error();
  EXPECT_EQ(expected, result.value());

  EXPECT_FALSE(IPEndpoint::Parse(""));              // Empty string.
  EXPECT_FALSE(IPEndpoint::Parse("beef"));          // Random word.
  EXPECT_FALSE(IPEndpoint::Parse("localhost:99"));  // We don't do DNS.
  EXPECT_FALSE(IPEndpoint::Parse(":80"));           // Missing address.
  EXPECT_FALSE(IPEndpoint::Parse("[]:22"));         // Missing address.
  EXPECT_FALSE(IPEndpoint::Parse("1.2.3.4"));       // Missing port after IPv4.
  EXPECT_FALSE(IPEndpoint::Parse("[abcd::1]"));     // Missing port after IPv6.
  EXPECT_FALSE(IPEndpoint::Parse("abcd::1:8080"));  // Missing square brackets.

  // No extra whitespace is allowed.
  EXPECT_FALSE(IPEndpoint::Parse(" 1.2.3.4:5678"));
  EXPECT_FALSE(IPEndpoint::Parse("1.2.3.4 :5678"));
  EXPECT_FALSE(IPEndpoint::Parse("1.2.3.4: 5678"));
  EXPECT_FALSE(IPEndpoint::Parse("1.2.3.4:5678 "));
  EXPECT_FALSE(IPEndpoint::Parse(" [abcd::1]:99"));
  EXPECT_FALSE(IPEndpoint::Parse("[abcd::1] :99"));
  EXPECT_FALSE(IPEndpoint::Parse("[abcd::1]: 99"));
  EXPECT_FALSE(IPEndpoint::Parse("[abcd::1]:99 "));
}

TEST(IPAddressTest, IPAddressComparisons) {
  const IPAddress kV4Low{192, 168, 0, 1};
  const IPAddress kV4High{192, 168, 0, 2};
  const IPAddress kV6Low{0, 0, 0, 0, 0, 0, 0, 1};
  const IPAddress kV6High{0, 0, 1, 0, 0, 0, 0, 0};

  EXPECT_TRUE(kV4Low == kV4Low);
  EXPECT_TRUE(kV4High == kV4High);
  EXPECT_TRUE(kV6Low == kV6Low);
  EXPECT_TRUE(kV6High == kV6High);
  EXPECT_FALSE(kV4Low == kV4High);
  EXPECT_FALSE(kV4High == kV4Low);
  EXPECT_FALSE(kV6Low == kV6High);
  EXPECT_FALSE(kV6High == kV6Low);

  EXPECT_FALSE(kV4Low != kV4Low);
  EXPECT_FALSE(kV4High != kV4High);
  EXPECT_FALSE(kV6Low != kV6Low);
  EXPECT_FALSE(kV6High != kV6High);
  EXPECT_TRUE(kV4Low != kV4High);
  EXPECT_TRUE(kV4High != kV4Low);
  EXPECT_TRUE(kV6Low != kV6High);
  EXPECT_TRUE(kV6High != kV6Low);

  EXPECT_TRUE(kV4Low < kV4High);
  EXPECT_TRUE(kV4High < kV6Low);
  EXPECT_TRUE(kV6Low < kV6High);
  EXPECT_FALSE(kV6High < kV6Low);
  EXPECT_FALSE(kV6Low < kV4High);
  EXPECT_FALSE(kV4High < kV4Low);

  EXPECT_FALSE(kV4Low > kV4High);
  EXPECT_FALSE(kV4High > kV6Low);
  EXPECT_FALSE(kV6Low > kV6High);
  EXPECT_TRUE(kV6High > kV6Low);
  EXPECT_TRUE(kV6Low > kV4High);
  EXPECT_TRUE(kV4High > kV4Low);

  EXPECT_TRUE(kV4Low <= kV4High);
  EXPECT_TRUE(kV4High <= kV6Low);
  EXPECT_TRUE(kV6Low <= kV6High);
  EXPECT_TRUE(kV4Low <= kV4Low);
  EXPECT_TRUE(kV4High <= kV4High);
  EXPECT_TRUE(kV6Low <= kV6Low);
  EXPECT_TRUE(kV6High <= kV6High);
  EXPECT_FALSE(kV6High <= kV6Low);
  EXPECT_FALSE(kV6Low <= kV4High);
  EXPECT_FALSE(kV4High <= kV4Low);

  EXPECT_FALSE(kV4Low >= kV4High);
  EXPECT_FALSE(kV4High >= kV6Low);
  EXPECT_FALSE(kV6Low >= kV6High);
  EXPECT_TRUE(kV4Low >= kV4Low);
  EXPECT_TRUE(kV4High >= kV4High);
  EXPECT_TRUE(kV6Low >= kV6Low);
  EXPECT_TRUE(kV6High >= kV6High);
  EXPECT_TRUE(kV6High >= kV6Low);
  EXPECT_TRUE(kV6Low >= kV4High);
  EXPECT_TRUE(kV4High >= kV4Low);
}

TEST(IPAddressTest, IPEndpointComparisons) {
  const IPEndpoint kV4LowHighPort{{192, 168, 0, 1}, 1000};
  const IPEndpoint kV4LowLowPort{{192, 168, 0, 1}, 1};
  const IPEndpoint kV4High{{192, 168, 0, 2}, 22};
  const IPEndpoint kV6Low{{0, 0, 0, 0, 0, 0, 0, 1}, 22};
  const IPEndpoint kV6High{{0, 0, 1, 0, 0, 0, 0, 0}, 22};

  EXPECT_TRUE(kV4LowHighPort == kV4LowHighPort);
  EXPECT_TRUE(kV4High == kV4High);
  EXPECT_TRUE(kV6Low == kV6Low);
  EXPECT_TRUE(kV6High == kV6High);

  EXPECT_TRUE(kV4LowLowPort != kV4LowHighPort);
  EXPECT_TRUE(kV4LowLowPort != kV4High);
  EXPECT_TRUE(kV4High != kV6Low);
  EXPECT_TRUE(kV6Low != kV6High);

  EXPECT_TRUE(kV4LowLowPort < kV4LowHighPort);
  EXPECT_TRUE(kV4LowLowPort < kV4High);
  EXPECT_TRUE(kV4High < kV6Low);
  EXPECT_TRUE(kV6Low < kV6High);

  EXPECT_TRUE(kV4LowHighPort > kV4LowLowPort);
  EXPECT_TRUE(kV4High > kV4LowLowPort);
  EXPECT_TRUE(kV6Low > kV4High);
  EXPECT_TRUE(kV6High > kV6Low);

  EXPECT_TRUE(kV4LowLowPort <= kV4LowHighPort);
  EXPECT_TRUE(kV4LowLowPort <= kV4High);
  EXPECT_TRUE(kV4High <= kV6Low);
  EXPECT_TRUE(kV6Low <= kV6High);
  EXPECT_TRUE(kV4LowLowPort <= kV4LowHighPort);
  EXPECT_TRUE(kV4LowLowPort <= kV4High);
  EXPECT_TRUE(kV4High <= kV6Low);
  EXPECT_TRUE(kV6Low <= kV6High);

  EXPECT_FALSE(kV4LowLowPort >= kV4LowHighPort);
  EXPECT_FALSE(kV4LowLowPort >= kV4High);
  EXPECT_FALSE(kV4High >= kV6Low);
  EXPECT_FALSE(kV6Low >= kV6High);
  EXPECT_TRUE(kV4LowHighPort >= kV4LowLowPort);
  EXPECT_TRUE(kV4High >= kV4LowLowPort);
  EXPECT_TRUE(kV6Low >= kV4High);
  EXPECT_TRUE(kV6High >= kV6Low);
  EXPECT_TRUE(kV4LowHighPort >= kV4LowLowPort);
  EXPECT_TRUE(kV4High >= kV4LowLowPort);
  EXPECT_TRUE(kV6Low >= kV4High);
  EXPECT_TRUE(kV6High >= kV6Low);
}

TEST(IPAddressTest, OstreamOperatorForIPv4) {
  std::ostringstream oss;
  oss << IPAddress{192, 168, 1, 2};
  EXPECT_EQ("192.168.1.2", oss.str());

  oss.str("");
  oss << IPAddress{192, 168, 0, 2};
  EXPECT_EQ("192.168.0.2", oss.str());

  oss.str("");
  oss << IPAddress{23, 45, 67, 89};
  EXPECT_EQ("23.45.67.89", oss.str());
}

TEST(IPAddressTest, V6IsLinkLocal) {
  ErrorOr<IPAddress> address = IPAddress::Parse("fe80::1");
  ASSERT_TRUE(address);
  EXPECT_TRUE(address.value().IsLinkLocal());

  address = IPAddress::Parse("fe90::1");
  ASSERT_TRUE(address);
  EXPECT_TRUE(address.value().IsLinkLocal());

  address = IPAddress::Parse("febf::ffff:ffff:ffff:ffff");
  ASSERT_TRUE(address);
  EXPECT_TRUE(address.value().IsLinkLocal());

  address = IPAddress::Parse("fec0::1");
  ASSERT_TRUE(address);
  EXPECT_FALSE(address.value().IsLinkLocal());

  address = IPAddress::Parse("::1");
  ASSERT_TRUE(address);
  EXPECT_FALSE(address.value().IsLinkLocal());
}

TEST(IPAddressTest, V6ParseLinkLocal) {
  // NOTE: This test is using the loopback interface kLoopbackInterface which
  // should exist on any Linux system.
  const ErrorOr<IPAddress> address =
      IPAddress::Parse(std::string("fe80::1%") + kLoopbackInterface);
  EXPECT_TRUE(address);
  if (address) {
    EXPECT_TRUE(address.value().IsLinkLocal());
    EXPECT_NE(0u, address.value().GetScopeId());
  }
}

TEST(IPAddressTest, V6ParseLinkLocalFailures) {
  // Scope ID on non-link-local address.
  EXPECT_FALSE(
      IPAddress::Parse(std::string("::1%") + kLoopbackInterface).is_value());
  // Invalid scope ID.
  EXPECT_FALSE(IPAddress::Parse("fe80::1%invalidscope"));
}

TEST(IPAddressTest, V6ComparisonWithScopeId) {
  const ErrorOr<IPAddress> address1_or_error = IPAddress::Parse("fe80::1");
  ASSERT_TRUE(address1_or_error);
  const IPAddress address1 = address1_or_error.value();

  const ErrorOr<IPAddress> address2_or_error = IPAddress::Parse("fe80::1");
  ASSERT_TRUE(address2_or_error);
  const IPAddress address2 = address2_or_error.value();

  const ErrorOr<IPAddress> address3_or_error = IPAddress::Parse("fe80::2");
  ASSERT_TRUE(address3_or_error);
  const IPAddress address3 = address3_or_error.value();

  EXPECT_EQ(address1, address2);
  EXPECT_NE(address1, address3);
  EXPECT_LT(address1, address3);

  // It is not possible to create an IPAddress with a scope ID in this test
  // without creating a network interface.
}

TEST(IPAddressTest, OstreamOperatorForIPv6LinkLocal) {
  std::ostringstream oss;
  const ErrorOr<IPAddress> address = IPAddress::Parse("fe80::1");
  ASSERT_TRUE(address);

  oss << address.value();
  EXPECT_EQ("fe80:0000:0000:0000:0000:0000:0000:0001", oss.str());
}

TEST(IPAddressTest, OstreamOperatorForIPv6LinkLocalWithScope) {
  std::ostringstream oss;
  ErrorOr<IPAddress> address =
      IPAddress::Parse(std::string("fe80::1%") + kLoopbackInterface);
  ASSERT_TRUE(address);

  oss << address.value();
  EXPECT_EQ(std::string("fe80:0000:0000:0000:0000:0000:0000:0001%") +
                kLoopbackInterface,
            oss.str());
}

TEST(IPAddressTest, IPEndpointParseWithScope) {
  // NOTE: This test is using the loopback interface kLoopbackInterface which
  // should exist on any Linux system.
  const ErrorOr<IPEndpoint> result =
      IPEndpoint::Parse(std::format("[fe80::1%{}]:8080", kLoopbackInterface));
  ASSERT_TRUE(result.is_value()) << result.error();
  EXPECT_TRUE(result.value().address.IsLinkLocal());
  EXPECT_NE(0u, result.value().address.GetScopeId());
  EXPECT_EQ(8080, result.value().port);

  // Numeric scope ID.
  const ErrorOr<IPEndpoint> result_scopeid =
      IPEndpoint::Parse("[fe80::1%1]:8080");
  ASSERT_TRUE(result_scopeid.is_value()) << result_scopeid.error();
  EXPECT_TRUE(result_scopeid.value().address.IsLinkLocal());
  EXPECT_EQ(1u, result_scopeid.value().address.GetScopeId());
  EXPECT_EQ(8080, result_scopeid.value().port);

  // Scope ID on non-link-local address should fail.
  EXPECT_FALSE(
      IPEndpoint::Parse(std::format("[::1%{}]:8080", kLoopbackInterface))
          .is_value());

  // Invalid scope ID should fail.
  EXPECT_FALSE(IPEndpoint::Parse("[fe80::1%nosuchinterface]:8080").is_value());
}

TEST(IPAddressTest, V4ConstructorFromSpan) {
  const uint8_t data[] = {192, 168, 0, 1};
  std::span<const uint8_t> span(data);
  const IPAddress address(IPAddress::Version::kV4, span);

  EXPECT_TRUE(address.IsV4());
  EXPECT_EQ(address, IPAddress(192, 168, 0, 1));
}

TEST(IPAddressTest, V6ConstructorFromSpan) {
  const uint8_t data[] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  std::span<const uint8_t> span(data);
  const IPAddress address(IPAddress::Version::kV6, span);

  EXPECT_TRUE(address.IsV6());
  EXPECT_THAT(address.bytes(), ElementsAreArray(data));
}

TEST(IPAddressTest, CopyToSpanV4) {
  const IPAddress address(192, 168, 1, 1);
  uint8_t buffer[4];
  std::span<uint8_t> span(buffer);

  address.CopyTo(span);

  const uint8_t expected[] = {192, 168, 1, 1};
  EXPECT_THAT(buffer, ElementsAreArray(expected));
}

TEST(IPAddressTest, CopyToSpanV6) {
  const uint8_t v6_bytes[] = {0, 1, 2,  3,  4,  5,  6,  7,
                              8, 9, 10, 11, 12, 13, 14, 15};
  const IPAddress address(IPAddress::Version::kV6, v6_bytes);

  uint8_t buffer[16];
  std::span<uint8_t> span(buffer);

  address.CopyTo(span);

  EXPECT_THAT(buffer, ElementsAreArray(v6_bytes));
}

TEST(IPAddressTest, BytesMethodReturnsSpan) {
  IPAddress v4_address(10, 0, 0, 1);
  auto v4_span = v4_address.bytes();
  EXPECT_EQ(v4_span.size(), 4u);
  const uint8_t expected_v4[] = {10, 0, 0, 1};
  EXPECT_THAT(v4_span, ElementsAreArray(expected_v4));

  const uint8_t v6_data[] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
                             0,    0,    0,    0,    0, 0, 0, 1};
  const IPAddress v6_address(IPAddress::Version::kV6, v6_data);
  auto v6_span = v6_address.bytes();
  EXPECT_EQ(v6_span.size(), 16u);
  EXPECT_THAT(v6_span, ElementsAreArray(v6_data));
}

}  // namespace openscreen
