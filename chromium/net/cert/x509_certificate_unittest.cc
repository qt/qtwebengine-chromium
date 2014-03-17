// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/asn1_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS)
#include <cert.h>
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using base::HexEncode;
using base::Time;

namespace net {

// Certificates for test data. They're obtained with:
//
// $ openssl s_client -connect [host]:443 -showcerts > /tmp/host.pem < /dev/null
// $ openssl x509 -inform PEM -outform DER < /tmp/host.pem > /tmp/host.der
//
// For fingerprint
// $ openssl x509 -inform DER -fingerprint -noout < /tmp/host.der

// For valid_start, valid_expiry
// $ openssl x509 -inform DER -text -noout < /tmp/host.der |
//    grep -A 2 Validity
// $ date +%s -d '<date str>'

// Google's cert.
uint8 google_fingerprint[] = {
  0xab, 0xbe, 0x5e, 0xb4, 0x93, 0x88, 0x4e, 0xe4, 0x60, 0xc6, 0xef, 0xf8,
  0xea, 0xd4, 0xb1, 0x55, 0x4b, 0xc9, 0x59, 0x3c
};

// webkit.org's cert.
uint8 webkit_fingerprint[] = {
  0xa1, 0x4a, 0x94, 0x46, 0x22, 0x8e, 0x70, 0x66, 0x2b, 0x94, 0xf9, 0xf8,
  0x57, 0x83, 0x2d, 0xa2, 0xff, 0xbc, 0x84, 0xc2
};

// thawte.com's cert (it's EV-licious!).
uint8 thawte_fingerprint[] = {
  0x85, 0x04, 0x2d, 0xfd, 0x2b, 0x0e, 0xc6, 0xc8, 0xaf, 0x2d, 0x77, 0xd6,
  0xa1, 0x3a, 0x64, 0x04, 0x27, 0x90, 0x97, 0x37
};

// A certificate for https://www.unosoft.hu/, whose AIA extension contains
// an LDAP URL without a host name.
uint8 unosoft_hu_fingerprint[] = {
  0x32, 0xff, 0xe3, 0xbe, 0x2c, 0x3b, 0xc7, 0xca, 0xbf, 0x2d, 0x64, 0xbd,
  0x25, 0x66, 0xf2, 0xec, 0x8b, 0x0f, 0xbf, 0xd8
};

// The fingerprint of the Google certificate used in the parsing tests,
// which is newer than the one included in the x509_certificate_data.h
uint8 google_parse_fingerprint[] = {
  0x40, 0x50, 0x62, 0xe5, 0xbe, 0xfd, 0xe4, 0xaf, 0x97, 0xe9, 0x38, 0x2a,
  0xf1, 0x6c, 0xc8, 0x7c, 0x8f, 0xb7, 0xc4, 0xe2
};

// The fingerprint for the Thawte SGC certificate
uint8 thawte_parse_fingerprint[] = {
  0xec, 0x07, 0x10, 0x03, 0xd8, 0xf5, 0xa3, 0x7f, 0x42, 0xc4, 0x55, 0x7f,
  0x65, 0x6a, 0xae, 0x86, 0x65, 0xfa, 0x4b, 0x02
};

// Dec 18 00:00:00 2009 GMT
const double kGoogleParseValidFrom = 1261094400;
// Dec 18 23:59:59 2011 GMT
const double kGoogleParseValidTo = 1324252799;

void CheckGoogleCert(const scoped_refptr<X509Certificate>& google_cert,
                     uint8* expected_fingerprint,
                     double valid_from, double valid_to) {
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  const CertPrincipal& subject = google_cert->subject();
  EXPECT_EQ("www.google.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Google Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = google_cert->issuer();
  EXPECT_EQ("Thawte SGC CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("ZA", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("Thawte Consulting (Pty) Ltd.", issuer.organization_names[0]);
  EXPECT_EQ(0U, issuer.organization_unit_names.size());
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = google_cert->valid_start();
  EXPECT_EQ(valid_from, valid_start.ToDoubleT());

  const Time& valid_expiry = google_cert->valid_expiry();
  EXPECT_EQ(valid_to, valid_expiry.ToDoubleT());

  const SHA1HashValue& fingerprint = google_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(expected_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  google_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.google.com", dns_names[0]);
}

TEST(X509CertificateTest, GoogleCertParsing) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  CheckGoogleCert(google_cert, google_fingerprint,
                  1238192407,   // Mar 27 22:20:07 2009 GMT
                  1269728407);  // Mar 27 22:20:07 2010 GMT
}

TEST(X509CertificateTest, WebkitCertParsing) {
  scoped_refptr<X509Certificate> webkit_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), webkit_cert);

  const CertPrincipal& subject = webkit_cert->subject();
  EXPECT_EQ("Cupertino", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Apple Inc.", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Mac OS Forge", subject.organization_unit_names[0]);
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = webkit_cert->issuer();
  EXPECT_EQ("Go Daddy Secure Certification Authority", issuer.common_name);
  EXPECT_EQ("Scottsdale", issuer.locality_name);
  EXPECT_EQ("Arizona", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("GoDaddy.com, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("http://certificates.godaddy.com/repository",
      issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = webkit_cert->valid_start();
  EXPECT_EQ(1205883319, valid_start.ToDoubleT());  // Mar 18 23:35:19 2008 GMT

  const Time& valid_expiry = webkit_cert->valid_expiry();
  EXPECT_EQ(1300491319, valid_expiry.ToDoubleT());  // Mar 18 23:35:19 2011 GMT

  const SHA1HashValue& fingerprint = webkit_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(webkit_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  webkit_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(2U, dns_names.size());
  EXPECT_EQ("*.webkit.org", dns_names[0]);
  EXPECT_EQ("webkit.org", dns_names[1]);

  // Test that the wildcard cert matches properly.
  bool unused = false;
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("www.webkit.org", &unused));
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("foo.webkit.org", &unused));
  EXPECT_TRUE(webkit_cert->VerifyNameMatch("webkit.org", &unused));
  EXPECT_FALSE(webkit_cert->VerifyNameMatch("www.webkit.com", &unused));
  EXPECT_FALSE(webkit_cert->VerifyNameMatch("www.foo.webkit.com", &unused));
}

TEST(X509CertificateTest, ThawteCertParsing) {
  scoped_refptr<X509Certificate> thawte_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), thawte_cert);

