// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"
#include "webkit/browser/database/database_tracker.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/database/database_identifier.h"

namespace {

const char kOrigin1Url[] = "http://origin1";
const char kOrigin2Url[] = "http://protected_origin2";

class TestObserver : public webkit_database::DatabaseTracker::Observer {
 public:
  TestObserver()
      : new_notification_received_(false),
        observe_size_changes_(true),
        observe_scheduled_deletions_(true) {
  }
  TestObserver(bool observe_size_changes, bool observe_scheduled_deletions)
      : new_notification_received_(false),
        observe_size_changes_(observe_size_changes),
        observe_scheduled_deletions_(observe_scheduled_deletions) {
  }

  virtual ~TestObserver() {}
  virtual void OnDatabaseSizeChanged(const std::string& origin_identifier,
                                     const base::string16& database_name,
                                     int64 database_size) OVERRIDE {
    if (!observe_size_changes_)
      return;
    new_notification_received_ = true;
    origin_identifier_ = origin_identifier;
    database_name_ = database_name;
    database_size_ = database_size;
  }
  virtual void OnDatabaseScheduledForDeletion(
      const std::string& origin_identifier,
      const base::string16& database_name) OVERRIDE {
    if (!observe_scheduled_deletions_)
      return;
    new_notification_received_ = true;
    origin_identifier_ = origin_identifier;
    database_name_ = database_name;
  }
  bool DidReceiveNewNotification() {
    bool temp_new_notification_received = new_notification_received_;
    new_notification_received_ = false;
    return temp_new_notification_received;
  }
  std::string GetNotificationOriginIdentifier() {
    return origin_identifier_;
  }
  base::string16 GetNotificationDatabaseName() { return database_name_; }
  int64 GetNotificationDatabaseSize() { return database_size_; }

 private:
  bool new_notification_received_;
  bool observe_size_changes_;
  bool observe_scheduled_deletions_;
  std::string origin_identifier_;
  base::string16 database_name_;
  int64 database_size_;
};

void CheckNotificationReceived(TestObserver* observer,
                               const std::string& expected_origin_identifier,
                               const base::string16& expected_database_name,
                               int64 expected_database_size) {
  EXPECT_TRUE(observer->DidReceiveNewNotification());
  EXPECT_EQ(expected_origin_identifier,
            observer->GetNotificationOriginIdentifier());
  EXPECT_EQ(expected_database_name,
            observer->GetNotificationDatabaseName());
  EXPECT_EQ(expected_database_size,
            observer->GetNotificationDatabaseSize());
}

class TestQuotaManagerProxy : public quota::QuotaManagerProxy {
 public:
  TestQuotaManagerProxy()
      : QuotaManagerProxy(NULL, NULL),
        registered_client_(NULL) {
  }

  virtual void RegisterClient(quota::QuotaClient* client) OVERRIDE {
    EXPECT_FALSE(registered_client_);
    registered_client_ = client;
  }

  virtual void NotifyStorageAccessed(quota::QuotaClient::ID client_id,
                                     const GURL& origin,
                                     quota::StorageType type) OVERRIDE {
    EXPECT_EQ(quota::QuotaClient::kDatabase, client_id);
    EXPECT_EQ(quota::kStorageTypeTemporary, type);
    accesses_[origin] += 1;
  }

  virtual void NotifyStorageModified(quota::QuotaClient::ID client_id,
                                     const GURL& origin,
                                     quota::StorageType type,
                                     int64 delta) OVERRIDE {
    EXPECT_EQ(quota::QuotaClient::kDatabase, client_id);
    EXPECT_EQ(quota::kStorageTypeTemporary, type);
    modifications_[origin].first += 1;
    modifications_[origin].second += delta;
  }

  // Not needed for our tests.
  virtual void NotifyOriginInUse(const GURL& origin) OVERRIDE {}
  virtual void NotifyOriginNoLongerInUse(const GURL& origin) OVERRIDE {}

  void SimulateQuotaManagerDestroyed() {
    if (registered_client_) {
      registered_client_->OnQuotaManagerDestroyed();
      registered_client_ = NULL;
    }
  }

  bool WasAccessNotified(const GURL& origin) {
    return accesses_[origin] != 0;
  }

  bool WasModificationNotified(const GURL& origin, int64 amount) {
    return modifications_[origin].first != 0 &&
           modifications_[origin].second == amount;
  }

