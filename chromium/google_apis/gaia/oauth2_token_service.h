// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_
#define GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

class GoogleServiceAuthError;

// Abstract base class for a service that fetches and caches OAuth2 access
// tokens. Concrete subclasses should implement GetRefreshToken to return
// the appropriate refresh token. Derived services might maintain refresh tokens
// for multiple accounts.
//
// All calls are expected from the UI thread.
//
// To use this service, call StartRequest() with a given set of scopes and a
// consumer of the request results. The consumer is required to outlive the
// request. The request can be deleted. The consumer may be called back
// asynchronously with the fetch results.
//
// - If the consumer is not called back before the request is deleted, it will
//   never be called back.
//   Note in this case, the actual network requests are not canceled and the
//   cache will be populated with the fetched results; it is just the consumer
//   callback that is aborted.
//
// - Otherwise the consumer will be called back with the request and the fetch
//   results.
//
// The caller of StartRequest() owns the returned request and is responsible to
// delete the request even once the callback has been invoked.
class OAuth2TokenService : public base::NonThreadSafe {
 public:
  // Class representing a request that fetches an OAuth2 access token.
  class Request {
   public:
    virtual ~Request();
   protected:
    Request();
  };

  // Class representing the consumer of a Request passed to |StartRequest|,
  // which will be called back when the request completes.
  class Consumer {
   public:
    Consumer();
    virtual ~Consumer();
    // |request| is a Request that is started by this consumer and has
    // completed.
    virtual void OnGetTokenSuccess(const Request* request,
                                   const std::string& access_token,
                                   const base::Time& expiration_time) = 0;
    virtual void OnGetTokenFailure(const Request* request,
                                   const GoogleServiceAuthError& error) = 0;
  };

  // Classes that want to listen for token availability should implement this
  // interface and register with the AddObserver() call.
  class Observer {
   public:
    // Called whenever a new login-scoped refresh token is available for
    // account |account_id|. Once available, access tokens can be retrieved for
    // this account.  This is called during initial startup for each token
    // loaded.
    virtual void OnRefreshTokenAvailable(const std::string& account_id) {}
    // Called whenever the login-scoped refresh token becomes unavailable for
    // account |account_id|.
    virtual void OnRefreshTokenRevoked(const std::string& account_id) {}
    // Called after all refresh tokens are loaded during OAuth2TokenService
    // startup.
    virtual void OnRefreshTokensLoaded() {}
   protected:
    virtual ~Observer() {}
  };

  // A set of scopes in OAuth2 authentication.
  typedef std::set<std::string> ScopeSet;

  OAuth2TokenService();
  virtual ~OAuth2TokenService();

  // Add or remove observers of this token service.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  // TODO(atwilson): Make this non-virtual when we change
  // ProfileOAuth2TokenServiceRequestTest to use FakeProfileOAuth2TokenService.
  virtual scoped_ptr<Request> StartRequest(const std::string& account_id,
                                           const ScopeSet& scopes,
                                           Consumer* consumer);

