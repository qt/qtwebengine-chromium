// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_AUTH_SERVICE_H_
#define GOOGLE_APIS_DRIVE_AUTH_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "google_apis/drive/auth_service_interface.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace net {
class URLRequestContextGetter;
}

namespace google_apis {

class AuthServiceObserver;

// This class provides authentication for Google services.
// It integrates specific service integration with OAuth2 stack
// (OAuth2TokenService) and provides OAuth2 token refresh infrastructure.
// All public functions must be called on UI thread.
class AuthService : public AuthServiceInterface,
                    public OAuth2TokenService::Observer {
 public:
  // |url_request_context_getter| is used to perform authentication with
  // URLFetcher.
  //
  // |scopes| specifies OAuth2 scopes.
  AuthService(OAuth2TokenService* oauth2_token_service,
              const std::string& account_id,
              net::URLRequestContextGetter* url_request_context_getter,
              const std::vector<std::string>& scopes);
  virtual ~AuthService();

  // Overriden from AuthServiceInterface:
  virtual void AddObserver(AuthServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(AuthServiceObserver* observer) OVERRIDE;
  virtual void StartAuthentication(const AuthStatusCallback& callback) OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual const std::string& access_token() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;

  // Overridden from OAuth2TokenService::Observer:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;

 private:
  // Called when the state of the refresh token changes.
  void OnHandleRefreshToken(bool has_refresh_token);

  // Called when authentication request from StartAuthentication() is
  // completed.
  void OnAuthCompleted(const AuthStatusCallback& callback,
                       GDataErrorCode error,
                       const std::string& access_token);

  OAuth2TokenService* oauth2_token_service_;
  std::string account_id_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  bool has_refresh_token_;
  std::string access_token_;
  std::vector<std::string> scopes_;
  ObserverList<AuthServiceObserver> observers_;
  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthService);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_AUTH_SERVICE_H_