  const CertPrincipal& subject = thawte_cert->subject();
  EXPECT_EQ("www.thawte.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Thawte Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = thawte_cert->issuer();
  EXPECT_EQ("thawte Extended Validation SSL CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("thawte, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("Terms of use at https://www.thawte.com/cps (c)06",
            issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = thawte_cert->valid_start();
  EXPECT_EQ(1227052800, valid_start.ToDoubleT());  // Nov 19 00:00:00 2008 GMT

  const Time& valid_expiry = thawte_cert->valid_expiry();
  EXPECT_EQ(1263772799, valid_expiry.ToDoubleT());  // Jan 17 23:59:59 2010 GMT

  const SHA1HashValue& fingerprint = thawte_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(thawte_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  thawte_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.thawte.com", dns_names[0]);
}

// Test that all desired AttributeAndValue pairs can be extracted when only
// a single RelativeDistinguishedName is present. "Normally" there is only
// one AVA per RDN, but some CAs place all AVAs within a single RDN.
// This is a regression test for http://crbug.com/101009
TEST(X509CertificateTest, MultivalueRDN) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> multivalue_rdn_cert =
      ImportCertFromFile(certs_dir, "multivalue_rdn.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), multivalue_rdn_cert);

  const CertPrincipal& subject = multivalue_rdn_cert->subject();
  EXPECT_EQ("Multivalue RDN Test", subject.common_name);
  EXPECT_EQ("", subject.locality_name);
  EXPECT_EQ("", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Chromium", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Chromium net_unittests", subject.organization_unit_names[0]);
  ASSERT_EQ(1U, subject.domain_components.size());
  EXPECT_EQ("Chromium", subject.domain_components[0]);
}

// Test that characters which would normally be escaped in the string form,
// such as '=' or '"', are not escaped when parsed as individual components.
// This is a regression test for http://crbug.com/102839
TEST(X509CertificateTest, UnescapedSpecialCharacters) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> unescaped_cert =
      ImportCertFromFile(certs_dir, "unescaped.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), unescaped_cert);

  const CertPrincipal& subject = unescaped_cert->subject();
  EXPECT_EQ("127.0.0.1", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  ASSERT_EQ(1U, subject.street_addresses.size());
  EXPECT_EQ("1600 Amphitheatre Parkway", subject.street_addresses[0]);
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Chromium = \"net_unittests\"", subject.organization_names[0]);
  ASSERT_EQ(2U, subject.organization_unit_names.size());
  EXPECT_EQ("net_unittests", subject.organization_unit_names[0]);
  EXPECT_EQ("Chromium", subject.organization_unit_names[1]);
  EXPECT_EQ(0U, subject.domain_components.size());
}

TEST(X509CertificateTest, SerialNumbers) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  static const uint8 google_serial[16] = {
    0x01,0x2a,0x39,0x76,0x0d,0x3f,0x4f,0xc9,
    0x0b,0xe7,0xbd,0x2b,0xcf,0x95,0x2e,0x7a,
  };

  ASSERT_EQ(sizeof(google_serial), google_cert->serial_number().size());
  EXPECT_TRUE(memcmp(google_cert->serial_number().data(), google_serial,
                     sizeof(google_serial)) == 0);

  // We also want to check a serial number where the first byte is >= 0x80 in
  // case the underlying library tries to pad it.
  scoped_refptr<X509Certificate> paypal_null_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der)));

  static const uint8 paypal_null_serial[3] = {0x00, 0xf0, 0x9b};
  ASSERT_EQ(sizeof(paypal_null_serial),
            paypal_null_cert->serial_number().size());
  EXPECT_TRUE(memcmp(paypal_null_cert->serial_number().data(),
                     paypal_null_serial, sizeof(paypal_null_serial)) == 0);
}

