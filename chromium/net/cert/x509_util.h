// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_H_
#define NET_CERT_X509_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace crypto {
class ECPrivateKey;
class RSAPrivateKey;
}

namespace net {

class X509Certificate;

namespace x509_util {

// Supported digest algorithms for signing certificates.
enum DigestAlgorithm {
  DIGEST_SHA1,
  DIGEST_SHA256
};

// Returns true if the times can be used to create an X.509 certificate.
// Certificates can accept dates from Jan 1st, 1 to Dec 31, 9999.  A bug in NSS
// limited the range to 1950-9999
// (https://bugzilla.mozilla.org/show_bug.cgi?id=786531).  This function will
// return whether it is supported by the currently used crypto library.
NET_EXPORT_PRIVATE bool IsSupportedValidityRange(base::Time not_valid_before,
                                                 base::Time not_valid_after);

// Creates a private keypair and server bound certificate.
// Domain, serial number and validity period are given as
// parameters. The certificate is signed by the private key in |key|.
// The signature algorithm may be updated periodically to match best practices.
//
// See Internet Draft draft-balfanz-tls-obc-00 for more details:
// http://tools.ietf.org/html/draft-balfanz-tls-obc-00
NET_EXPORT_PRIVATE bool CreateKeyAndDomainBoundCertEC(
    const std::string& domain,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    scoped_ptr<crypto::ECPrivateKey>* key,
    std::string* der_cert);

// Helper function for CreateKeyAndDomainBoundCertEC.
NET_EXPORT_PRIVATE bool CreateDomainBoundCertEC(crypto::ECPrivateKey* key,
                                                DigestAlgorithm alg,
                                                const std::string& domain,
                                                uint32 serial_number,
                                                base::Time not_valid_before,
                                                base::Time not_valid_after,
                                                std::string* der_cert);

// Creates a public-private keypair and a self-signed certificate.
// Subject, serial number and validity period are given as parameters.
// The certificate is signed by the private key in |key|. The key length and
// signature algorithm may be updated periodically to match best practices.
//
// |subject| is a distinguished name defined in RFC4514 with _only_ a CN
// component, as in:
//   CN=Michael Wong
//
// SECURITY WARNING
//
// Using self-signed certificates has the following security risks:
// 1. Encryption without authentication and thus vulnerable to
//    man-in-the-middle attacks.
// 2. Self-signed certificates cannot be revoked.
//
// Use this certificate only after the above risks are acknowledged.
NET_EXPORT bool CreateKeyAndSelfSignedCert(
    const std::string& subject,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    scoped_ptr<crypto::RSAPrivateKey>* key,
    std::string* der_cert);

// Creates a self-signed certificate from a provided key, using the specified
// hash algorithm.  You should not re-use a key for signing data with multiple
// signature algorithms or parameters.
NET_EXPORT bool CreateSelfSignedCert(crypto::RSAPrivateKey* key,
                                     DigestAlgorithm alg,
                                     const std::string& subject,
                                     uint32 serial_number,
                                     base::Time not_valid_before,
                                     base::Time not_valid_after,
                                     std::string* der_cert);

// Comparator for use in STL algorithms that will sort client certificates by
// order of preference.
// Returns true if |a| is more preferable than |b|, allowing it to be used
// with any algorithm that compares according to strict weak ordering.
//
// Criteria include:
// - Prefer certificates that have a longer validity period (later
//   expiration dates)
// - If equal, prefer certificates that were issued more recently
// - If equal, prefer shorter chains (if available)
class NET_EXPORT_PRIVATE ClientCertSorter {
 public:
  ClientCertSorter();

  bool operator()(
      const scoped_refptr<X509Certificate>& a,
      const scoped_refptr<X509Certificate>& b) const;

 private:
  base::Time now_;
};

} // namespace x509_util

} // namespace net

#endif  // NET_CERT_X509_UTIL_H_
