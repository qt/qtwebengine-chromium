// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

URLRequestTestJob* job_a;

URLRequestJob* FactoryA(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& scheme) {
  job_a = new URLRequestTestJob(request, network_delegate);
  return job_a;
}

URLRequestTestJob* job_b;

URLRequestJob* FactoryB(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const std::string& scheme) {
  job_b = new URLRequestTestJob(request, network_delegate);
  return job_b;
}

URLRequestTestJob* job_c;

class TestProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  virtual ~TestProtocolHandler() {}

  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    job_c = new URLRequestTestJob(request, network_delegate);
    return job_c;
  }
};

TEST(URLRequestFilter, BasicMatching) {
  TestDelegate delegate;
  TestURLRequestContext request_context;

  GURL url_1("http://foo.com/");
  TestURLRequest request_1(
      url_1, DEFAULT_PRIORITY, &delegate, &request_context);

  GURL url_2("http://bar.com/");
  TestURLRequest request_2(
      url_2, DEFAULT_PRIORITY, &delegate, &request_context);

  // Check AddUrlHandler checks for invalid URLs.
  EXPECT_FALSE(URLRequestFilter::GetInstance()->AddUrlHandler(GURL(),
                                                              &FactoryA));

  // Check URL matching.
  URLRequestFilter::GetInstance()->ClearHandlers();
  EXPECT_TRUE(URLRequestFilter::GetInstance()->AddUrlHandler(url_1,
                                                             &FactoryA));
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, NULL, url_1.scheme());
    EXPECT_EQ(job_a, found);
    EXPECT_TRUE(job_a != NULL);
    job_a = NULL;
  }
  EXPECT_EQ(URLRequestFilter::GetInstance()->hit_count(), 1);

  // Check we don't match other URLs.
  EXPECT_TRUE(URLRequestFilter::Factory(
      &request_2, NULL, url_2.scheme()) == NULL);
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check we can remove URL matching.
  URLRequestFilter::GetInstance()->RemoveUrlHandler(url_1);
  EXPECT_TRUE(URLRequestFilter::Factory(
      &request_1, NULL, url_1.scheme()) == NULL);
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check hostname matching.
  URLRequestFilter::GetInstance()->ClearHandlers();
  EXPECT_EQ(0, URLRequestFilter::GetInstance()->hit_count());
  URLRequestFilter::GetInstance()->AddHostnameHandler(url_1.scheme(),
                                                      url_1.host(),
                                                      &FactoryB);
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, NULL, url_1.scheme());
    EXPECT_EQ(job_b, found);
    EXPECT_TRUE(job_b != NULL);
    job_b = NULL;
  }
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check we don't match other hostnames.
  EXPECT_TRUE(URLRequestFilter::Factory(
      &request_2, NULL, url_2.scheme()) == NULL);
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check we can remove hostname matching.
  URLRequestFilter::GetInstance()->RemoveHostnameHandler(url_1.scheme(),
                                                         url_1.host());
  EXPECT_TRUE(URLRequestFilter::Factory(
      &request_1, NULL, url_1.scheme()) == NULL);
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check ProtocolHandler hostname matching.
  URLRequestFilter::GetInstance()->ClearHandlers();
  EXPECT_EQ(0, URLRequestFilter::GetInstance()->hit_count());
  URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
      url_1.scheme(), url_1.host(),
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(
          new TestProtocolHandler()));
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_1, NULL, url_1.scheme());
    EXPECT_EQ(job_c, found);
    EXPECT_TRUE(job_c != NULL);
    job_c = NULL;
  }
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  // Check ProtocolHandler URL matching.
  URLRequestFilter::GetInstance()->ClearHandlers();
  EXPECT_EQ(0, URLRequestFilter::GetInstance()->hit_count());
  URLRequestFilter::GetInstance()->AddUrlProtocolHandler(
      url_2,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(
          new TestProtocolHandler()));
  {
    scoped_refptr<URLRequestJob> found = URLRequestFilter::Factory(
        &request_2, NULL, url_2.scheme());
    EXPECT_EQ(job_c, found);
    EXPECT_TRUE(job_c != NULL);
    job_c = NULL;
  }
  EXPECT_EQ(1, URLRequestFilter::GetInstance()->hit_count());

  URLRequestFilter::GetInstance()->ClearHandlers();
}

}  // namespace

}  // namespace net