TEST(X509CertificateTest, CAFingerprints) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "salesforce_com_test.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert1 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2011.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert1);

  scoped_refptr<X509Certificate> intermediate_cert2 =
      ImportCertFromFile(certs_dir, "verisign_intermediate_ca_2016.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert2);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert1->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain1 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  intermediates.clear();
  intermediates.push_back(intermediate_cert2->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain2 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  // No intermediate CA certicates.
  intermediates.clear();
  scoped_refptr<X509Certificate> cert_chain3 =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  static const uint8 cert_chain1_ca_fingerprint[20] = {
    0xc2, 0xf0, 0x08, 0x7d, 0x01, 0xe6, 0x86, 0x05, 0x3a, 0x4d,
    0x63, 0x3e, 0x7e, 0x70, 0xd4, 0xef, 0x65, 0xc2, 0xcc, 0x4f
  };
  static const uint8 cert_chain2_ca_fingerprint[20] = {
    0xd5, 0x59, 0xa5, 0x86, 0x66, 0x9b, 0x08, 0xf4, 0x6a, 0x30,
    0xa1, 0x33, 0xf8, 0xa9, 0xed, 0x3d, 0x03, 0x8e, 0x2e, 0xa8
  };
  // The SHA-1 hash of nothing.
  static const uint8 cert_chain3_ca_fingerprint[20] = {
    0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
    0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09
  };
  EXPECT_TRUE(memcmp(cert_chain1->ca_fingerprint().data,
                     cert_chain1_ca_fingerprint, 20) == 0);
  EXPECT_TRUE(memcmp(cert_chain2->ca_fingerprint().data,
                     cert_chain2_ca_fingerprint, 20) == 0);
  EXPECT_TRUE(memcmp(cert_chain3->ca_fingerprint().data,
                     cert_chain3_ca_fingerprint, 20) == 0);
}

TEST(X509CertificateTest, ParseSubjectAltNames) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> san_cert =
      ImportCertFromFile(certs_dir, "subjectAltName_sanity_check.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), san_cert);

  std::vector<std::string> dns_names;
  std::vector<std::string> ip_addresses;
  san_cert->GetSubjectAltName(&dns_names, &ip_addresses);

  // Ensure that DNS names are correctly parsed.
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("test.example", dns_names[0]);

  // Ensure that both IPv4 and IPv6 addresses are correctly parsed.
  ASSERT_EQ(2U, ip_addresses.size());

  static const uint8 kIPv4Address[] = {
      0x7F, 0x00, 0x00, 0x02
  };
  ASSERT_EQ(arraysize(kIPv4Address), ip_addresses[0].size());
  EXPECT_EQ(0, memcmp(ip_addresses[0].data(), kIPv4Address,
                      arraysize(kIPv4Address)));

  static const uint8 kIPv6Address[] = {
      0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  ASSERT_EQ(arraysize(kIPv6Address), ip_addresses[1].size());
  EXPECT_EQ(0, memcmp(ip_addresses[1].data(), kIPv6Address,
                      arraysize(kIPv6Address)));

  // Ensure the subjectAltName dirName has not influenced the handling of
  // the subject commonName.
  EXPECT_EQ("127.0.0.1", san_cert->subject().common_name);
}

TEST(X509CertificateTest, ExtractSPKIFromDERCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "nist.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert);

  std::string derBytes;
  EXPECT_TRUE(X509Certificate::GetDEREncoded(cert->os_cert_handle(),
                                             &derBytes));

  base::StringPiece spkiBytes;
  EXPECT_TRUE(asn1::ExtractSPKIFromDERCert(derBytes, &spkiBytes));

  uint8 hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<const uint8*>(spkiBytes.data()),
                      spkiBytes.size(), hash);

  EXPECT_EQ(0, memcmp(hash, kNistSPKIHash, sizeof(hash)));
}

TEST(X509CertificateTest, ExtractCRLURLsFromDERCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, "nist.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert);

  std::string derBytes;
  EXPECT_TRUE(X509Certificate::GetDEREncoded(cert->os_cert_handle(),
                                             &derBytes));

  std::vector<base::StringPiece> crl_urls;
  EXPECT_TRUE(asn1::ExtractCRLURLsFromDERCert(derBytes, &crl_urls));

  EXPECT_EQ(1u, crl_urls.size());
  if (crl_urls.size() > 0) {
    EXPECT_EQ("http://SVRSecure-G3-crl.verisign.com/SVRSecureG3.crl",
              crl_urls[0].as_string());
  }
}

// Tests X509CertificateCache via X509Certificate::CreateFromHandle.  We
// call X509Certificate::CreateFromHandle several times and observe whether
// it returns a cached or new OSCertHandle.
TEST(X509CertificateTest, Cache) {
  X509Certificate::OSCertHandle google_cert_handle;
  X509Certificate::OSCertHandle thawte_cert_handle;

  // Add a single certificate to the certificate cache.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert1(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  // Add the same certificate, but as a new handle.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert2(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  // A new X509Certificate should be returned.
  EXPECT_NE(cert1.get(), cert2.get());
  // But both instances should share the underlying OS certificate handle.
  EXPECT_EQ(cert1->os_cert_handle(), cert2->os_cert_handle());
  EXPECT_EQ(0u, cert1->GetIntermediateCertificates().size());
  EXPECT_EQ(0u, cert2->GetIntermediateCertificates().size());

  // Add the same certificate, but this time with an intermediate. This
  // should result in the intermediate being cached. Note that this is not
  // a legitimate chain, but is suitable for testing.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  thawte_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(thawte_cert_handle);
  scoped_refptr<X509Certificate> cert3(X509Certificate::CreateFromHandle(
      google_cert_handle, intermediates));
  X509Certificate::FreeOSCertHandle(google_cert_handle);
  X509Certificate::FreeOSCertHandle(thawte_cert_handle);

  // Test that the new certificate, even with intermediates, results in the
  // same underlying handle being used.
  EXPECT_EQ(cert1->os_cert_handle(), cert3->os_cert_handle());
  // Though they use the same OS handle, the intermediates should be different.
  EXPECT_NE(cert1->GetIntermediateCertificates().size(),
      cert3->GetIntermediateCertificates().size());
}

TEST(X509CertificateTest, Pickle) {
  X509Certificate::OSCertHandle google_cert_handle =
      X509Certificate::CreateOSCertHandleFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der));
  X509Certificate::OSCertHandle thawte_cert_handle =
      X509Certificate::CreateOSCertHandleFromBytes(
          reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(thawte_cert_handle);
  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromHandle(
      google_cert_handle, intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert.get());

  X509Certificate::FreeOSCertHandle(google_cert_handle);
  X509Certificate::FreeOSCertHandle(thawte_cert_handle);

  Pickle pickle;
  cert->Persist(&pickle);

  PickleIterator iter(pickle);
  scoped_refptr<X509Certificate> cert_from_pickle =
      X509Certificate::CreateFromPickle(
          pickle, &iter, X509Certificate::PICKLETYPE_CERTIFICATE_CHAIN_V3);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), cert_from_pickle);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      cert->os_cert_handle(), cert_from_pickle->os_cert_handle()));
  const X509Certificate::OSCertHandles& cert_intermediates =
      cert->GetIntermediateCertificates();
  const X509Certificate::OSCertHandles& pickle_intermediates =
      cert_from_pickle->GetIntermediateCertificates();
  ASSERT_EQ(cert_intermediates.size(), pickle_intermediates.size());
  for (size_t i = 0; i < cert_intermediates.size(); ++i) {
    EXPECT_TRUE(X509Certificate::IsSameOSCert(cert_intermediates[i],
                                              pickle_intermediates[i]));
  }
}

