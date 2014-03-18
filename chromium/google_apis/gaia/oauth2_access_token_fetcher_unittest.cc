// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2AccessTokenFetcher.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::ResponseCookies;
using net::ScopedURLFetcherFactory;
using net::TestURLFetcher;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLFetcherFactory;
using net::URLRequestStatus;
using testing::_;
using testing::Return;

namespace {

typedef std::vector<std::string> ScopeList;

static const char kValidTokenResponse[] =
    "{"
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";
static const char kTokenResponseNoAccessToken[] =
    "{"
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";

static const char kValidFailureTokenResponse[] =
    "{"
    "  \"error\": \"invalid_grant\""
    "}";

class MockUrlFetcherFactory : public ScopedURLFetcherFactory,
                              public URLFetcherFactory {
public:
  MockUrlFetcherFactory()
      : ScopedURLFetcherFactory(this) {
  }
  virtual ~MockUrlFetcherFactory() {}

  MOCK_METHOD4(
      CreateURLFetcher,
      URLFetcher* (int id,
                   const GURL& url,
                   URLFetcher::RequestType request_type,
                   URLFetcherDelegate* d));
};

class MockOAuth2AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuth2AccessTokenConsumer() {}
  ~MockOAuth2AccessTokenConsumer() {}

  MOCK_METHOD2(OnGetTokenSuccess, void(const std::string& access_token,
                                       const base::Time& expiration_time));
  MOCK_METHOD1(OnGetTokenFailure,
               void(const GoogleServiceAuthError& error));
};

}  // namespace

class OAuth2AccessTokenFetcherTest : public testing::Test {
 public:
  OAuth2AccessTokenFetcherTest()
    : request_context_getter_(new net::TestURLRequestContextGetter(
          base::MessageLoopProxy::current())),
      fetcher_(&consumer_, request_context_getter_) {
  }

  virtual ~OAuth2AccessTokenFetcherTest() {}

  virtual TestURLFetcher* SetupGetAccessToken(
      bool fetch_succeeds, int response_code, const std::string& body) {
    GURL url(GaiaUrls::GetInstance()->oauth2_token_url());
    TestURLFetcher* url_fetcher = new TestURLFetcher(0, url, &fetcher_);
    URLRequestStatus::Status status =
        fetch_succeeds ? URLRequestStatus::SUCCESS : URLRequestStatus::FAILED;
    url_fetcher->set_status(URLRequestStatus(status, 0));

    if (response_code != 0)
      url_fetcher->set_response_code(response_code);

    if (!body.empty())
      url_fetcher->SetResponseString(body);

    EXPECT_CALL(factory_, CreateURLFetcher(_, url, _, _))
        .WillOnce(Return(url_fetcher));
    return url_fetcher;
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  MockUrlFetcherFactory factory_;
  MockOAuth2AccessTokenConsumer consumer_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  OAuth2AccessTokenFetcher fetcher_;
};

// These four tests time out, see http://crbug.com/113446.
TEST_F(OAuth2AccessTokenFetcherTest, DISABLED_GetAccessTokenRequestFailure) {
  TestURLFetcher* url_fetcher = SetupGetAccessToken(false, 0, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", "refresh_token", ScopeList());
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2AccessTokenFetcherTest,
       DISABLED_GetAccessTokenResponseCodeFailure) {
  TestURLFetcher* url_fetcher =
      SetupGetAccessToken(true, net::HTTP_FORBIDDEN, std::string());
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", "refresh_token", ScopeList());
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2AccessTokenFetcherTest, DISABLED_Success) {
  TestURLFetcher* url_fetcher = SetupGetAccessToken(
      true, net::HTTP_OK, kValidTokenResponse);
  EXPECT_CALL(consumer_, OnGetTokenSuccess("at1", _)).Times(1);
  fetcher_.Start("client_id", "client_secret", "refresh_token", ScopeList());
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2AccessTokenFetcherTest, DISABLED_MakeGetAccessTokenBody) {
  {  // No scope.
    std::string body =
        "client_id=cid1&"
        "client_secret=cs1&"
        "grant_type=refresh_token&"
        "refresh_token=rt1";
    EXPECT_EQ(body, OAuth2AccessTokenFetcher::MakeGetAccessTokenBody(
        "cid1", "cs1", "rt1", ScopeList()));
  }

  {  // One scope.
    std::string body =
        "client_id=cid1&"
        "client_secret=cs1&"
        "grant_type=refresh_token&"
        "refresh_token=rt1&"
        "scope=https://www.googleapis.com/foo";
    ScopeList scopes;
    scopes.push_back("https://www.googleapis.com/foo");
    EXPECT_EQ(body, OAuth2AccessTokenFetcher::MakeGetAccessTokenBody(
        "cid1", "cs1", "rt1", scopes));
  }

  {  // Multiple scopes.
    std::string body =
        "client_id=cid1&"
        "client_secret=cs1&"
        "grant_type=refresh_token&"
        "refresh_token=rt1&"
        "scope=https://www.googleapis.com/foo+"
        "https://www.googleapis.com/bar+"
        "https://www.googleapis.com/baz";
    ScopeList scopes;
    scopes.push_back("https://www.googleapis.com/foo");
    scopes.push_back("https://www.googleapis.com/bar");
    scopes.push_back("https://www.googleapis.com/baz");
    EXPECT_EQ(body, OAuth2AccessTokenFetcher::MakeGetAccessTokenBody(
        "cid1", "cs1", "rt1", scopes));
  }
}

// http://crbug.com/114215
#if defined(OS_WIN)
#define MAYBE_ParseGetAccessTokenResponse DISABLED_ParseGetAccessTokenResponse
#else
#define MAYBE_ParseGetAccessTokenResponse ParseGetAccessTokenResponse
#endif // defined(OS_WIN)
TEST_F(OAuth2AccessTokenFetcherTest, MAYBE_ParseGetAccessTokenResponse) {
  {  // No body.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);

    std::string at;
    int expires_in;
    EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenSuccessResponse(
        &url_fetcher, &at, &expires_in));
    EXPECT_TRUE(at.empty());
  }
  {  // Bad json.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString("foo");

    std::string at;
    int expires_in;
    EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenSuccessResponse(
        &url_fetcher, &at, &expires_in));
    EXPECT_TRUE(at.empty());
  }
  {  // Valid json: access token missing.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString(kTokenResponseNoAccessToken);

    std::string at;
    int expires_in;
    EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenSuccessResponse(
        &url_fetcher, &at, &expires_in));
    EXPECT_TRUE(at.empty());
  }
  {  // Valid json: all good.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString(kValidTokenResponse);

    std::string at;
    int expires_in;
    EXPECT_TRUE(OAuth2AccessTokenFetcher::ParseGetAccessTokenSuccessResponse(
        &url_fetcher, &at, &expires_in));
    EXPECT_EQ("at1", at);
    EXPECT_EQ(3600, expires_in);
  }
  {  // Valid json: invalid error response.
     TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
     url_fetcher.SetResponseString(kTokenResponseNoAccessToken);

     std::string error;
     EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenFailureResponse(
         &url_fetcher, &error));
     EXPECT_TRUE(error.empty());
   }
   {  // Valid json: error response.
     TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
     url_fetcher.SetResponseString(kValidFailureTokenResponse);

     std::string error;
     EXPECT_TRUE(OAuth2AccessTokenFetcher::ParseGetAccessTokenFailureResponse(
         &url_fetcher, &error));
     EXPECT_EQ("invalid_grant", error);
   }
}