  // This method does the same as |StartRequest| except it uses |client_id| and
  // |client_secret| to identify OAuth client app instead of using
  // Chrome's default values.
  scoped_ptr<Request> StartRequestForClient(
      const std::string& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the request
  // context given by |getter| instead of using the one returned by
  // |GetRequestContext| implemented by derived classes.
  scoped_ptr<Request> StartRequestWithContext(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      const ScopeSet& scopes,
      Consumer* consumer);

  // Lists account IDs of all accounts with a refresh token maintained by this
  // instance.
  virtual std::vector<std::string> GetAccounts();

  // Returns true if a refresh token exists for |account_id|. If false, calls to
  // |StartRequest| will result in a Consumer::OnGetTokenFailure callback.
  virtual bool RefreshTokenIsAvailable(const std::string& account_id);

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  void InvalidateToken(const std::string& account_id,
                       const ScopeSet& scopes,
                       const std::string& access_token);

  // Like |InvalidateToken| except is uses |client_id| to identity OAuth2 client
  // app that issued the request instead of Chrome's default values.
  void InvalidateTokenForClient(const std::string& account_id,
                                const std::string& client_id,
                                const ScopeSet& scopes,
                                const std::string& access_token);


  // Return the current number of entries in the cache.
  int cache_size_for_testing() const;
  void set_max_authorization_token_fetch_retries_for_testing(int max_retries);
  // Returns the current number of pending fetchers matching given params.
  size_t GetNumPendingRequestsForTesting(
      const std::string& client_id,
      const std::string& account_id,
      const ScopeSet& scopes) const;

 protected:
  // Implements a cancelable |OAuth2TokenService::Request|, which should be
  // operated on the UI thread.
  // TODO(davidroche): move this out of header file.
  class RequestImpl : public base::SupportsWeakPtr<RequestImpl>,
                      public base::NonThreadSafe,
                      public Request {
   public:
    // |consumer| is required to outlive this.
    explicit RequestImpl(Consumer* consumer);
    virtual ~RequestImpl();

    // Informs |consumer_| that this request is completed.
    void InformConsumer(const GoogleServiceAuthError& error,
                        const std::string& access_token,
                        const base::Time& expiration_date);

   private:
    // |consumer_| to call back when this request completes.
    Consumer* const consumer_;
  };

  // Subclasses should return the maintained refresh token for |account_id|.
  // If no token is available, return an empty string.
  virtual std::string GetRefreshToken(const std::string& account_id) = 0;

  // Subclasses can override if they want to report errors to the user.
  virtual void UpdateAuthError(
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  // Add a new entry to the cache.
  // Subclasses can override if there are implementation-specific reasons
  // that an access token should ever not be cached.
  virtual void RegisterCacheEntry(const std::string& client_id,
                                  const std::string& account_id,
                                  const ScopeSet& scopes,
                                  const std::string& access_token,
                                  const base::Time& expiration_date);

  // Clears the internal token cache.
  void ClearCache();

  // Clears all of the tokens belonging to |account_id| from the internal token
  // cache. It does not matter what other parameters, like |client_id| were
  // used to request the tokens.
  void ClearCacheForAccount(const std::string& account_id);

  // Cancels all requests that are currently in progress.
  void CancelAllRequests();

  // Cancels all requests related to a given |account_id|.
  void CancelRequestsForAccount(const std::string& account_id);

  // Called by subclasses to notify observers.
  void FireRefreshTokenAvailable(const std::string& account_id);
  void FireRefreshTokenRevoked(const std::string& account_id);
  void FireRefreshTokensLoaded();

  // Fetches an OAuth token for the specified client/scopes. Virtual so it can
  // be overridden for tests and for platform-specific behavior on Android.
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes);

  // Invalidates the |access_token| issued for |account_id|, |client_id| and
  // |scopes|. Virtual so it can be overriden for tests and for platform-
  // specifc behavior.
  virtual void InvalidateOAuth2Token(const std::string& account_id,
                                     const std::string& client_id,
                                     const ScopeSet& scopes,
                                     const std::string& access_token);

 private:
  class Fetcher;
  friend class Fetcher;

  // The parameters used to fetch an OAuth2 access token.
  struct RequestParameters {
    RequestParameters(const std::string& client_id,
                      const std::string& account_id,
                      const ScopeSet& scopes);
    ~RequestParameters();
    bool operator<(const RequestParameters& params) const;

    // OAuth2 client id.
    std::string client_id;
    // Account id for which the request is made.
    std::string account_id;
    // URL scopes for the requested access token.
    ScopeSet scopes;
  };

  typedef std::map<RequestParameters, Fetcher*> PendingFetcherMap;

  // Derived classes must provide a request context used for fetching access
  // tokens with the |StartRequest| method.
  virtual net::URLRequestContextGetter* GetRequestContext() = 0;

  // Struct that contains the information of an OAuth2 access token.
  struct CacheEntry {
    std::string access_token;
    base::Time expiration_date;
  };

  // This method does the same as |StartRequestWithContext| except it
  // uses |client_id| and |client_secret| to identify OAuth
  // client app instead of using Chrome's default values.
  scoped_ptr<Request> StartRequestForClientWithContext(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      const std::string& client_id,
      const std::string& client_secret,
      const ScopeSet& scopes,
      Consumer* consumer);

  // Returns true if GetCacheEntry would return a valid cache entry for the
  // given scopes.
  bool HasCacheEntry(const RequestParameters& client_scopes);

  // Posts a task to fire the Consumer callback with the cached token.  Must
  // Must only be called if HasCacheEntry() returns true.
  void StartCacheLookupRequest(RequestImpl* request,
                               const RequestParameters& client_scopes,
                               Consumer* consumer);

  // Returns a currently valid OAuth2 access token for the given set of scopes,
  // or NULL if none have been cached. Note the user of this method should
  // ensure no entry with the same |client_scopes| is added before the usage of
  // the returned entry is done.
  const CacheEntry* GetCacheEntry(const RequestParameters& client_scopes);

  // Removes an access token for the given set of scopes from the cache.
  // Returns true if the entry was removed, otherwise false.
  bool RemoveCacheEntry(const RequestParameters& client_scopes,
                        const std::string& token_to_remove);

  // Called when |fetcher| finishes fetching.
  void OnFetchComplete(Fetcher* fetcher);

  // Called when a number of fetchers need to be canceled.
  void CancelFetchers(std::vector<Fetcher*> fetchers_to_cancel);

  // The cache of currently valid tokens.
  typedef std::map<RequestParameters, CacheEntry> TokenCache;
  TokenCache token_cache_;

  // A map from fetch parameters to a fetcher that is fetching an OAuth2 access
  // token using these parameters.
  PendingFetcherMap pending_fetchers_;

  // List of observers to notify when token availability changes.
  // Makes sure list is empty on destruction.
  ObserverList<Observer, true> observer_list_;

  // Maximum number of retries in fetching an OAuth2 access token.
  static int max_fetch_retry_num_;

  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest, RequestParametersOrderTest);
  FRIEND_TEST_ALL_PREFIXES(OAuth2TokenServiceTest,
                           SameScopesRequestedForDifferentClients);

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenService);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_TOKEN_SERVICE_H_
