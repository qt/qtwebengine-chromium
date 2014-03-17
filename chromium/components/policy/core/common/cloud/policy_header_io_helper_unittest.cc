// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_simple_task_runner.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
const char kDMServerURL[] = "http://server_url";
const char kPolicyHeaderName[] = "Chrome-Policy-Posture";
const char kInitialPolicyHeader[] = "initial_header";

class PolicyHeaderIOHelperTest : public testing::Test {
 public:
  PolicyHeaderIOHelperTest() {
    task_runner_ = make_scoped_refptr(new base::TestSimpleTaskRunner());
  }
  virtual ~PolicyHeaderIOHelperTest() {}

  virtual void SetUp() OVERRIDE {
    helper_ = make_scoped_ptr(new PolicyHeaderIOHelper(kDMServerURL,
                                                       kInitialPolicyHeader,
                                                       task_runner_));
    task_runner_->RunUntilIdle();
  }
  virtual void TearDown() OVERRIDE {
    task_runner_->RunUntilIdle();
    helper_.reset();
  }

  void ValidateHeader(const net::HttpRequestHeaders& headers,
                      const std::string& expected) {
    std::string header;
    EXPECT_TRUE(headers.GetHeader(kPolicyHeaderName, &header));
    EXPECT_EQ(header, expected);
  }

  base::MessageLoop loop_;
  scoped_ptr<PolicyHeaderIOHelper> helper_;
  net::TestURLRequestContext context_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

}  // namespace

TEST_F(PolicyHeaderIOHelperTest, InitialHeader) {
  net::TestURLRequest request(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context_);
  helper_->AddPolicyHeaders(&request);
  ValidateHeader(request.extra_request_headers(), kInitialPolicyHeader);
}

TEST_F(PolicyHeaderIOHelperTest, NoHeaderOnNonMatchingURL) {
  net::TestURLRequest request(
      GURL("http://non-matching.com"), net::DEFAULT_PRIORITY, NULL, &context_);
  helper_->AddPolicyHeaders(&request);
  EXPECT_TRUE(request.extra_request_headers().IsEmpty());
}

TEST_F(PolicyHeaderIOHelperTest, HeaderChange) {
  std::string new_header = "new_header";
  helper_->UpdateHeader(new_header);
  task_runner_->RunUntilIdle();
  net::TestURLRequest request(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context_);
  helper_->AddPolicyHeaders(&request);
  ValidateHeader(request.extra_request_headers(), new_header);
}

TEST_F(PolicyHeaderIOHelperTest, ChangeToNoHeader) {
  helper_->UpdateHeader("");
  task_runner_->RunUntilIdle();
  net::TestURLRequest request(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context_);
  helper_->AddPolicyHeaders(&request);
  EXPECT_TRUE(request.extra_request_headers().IsEmpty());
}

}  // namespace policy
