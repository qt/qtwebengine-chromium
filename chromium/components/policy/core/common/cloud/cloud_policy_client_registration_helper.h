// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_REGISTRATION_HELPER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_REGISTRATION_HELPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_info_fetcher.h"
#include "components/policy/policy_export.h"
#include "policy/proto/device_management_backend.pb.h"

class OAuth2TokenService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

// Helper class that registers a CloudPolicyClient. It fetches an OAuth2 token
// for the DM service if needed, and checks with Gaia if the account has policy
// management enabled.
class POLICY_EXPORT CloudPolicyClientRegistrationHelper
    : public UserInfoFetcher::Delegate,
      public CloudPolicyClient::Observer {
 public:
  // |context| and |client| are not owned and must outlive this object.
  // If |should_force_load_policy| then the cloud policy registration is
  // performed even if Gaia indicates that this account doesn't have management
  // enabled.
  CloudPolicyClientRegistrationHelper(
      CloudPolicyClient* client,
      bool should_force_load_policy,
      enterprise_management::DeviceRegisterRequest::Type registration_type);
  virtual ~CloudPolicyClientRegistrationHelper();

  // Starts the client registration process. This version uses the
  // supplied OAuth2TokenService to mint the new token for the userinfo
  // and DM services, using the |account_id|.
  // |callback| is invoked when the registration is complete.
  void StartRegistration(
      OAuth2TokenService* token_service,
      const std::string& account_id,
      const base::Closure& callback);

#if !defined(OS_ANDROID)
  // Starts the client registration process. The |login_refresh_token| is used
  // to mint a new token for the userinfo and DM services.
  // |callback| is invoked when the registration is complete.
  void StartRegistrationWithLoginToken(const std::string& login_refresh_token,
                                       const base::Closure& callback);
#endif

 private:
  class TokenServiceHelper;
#if !defined(OS_ANDROID)
  class LoginTokenHelper;
#endif

  void OnTokenFetched(const std::string& oauth_access_token);

  // UserInfoFetcher::Delegate implementation:
  virtual void OnGetUserInfoSuccess(
      const base::DictionaryValue* response) OVERRIDE;
  virtual void OnGetUserInfoFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // CloudPolicyClient::Observer implementation:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // Invoked when the registration request has been completed.
  void RequestCompleted();

  // Internal helper class that uses OAuth2TokenService to fetch an OAuth
  // access token. On desktop, this is only used after the user has signed in -
  // desktop platforms use LoginTokenHelper for policy fetches performed before
  // signin is complete.
  scoped_ptr<TokenServiceHelper> token_service_helper_;

#if !defined(OS_ANDROID)
  // Special desktop-only helper to fetch an OAuth access token prior to
  // the completion of signin. Not used on Android since all token fetching
  // is done via OAuth2TokenService.
  scoped_ptr<LoginTokenHelper> login_token_helper_;
#endif

  // Helper class for fetching information from GAIA about the currently
  // signed-in user.
  scoped_ptr<UserInfoFetcher> user_info_fetcher_;

  // Access token used to register the CloudPolicyClient and also access
  // GAIA to get information about the signed in user.
  std::string oauth_access_token_;

  net::URLRequestContextGetter* context_;
  CloudPolicyClient* client_;
  bool should_force_load_policy_;
  enterprise_management::DeviceRegisterRequest::Type registration_type_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyClientRegistrationHelper);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_REGISTRATION_HELPER_H_
