// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/common/database/database_identifier.h"

namespace content {

class IndexedDBTest : public testing::Test {
 public:
  const GURL kNormalOrigin;
  const GURL kSessionOnlyOrigin;

  IndexedDBTest()
      : kNormalOrigin("http://normal/"),
        kSessionOnlyOrigin("http://session-only/"),
        message_loop_(base::MessageLoop::TYPE_IO),
        task_runner_(new base::TestSimpleTaskRunner),
        special_storage_policy_(new quota::MockSpecialStoragePolicy),
        file_thread_(BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    special_storage_policy_->AddSessionOnly(kSessionOnlyOrigin);
  }

 protected:
  void FlushIndexedDBTaskRunner() { task_runner_->RunUntilIdle(); }

  base::MessageLoop message_loop_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy_;

 private:
  BrowserThreadImpl file_thread_;
  BrowserThreadImpl io_thread_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTest);
};

TEST_F(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context
  // which should trigger the clean up.
  {
    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.path(), special_storage_policy_, NULL, task_runner_);

    normal_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kNormalOrigin));
    session_only_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kSessionOnlyOrigin));
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    FlushIndexedDBTaskRunner();
    message_loop_.RunUntilIdle();
  }

  FlushIndexedDBTaskRunner();
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_FALSE(base::DirectoryExists(session_only_path));
}

TEST_F(IndexedDBTest, SetForceKeepSessionState) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath normal_path;
  base::FilePath session_only_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    // Create some indexedDB paths.
    // With the levelDB backend, these are directories.
    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.path(), special_storage_policy_, NULL, task_runner_);

    // Save session state. This should bypass the destruction-time deletion.
    idb_context->SetForceKeepSessionState();

    normal_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kNormalOrigin));
    session_only_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kSessionOnlyOrigin));
    ASSERT_TRUE(base::CreateDirectory(normal_path));
    ASSERT_TRUE(base::CreateDirectory(session_only_path));
    message_loop_.RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  message_loop_.RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path));
  EXPECT_TRUE(base::DirectoryExists(session_only_path));
}

class MockConnection : public IndexedDBConnection {
 public:
  explicit MockConnection(bool expect_force_close)
      : IndexedDBConnection(NULL, NULL),
        expect_force_close_(expect_force_close),
        force_close_called_(false) {}

  virtual ~MockConnection() {
    EXPECT_TRUE(force_close_called_ == expect_force_close_);
  }

  virtual void ForceClose() OVERRIDE {
    ASSERT_TRUE(expect_force_close_);
    force_close_called_ = true;
  }

  virtual bool IsConnected() OVERRIDE {
    return !force_close_called_;
  }

 private:
  bool expect_force_close_;
  bool force_close_called_;
};

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnDelete) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_path;

  // Create the scope which will ensure we run the destructor of the context.
  {
    TestBrowserContext browser_context;

    const GURL kTestOrigin("http://test/");

    scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
        temp_dir.path(), special_storage_policy_, NULL, task_runner_);

    test_path = idb_context->GetFilePathForTesting(
        webkit_database::GetIdentifierFromOrigin(kTestOrigin));
    ASSERT_TRUE(base::CreateDirectory(test_path));

    const bool kExpectForceClose = true;

    MockConnection connection1(kExpectForceClose);
    idb_context->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&IndexedDBContextImpl::ConnectionOpened,
                   idb_context,
                   kTestOrigin,
                   &connection1));

    MockConnection connection2(!kExpectForceClose);
    idb_context->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&IndexedDBContextImpl::ConnectionOpened,
                   idb_context,
                   kTestOrigin,
                   &connection2));
    idb_context->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&IndexedDBContextImpl::ConnectionClosed,
                   idb_context,
                   kTestOrigin,
                   &connection2));

    idb_context->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(
            &IndexedDBContextImpl::DeleteForOrigin, idb_context, kTestOrigin));
    FlushIndexedDBTaskRunner();
    message_loop_.RunUntilIdle();
  }

  // Make sure we wait until the destructor has run.
  message_loop_.RunUntilIdle();

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, DeleteFailsIfDirectoryLocked) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const GURL kTestOrigin("http://test/");

  scoped_refptr<IndexedDBContextImpl> idb_context = new IndexedDBContextImpl(
      temp_dir.path(), special_storage_policy_, NULL, task_runner_);

  base::FilePath test_path = idb_context->GetFilePathForTesting(
      webkit_database::GetIdentifierFromOrigin(kTestOrigin));
  ASSERT_TRUE(base::CreateDirectory(test_path));

  scoped_ptr<LevelDBLock> lock =
      LevelDBDatabase::LockForTesting(test_path);
  ASSERT_TRUE(lock);

  idb_context->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &IndexedDBContextImpl::DeleteForOrigin, idb_context, kTestOrigin));
  FlushIndexedDBTaskRunner();

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_F(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailure) {
  const GURL kTestOrigin("http://test/");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<IndexedDBContextImpl> context = new IndexedDBContextImpl(
      temp_dir.path(), special_storage_policy_, NULL, task_runner_);

  scoped_refptr<IndexedDBFactory> factory = context->GetIDBFactory();

  scoped_refptr<MockIndexedDBCallbacks> callbacks(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> db_callbacks(
      new MockIndexedDBDatabaseCallbacks());
  const int64 transaction_id = 1;
  factory->Open(ASCIIToUTF16("db"),
                IndexedDBDatabaseMetadata::DEFAULT_INT_VERSION,
                transaction_id,
                callbacks,
                db_callbacks,
                kTestOrigin,
                temp_dir.path());

  EXPECT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context->ConnectionOpened(kTestOrigin, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(kTestOrigin));

  // Simulate the write failure.
  callbacks->connection()->database()->TransactionCommitFailed();

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(kTestOrigin));
}

}  // namespace content