TEST(X509CertificateTest, Policy) {
  scoped_refptr<X509Certificate> google_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  scoped_refptr<X509Certificate> webkit_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  CertPolicy policy;

  // To begin with, everything should be unknown.
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  // Test adding one certificate with one error.
  policy.Allow(google_cert.get(), CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(),
                CERT_STATUS_DATE_INVALID | CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  // Test saving the same certificate with a new error.
  policy.Allow(google_cert.get(), CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(google_cert.get(), CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  // Test adding one certificate with two errors.
  policy.Allow(google_cert.get(),
               CERT_STATUS_DATE_INVALID | CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(google_cert.get(), CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  // Test removing a certificate that was previously allowed.
  policy.Deny(google_cert.get(), CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());

  // Test removing a certificate that was previously unknown.
  policy.Deny(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID);
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());

  // Test saving a certificate that was previously denied.
  policy.Allow(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID);
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::ALLOWED,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());

  // Test denying an overlapping certificate.
  policy.Allow(google_cert.get(),
               CERT_STATUS_COMMON_NAME_INVALID | CERT_STATUS_DATE_INVALID);
  policy.Deny(google_cert.get(), CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(google_cert.get(), CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertPolicy::UNKNOWN,
            policy.Check(google_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(google_cert.get(),
                CERT_STATUS_COMMON_NAME_INVALID | CERT_STATUS_DATE_INVALID));

  // Test denying an overlapping certificate (other direction).
  policy.Allow(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID);
  policy.Deny(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID);
  policy.Deny(webkit_cert.get(), CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(webkit_cert.get(), CERT_STATUS_COMMON_NAME_INVALID));
  EXPECT_EQ(CertPolicy::DENIED,
            policy.Check(webkit_cert.get(), CERT_STATUS_DATE_INVALID));
}

TEST(X509CertificateTest, IntermediateCertificates) {
  scoped_refptr<X509Certificate> webkit_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  scoped_refptr<X509Certificate> thawte_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der)));

  X509Certificate::OSCertHandle google_handle;
  // Create object with no intermediates:
  google_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  X509Certificate::OSCertHandles intermediates1;
  scoped_refptr<X509Certificate> cert1;
  cert1 = X509Certificate::CreateFromHandle(google_handle, intermediates1);
  EXPECT_EQ(0u, cert1->GetIntermediateCertificates().size());

  // Create object with 2 intermediates:
  X509Certificate::OSCertHandles intermediates2;
  intermediates2.push_back(webkit_cert->os_cert_handle());
  intermediates2.push_back(thawte_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert2;
  cert2 = X509Certificate::CreateFromHandle(google_handle, intermediates2);

  // Verify it has all the intermediates:
  const X509Certificate::OSCertHandles& cert2_intermediates =
      cert2->GetIntermediateCertificates();
  ASSERT_EQ(2u, cert2_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert2_intermediates[0],
                                            webkit_cert->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(cert2_intermediates[1],
                                            thawte_cert->os_cert_handle()));

  // Cleanup
  X509Certificate::FreeOSCertHandle(google_handle);
}

TEST(X509CertificateTest, IsIssuedByEncoded) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Test a client certificate from MIT.
  scoped_refptr<X509Certificate> mit_davidben_cert(
      ImportCertFromFile(certs_dir, "mit.davidben.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), mit_davidben_cert);

  std::string mit_issuer(reinterpret_cast<const char*>(MITDN),
                         sizeof(MITDN));

  // Test a certificate from Google, issued by Thawte
  scoped_refptr<X509Certificate> google_cert(
      ImportCertFromFile(certs_dir, "google.single.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  std::string thawte_issuer(reinterpret_cast<const char*>(ThawteDN),
                            sizeof(ThawteDN));

  // Check that the David Ben certificate is issued by MIT, but not
  // by Thawte.
  std::vector<std::string> issuers;
  issuers.clear();
  issuers.push_back(mit_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_FALSE(google_cert->IsIssuedByEncoded(issuers));

  // Check that the Google certificate is issued by Thawte and not
  // by MIT.
  issuers.clear();
  issuers.push_back(thawte_issuer);
  EXPECT_FALSE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_TRUE(google_cert->IsIssuedByEncoded(issuers));

  // Check that they both pass when given a list of the two issuers.
  issuers.clear();
  issuers.push_back(mit_issuer);
  issuers.push_back(thawte_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedByEncoded(issuers));
  EXPECT_TRUE(google_cert->IsIssuedByEncoded(issuers));
}

TEST(X509CertificateTest, IsIssuedByEncodedWithIntermediates) {
  static const unsigned char kPolicyRootDN[] = {
    0x30, 0x1e, 0x31, 0x1c, 0x30, 0x1a, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x13, 0x50, 0x6f, 0x6c, 0x69, 0x63, 0x79, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41
  };
  static const unsigned char kPolicyIntermediateDN[] = {
    0x30, 0x26, 0x31, 0x24, 0x30, 0x22, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x1b, 0x50, 0x6f, 0x6c, 0x69, 0x63, 0x79, 0x20, 0x54, 0x65, 0x73, 0x74,
    0x20, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x6d, 0x65, 0x64, 0x69, 0x61, 0x74,
    0x65, 0x20, 0x43, 0x41
  };

  base::FilePath certs_dir = GetTestCertsDirectory();

  CertificateList policy_chain = CreateCertificateListFromFile(
      certs_dir, "explicit-policy-chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3u, policy_chain.size());

  // The intermediate CA certificate's policyConstraints extension has a
  // requireExplicitPolicy field with SkipCerts=0.
  std::string policy_intermediate_dn(
      reinterpret_cast<const char*>(kPolicyIntermediateDN),
      sizeof(kPolicyIntermediateDN));
  std::string policy_root_dn(reinterpret_cast<const char*>(kPolicyRootDN),
                             sizeof(kPolicyRootDN));

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(policy_chain[1]->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(policy_chain[0]->os_cert_handle(),
                                        intermediates);

  std::vector<std::string> issuers;

  // Check that the chain is issued by the intermediate.
  issuers.clear();
  issuers.push_back(policy_intermediate_dn);
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that the chain is also issued by the root.
  issuers.clear();
  issuers.push_back(policy_root_dn);
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that the chain is issued by either the intermediate or the root.
  issuers.clear();
  issuers.push_back(policy_intermediate_dn);
  issuers.push_back(policy_root_dn);
  EXPECT_TRUE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that an empty issuers list returns false.
  issuers.clear();
  EXPECT_FALSE(cert_chain->IsIssuedByEncoded(issuers));

  // Check that the chain is not issued by Verisign
  std::string mit_issuer(reinterpret_cast<const char*>(VerisignDN),
                         sizeof(VerisignDN));
  issuers.clear();
  issuers.push_back(mit_issuer);
  EXPECT_FALSE(cert_chain->IsIssuedByEncoded(issuers));
}

#if defined(USE_NSS)
TEST(X509CertificateTest, GetDefaultNickname) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "no_subject_common_name_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  std::string nickname = test_cert->GetDefaultNickname(USER_CERT);
  EXPECT_EQ("wtc@google.com's COMODO Client Authentication and "
            "Secure Email CA ID", nickname);
}
#endif

const struct CertificateFormatTestData {
  const char* file_name;
  X509Certificate::Format format;
  uint8* chain_fingerprints[3];
} kFormatTestData[] = {
  // DER Parsing - single certificate, DER encoded
  { "google.single.der", X509Certificate::FORMAT_SINGLE_CERTIFICATE,
    { google_parse_fingerprint,
      NULL, } },
  // DER parsing - single certificate, PEM encoded
  { "google.single.pem", X509Certificate::FORMAT_SINGLE_CERTIFICATE,
    { google_parse_fingerprint,
      NULL, } },
  // PEM parsing - single certificate, PEM encoded with a PEB of
  // "CERTIFICATE"
  { "google.single.pem", X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
    { google_parse_fingerprint,
      NULL, } },
  // PEM parsing - sequence of certificates, PEM encoded with a PEB of
  // "CERTIFICATE"
  { "google.chain.pem", X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // PKCS#7 parsing - "degenerate" SignedData collection of certificates, DER
  // encoding
  { "google.binary.p7b", X509Certificate::FORMAT_PKCS7,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
  // encoded with a PEM PEB of "CERTIFICATE"
  { "google.pem_cert.p7b", X509Certificate::FORMAT_PKCS7,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
  // encoded with a PEM PEB of "PKCS7"
  { "google.pem_pkcs7.p7b", X509Certificate::FORMAT_PKCS7,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // All of the above, this time using auto-detection
  { "google.single.der", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      NULL, } },
  { "google.single.pem", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      NULL, } },
  { "google.chain.pem", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  { "google.binary.p7b", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  { "google.pem_cert.p7b", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  { "google.pem_pkcs7.p7b", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
};

class X509CertificateParseTest
    : public testing::TestWithParam<CertificateFormatTestData> {
 public:
  virtual ~X509CertificateParseTest() {}
  virtual void SetUp() {
    test_data_ = GetParam();
  }
  virtual void TearDown() {}

 protected:
  CertificateFormatTestData test_data_;
};

TEST_P(X509CertificateParseTest, CanParseFormat) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, test_data_.file_name, test_data_.format);
  ASSERT_FALSE(certs.empty());
  ASSERT_LE(certs.size(), arraysize(test_data_.chain_fingerprints));
  CheckGoogleCert(certs.front(), google_parse_fingerprint,
                  kGoogleParseValidFrom, kGoogleParseValidTo);

  size_t i;
  for (i = 0; i < arraysize(test_data_.chain_fingerprints); ++i) {
    if (test_data_.chain_fingerprints[i] == NULL) {
      // No more test certificates expected - make sure no more were
      // returned before marking this test a success.
      EXPECT_EQ(i, certs.size());
      break;
    }

    // A cert is expected - make sure that one was parsed.
    ASSERT_LT(i, certs.size());

    // Compare the parsed certificate with the expected certificate, by
    // comparing fingerprints.
    const X509Certificate* cert = certs[i].get();
    const SHA1HashValue& actual_fingerprint = cert->fingerprint();
    uint8* expected_fingerprint = test_data_.chain_fingerprints[i];

    for (size_t j = 0; j < 20; ++j)
      EXPECT_EQ(expected_fingerprint[j], actual_fingerprint.data[j]);
  }
}

INSTANTIATE_TEST_CASE_P(, X509CertificateParseTest,
                        testing::ValuesIn(kFormatTestData));

struct CertificateNameVerifyTestData {
  // true iff we expect hostname to match an entry in cert_names.
  bool expected;
  // The hostname to match.
  const char* hostname;
  // Common name, may be used if |dns_names| or |ip_addrs| are empty.
  const char* common_name;
  // Comma separated list of certificate names to match against. Any occurrence
  // of '#' will be replaced with a null character before processing.
  const char* dns_names;
  // Comma separated list of certificate IP Addresses to match against. Each
  // address is x prefixed 16 byte hex code for v6 or dotted-decimals for v4.
  const char* ip_addrs;
};

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const CertificateNameVerifyTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname && data.common_name);
  // Using StringPiece to allow for optional fields being NULL.
  *os << " expected: " << data.expected
      << "; hostname: " << data.hostname
      << "; common_name: " << data.common_name
      << "; dns_names: " << base::StringPiece(data.dns_names)
      << "; ip_addrs: " << base::StringPiece(data.ip_addrs);
}

const CertificateNameVerifyTestData kNameVerifyTestData[] = {
    { true, "foo.com", "foo.com" },
    { true, "f", "f" },
    { false, "h", "i" },
    { true, "bar.foo.com", "*.foo.com" },
    { true, "www.test.fr", "common.name",
        "*.test.com,*.test.co.uk,*.test.de,*.test.fr" },
    { true, "wwW.tESt.fr",  "common.name",
        ",*.*,*.test.de,*.test.FR,www" },
    { false, "f.uk", ".uk" },
    { false, "w.bar.foo.com", "?.bar.foo.com" },
    { false, "www.foo.com", "(www|ftp).foo.com" },
    { false, "www.foo.com", "www.foo.com#" },  // # = null char.
    { false, "www.foo.com", "", "www.foo.com#*.foo.com,#,#" },
    { false, "www.house.example", "ww.house.example" },
    { false, "test.org", "", "www.test.org,*.test.org,*.org" },
    { false, "w.bar.foo.com", "w*.bar.foo.com" },
    { false, "www.bar.foo.com", "ww*ww.bar.foo.com" },
    { false, "wwww.bar.foo.com", "ww*ww.bar.foo.com" },
    { true, "wwww.bar.foo.com", "w*w.bar.foo.com" },
    { false, "wwww.bar.foo.com", "w*w.bar.foo.c0m" },
    { true, "WALLY.bar.foo.com", "wa*.bar.foo.com" },
    { true, "wally.bar.foo.com", "*Ly.bar.foo.com" },
    { true, "ww%57.foo.com", "", "www.foo.com" },
    { true, "www&.foo.com", "www%26.foo.com" },
    // Common name must not be used if subject alternative name was provided.
    { false, "www.test.co.jp",  "www.test.co.jp",
        "*.test.de,*.jp,www.test.co.uk,www.*.co.jp" },
    { false, "www.bar.foo.com", "www.bar.foo.com",
      "*.foo.com,*.*.foo.com,*.*.bar.foo.com,*..bar.foo.com," },
    { false, "www.bath.org", "www.bath.org", "", "20.30.40.50" },
    { false, "66.77.88.99", "www.bath.org", "www.bath.org" },
    // IDN tests
    { true, "xn--poema-9qae5a.com.br", "xn--poema-9qae5a.com.br" },
    { true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br" },
    { false, "xn--poema-9qae5a.com.br", "", "*.xn--poema-9qae5a.com.br,"
                                            "xn--poema-*.com.br,"
                                            "xn--*-9qae5a.com.br,"
                                            "*--poema-9qae5a.com.br" },
    // The following are adapted from the  examples quoted from
    // http://tools.ietf.org/html/rfc6125#section-6.4.3
    //  (e.g., *.example.com would match foo.example.com but
    //   not bar.foo.example.com or example.com).
    { true, "foo.example.com", "*.example.com" },
    { false, "bar.foo.example.com", "*.example.com" },
    { false, "example.com", "*.example.com" },
    //   (e.g., baz*.example.net and *baz.example.net and b*z.example.net would
    //   be taken to match baz1.example.net and foobaz.example.net and
    //   buzz.example.net, respectively
    { true, "baz1.example.net", "baz*.example.net" },
    { true, "foobaz.example.net", "*baz.example.net" },
    { true, "buzz.example.net", "b*z.example.net" },
    // Wildcards should not be valid for public registry controlled domains,
    // and unknown/unrecognized domains, at least three domain components must
    // be present.
    { true, "www.test.example", "*.test.example" },
    { true, "test.example.co.uk", "*.example.co.uk" },
    { false, "test.example", "*.exmaple" },
    { false, "example.co.uk", "*.co.uk" },
    { false, "foo.com", "*.com" },
    { false, "foo.us", "*.us" },
    { false, "foo", "*" },
    // IDN variants of wildcards and registry controlled domains.
    { true, "www.xn--poema-9qae5a.com.br", "*.xn--poema-9qae5a.com.br" },
    { true, "test.example.xn--mgbaam7a8h", "*.example.xn--mgbaam7a8h" },
    { false, "xn--poema-9qae5a.com.br", "*.com.br" },
    { false, "example.xn--mgbaam7a8h", "*.xn--mgbaam7a8h" },
    // Wildcards should be permissible for 'private' registry controlled
    // domains.
    { true, "www.appspot.com", "*.appspot.com" },
    { true, "foo.s3.amazonaws.com", "*.s3.amazonaws.com" },
    // Multiple wildcards are not valid.
    { false, "foo.example.com", "*.*.com" },
    { false, "foo.bar.example.com", "*.bar.*.com" },
    // Absolute vs relative DNS name tests. Although not explicitly specified
    // in RFC 6125, absolute reference names (those ending in a .) should
    // match either absolute or relative presented names.
    { true, "foo.com", "foo.com." },
    { true, "foo.com.", "foo.com" },
    { true, "foo.com.", "foo.com." },
    { true, "f", "f." },
    { true, "f.", "f" },
    { true, "f.", "f." },
    { true, "www-3.bar.foo.com", "*.bar.foo.com." },
    { true, "www-3.bar.foo.com.", "*.bar.foo.com" },
    { true, "www-3.bar.foo.com.", "*.bar.foo.com." },
    { false, ".", "." },
    { false, "example.com", "*.com." },
    { false, "example.com.", "*.com" },
    { false, "example.com.", "*.com." },
    { false, "foo.", "*." },
    { false, "foo", "*." },
    { false, "foo.co.uk", "*.co.uk." },
    { false, "foo.co.uk.", "*.co.uk." },
    // IP addresses in common name; IPv4 only.
    { true, "127.0.0.1", "127.0.0.1" },
    { true, "192.168.1.1", "192.168.1.1" },
    { true,  "676768", "0.10.83.160" },
    { true,  "1.2.3", "1.2.0.3" },
    { false, "192.169.1.1", "192.168.1.1" },
    { false, "12.19.1.1", "12.19.1.1/255.255.255.0" },
    { false, "FEDC:ba98:7654:3210:FEDC:BA98:7654:3210",
      "FEDC:BA98:7654:3210:FEDC:ba98:7654:3210" },
    { false, "1111:2222:3333:4444:5555:6666:7777:8888",
      "1111:2222:3333:4444:5555:6666:7777:8888" },
    { false, "::192.9.5.5", "[::192.9.5.5]" },
    // No wildcard matching in valid IP addresses
    { false, "::192.9.5.5", "*.9.5.5" },
    { false, "2010:836B:4179::836B:4179", "*:836B:4179::836B:4179" },
    { false, "192.168.1.11", "*.168.1.11" },
    { false, "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", "*.]" },
    // IP addresses in subject alternative name (common name ignored)
    { true, "10.1.2.3", "", "", "10.1.2.3" },
    { true,  "14.15", "", "", "14.0.0.15" },
    { false, "10.1.2.7", "10.1.2.7", "", "10.1.2.6,10.1.2.8" },
    { false, "10.1.2.8", "10.20.2.8", "foo" },
    { true, "::4.5.6.7", "", "", "x00000000000000000000000004050607" },
    { false, "::6.7.8.9", "::6.7.8.9", "::6.7.8.9",
        "x00000000000000000000000006070808,x0000000000000000000000000607080a,"
        "xff000000000000000000000006070809,6.7.8.9" },
    { true, "FE80::200:f8ff:fe21:67cf", "no.common.name", "",
        "x00000000000000000000000006070808,xfe800000000000000200f8fffe2167cf,"
        "xff0000000000000000000000060708ff,10.0.0.1" },
    // Numeric only hostnames (none of these are considered valid IP addresses).
    { false,  "12345.6", "12345.6" },
    { false, "121.2.3.512", "", "1*1.2.3.512,*1.2.3.512,1*.2.3.512,*.2.3.512",
        "121.2.3.0"},
    { false, "1.2.3.4.5.6", "*.2.3.4.5.6" },
    { true, "1.2.3.4.5", "", "1.2.3.4.5" },
    // Invalid host names.
    { false, "junk)(£)$*!@~#", "junk)(£)$*!@~#" },
    { false, "www.*.com", "www.*.com" },
    { false, "w$w.f.com", "w$w.f.com" },
    { false, "nocolonallowed:example", "", "nocolonallowed:example" },
    { false, "www-1.[::FFFF:129.144.52.38]", "*.[::FFFF:129.144.52.38]" },
    { false, "[::4.5.6.9]", "", "", "x00000000000000000000000004050609" },
};

class X509CertificateNameVerifyTest
    : public testing::TestWithParam<CertificateNameVerifyTestData> {
};

TEST_P(X509CertificateNameVerifyTest, VerifyHostname) {
  CertificateNameVerifyTestData test_data = GetParam();

  std::string common_name(test_data.common_name);
  ASSERT_EQ(std::string::npos, common_name.find(','));
  std::replace(common_name.begin(), common_name.end(), '#', '\0');

  std::vector<std::string> dns_names, ip_addressses;
  if (test_data.dns_names) {
    // Build up the certificate DNS names list.
    std::string dns_name_line(test_data.dns_names);
    std::replace(dns_name_line.begin(), dns_name_line.end(), '#', '\0');
    base::SplitString(dns_name_line, ',', &dns_names);
  }

  if (test_data.ip_addrs) {
    // Build up the certificate IP address list.
    std::string ip_addrs_line(test_data.ip_addrs);
    std::vector<std::string> ip_addressses_ascii;
    base::SplitString(ip_addrs_line, ',', &ip_addressses_ascii);
    for (size_t i = 0; i < ip_addressses_ascii.size(); ++i) {
      std::string& addr_ascii = ip_addressses_ascii[i];
      ASSERT_NE(0U, addr_ascii.length());
      if (addr_ascii[0] == 'x') {  // Hex encoded address
        addr_ascii.erase(0, 1);
        std::vector<uint8> bytes;
        EXPECT_TRUE(base::HexStringToBytes(addr_ascii, &bytes))
            << "Could not parse hex address " << addr_ascii << " i = " << i;
        ip_addressses.push_back(std::string(reinterpret_cast<char*>(&bytes[0]),
                                            bytes.size()));
        ASSERT_EQ(16U, ip_addressses.back().size()) << i;
      } else {  // Decimal groups
        std::vector<std::string> decimals_ascii;
        base::SplitString(addr_ascii, '.', &decimals_ascii);
        EXPECT_EQ(4U, decimals_ascii.size()) << i;
        std::string addr_bytes;
        for (size_t j = 0; j < decimals_ascii.size(); ++j) {
          int decimal_value;
          EXPECT_TRUE(base::StringToInt(decimals_ascii[j], &decimal_value));
          EXPECT_GE(decimal_value, 0);
          EXPECT_LE(decimal_value, 255);
          addr_bytes.push_back(static_cast<char>(decimal_value));
        }
        ip_addressses.push_back(addr_bytes);
        ASSERT_EQ(4U, ip_addressses.back().size()) << i;
      }
    }
  }

  bool unused = false;
  EXPECT_EQ(test_data.expected, X509Certificate::VerifyHostname(
      test_data.hostname, common_name, dns_names, ip_addressses, &unused));
}

INSTANTIATE_TEST_CASE_P(, X509CertificateNameVerifyTest,
                        testing::ValuesIn(kNameVerifyTestData));

const struct PublicKeyInfoTestData {
  const char* cert_file;
  size_t expected_bits;
  X509Certificate::PublicKeyType expected_type;
} kPublicKeyInfoTestData[] = {
  { "768-rsa-ee-by-768-rsa-intermediate.pem", 768,
    X509Certificate::kPublicKeyTypeRSA },
  { "1024-rsa-ee-by-768-rsa-intermediate.pem", 1024,
    X509Certificate::kPublicKeyTypeRSA },
  { "prime256v1-ecdsa-ee-by-1024-rsa-intermediate.pem", 256,
    X509Certificate::kPublicKeyTypeECDSA },
};

class X509CertificatePublicKeyInfoTest
    : public testing::TestWithParam<PublicKeyInfoTestData> {
};

TEST_P(X509CertificatePublicKeyInfoTest, GetPublicKeyInfo) {
  PublicKeyInfoTestData data = GetParam();

#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA &&
      data.expected_type == X509Certificate::kPublicKeyTypeECDSA) {
    // ECC is only supported on Vista+. Skip the test.
    return;
  }
#endif

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), data.cert_file));
  ASSERT_TRUE(cert.get());

  size_t actual_bits = 0;
  X509Certificate::PublicKeyType actual_type =
      X509Certificate::kPublicKeyTypeUnknown;

  X509Certificate::GetPublicKeyInfo(cert->os_cert_handle(), &actual_bits,
                                    &actual_type);

  EXPECT_EQ(data.expected_bits, actual_bits);
  EXPECT_EQ(data.expected_type, actual_type);
}

INSTANTIATE_TEST_CASE_P(, X509CertificatePublicKeyInfoTest,
                        testing::ValuesIn(kPublicKeyInfoTestData));

}  // namespace net
