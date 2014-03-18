// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/pnacl_host.h"

#include <stdio.h>
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/nacl/browser/pnacl_translation_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#define snprintf _snprintf
#endif

namespace pnacl {

class PnaclHostTest : public testing::Test {
 protected:
  PnaclHostTest()
      : host_(NULL),
        temp_callback_count_(0),
        write_callback_count_(0),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual void SetUp() {
    host_ = new PnaclHost();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    host_->InitForTest(temp_dir_.path());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(PnaclHost::CacheReady, host_->cache_state_);
  }
  virtual void TearDown() {
    EXPECT_EQ(0U, host_->pending_translations());
    // Give the host a chance to de-init the backend, and then delete it.
    host_->RendererClosing(0);
    FlushQueues();
    EXPECT_EQ(PnaclHost::CacheUninitialized, host_->cache_state_);
    delete host_;
  }
  // Flush the blocking pool first, then any tasks it posted to the IO thread.
  // Do 2 rounds of flushing, because some operations require 2 trips back and
  // forth between the threads.
  void FlushQueues() {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }
  int GetCacheSize() { return host_->disk_cache_->Size(); }
  int CacheIsInitialized() {
    return host_->cache_state_ == PnaclHost::CacheReady;
  }
  void ReInitBackend() {
    host_->InitForTest(temp_dir_.path());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(PnaclHost::CacheReady, host_->cache_state_);
  }

 public:  // Required for derived classes to bind this method
          // Callbacks used by tests which call GetNexeFd.
  // CallbackExpectMiss checks that the fd is valid and a miss is reported,
  // and also writes some data into the file, which is read back by
  // CallbackExpectHit
  void CallbackExpectMiss(base::PlatformFile fd, bool is_hit) {
    EXPECT_FALSE(is_hit);
    ASSERT_FALSE(fd == base::kInvalidPlatformFileValue);
    base::PlatformFileInfo info;
    EXPECT_TRUE(base::GetPlatformFileInfo(fd, &info));
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(0LL, info.size);
    char str[16];
    memset(str, 0x0, 16);
    snprintf(str, 16, "testdata%d", ++write_callback_count_);
    EXPECT_EQ(16, base::WritePlatformFile(fd, 0, str, 16));
    temp_callback_count_++;
  }
  void CallbackExpectHit(base::PlatformFile fd, bool is_hit) {
    EXPECT_TRUE(is_hit);
    ASSERT_FALSE(fd == base::kInvalidPlatformFileValue);
    base::PlatformFileInfo info;
    EXPECT_TRUE(base::GetPlatformFileInfo(fd, &info));
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(16LL, info.size);
    char data[16];
    memset(data, 0x0, 16);
    char str[16];
    memset(str, 0x0, 16);
    snprintf(str, 16, "testdata%d", write_callback_count_);
    EXPECT_EQ(16, base::ReadPlatformFile(fd, 0, data, 16));
    EXPECT_STREQ(str, data);
    temp_callback_count_++;
  }

 protected:
  PnaclHost* host_;
  int temp_callback_count_;
  int write_callback_count_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
};

static nacl::PnaclCacheInfo GetTestCacheInfo() {
  nacl::PnaclCacheInfo info;
  info.pexe_url = GURL("http://www.google.com");
  info.abi_version = 0;
  info.opt_level = 0;
  info.has_no_store_header = false;
  return info;
}

#define GET_NEXE_FD(renderer, instance, incognito, info, expect_hit) \
  do {                                                               \
    SCOPED_TRACE("");                                                \
    host_->GetNexeFd(                                                \
        renderer,                                                    \
        0, /* ignore render_view_id for now */                       \
        instance,                                                    \
        incognito,                                                   \
        info,                                                        \
        base::Bind(expect_hit ? &PnaclHostTest::CallbackExpectHit    \
                              : &PnaclHostTest::CallbackExpectMiss,  \
                   base::Unretained(this)));                         \
  } while (0)

TEST_F(PnaclHostTest, BasicMiss) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Test cold miss.
  GET_NEXE_FD(0, 0, false, info, false);
  EXPECT_EQ(1U, host_->pending_translations());
  FlushQueues();
  EXPECT_EQ(1U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  // Test that a different cache info field also misses.
  info.etag = std::string("something else");
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(1U, host_->pending_translations());
  host_->RendererClosing(0);
  FlushQueues();
  // Check that the cache has de-initialized after the last renderer goes away.
  EXPECT_FALSE(CacheIsInitialized());
}

TEST_F(PnaclHostTest, BadArguments) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  EXPECT_EQ(1U, host_->pending_translations());
  host_->TranslationFinished(0, 1, true);  // nonexistent translation
  EXPECT_EQ(1U, host_->pending_translations());
  host_->RendererClosing(1);  // nonexistent renderer
  EXPECT_EQ(1U, host_->pending_translations());
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  host_->RendererClosing(0);  // close without finishing
}