  void reset() {
    accesses_.clear();
    modifications_.clear();
  }

  quota::QuotaClient* registered_client_;

  // Map from origin to count of access notifications.
  std::map<GURL, int> accesses_;

  // Map from origin to <count, sum of deltas>
  std::map<GURL, std::pair<int, int64> > modifications_;

 protected:
  virtual ~TestQuotaManagerProxy() {
    EXPECT_FALSE(registered_client_);
  }
};


bool EnsureFileOfSize(const base::FilePath& file_path, int64 length) {
  base::PlatformFileError error_code(base::PLATFORM_FILE_ERROR_FAILED);
  base::PlatformFile file =
      base::CreatePlatformFile(
          file_path,
          base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE,
          NULL,
          &error_code);
  if (error_code != base::PLATFORM_FILE_OK)
    return false;
  if (!base::TruncatePlatformFile(file, length))
    error_code = base::PLATFORM_FILE_ERROR_FAILED;
  base::ClosePlatformFile(file);
  return error_code == base::PLATFORM_FILE_OK;
}

}  // namespace

namespace webkit_database {

// We declare a helper class, and make it a friend of DatabaseTracker using
// the FRIEND_TEST() macro, and we implement all tests we want to run as
// static methods of this class. Then we make our TEST() targets call these
// static functions. This allows us to run each test in normal mode and
// incognito mode without writing the same code twice.
class DatabaseTracker_TestHelper_Test {
 public:
  static void TestDeleteOpenDatabase(bool incognito_mode) {
    // Initialize the tracker database.
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
        new quota::MockSpecialStoragePolicy;
    special_storage_policy->AddProtected(GURL(kOrigin2Url));
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(),
                            incognito_mode,
                            special_storage_policy.get(),
                            NULL,
                            NULL));

