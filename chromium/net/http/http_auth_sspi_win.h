// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains common routines used by NTLM and Negotiate authentication
// using the SSPI API on Windows.

#ifndef NET_HTTP_HTTP_AUTH_SSPI_WIN_H_
#define NET_HTTP_HTTP_AUTH_SSPI_WIN_H_

// security.h needs to be included for CredHandle. Unfortunately CredHandle
// is a typedef and can't be forward declared.
#define SECURITY_WIN32 1
#include <windows.h>
#include <security.h>

#include <string>

#include "base/strings/string16.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"

namespace net {

// SSPILibrary is introduced so unit tests can mock the calls to Windows' SSPI
// implementation. The default implementation simply passes the arguments on to
// the SSPI implementation provided by Secur32.dll.
// NOTE(cbentzel): I considered replacing the Secur32.dll with a mock DLL, but
// decided that it wasn't worth the effort as this is unlikely to be performance
// sensitive code.
class SSPILibrary {
 public:
  virtual ~SSPILibrary() {}

  virtual SECURITY_STATUS AcquireCredentialsHandle(LPWSTR pszPrincipal,
                                                   LPWSTR pszPackage,
                                                   unsigned long fCredentialUse,
                                                   void* pvLogonId,
                                                   void* pvAuthData,
                                                   SEC_GET_KEY_FN pGetKeyFn,
                                                   void* pvGetKeyArgument,
                                                   PCredHandle phCredential,
                                                   PTimeStamp ptsExpiry) = 0;

  virtual SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                                    PCtxtHandle phContext,
                                                    SEC_WCHAR* pszTargetName,
                                                    unsigned long fContextReq,
                                                    unsigned long Reserved1,
                                                    unsigned long TargetDataRep,
                                                    PSecBufferDesc pInput,
                                                    unsigned long Reserved2,
                                                    PCtxtHandle phNewContext,
                                                    PSecBufferDesc pOutput,
                                                    unsigned long* contextAttr,
                                                    PTimeStamp ptsExpiry) = 0;

  virtual SECURITY_STATUS QuerySecurityPackageInfo(LPWSTR pszPackageName,
                                                   PSecPkgInfoW *pkgInfo) = 0;

  virtual SECURITY_STATUS FreeCredentialsHandle(PCredHandle phCredential) = 0;

  virtual SECURITY_STATUS DeleteSecurityContext(PCtxtHandle phContext) = 0;

  virtual SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) = 0;
};

class SSPILibraryDefault : public SSPILibrary {
 public:
  SSPILibraryDefault() {}
  virtual ~SSPILibraryDefault() {}

  virtual SECURITY_STATUS AcquireCredentialsHandle(LPWSTR pszPrincipal,
                                                   LPWSTR pszPackage,
                                                   unsigned long fCredentialUse,
                                                   void* pvLogonId,
                                                   void* pvAuthData,
                                                   SEC_GET_KEY_FN pGetKeyFn,
                                                   void* pvGetKeyArgument,
                                                   PCredHandle phCredential,
                                                   PTimeStamp ptsExpiry) {
    return ::AcquireCredentialsHandle(pszPrincipal, pszPackage, fCredentialUse,
                                      pvLogonId, pvAuthData, pGetKeyFn,
                                      pvGetKeyArgument, phCredential,
                                      ptsExpiry);
  }