TEST_F(PnaclHostTest, BasicHit) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  GET_NEXE_FD(0, 1, false, info, true);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, TranslationErrors) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Early abort, before temp file request returns
  host_->TranslationFinished(0, 0, false);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  EXPECT_EQ(0, temp_callback_count_);
  // The backend will have been freed when the query comes back and there
  // are no pending translations.
  EXPECT_FALSE(CacheIsInitialized());
  ReInitBackend();
  // Check that another request for the same info misses successfully.
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());

  // Now try sending the error after the temp file request returns
  info.abi_version = 222;
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, false);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  // Check another successful miss
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(3, temp_callback_count_);
  host_->TranslationFinished(0, 0, false);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedMissesAfterTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(1U, host_->pending_translations());
  // Test that a second request for the same nexe while the first one is still
  // outstanding eventually hits.
  GET_NEXE_FD(0, 1, false, info, true);
  FlushQueues();
  EXPECT_EQ(2U, host_->pending_translations());
  // The temp file should not be returned to the second request until after the
  // first is finished translating.
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedMissesBeforeTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Send the 2nd fd request before the first one returns a temp file.
  GET_NEXE_FD(0, 1, false, info, true);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(2U, host_->pending_translations());
  FlushQueues();
  EXPECT_EQ(2U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedHitsBeforeTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Store one in the cache and complete it.
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  GET_NEXE_FD(0, 0, false, info, true);
  // Request the second before the first temp file returns.
  GET_NEXE_FD(0, 1, false, info, true);
  FlushQueues();
  EXPECT_EQ(3, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedHitsAfterTempReturn) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Store one in the cache and complete it.
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  GET_NEXE_FD(0, 0, false, info, true);
  FlushQueues();
  GET_NEXE_FD(0, 1, false, info, true);
  FlushQueues();
  EXPECT_EQ(3, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

TEST_F(PnaclHostTest, OverlappedMissesRendererClosing) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Send the 2nd fd request from a different renderer.
  // Test that it eventually gets an fd after the first renderer closes.
  GET_NEXE_FD(1, 1, false, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(2U, host_->pending_translations());
  FlushQueues();
  EXPECT_EQ(2U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->RendererClosing(0);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(1U, host_->pending_translations());
  host_->RendererClosing(1);
}

TEST_F(PnaclHostTest, Incognito) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, true, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  // Check that an incognito translation is not stored in the cache
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  // Check that an incognito translation can hit from a normal one.
  GET_NEXE_FD(0, 0, true, info, true);
  FlushQueues();
  EXPECT_EQ(3, temp_callback_count_);
}

TEST_F(PnaclHostTest, IncognitoOverlappedMiss) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, true, info, false);
  GET_NEXE_FD(0, 1, false, info, false);
  FlushQueues();
  // Check that both translations have returned misses, (i.e. that the
  // second one has not blocked on the incognito one)
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  host_->TranslationFinished(0, 1, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());

  // Same test, but issue the 2nd request after the first has returned a miss.
  info.abi_version = 222;
  GET_NEXE_FD(0, 0, true, info, false);
  FlushQueues();
  EXPECT_EQ(3, temp_callback_count_);
  GET_NEXE_FD(0, 1, false, info, false);
  FlushQueues();
  EXPECT_EQ(4, temp_callback_count_);
  host_->RendererClosing(0);
}

TEST_F(PnaclHostTest, IncognitoSecondOverlappedMiss) {
  // If the non-incognito request comes first, it should
  // behave exactly like OverlappedMissBeforeTempReturn
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  GET_NEXE_FD(0, 0, false, info, false);
  // Send the 2nd fd request before the first one returns a temp file.
  GET_NEXE_FD(0, 1, true, info, true);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  EXPECT_EQ(2U, host_->pending_translations());
  FlushQueues();
  EXPECT_EQ(2U, host_->pending_translations());
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  EXPECT_EQ(0U, host_->pending_translations());
}

// Test that pexes with the no-store header do not get cached.
TEST_F(PnaclHostTest, CacheControlNoStore) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  info.has_no_store_header = true;
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(1, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  EXPECT_EQ(0, GetCacheSize());
}

// Test that no-store pexes do not wait, but do duplicate translations
TEST_F(PnaclHostTest, NoStoreOverlappedMiss) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  info.has_no_store_header = true;
  GET_NEXE_FD(0, 0, false, info, false);
  GET_NEXE_FD(0, 1, false, info, false);
  FlushQueues();
  // Check that both translations have returned misses, (i.e. that the
  // second one has not blocked on the first one)
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  host_->TranslationFinished(0, 1, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());

  // Same test, but issue the 2nd request after the first has returned a miss.
  info.abi_version = 222;
  GET_NEXE_FD(0, 0, false, info, false);
  FlushQueues();
  EXPECT_EQ(3, temp_callback_count_);
  GET_NEXE_FD(0, 1, false, info, false);
  FlushQueues();
  EXPECT_EQ(4, temp_callback_count_);
  host_->RendererClosing(0);
}

TEST_F(PnaclHostTest, ClearTranslationCache) {
  nacl::PnaclCacheInfo info = GetTestCacheInfo();
  // Add 2 entries in the cache
  GET_NEXE_FD(0, 0, false, info, false);
  info.abi_version = 222;
  GET_NEXE_FD(0, 1, false, info, false);
  FlushQueues();
  EXPECT_EQ(2, temp_callback_count_);
  host_->TranslationFinished(0, 0, true);
  host_->TranslationFinished(0, 1, true);
  FlushQueues();
  EXPECT_EQ(0U, host_->pending_translations());
  EXPECT_EQ(2, GetCacheSize());
  net::TestCompletionCallback cb;
  // Since we are using a memory backend, the clear should happen immediately.
  host_->ClearTranslationCacheEntriesBetween(
      base::Time(), base::Time(), base::Bind(cb.callback(), 0));
  // Check that the translation cache has been cleared before flushing the
  // queues, because the backend will be freed once it is.
  EXPECT_EQ(0, GetCacheSize());
  EXPECT_EQ(0, cb.GetResult(net::ERR_IO_PENDING));
  // Now check that the backend has been freed.
  EXPECT_FALSE(CacheIsInitialized());
}

}  // namespace pnacl
