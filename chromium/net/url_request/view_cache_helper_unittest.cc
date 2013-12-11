// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/view_cache_helper.h"

#include "base/pickle.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext();
  virtual ~TestURLRequestContext() {}

  // Gets a pointer to the cache backend.
  disk_cache::Backend* GetBackend();

 private:
  HttpCache cache_;
};

TestURLRequestContext::TestURLRequestContext()
    : cache_(reinterpret_cast<HttpTransactionFactory*>(NULL), NULL,
             HttpCache::DefaultBackend::InMemory(0)) {
  set_http_transaction_factory(&cache_);
}

void WriteHeaders(disk_cache::Entry* entry, int flags,
                  const std::string& data) {
  if (data.empty())
    return;

  Pickle pickle;
  pickle.WriteInt(flags | 1);  // Version 1.
  pickle.WriteInt64(0);
  pickle.WriteInt64(0);
  pickle.WriteString(data);

  scoped_refptr<WrappedIOBuffer> buf(new WrappedIOBuffer(
      reinterpret_cast<const char*>(pickle.data())));
  int len = static_cast<int>(pickle.size());

  net::TestCompletionCallback cb;
  int rv = entry->WriteData(0, 0, buf.get(), len, cb.callback(), true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteData(disk_cache::Entry* entry, int index, const std::string& data) {
  if (data.empty())
    return;

  int len = data.length();
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  memcpy(buf->data(), data.data(), data.length());

  net::TestCompletionCallback cb;
  int rv = entry->WriteData(index, 0, buf.get(), len, cb.callback(), true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteToEntry(disk_cache::Backend* cache, const std::string& key,
                  const std::string& data0, const std::string& data1,
                  const std::string& data2) {
  net::TestCompletionCallback cb;
  disk_cache::Entry* entry;
  int rv = cache->CreateEntry(key, &entry, cb.callback());
  rv = cb.GetResult(rv);
  if (rv != OK) {
    rv = cache->OpenEntry(key, &entry, cb.callback());
    ASSERT_EQ(OK, cb.GetResult(rv));
  }

  WriteHeaders(entry, 0, data0);
  WriteData(entry, 1, data1);
  WriteData(entry, 2, data2);

  entry->Close();
}

void FillCache(URLRequestContext* context) {
  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv =
      context->http_transaction_factory()->GetCache()->GetBackend(
          &cache, cb.callback());
  ASSERT_EQ(OK, cb.GetResult(rv));

  std::string empty;
  WriteToEntry(cache, "first", "some", empty, empty);
  WriteToEntry(cache, "second", "only hex_dumped", "same", "kind");
  WriteToEntry(cache, "third", empty, "another", "thing");
}

}  // namespace.

TEST(ViewCacheHelper, EmptyCache) {
  TestURLRequestContext context;
  ViewCacheHelper helper;

  TestCompletionCallback cb;
  std::string prefix, data;
  int rv = helper.GetContentsHTML(&context, prefix, &data, cb.callback());
  EXPECT_EQ(OK, cb.GetResult(rv));
  EXPECT_FALSE(data.empty());
}

TEST(ViewCacheHelper, ListContents) {
  TestURLRequestContext context;
  ViewCacheHelper helper;

  FillCache(&context);

  std::string prefix, data;
  TestCompletionCallback cb;
  int rv = helper.GetContentsHTML(&context, prefix, &data, cb.callback());
  EXPECT_EQ(OK, cb.GetResult(rv));

  EXPECT_EQ(0U, data.find("<html>"));
  EXPECT_NE(std::string::npos, data.find("</html>"));
  EXPECT_NE(std::string::npos, data.find("first"));
  EXPECT_NE(std::string::npos, data.find("second"));
  EXPECT_NE(std::string::npos, data.find("third"));

  EXPECT_EQ(std::string::npos, data.find("some"));
  EXPECT_EQ(std::string::npos, data.find("same"));
  EXPECT_EQ(std::string::npos, data.find("thing"));
}

TEST(ViewCacheHelper, DumpEntry) {
  TestURLRequestContext context;
  ViewCacheHelper helper;

  FillCache(&context);

  std::string data;
  TestCompletionCallback cb;
  int rv = helper.GetEntryInfoHTML("second", &context, &data, cb.callback());
  EXPECT_EQ(OK, cb.GetResult(rv));

  EXPECT_EQ(0U, data.find("<html>"));
  EXPECT_NE(std::string::npos, data.find("</html>"));

  EXPECT_NE(std::string::npos, data.find("hex_dumped"));
  EXPECT_NE(std::string::npos, data.find("same"));
  EXPECT_NE(std::string::npos, data.find("kind"));

  EXPECT_EQ(std::string::npos, data.find("first"));
  EXPECT_EQ(std::string::npos, data.find("third"));
  EXPECT_EQ(std::string::npos, data.find("some"));
  EXPECT_EQ(std::string::npos, data.find("another"));
}

// Makes sure the links are correct.
TEST(ViewCacheHelper, Prefix) {
  TestURLRequestContext context;
  ViewCacheHelper helper;

  FillCache(&context);

  std::string key, data;
  std::string prefix("prefix:");
  TestCompletionCallback cb;
  int rv = helper.GetContentsHTML(&context, prefix, &data, cb.callback());
  EXPECT_EQ(OK, cb.GetResult(rv));

  EXPECT_EQ(0U, data.find("<html>"));
  EXPECT_NE(std::string::npos, data.find("</html>"));
  EXPECT_NE(std::string::npos, data.find("<a href=\"prefix:first\">"));
  EXPECT_NE(std::string::npos, data.find("<a href=\"prefix:second\">"));
  EXPECT_NE(std::string::npos, data.find("<a href=\"prefix:third\">"));
}

TEST(ViewCacheHelper, TruncatedFlag) {
  TestURLRequestContext context;
  ViewCacheHelper helper;

  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv =
      context.http_transaction_factory()->GetCache()->GetBackend(
          &cache, cb.callback());
  ASSERT_EQ(OK, cb.GetResult(rv));

  std::string key("the key");
  disk_cache::Entry* entry;
  rv = cache->CreateEntry(key, &entry, cb.callback());
  ASSERT_EQ(OK, cb.GetResult(rv));

  // RESPONSE_INFO_TRUNCATED defined on response_info.cc
  int flags = 1 << 12;
  WriteHeaders(entry, flags, "something");
  entry->Close();

  std::string data;
  TestCompletionCallback cb1;
  rv = helper.GetEntryInfoHTML(key, &context, &data, cb1.callback());
  EXPECT_EQ(OK, cb1.GetResult(rv));

  EXPECT_NE(std::string::npos, data.find("RESPONSE_INFO_TRUNCATED"));
}

}  // namespace net