    // Create and open three databases.
    int64 database_size = 0;
    const std::string kOrigin1 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin1Url));
    const std::string kOrigin2 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin2Url));
    const base::string16 kDB1 = ASCIIToUTF16("db1");
    const base::string16 kDB2 = ASCIIToUTF16("db2");
    const base::string16 kDB3 = ASCIIToUTF16("db3");
    const base::string16 kDescription = ASCIIToUTF16("database_description");

    tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                            &database_size);
    tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, 0,
                            &database_size);
    tracker->DatabaseOpened(kOrigin2, kDB3, kDescription, 0,
                            &database_size);

    EXPECT_TRUE(file_util::CreateDirectory(
        tracker->DatabaseDirectory().Append(base::FilePath::FromUTF16Unsafe(
            tracker->GetOriginDirectory(kOrigin1)))));
    EXPECT_TRUE(file_util::CreateDirectory(
        tracker->DatabaseDirectory().Append(base::FilePath::FromUTF16Unsafe(
            tracker->GetOriginDirectory(kOrigin2)))));
    EXPECT_EQ(1, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin1, kDB1), "a", 1));
    EXPECT_EQ(2, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin2, kDB2), "aa", 2));
    EXPECT_EQ(3, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin2, kDB3), "aaa", 3));
    tracker->DatabaseModified(kOrigin1, kDB1);
    tracker->DatabaseModified(kOrigin2, kDB2);
    tracker->DatabaseModified(kOrigin2, kDB3);

    // Delete db1. Should also delete origin1.
    TestObserver observer;
    tracker->AddObserver(&observer);
    net::TestCompletionCallback callback;
    int result = tracker->DeleteDatabase(kOrigin1, kDB1, callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_FALSE(callback.have_result());
    EXPECT_TRUE(observer.DidReceiveNewNotification());
    EXPECT_EQ(kOrigin1, observer.GetNotificationOriginIdentifier());
    EXPECT_EQ(kDB1, observer.GetNotificationDatabaseName());
    tracker->DatabaseClosed(kOrigin1, kDB1);
    result = callback.GetResult(result);
    EXPECT_EQ(net::OK, result);
    EXPECT_FALSE(base::PathExists(
          tracker->DatabaseDirectory().AppendASCII(kOrigin1)));

    // Recreate db1.
    tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                            &database_size);
    EXPECT_TRUE(file_util::CreateDirectory(
        tracker->DatabaseDirectory().Append(base::FilePath::FromUTF16Unsafe(
            tracker->GetOriginDirectory(kOrigin1)))));
    EXPECT_EQ(1, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin1, kDB1), "a", 1));
    tracker->DatabaseModified(kOrigin1, kDB1);

    // Setup file modification times.  db1 and db2 are modified now, db3 three
    // days ago.
    EXPECT_TRUE(file_util::SetLastModifiedTime(
        tracker->GetFullDBFilePath(kOrigin1, kDB1), base::Time::Now()));
    EXPECT_TRUE(file_util::SetLastModifiedTime(
        tracker->GetFullDBFilePath(kOrigin2, kDB2), base::Time::Now()));
    base::Time three_days_ago = base::Time::Now();
    three_days_ago -= base::TimeDelta::FromDays(3);
    EXPECT_TRUE(file_util::SetLastModifiedTime(
        tracker->GetFullDBFilePath(kOrigin2, kDB3), three_days_ago));

    // Delete databases modified since yesterday. db2 is whitelisted.
    base::Time yesterday = base::Time::Now();
    yesterday -= base::TimeDelta::FromDays(1);
    result = tracker->DeleteDataModifiedSince(
        yesterday, callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, result);
    ASSERT_FALSE(callback.have_result());
    EXPECT_TRUE(observer.DidReceiveNewNotification());
    tracker->DatabaseClosed(kOrigin1, kDB1);
    tracker->DatabaseClosed(kOrigin2, kDB2);
    result = callback.GetResult(result);
    EXPECT_EQ(net::OK, result);
    EXPECT_FALSE(base::PathExists(
        tracker->DatabaseDirectory().AppendASCII(kOrigin1)));
    EXPECT_TRUE(
        base::PathExists(tracker->GetFullDBFilePath(kOrigin2, kDB2)));
    EXPECT_TRUE(
        base::PathExists(tracker->GetFullDBFilePath(kOrigin2, kDB3)));

    tracker->DatabaseClosed(kOrigin2, kDB3);
    tracker->RemoveObserver(&observer);
  }

  static void TestDatabaseTracker(bool incognito_mode) {
    // Initialize the tracker database.
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
        new quota::MockSpecialStoragePolicy;
    special_storage_policy->AddProtected(GURL(kOrigin2Url));
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(),
                            incognito_mode,
                            special_storage_policy.get(),
                            NULL,
                            NULL));

    // Add two observers.
    TestObserver observer1;
    TestObserver observer2;
    tracker->AddObserver(&observer1);
    tracker->AddObserver(&observer2);

    // Open three new databases.
    int64 database_size = 0;
    const std::string kOrigin1 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin1Url));
    const std::string kOrigin2 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin2Url));
    const base::string16 kDB1 = ASCIIToUTF16("db1");
    const base::string16 kDB2 = ASCIIToUTF16("db2");
    const base::string16 kDB3 = ASCIIToUTF16("db3");
    const base::string16 kDescription = ASCIIToUTF16("database_description");

    // Get the info for kOrigin1 and kOrigin2
    DatabaseTracker::CachedOriginInfo* origin1_info =
        tracker->GetCachedOriginInfo(kOrigin1);
    DatabaseTracker::CachedOriginInfo* origin2_info =
        tracker->GetCachedOriginInfo(kOrigin1);
    EXPECT_TRUE(origin1_info);
    EXPECT_TRUE(origin2_info);


    tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                            &database_size);
    EXPECT_EQ(0, database_size);
    tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, 0,
                            &database_size);
    EXPECT_EQ(0, database_size);
    tracker->DatabaseOpened(kOrigin1, kDB3, kDescription, 0,
                            &database_size);
    EXPECT_EQ(0, database_size);

    // Write some data to each file and check that the listeners are
    // called with the appropriate values.
    EXPECT_TRUE(file_util::CreateDirectory(
        tracker->DatabaseDirectory().Append(base::FilePath::FromUTF16Unsafe(
            tracker->GetOriginDirectory(kOrigin1)))));
    EXPECT_TRUE(file_util::CreateDirectory(
        tracker->DatabaseDirectory().Append(base::FilePath::FromUTF16Unsafe(
            tracker->GetOriginDirectory(kOrigin2)))));
    EXPECT_EQ(1, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin1, kDB1), "a", 1));
    EXPECT_EQ(2, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin2, kDB2), "aa", 2));
    EXPECT_EQ(4, file_util::WriteFile(
        tracker->GetFullDBFilePath(kOrigin1, kDB3), "aaaa", 4));
    tracker->DatabaseModified(kOrigin1, kDB1);
    CheckNotificationReceived(&observer1, kOrigin1, kDB1, 1);
    CheckNotificationReceived(&observer2, kOrigin1, kDB1, 1);
    tracker->DatabaseModified(kOrigin2, kDB2);
    CheckNotificationReceived(&observer1, kOrigin2, kDB2, 2);
    CheckNotificationReceived(&observer2, kOrigin2, kDB2, 2);
    tracker->DatabaseModified(kOrigin1, kDB3);
    CheckNotificationReceived(&observer1, kOrigin1, kDB3, 4);
    CheckNotificationReceived(&observer2, kOrigin1, kDB3, 4);

    // Close all databases
    tracker->DatabaseClosed(kOrigin1, kDB1);
    tracker->DatabaseClosed(kOrigin2, kDB2);
    tracker->DatabaseClosed(kOrigin1, kDB3);

    // Open an existing database and check the reported size
    tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                            &database_size);
    EXPECT_EQ(1, database_size);
    tracker->DatabaseClosed(kOrigin1, kDB1);

    // Remove an observer; this should clear all caches.
    tracker->RemoveObserver(&observer2);

    // Close the tracker database and clear all caches.
    // Then make sure that DatabaseOpened() still returns the correct result.
    tracker->CloseTrackerDatabaseAndClearCaches();
    tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                            &database_size);
    EXPECT_EQ(1, database_size);
    tracker->DatabaseClosed(kOrigin1, kDB1);

    // Remove all observers.
    tracker->RemoveObserver(&observer1);

    // Trying to delete a database in use should fail
    tracker->DatabaseOpened(kOrigin1, kDB3, kDescription, 0,
                            &database_size);
    EXPECT_FALSE(tracker->DeleteClosedDatabase(kOrigin1, kDB3));
    origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
    EXPECT_TRUE(origin1_info);
    EXPECT_EQ(4, origin1_info->GetDatabaseSize(kDB3));
    tracker->DatabaseClosed(kOrigin1, kDB3);

    // Delete a database and make sure the space used by that origin is updated
    EXPECT_TRUE(tracker->DeleteClosedDatabase(kOrigin1, kDB3));
    origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
    EXPECT_TRUE(origin1_info);
    EXPECT_EQ(1, origin1_info->GetDatabaseSize(kDB1));
    EXPECT_EQ(0, origin1_info->GetDatabaseSize(kDB3));

    // Get all data for all origins
    std::vector<OriginInfo> origins_info;
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
    EXPECT_EQ(size_t(2), origins_info.size());
    EXPECT_EQ(kOrigin1, origins_info[0].GetOriginIdentifier());
    EXPECT_EQ(1, origins_info[0].TotalSize());
    EXPECT_EQ(1, origins_info[0].GetDatabaseSize(kDB1));
    EXPECT_EQ(0, origins_info[0].GetDatabaseSize(kDB3));

    EXPECT_EQ(kOrigin2, origins_info[1].GetOriginIdentifier());
    EXPECT_EQ(2, origins_info[1].TotalSize());

    // Trying to delete an origin with databases in use should fail
    tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                            &database_size);
    EXPECT_FALSE(tracker->DeleteOrigin(kOrigin1, false));
    origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
    EXPECT_TRUE(origin1_info);
    EXPECT_EQ(1, origin1_info->GetDatabaseSize(kDB1));
    tracker->DatabaseClosed(kOrigin1, kDB1);

    // Delete an origin that doesn't have any database in use
    EXPECT_TRUE(tracker->DeleteOrigin(kOrigin1, false));
    origins_info.clear();
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
    EXPECT_EQ(size_t(1), origins_info.size());
    EXPECT_EQ(kOrigin2, origins_info[0].GetOriginIdentifier());

    origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
    EXPECT_TRUE(origin1_info);
    EXPECT_EQ(0, origin1_info->TotalSize());
  }

  static void DatabaseTrackerQuotaIntegration() {
    const GURL kOrigin(kOrigin1Url);
    const std::string kOriginId =
        webkit_database::GetIdentifierFromOrigin(kOrigin);
    const base::string16 kName = ASCIIToUTF16("name");
    const base::string16 kDescription = ASCIIToUTF16("description");

    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    // Initialize the tracker with a QuotaManagerProxy
    scoped_refptr<TestQuotaManagerProxy> test_quota_proxy(
        new TestQuotaManagerProxy);
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(),
                            false /* incognito */,
                            NULL,
                            test_quota_proxy.get(),
                            NULL));
    EXPECT_TRUE(test_quota_proxy->registered_client_);

    // Create a database and modify it a couple of times, close it,
    // then delete it. Observe the tracker notifies accordingly.

    int64 database_size = 0;
    tracker->DatabaseOpened(kOriginId, kName, kDescription, 0,
                            &database_size);
    EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kOrigin));
    test_quota_proxy->reset();

    base::FilePath db_file(tracker->GetFullDBFilePath(kOriginId, kName));
    EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
    EXPECT_TRUE(EnsureFileOfSize(db_file, 10));
    tracker->DatabaseModified(kOriginId, kName);
    EXPECT_TRUE(test_quota_proxy->WasModificationNotified(kOrigin, 10));
    test_quota_proxy->reset();

    EXPECT_TRUE(EnsureFileOfSize(db_file, 100));
    tracker->DatabaseModified(kOriginId, kName);
    EXPECT_TRUE(test_quota_proxy->WasModificationNotified(kOrigin, 90));
    test_quota_proxy->reset();

    tracker->DatabaseClosed(kOriginId, kName);
    EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kOrigin));
    EXPECT_EQ(net::OK, tracker->DeleteDatabase(
        kOriginId, kName, net::CompletionCallback()));
    EXPECT_TRUE(test_quota_proxy->WasModificationNotified(kOrigin, -100));
    test_quota_proxy->reset();

    // Create a database and modify it, try to delete it while open,
    // then close it (at which time deletion will actually occur).
    // Observe the tracker notifies accordingly.

    tracker->DatabaseOpened(kOriginId, kName, kDescription, 0,
                            &database_size);
    EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kOrigin));
    test_quota_proxy->reset();

    db_file = tracker->GetFullDBFilePath(kOriginId, kName);
    EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
    EXPECT_TRUE(EnsureFileOfSize(db_file, 100));
    tracker->DatabaseModified(kOriginId, kName);
    EXPECT_TRUE(test_quota_proxy->WasModificationNotified(kOrigin, 100));
    test_quota_proxy->reset();

    EXPECT_EQ(net::ERR_IO_PENDING,
              tracker->DeleteDatabase(kOriginId, kName,
                                      net::CompletionCallback()));
    EXPECT_FALSE(test_quota_proxy->WasModificationNotified(kOrigin, -100));

    tracker->DatabaseClosed(kOriginId, kName);
    EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kOrigin));
    EXPECT_TRUE(test_quota_proxy->WasModificationNotified(kOrigin, -100));
    test_quota_proxy->reset();

    // Create a database and up the file size without telling
    // the tracker about the modification, than simulate a
    // a renderer crash.
    // Observe the tracker notifies accordingly.

    tracker->DatabaseOpened(kOriginId, kName, kDescription, 0,
                            &database_size);
    EXPECT_TRUE(test_quota_proxy->WasAccessNotified(kOrigin));
    test_quota_proxy->reset();
    db_file = tracker->GetFullDBFilePath(kOriginId, kName);
    EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
    EXPECT_TRUE(EnsureFileOfSize(db_file, 100));
    DatabaseConnections crashed_renderer_connections;
    crashed_renderer_connections.AddConnection(kOriginId, kName);
    EXPECT_FALSE(test_quota_proxy->WasModificationNotified(kOrigin, 100));
    tracker->CloseDatabases(crashed_renderer_connections);
    EXPECT_TRUE(test_quota_proxy->WasModificationNotified(kOrigin, 100));

    // Cleanup.
    crashed_renderer_connections.RemoveAllConnections();
    test_quota_proxy->SimulateQuotaManagerDestroyed();
  }

  static void DatabaseTrackerClearSessionOnlyDatabasesOnExit() {
    int64 database_size = 0;
    const std::string kOrigin1 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin1Url));
    const std::string kOrigin2 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin2Url));
    const base::string16 kDB1 = ASCIIToUTF16("db1");
    const base::string16 kDB2 = ASCIIToUTF16("db2");
    const base::string16 kDescription = ASCIIToUTF16("database_description");

    // Initialize the tracker database.
    base::MessageLoop message_loop;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath origin1_db_dir;
    base::FilePath origin2_db_dir;
    {
      scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
          new quota::MockSpecialStoragePolicy;
      special_storage_policy->AddSessionOnly(GURL(kOrigin2Url));
      scoped_refptr<DatabaseTracker> tracker(
          new DatabaseTracker(temp_dir.path(),
                              false,
                              special_storage_policy.get(),
                              NULL,
                              base::MessageLoopProxy::current().get()));

      // Open two new databases.
      tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                              &database_size);
      EXPECT_EQ(0, database_size);
      tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, 0,
                              &database_size);
      EXPECT_EQ(0, database_size);

      // Write some data to each file.
      base::FilePath db_file;
      db_file = tracker->GetFullDBFilePath(kOrigin1, kDB1);
      EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
      EXPECT_TRUE(EnsureFileOfSize(db_file, 1));

      db_file = tracker->GetFullDBFilePath(kOrigin2, kDB2);
      EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
      EXPECT_TRUE(EnsureFileOfSize(db_file, 2));

      // Store the origin database directories as long as they still exist.
      origin1_db_dir = tracker->GetFullDBFilePath(kOrigin1, kDB1).DirName();
      origin2_db_dir = tracker->GetFullDBFilePath(kOrigin2, kDB2).DirName();

      tracker->DatabaseModified(kOrigin1, kDB1);
      tracker->DatabaseModified(kOrigin2, kDB2);

      // Close all databases.
      tracker->DatabaseClosed(kOrigin1, kDB1);
      tracker->DatabaseClosed(kOrigin2, kDB2);

      tracker->Shutdown();
    }

    // At this point, the database tracker should be gone. Create a new one.
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(), false, NULL, NULL, NULL));

    // Get all data for all origins.
    std::vector<OriginInfo> origins_info;
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
    // kOrigin1 was not session-only, so it survived. kOrigin2 was session-only
    // and it got deleted.
    EXPECT_EQ(size_t(1), origins_info.size());
    EXPECT_EQ(kOrigin1, origins_info[0].GetOriginIdentifier());
    EXPECT_TRUE(
        base::PathExists(tracker->GetFullDBFilePath(kOrigin1, kDB1)));
    EXPECT_EQ(base::FilePath(), tracker->GetFullDBFilePath(kOrigin2, kDB2));

    // The origin directory of kOrigin1 remains, but the origin directory of
    // kOrigin2 is deleted.
    EXPECT_TRUE(base::PathExists(origin1_db_dir));
    EXPECT_FALSE(base::PathExists(origin2_db_dir));
  }

  static void DatabaseTrackerSetForceKeepSessionState() {
    int64 database_size = 0;
    const std::string kOrigin1 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin1Url));
    const std::string kOrigin2 =
        webkit_database::GetIdentifierFromOrigin(GURL(kOrigin2Url));
    const base::string16 kDB1 = ASCIIToUTF16("db1");
    const base::string16 kDB2 = ASCIIToUTF16("db2");
    const base::string16 kDescription = ASCIIToUTF16("database_description");

    // Initialize the tracker database.
    base::MessageLoop message_loop;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath origin1_db_dir;
    base::FilePath origin2_db_dir;
    {
      scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
          new quota::MockSpecialStoragePolicy;
      special_storage_policy->AddSessionOnly(GURL(kOrigin2Url));
      scoped_refptr<DatabaseTracker> tracker(
          new DatabaseTracker(temp_dir.path(),
                              false,
                              special_storage_policy.get(),
                              NULL,
                              base::MessageLoopProxy::current().get()));
      tracker->SetForceKeepSessionState();

      // Open two new databases.
      tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                              &database_size);
      EXPECT_EQ(0, database_size);
      tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, 0,
                              &database_size);
      EXPECT_EQ(0, database_size);

      // Write some data to each file.
      base::FilePath db_file;
      db_file = tracker->GetFullDBFilePath(kOrigin1, kDB1);
      EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
      EXPECT_TRUE(EnsureFileOfSize(db_file, 1));

      db_file = tracker->GetFullDBFilePath(kOrigin2, kDB2);
      EXPECT_TRUE(file_util::CreateDirectory(db_file.DirName()));
      EXPECT_TRUE(EnsureFileOfSize(db_file, 2));

      // Store the origin database directories as long as they still exist.
      origin1_db_dir = tracker->GetFullDBFilePath(kOrigin1, kDB1).DirName();
      origin2_db_dir = tracker->GetFullDBFilePath(kOrigin2, kDB2).DirName();

      tracker->DatabaseModified(kOrigin1, kDB1);
      tracker->DatabaseModified(kOrigin2, kDB2);

      // Close all databases.
      tracker->DatabaseClosed(kOrigin1, kDB1);
      tracker->DatabaseClosed(kOrigin2, kDB2);

      tracker->Shutdown();
    }

    // At this point, the database tracker should be gone. Create a new one.
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(), false, NULL, NULL, NULL));

    // Get all data for all origins.
    std::vector<OriginInfo> origins_info;
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
    // No origins were deleted.
    EXPECT_EQ(size_t(2), origins_info.size());
    EXPECT_TRUE(
        base::PathExists(tracker->GetFullDBFilePath(kOrigin1, kDB1)));
    EXPECT_TRUE(
        base::PathExists(tracker->GetFullDBFilePath(kOrigin2, kDB2)));

    EXPECT_TRUE(base::PathExists(origin1_db_dir));
    EXPECT_TRUE(base::PathExists(origin2_db_dir));
  }

  static void EmptyDatabaseNameIsValid() {
    const GURL kOrigin(kOrigin1Url);
    const std::string kOriginId =
        webkit_database::GetIdentifierFromOrigin(kOrigin);
    const base::string16 kEmptyName;
    const base::string16 kDescription(ASCIIToUTF16("description"));
    const base::string16 kChangedDescription(
        ASCIIToUTF16("changed_description"));

    // Initialize a tracker database, no need to put it on disk.
    const bool kUseInMemoryTrackerDatabase = true;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(), kUseInMemoryTrackerDatabase,
                            NULL, NULL, NULL));

    // Starts off with no databases.
    std::vector<OriginInfo> infos;
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
    EXPECT_TRUE(infos.empty());

    // Create a db with an empty name.
    int64 database_size = -1;
    tracker->DatabaseOpened(kOriginId, kEmptyName, kDescription, 0,
                            &database_size);
    EXPECT_EQ(0, database_size);
    tracker->DatabaseModified(kOriginId, kEmptyName);
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
    EXPECT_EQ(1u, infos.size());
    EXPECT_EQ(kDescription, infos[0].GetDatabaseDescription(kEmptyName));
    EXPECT_FALSE(tracker->GetFullDBFilePath(kOriginId, kEmptyName).empty());
    tracker->DatabaseOpened(kOriginId, kEmptyName, kChangedDescription, 0,
                            &database_size);
    infos.clear();
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
    EXPECT_EQ(1u, infos.size());
    EXPECT_EQ(kChangedDescription, infos[0].GetDatabaseDescription(kEmptyName));
    tracker->DatabaseClosed(kOriginId, kEmptyName);
    tracker->DatabaseClosed(kOriginId, kEmptyName);

    // Deleting it should return to the initial state.
    EXPECT_EQ(net::OK, tracker->DeleteDatabase(kOriginId, kEmptyName,
                                               net::CompletionCallback()));
    infos.clear();
    EXPECT_TRUE(tracker->GetAllOriginsInfo(&infos));
    EXPECT_TRUE(infos.empty());
  }

  static void HandleSqliteError() {
    const GURL kOrigin(kOrigin1Url);
    const std::string kOriginId =
        webkit_database::GetIdentifierFromOrigin(kOrigin);
    const base::string16 kName(ASCIIToUTF16("name"));
    const base::string16 kDescription(ASCIIToUTF16("description"));

    // Initialize a tracker database, no need to put it on disk.
    const bool kUseInMemoryTrackerDatabase = true;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    scoped_refptr<DatabaseTracker> tracker(
        new DatabaseTracker(temp_dir.path(), kUseInMemoryTrackerDatabase,
                            NULL, NULL, NULL));

    // Setup to observe OnScheduledForDelete notifications.
    TestObserver observer(false, true);
    tracker->AddObserver(&observer);

    // Verify does no harm when there is no such database.
    tracker->HandleSqliteError(kOriginId, kName, SQLITE_CORRUPT);
    EXPECT_FALSE(tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
    EXPECT_FALSE(observer.DidReceiveNewNotification());

    // --------------------------------------------------------
    // Create a record of a database in the tracker db and create
    // a spoof_db_file on disk in the expected location.
    int64 database_size = 0;
    tracker->DatabaseOpened(kOriginId, kName, kDescription, 0,
                            &database_size);
    base::FilePath spoof_db_file = tracker->GetFullDBFilePath(kOriginId, kName);
    EXPECT_FALSE(tracker->GetFullDBFilePath(kOriginId, kName).empty());
    EXPECT_TRUE(file_util::CreateDirectory(spoof_db_file.DirName()));
    EXPECT_TRUE(EnsureFileOfSize(spoof_db_file, 1));

    // Verify does no harm with a non-error is reported.
    tracker->HandleSqliteError(kOriginId, kName, SQLITE_OK);
    EXPECT_FALSE(tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
    EXPECT_FALSE(observer.DidReceiveNewNotification());

    // Verify that with a connection open, the db is scheduled for deletion,
    // but that the file still exists.
    tracker->HandleSqliteError(kOriginId, kName, SQLITE_CORRUPT);
    EXPECT_TRUE(tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
    EXPECT_TRUE(observer.DidReceiveNewNotification());
    EXPECT_TRUE(base::PathExists(spoof_db_file));

    // Verify that once closed, the file is deleted and the record in the
    // tracker db is removed.
    tracker->DatabaseClosed(kOriginId, kName);
    EXPECT_FALSE(base::PathExists(spoof_db_file));
    EXPECT_TRUE(tracker->GetFullDBFilePath(kOriginId, kName).empty());

    // --------------------------------------------------------
    // Create another record of a database in the tracker db and create
    // a spoof_db_file on disk in the expected location.
    tracker->DatabaseOpened(kOriginId, kName, kDescription, 0,
                            &database_size);
    base::FilePath spoof_db_file2 = tracker->GetFullDBFilePath(kOriginId, kName);
    EXPECT_FALSE(tracker->GetFullDBFilePath(kOriginId, kName).empty());
    EXPECT_NE(spoof_db_file, spoof_db_file2);
    EXPECT_TRUE(file_util::CreateDirectory(spoof_db_file2.DirName()));
    EXPECT_TRUE(EnsureFileOfSize(spoof_db_file2, 1));

    // Verify that with no connection open, the db is deleted immediately.
    tracker->DatabaseClosed(kOriginId, kName);
    tracker->HandleSqliteError(kOriginId, kName, SQLITE_CORRUPT);
    EXPECT_FALSE(tracker->IsDatabaseScheduledForDeletion(kOriginId, kName));
    EXPECT_FALSE(observer.DidReceiveNewNotification());
    EXPECT_TRUE(tracker->GetFullDBFilePath(kOriginId, kName).empty());
    EXPECT_FALSE(base::PathExists(spoof_db_file2));

    tracker->RemoveObserver(&observer);
  }
};

TEST(DatabaseTrackerTest, DeleteOpenDatabase) {
  DatabaseTracker_TestHelper_Test::TestDeleteOpenDatabase(false);
}

TEST(DatabaseTrackerTest, DeleteOpenDatabaseIncognitoMode) {
  DatabaseTracker_TestHelper_Test::TestDeleteOpenDatabase(true);
}

TEST(DatabaseTrackerTest, DatabaseTracker) {
  DatabaseTracker_TestHelper_Test::TestDatabaseTracker(false);
}

TEST(DatabaseTrackerTest, DatabaseTrackerIncognitoMode) {
  DatabaseTracker_TestHelper_Test::TestDatabaseTracker(true);
}

TEST(DatabaseTrackerTest, DatabaseTrackerQuotaIntegration) {
  // There is no difference in behavior between incognito and not.
  DatabaseTracker_TestHelper_Test::DatabaseTrackerQuotaIntegration();
}

TEST(DatabaseTrackerTest, DatabaseTrackerClearSessionOnlyDatabasesOnExit) {
  // Only works for regular mode.
  DatabaseTracker_TestHelper_Test::
      DatabaseTrackerClearSessionOnlyDatabasesOnExit();
}

TEST(DatabaseTrackerTest, DatabaseTrackerSetForceKeepSessionState) {
  // Only works for regular mode.
  DatabaseTracker_TestHelper_Test::DatabaseTrackerSetForceKeepSessionState();
}

TEST(DatabaseTrackerTest, EmptyDatabaseNameIsValid) {
  DatabaseTracker_TestHelper_Test::EmptyDatabaseNameIsValid();
}

TEST(DatabaseTrackerTest, HandleSqliteError) {
  DatabaseTracker_TestHelper_Test::HandleSqliteError();
}

}  // namespace webkit_database
