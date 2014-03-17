// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/request_sender.h"

#include "base/strings/string_number_conversions.h"
#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestRefreshToken[] = "valid-refresh-token";
const char kTestAccessToken[] = "valid-access-token";

// Enum for indicating the reason why a request is finished.
enum FinishReason {
  NONE,
  SUCCESS,
  CANCEL,
  AUTH_FAILURE,
};

// AuthService for testing purpose. It accepts kTestRefreshToken and returns
// kTestAccessToken + {"1", "2", "3", ...}.
class TestAuthService : public DummyAuthService {
 public:
  TestAuthService() : auth_try_count_(0) {}

  virtual void StartAuthentication(
      const AuthStatusCallback& callback) OVERRIDE {
    // RequestSender should clear the rejected access token before starting
    // to request another one.
    EXPECT_FALSE(HasAccessToken());

    ++auth_try_count_;

    if (refresh_token() == kTestRefreshToken) {
      const std::string token =
          kTestAccessToken + base::IntToString(auth_try_count_);
      set_access_token(token);
      callback.Run(HTTP_SUCCESS, token);
    } else {
      set_access_token("");
      callback.Run(HTTP_UNAUTHORIZED, "");
    }
  }

 private:
  int auth_try_count_;
};

// The main test fixture class.
class RequestSenderTest : public testing::Test {
 protected:
  RequestSenderTest()
     : auth_service_(new TestAuthService),
       request_sender_(auth_service_, NULL, NULL, "dummy-user-agent") {
    auth_service_->set_refresh_token(kTestRefreshToken);
    auth_service_->set_access_token(kTestAccessToken);
  }

  TestAuthService* auth_service_;  // Owned by |request_sender_|.
  RequestSender request_sender_;
};

// Minimal implementation for AuthenticatedRequestInterface that can interact
// with RequestSender correctly.
class TestRequest : public AuthenticatedRequestInterface {
 public:
  TestRequest(RequestSender* sender,
              bool* start_called,
              FinishReason* finish_reason)
      : sender_(sender),
        start_called_(start_called),
        finish_reason_(finish_reason),
        weak_ptr_factory_(this) {
  }

  // Test the situation that the request has finished.
  void FinishRequestWithSuccess() {
    *finish_reason_ = SUCCESS;
    sender_->RequestFinished(this);
  }

  const std::string& passed_access_token() const {
    return passed_access_token_;
  }

  const ReAuthenticateCallback& passed_reauth_callback() const {
    return passed_reauth_callback_;
  }

  virtual void Start(const std::string& access_token,
                     const std::string& custom_user_agent,
                     const ReAuthenticateCallback& callback) OVERRIDE {
    *start_called_ = true;
    passed_access_token_ = access_token;
    passed_reauth_callback_ = callback;

    // This request class itself does not return any response at this point.
    // Each test case should respond properly by using the above methods.
  }

  virtual void Cancel() OVERRIDE {
    EXPECT_TRUE(*start_called_);
    *finish_reason_ = CANCEL;
    sender_->RequestFinished(this);
  }

  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE {
    *finish_reason_ = AUTH_FAILURE;
    sender_->RequestFinished(this);
  }

  virtual base::WeakPtr<AuthenticatedRequestInterface> GetWeakPtr() OVERRIDE {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  RequestSender* sender_;
  bool* start_called_;
  FinishReason* finish_reason_;
  std::string passed_access_token_;
  ReAuthenticateCallback passed_reauth_callback_;
  base::WeakPtrFactory<TestRequest> weak_ptr_factory_;
};

}  // namespace

TEST_F(RequestSenderTest, StartAndFinishRequest) {
  bool start_called  = false;
  FinishReason finish_reason = NONE;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &finish_reason);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());

  // Start is called with the specified access token. Let it succeed.
  EXPECT_TRUE(start_called);
  EXPECT_EQ(kTestAccessToken, request->passed_access_token());
  request->FinishRequestWithSuccess();
  EXPECT_FALSE(weak_ptr);  // The request object is deleted.

  // It is safe to run the cancel closure even after the request is finished.
  // It is just no-op. The TestRequest::Cancel method is not called.
  cancel_closure.Run();
  EXPECT_EQ(SUCCESS, finish_reason);
}

TEST_F(RequestSenderTest, StartAndCancelRequest) {
  bool start_called  = false;
  FinishReason finish_reason = NONE;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &finish_reason);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());
  EXPECT_TRUE(start_called);

  cancel_closure.Run();
  EXPECT_EQ(CANCEL, finish_reason);
  EXPECT_FALSE(weak_ptr);  // The request object is deleted.
}

TEST_F(RequestSenderTest, NoRefreshToken) {
  auth_service_->ClearRefreshToken();
  auth_service_->ClearAccessToken();

  bool start_called  = false;
  FinishReason finish_reason = NONE;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &finish_reason);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());

  // The request is not started at all because no access token is obtained.
  EXPECT_FALSE(start_called);
  EXPECT_EQ(AUTH_FAILURE, finish_reason);
  EXPECT_FALSE(weak_ptr);  // The request object is deleted.
}

TEST_F(RequestSenderTest, ValidRefreshTokenAndNoAccessToken) {
  auth_service_->ClearAccessToken();

  bool start_called  = false;
  FinishReason finish_reason = NONE;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &finish_reason);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());

  // Access token should indicate that this is the first retry.
  EXPECT_TRUE(start_called);
  EXPECT_EQ(kTestAccessToken + std::string("1"),
            request->passed_access_token());
  request->FinishRequestWithSuccess();
  EXPECT_EQ(SUCCESS, finish_reason);
  EXPECT_FALSE(weak_ptr);  // The request object is deleted.
}

TEST_F(RequestSenderTest, AccessTokenRejectedSeveralTimes) {
  bool start_called  = false;
  FinishReason finish_reason = NONE;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &finish_reason);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());

  EXPECT_TRUE(start_called);
  EXPECT_EQ(kTestAccessToken, request->passed_access_token());
  // Emulate the case that the access token was rejected by the remote service.
  request->passed_reauth_callback().Run(request);
  // New access token is fetched. Let it fail once again.
  EXPECT_EQ(kTestAccessToken + std::string("1"),
            request->passed_access_token());
  request->passed_reauth_callback().Run(request);
  // Once more.
  EXPECT_EQ(kTestAccessToken + std::string("2"),
            request->passed_access_token());
  request->passed_reauth_callback().Run(request);

  // Currently, limit for the retry is controlled in each request object, not
  // by the RequestSender. So with this TestRequest, RequestSender retries
  // infinitely. Let it succeed/
  EXPECT_EQ(kTestAccessToken + std::string("3"),
            request->passed_access_token());
  request->FinishRequestWithSuccess();
  EXPECT_EQ(SUCCESS, finish_reason);
  EXPECT_FALSE(weak_ptr);
}

}  // namespace google_apis