  virtual SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                                    PCtxtHandle phContext,
                                                    SEC_WCHAR* pszTargetName,
                                                    unsigned long fContextReq,
                                                    unsigned long Reserved1,
                                                    unsigned long TargetDataRep,
                                                    PSecBufferDesc pInput,
                                                    unsigned long Reserved2,
                                                    PCtxtHandle phNewContext,
                                                    PSecBufferDesc pOutput,
                                                    unsigned long* contextAttr,
                                                    PTimeStamp ptsExpiry) {
    return ::InitializeSecurityContext(phCredential, phContext, pszTargetName,
                                       fContextReq, Reserved1, TargetDataRep,
                                       pInput, Reserved2, phNewContext, pOutput,
                                       contextAttr, ptsExpiry);
  }

  virtual SECURITY_STATUS QuerySecurityPackageInfo(LPWSTR pszPackageName,
                                                   PSecPkgInfoW *pkgInfo) {
    return ::QuerySecurityPackageInfo(pszPackageName, pkgInfo);
  }

  virtual SECURITY_STATUS FreeCredentialsHandle(PCredHandle phCredential) {
    return ::FreeCredentialsHandle(phCredential);
  }

  virtual SECURITY_STATUS DeleteSecurityContext(PCtxtHandle phContext) {
    return ::DeleteSecurityContext(phContext);
  }

  virtual SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) {
    return ::FreeContextBuffer(pvContextBuffer);
  }
};

class NET_EXPORT_PRIVATE HttpAuthSSPI {
 public:
  HttpAuthSSPI(SSPILibrary* sspi_library,
               const std::string& scheme,
               const SEC_WCHAR* security_package,
               ULONG max_token_length);
  ~HttpAuthSSPI();

  bool NeedsIdentity() const;

  bool AllowsExplicitCredentials() const;

  HttpAuth::AuthorizationResult ParseChallenge(
      HttpAuth::ChallengeTokenizer* tok);

  // Generates an authentication token for the service specified by the
  // Service Principal Name |spn| and stores the value in |*auth_token|.
  // If the return value is not |OK|, then the value of |*auth_token| is
  // unspecified. ERR_IO_PENDING is not a valid return code.
  // If this is the first round of a multiple round scheme, credentials are
  // obtained using |*credentials|. If |credentials| is NULL, the credentials
  // for the currently logged in user are used instead.
  int GenerateAuthToken(const AuthCredentials* credentials,
                        const std::string& spn,
                        std::string* auth_token);

  // Delegation is allowed on the Kerberos ticket. This allows certain servers
  // to act as the user, such as an IIS server retrieiving data from a
  // Kerberized MSSQL server.
  void Delegate();

 private:
  int OnFirstRound(const AuthCredentials* credentials);

  int GetNextSecurityToken(
      const std::string& spn,
      const void* in_token,
      int in_token_len,
      void** out_token,
      int* out_token_len);

  void ResetSecurityContext();

  SSPILibrary* library_;
  std::string scheme_;
  const SEC_WCHAR* security_package_;
  std::string decoded_server_auth_token_;
  ULONG max_token_length_;
  CredHandle cred_;
  CtxtHandle ctxt_;
  bool can_delegate_;
};

// Splits |combined| into domain and username.
// If |combined| is of form "FOO\bar", |domain| will contain "FOO" and |user|
// will contain "bar".
// If |combined| is of form "bar", |domain| will be empty and |user| will
// contain "bar".
// |domain| and |user| must be non-NULL.
NET_EXPORT_PRIVATE void SplitDomainAndUser(const base::string16& combined,
                                           base::string16* domain,
                                           base::string16* user);

// Determines the maximum token length in bytes for a particular SSPI package.
//
// |library| and |max_token_length| must be non-NULL pointers to valid objects.
//
// If the return value is OK, |*max_token_length| contains the maximum token
// length in bytes.
//
// If the return value is ERR_UNSUPPORTED_AUTH_SCHEME, |package| is not an
// known SSPI authentication scheme on this system. |*max_token_length| is not
// changed.
//
// If the return value is ERR_UNEXPECTED, there was an unanticipated problem
// in the underlying SSPI call. The details are logged, and |*max_token_length|
// is not changed.
NET_EXPORT_PRIVATE int DetermineMaxTokenLength(SSPILibrary* library,
                                               const std::wstring& package,
                                               ULONG* max_token_length);

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_SSPI_WIN_H_
