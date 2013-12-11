// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "net/http/http_transaction_unittest.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is a header that signals the end of the data.
const char kGzipGata[] = "\x1f\x08b\x08\0\0\0\0\0\0\3\3\0\0\0\0\0\0\0\0";

void GZipServer(const net::HttpRequestInfo* request,
                std::string* response_status, std::string* response_headers,
                std::string* response_data) {
  response_data->assign(kGzipGata, sizeof(kGzipGata));
}

const MockTransaction kGZip_Transaction = {
  "http://www.google.com/gzyp",
  "GET",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n"
  "Content-Encoding: gzip\n"
  "Content-Length: 30\n",  // Intentionally wrong.
  base::Time(),
  "",
  TEST_MODE_NORMAL,
  &GZipServer,
  0,
  net::OK
};

const MockTransaction kRedirect_Transaction = {
  "http://www.google.com/redirect",
  "GET",
  base::Time(),
  "",
  net::LOAD_NORMAL,
  "HTTP/1.1 302 Found",
  "Cache-Control: max-age=10000\n"
  "Location: http://www.google.com/destination\n"
  "Content-Length: 5\n",
  base::Time(),
  "hello",
  TEST_MODE_NORMAL,
  NULL,
  0,
  net::OK
};

}  // namespace

TEST(URLRequestJob, TransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  net::TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  net::TestDelegate d;
  net::TestURLRequest req(GURL(kGZip_Transaction.url), &d, &context, NULL);
  AddMockTransaction(&kGZip_Transaction);

  req.set_method("GET");
  req.Start();

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kGZip_Transaction);
}

TEST(URLRequestJob, SyncTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  net::TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  net::TestDelegate d;
  net::TestURLRequest req(GURL(kGZip_Transaction.url), &d, &context, NULL);
  MockTransaction transaction(kGZip_Transaction);
  transaction.test_mode = TEST_MODE_SYNC_ALL;
  AddMockTransaction(&transaction);

  req.set_method("GET");
  req.Start();

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&transaction);
}

TEST(URLRequestJob, RedirectTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  net::TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  net::TestDelegate d;
  net::TestURLRequest req(GURL(kRedirect_Transaction.url), &d, &context, NULL);
  AddMockTransaction(&kRedirect_Transaction);

  req.set_method("GET");
  req.Start();

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kRedirect_Transaction);
}
