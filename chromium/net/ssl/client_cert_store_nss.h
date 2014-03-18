// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_NSS_H_
#define NET_SSL_CLIENT_CERT_STORE_NSS_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}

namespace net {

class NET_EXPORT ClientCertStoreNSS : public ClientCertStore {
 public:
  typedef base::Callback<crypto::CryptoModuleBlockingPasswordDelegate*(
      const std::string& /* server */)> PasswordDelegateFactory;

  explicit ClientCertStoreNSS(
      const PasswordDelegateFactory& password_delegate_factory);
  virtual ~ClientCertStoreNSS();

  // ClientCertStore:
  virtual void GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                              CertificateList* selected_certs,
                              const base::Closure& callback) OVERRIDE;

 private:
  friend class ClientCertStoreNSSTestDelegate;

  // A hook for testing. Filters |input_certs| using the logic being used to
  // filter the system store when GetClientCerts() is called.
  // Implemented by creating a list of certificates that otherwise would be
  // extracted from the system store and filtering it using the common logic
  // (less adequate than the approach used on Windows).
  bool SelectClientCertsForTesting(const CertificateList& input_certs,
                                   const SSLCertRequestInfo& cert_request_info,
                                   CertificateList* selected_certs);

  // The factory for creating the delegate for requesting a password to a
  // PKCS #11 token. May be null.
  PasswordDelegateFactory password_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertStoreNSS);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_NSS_H_
