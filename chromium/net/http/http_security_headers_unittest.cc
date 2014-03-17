// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/base64.h"
#include "base/sha1.h"
#include "base/strings/string_piece.h"
#include "crypto/sha2.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_security_headers.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

HashValue GetTestHashValue(uint8 label, HashValueTag tag) {
  HashValue hash_value(tag);
  memset(hash_value.data(), label, hash_value.size());
  return hash_value;
}

std::string GetTestPin(uint8 label, HashValueTag tag) {
  HashValue hash_value = GetTestHashValue(label, tag);
  std::string base64;
  base::Base64Encode(base::StringPiece(
      reinterpret_cast<char*>(hash_value.data()), hash_value.size()), &base64);

  switch (hash_value.tag) {
    case HASH_VALUE_SHA1:
      return std::string("pin-sha1=\"") + base64 + "\"";
    case HASH_VALUE_SHA256:
      return std::string("pin-sha256=\"") + base64 + "\"";
    default:
      NOTREACHED() << "Unknown HashValueTag " << hash_value.tag;
      return std::string("ERROR");
  }
}

};


class HttpSecurityHeadersTest : public testing::Test {
};


TEST_F(HttpSecurityHeadersTest, BogusHeaders) {
  base::TimeDelta max_age;
  bool include_subdomains = false;

  EXPECT_FALSE(
      ParseHSTSHeader(std::string(), &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("    ", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("abc", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  abc", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  abc   ", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  max-age", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  max-age  ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age=", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age=   ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =     ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =     xy", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =     3488a923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488a923  ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-ag=3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-aged=3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age==3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("amax-age=3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=-3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923;", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923     e", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923     includesubdomain",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923=includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomainx",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomain=",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomain=true",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomainsx",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomains x",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=34889.23 includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=34889 includesubdomains",
                               &max_age, &include_subdomains));

  // Check the out args were not updated by checking the default
  // values for its predictable fields.
  EXPECT_EQ(0, max_age.InSeconds());
  EXPECT_FALSE(include_subdomains);
}

static void TestBogusPinsHeaders(HashValueTag tag) {
  base::TimeDelta max_age;
  bool include_subdomains;
  HashValueVector hashes;
  HashValueVector chain_hashes;

  // Set some fake "chain" hashes
  chain_hashes.push_back(GetTestHashValue(1, tag));
  chain_hashes.push_back(GetTestHashValue(2, tag));
  chain_hashes.push_back(GetTestHashValue(3, tag));

  // The good pin must be in the chain, the backup pin must not be
  std::string good_pin = GetTestPin(2, tag);
  std::string backup_pin = GetTestPin(4, tag);

  EXPECT_FALSE(ParseHPKPHeader(std::string(), chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("    ", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("abc", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("  abc", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("  abc   ", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("  max-age", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("  max-age  ", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("   max-age=", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("   max-age  =", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("   max-age=   ", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("   max-age  =     ", chain_hashes,
                               &max_age, &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("   max-age  =     xy", chain_hashes,
                               &max_age, &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("   max-age  =     3488a923",
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=3488a923  ", chain_hashes,
                               &max_age, &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-ag=3488923pins=" + good_pin + "," +
                               backup_pin,
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-aged=3488923" + backup_pin,
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-aged=3488923; " + backup_pin,
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-aged=3488923; " + backup_pin + ";" +
                               backup_pin,
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-aged=3488923; " + good_pin + ";" +
                               good_pin,
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-aged=3488923; " + good_pin,
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age==3488923", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("amax-age=3488923", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=-3488923", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=3488923;", chain_hashes, &max_age,
                               &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=3488923     e", chain_hashes,
                               &max_age, &include_subdomains, &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=3488923     includesubdomain",
                               chain_hashes, &max_age, &include_subdomains,
                               &hashes));
  EXPECT_FALSE(ParseHPKPHeader("max-age=34889.23", chain_hashes, &max_age,
                               &include_subdomains, &hashes));

  // Check the out args were not updated by checking the default
  // values for its predictable fields.
  EXPECT_EQ(0, max_age.InSeconds());
  EXPECT_EQ(hashes.size(), (size_t)0);
}

TEST_F(HttpSecurityHeadersTest, ValidSTSHeaders) {
  base::TimeDelta max_age;
  base::TimeDelta expect_max_age;
  bool include_subdomains = false;

  EXPECT_TRUE(ParseHSTSHeader("max-age=243", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(243);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("  Max-agE    = 567", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(567);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("  mAx-aGe    = 890      ", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(890);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=123;incLudesUbdOmains", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("incLudesUbdOmains; max-age=123", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("   incLudesUbdOmains; max-age=123",
                              &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   incLudesUbdOmains; max-age=123; pumpkin=kitten", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin=894; incLudesUbdOmains; max-age=123  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin; incLudesUbdOmains; max-age=123  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin; incLudesUbdOmains; max-age=\"123\"  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "animal=\"squirrel; distinguished\"; incLudesUbdOmains; max-age=123",
                                   &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=394082;  incLudesUbdOmains",
                              &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(394082);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=39408299  ;incLudesUbdOmains", &max_age,
      &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(
      std::min(kMaxHSTSAgeSecs, static_cast<int64>(GG_INT64_C(39408299))));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=394082038  ; incLudesUbdOmains", &max_age,
      &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(
      std::min(kMaxHSTSAgeSecs, static_cast<int64>(GG_INT64_C(394082038))));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "  max-age=0  ;  incLudesUbdOmains   ", &max_age,
      &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "  max-age=999999999999999999999999999999999999999999999  ;"
      "  incLudesUbdOmains   ", &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(
      kMaxHSTSAgeSecs);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);
}

static void TestValidPKPHeaders(HashValueTag tag) {
  base::TimeDelta max_age;
  base::TimeDelta expect_max_age;
  bool include_subdomains;
  HashValueVector hashes;
  HashValueVector chain_hashes;

  // Set some fake "chain" hashes into chain_hashes
  chain_hashes.push_back(GetTestHashValue(1, tag));
  chain_hashes.push_back(GetTestHashValue(2, tag));
  chain_hashes.push_back(GetTestHashValue(3, tag));

  // The good pin must be in the chain, the backup pin must not be
  std::string good_pin = GetTestPin(2, tag);
  std::string backup_pin = GetTestPin(4, tag);

  EXPECT_TRUE(ParseHPKPHeader(
      "max-age=243; " + good_pin + ";" + backup_pin,
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(243);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "   " + good_pin + "; " + backup_pin + "  ; Max-agE    = 567",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(567);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "includeSubDOMAINS;" + good_pin + ";" + backup_pin +
      "  ; mAx-aGe    = 890      ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(890);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      good_pin + ";" + backup_pin + "; max-age=123;IGNORED;",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "max-age=394082;" + backup_pin + ";" + good_pin + ";  ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(394082);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "max-age=39408299  ;" + backup_pin + ";" + good_pin + ";  ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(
      std::min(kMaxHSTSAgeSecs, static_cast<int64>(GG_INT64_C(39408299))));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "max-age=39408038  ;    cybers=39408038  ;  includeSubdomains; " +
          good_pin + ";" + backup_pin + ";   ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(
      std::min(kMaxHSTSAgeSecs, static_cast<int64>(GG_INT64_C(394082038))));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "  max-age=0  ;  " + good_pin + ";" + backup_pin,
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "  max-age=0 ; includeSubdomains;  " + good_pin + ";" + backup_pin,
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHPKPHeader(
      "  max-age=999999999999999999999999999999999999999999999  ;  " +
          backup_pin + ";" + good_pin + ";   ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  expect_max_age = base::TimeDelta::FromSeconds(kMaxHSTSAgeSecs);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  // Test that parsing the same header twice doesn't duplicate the recorded
  // hashes.
  hashes.clear();
  EXPECT_TRUE(ParseHPKPHeader(
      "  max-age=999;  " +
          backup_pin + ";" + good_pin + ";   ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  EXPECT_EQ(2u, hashes.size());
  EXPECT_TRUE(ParseHPKPHeader(
      "  max-age=999;  " +
          backup_pin + ";" + good_pin + ";   ",
      chain_hashes, &max_age, &include_subdomains, &hashes));
  EXPECT_EQ(2u, hashes.size());
}

TEST_F(HttpSecurityHeadersTest, BogusPinsHeadersSHA1) {
  TestBogusPinsHeaders(HASH_VALUE_SHA1);
}

TEST_F(HttpSecurityHeadersTest, BogusPinsHeadersSHA256) {
  TestBogusPinsHeaders(HASH_VALUE_SHA256);
}

TEST_F(HttpSecurityHeadersTest, ValidPKPHeadersSHA1) {
  TestValidPKPHeaders(HASH_VALUE_SHA1);
}

TEST_F(HttpSecurityHeadersTest, ValidPKPHeadersSHA256) {
  TestValidPKPHeaders(HASH_VALUE_SHA256);
}

TEST_F(HttpSecurityHeadersTest, UpdateDynamicPKPOnly) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  // docs.google.com has preloaded pins.
  std::string domain = "docs.google.com";
  EXPECT_TRUE(state.GetDomainState(domain, true, &domain_state));
  EXPECT_GT(domain_state.static_spki_hashes.size(), 1UL);
  HashValueVector saved_hashes = domain_state.static_spki_hashes;

  // Add a header, which should only update the dynamic state.
  HashValue good_hash = GetTestHashValue(1, HASH_VALUE_SHA1);
  HashValue backup_hash = GetTestHashValue(2, HASH_VALUE_SHA1);
  std::string good_pin = GetTestPin(1, HASH_VALUE_SHA1);
  std::string backup_pin = GetTestPin(2, HASH_VALUE_SHA1);
  std::string header = "max-age = 10000; " + good_pin + "; " + backup_pin;

  // Construct a fake SSLInfo that will pass AddHPKPHeader's checks.
  SSLInfo ssl_info;
  ssl_info.public_key_hashes.push_back(good_hash);
  ssl_info.public_key_hashes.push_back(saved_hashes[0]);
  EXPECT_TRUE(state.AddHPKPHeader(domain, header, ssl_info));

  // Expect the preloaded state to remain unchanged.
  std::string canonicalized_host = TransportSecurityState::CanonicalizeHost(
      domain);
  TransportSecurityState::DomainState static_domain_state;
  EXPECT_TRUE(state.GetStaticDomainState(canonicalized_host,
                                         true,
                                         &static_domain_state));
  for (size_t i = 0; i < saved_hashes.size(); ++i) {
    EXPECT_TRUE(HashValuesEqual(
        saved_hashes[i])(static_domain_state.static_spki_hashes[i]));
  }

  // Expect the dynamic state to reflect the header.
  TransportSecurityState::DomainState dynamic_domain_state;
  EXPECT_TRUE(state.GetDynamicDomainState(domain, &dynamic_domain_state));
  EXPECT_EQ(2UL, dynamic_domain_state.dynamic_spki_hashes.size());

  HashValueVector::const_iterator hash = std::find_if(
      dynamic_domain_state.dynamic_spki_hashes.begin(),
      dynamic_domain_state.dynamic_spki_hashes.end(),
      HashValuesEqual(good_hash));
  EXPECT_NE(dynamic_domain_state.dynamic_spki_hashes.end(), hash);

  hash = std::find_if(
      dynamic_domain_state.dynamic_spki_hashes.begin(),
      dynamic_domain_state.dynamic_spki_hashes.end(),
      HashValuesEqual(backup_hash));
  EXPECT_NE(dynamic_domain_state.dynamic_spki_hashes.end(), hash);

  // Expect the overall state to reflect the header, too.
  EXPECT_TRUE(state.GetDomainState(domain, true, &domain_state));
  EXPECT_EQ(2UL, domain_state.dynamic_spki_hashes.size());

  hash = std::find_if(domain_state.dynamic_spki_hashes.begin(),
                      domain_state.dynamic_spki_hashes.end(),
                      HashValuesEqual(good_hash));
  EXPECT_NE(domain_state.dynamic_spki_hashes.end(), hash);

  hash = std::find_if(
      domain_state.dynamic_spki_hashes.begin(),
      domain_state.dynamic_spki_hashes.end(),
      HashValuesEqual(backup_hash));
  EXPECT_NE(domain_state.dynamic_spki_hashes.end(), hash);
}

};    // namespace net
