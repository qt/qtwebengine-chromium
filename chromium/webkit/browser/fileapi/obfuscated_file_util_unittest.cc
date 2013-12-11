// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_test_helper.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/mock_file_change_observer.h"
#include "webkit/browser/fileapi/mock_file_system_context.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/sandbox_directory_database.h"
#include "webkit/browser/fileapi/sandbox_file_system_test_helper.h"
#include "webkit/browser/fileapi/sandbox_isolated_origin_database.h"
#include "webkit/browser/fileapi/sandbox_origin_database.h"
#include "webkit/browser/fileapi/test_file_set.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/database/database_identifier.h"
#include "webkit/common/quota/quota_types.h"

namespace fileapi {

namespace {

bool FileExists(const base::FilePath& path) {
  return base::PathExists(path) && !base::DirectoryExists(path);
}

int64 GetSize(const base::FilePath& path) {
  int64 size;
  EXPECT_TRUE(file_util::GetFileSize(path, &size));
  return size;
}

// After a move, the dest exists and the source doesn't.
// After a copy, both source and dest exist.
struct CopyMoveTestCaseRecord {
  bool is_copy_not_move;
  const char source_path[64];
  const char dest_path[64];
  bool cause_overwrite;
};

const CopyMoveTestCaseRecord kCopyMoveTestCases[] = {
  // This is the combinatoric set of:
  //  rename vs. same-name
  //  different directory vs. same directory
  //  overwrite vs. no-overwrite
  //  copy vs. move
  //  We can never be called with source and destination paths identical, so
  //  those cases are omitted.
  {true, "dir0/file0", "dir0/file1", false},
  {false, "dir0/file0", "dir0/file1", false},
  {true, "dir0/file0", "dir0/file1", true},
  {false, "dir0/file0", "dir0/file1", true},

  {true, "dir0/file0", "dir1/file0", false},
  {false, "dir0/file0", "dir1/file0", false},
  {true, "dir0/file0", "dir1/file0", true},
  {false, "dir0/file0", "dir1/file0", true},
  {true, "dir0/file0", "dir1/file1", false},
  {false, "dir0/file0", "dir1/file1", false},
  {true, "dir0/file0", "dir1/file1", true},
  {false, "dir0/file0", "dir1/file1", true},
};

struct OriginEnumerationTestRecord {
  std::string origin_url;
  bool has_temporary;
  bool has_persistent;
};

const OriginEnumerationTestRecord kOriginEnumerationTestRecords[] = {
  {"http://example.com", false, true},
  {"http://example1.com", true, false},
  {"https://example1.com", true, true},
  {"file://", false, true},
  {"http://example.com:8000", false, true},
};

FileSystemURL FileSystemURLAppend(
    const FileSystemURL& url, const base::FilePath::StringType& child) {
  return FileSystemURL::CreateForTest(
      url.origin(), url.mount_type(), url.virtual_path().Append(child));
}

FileSystemURL FileSystemURLAppendUTF8(
    const FileSystemURL& url, const std::string& child) {
  return FileSystemURL::CreateForTest(
      url.origin(),
      url.mount_type(),
      url.virtual_path().Append(base::FilePath::FromUTF8Unsafe(child)));
}

FileSystemURL FileSystemURLDirName(const FileSystemURL& url) {
  return FileSystemURL::CreateForTest(
      url.origin(), url.mount_type(), VirtualPath::DirName(url.virtual_path()));
}

}  // namespace (anonymous)

// TODO(ericu): The vast majority of this and the other FSFU subclass tests
// could theoretically be shared.  It would basically be a FSFU interface
// compliance test, and only the subclass-specific bits that look into the
// implementation would need to be written per-subclass.
class ObfuscatedFileUtilTest : public testing::Test {
 public:
  ObfuscatedFileUtilTest()
      : origin_(GURL("http://www.example.com")),
        type_(kFileSystemTypeTemporary),
        weak_factory_(this),
        sandbox_file_system_(origin_, type_),
        quota_status_(quota::kQuotaStatusUnknown),
        usage_(-1) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    storage_policy_ = new quota::MockSpecialStoragePolicy();

    quota_manager_ =
        new quota::QuotaManager(false /* is_incognito */,
                                data_dir_.path(),
                                base::MessageLoopProxy::current().get(),
                                base::MessageLoopProxy::current().get(),
                                storage_policy_.get());

    // Every time we create a new sandbox_file_system helper,
    // it creates another context, which creates another path manager,
    // another sandbox_backend, and another OFU.
    // We need to pass in the context to skip all that.
    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_->proxy(),
        data_dir_.path());

    sandbox_file_system_.SetUp(file_system_context_.get());

    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);
  }

  virtual void TearDown() {
    quota_manager_ = NULL;
    sandbox_file_system_.TearDown();
  }

  scoped_ptr<FileSystemOperationContext> LimitedContext(
      int64 allowed_bytes_growth) {
    scoped_ptr<FileSystemOperationContext> context(
        sandbox_file_system_.NewOperationContext());
    context->set_allowed_bytes_growth(allowed_bytes_growth);
    return context.Pass();
  }

  scoped_ptr<FileSystemOperationContext> UnlimitedContext() {
    return LimitedContext(kint64max);
  }

  FileSystemOperationContext* NewContext(
      SandboxFileSystemTestHelper* file_system) {
    change_observer()->ResetCount();
    FileSystemOperationContext* context;
    if (file_system)
      context = file_system->NewOperationContext();
    else
      context = sandbox_file_system_.NewOperationContext();
    // Setting allowed_bytes_growth big enough for all tests.
    context->set_allowed_bytes_growth(1024 * 1024);
    context->set_change_observers(change_observers());
    return context;
  }

  const ChangeObserverList& change_observers() const {
    return change_observers_;
  }

  MockFileChangeObserver* change_observer() {
    return &change_observer_;
  }

  // This can only be used after SetUp has run and created file_system_context_
  // and obfuscated_file_util_.
  // Use this for tests which need to run in multiple origins; we need a test
  // helper per origin.
  SandboxFileSystemTestHelper* NewFileSystem(
      const GURL& origin, fileapi::FileSystemType type) {
    SandboxFileSystemTestHelper* file_system =
        new SandboxFileSystemTestHelper(origin, type);

    file_system->SetUp(file_system_context_.get());
    return file_system;
  }

  ObfuscatedFileUtil* ofu() {
    return static_cast<ObfuscatedFileUtil*>(sandbox_file_system_.file_util());
  }

  const base::FilePath& test_directory() const {
    return data_dir_.path();
  }

  const GURL& origin() const {
    return origin_;
  }

  fileapi::FileSystemType type() const {
    return type_;
  }

  int64 ComputeTotalFileSize() {
    return sandbox_file_system_.ComputeCurrentOriginUsage() -
        sandbox_file_system_.ComputeCurrentDirectoryDatabaseUsage();
  }

  void GetUsageFromQuotaManager() {
    int64 quota = -1;
    quota_status_ =
        AsyncFileTestHelper::GetUsageAndQuota(quota_manager_.get(),
                                              origin(),
                                              sandbox_file_system_.type(),
                                              &usage_,
                                              &quota);
    EXPECT_EQ(quota::kQuotaStatusOk, quota_status_);
  }

  void RevokeUsageCache() {
    quota_manager_->ResetUsageTracker(sandbox_file_system_.storage_type());
    usage_cache()->Delete(sandbox_file_system_.GetUsageCachePath());
  }

  int64 SizeByQuotaUtil() {
    return sandbox_file_system_.GetCachedOriginUsage();
  }

  int64 SizeInUsageFile() {
    base::RunLoop().RunUntilIdle();
    int64 usage = 0;
    return usage_cache()->GetUsage(
        sandbox_file_system_.GetUsageCachePath(), &usage) ? usage : -1;
  }

  bool PathExists(const FileSystemURL& url) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    base::PlatformFileInfo file_info;
    base::FilePath platform_path;
    base::PlatformFileError error = ofu()->GetFileInfo(
        context.get(), url, &file_info, &platform_path);
    return error == base::PLATFORM_FILE_OK;
  }

  bool DirectoryExists(const FileSystemURL& url) {
    return AsyncFileTestHelper::DirectoryExists(file_system_context(), url);
  }

  int64 usage() const { return usage_; }
  FileSystemUsageCache* usage_cache() {
    return sandbox_file_system_.usage_cache();
  }

  FileSystemURL CreateURLFromUTF8(const std::string& path) {
    return sandbox_file_system_.CreateURLFromUTF8(path);
  }

  int64 PathCost(const FileSystemURL& url) {
    return ObfuscatedFileUtil::ComputeFilePathCost(url.path());
  }

  FileSystemURL CreateURL(const base::FilePath& path) {
    return sandbox_file_system_.CreateURL(path);
  }

  void CheckFileAndCloseHandle(
      const FileSystemURL& url, base::PlatformFile file_handle) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    base::FilePath local_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
        context.get(), url, &local_path));

    base::PlatformFileInfo file_info0;
    base::FilePath data_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), url, &file_info0, &data_path));
    EXPECT_EQ(data_path, local_path);
    EXPECT_TRUE(FileExists(data_path));
    EXPECT_EQ(0, GetSize(data_path));

    const char data[] = "test data";
    const int length = arraysize(data) - 1;

    if (base::kInvalidPlatformFileValue == file_handle) {
      bool created = true;
      base::PlatformFileError error;
      file_handle = base::CreatePlatformFile(
          data_path,
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
          &created,
          &error);
      ASSERT_NE(base::kInvalidPlatformFileValue, file_handle);
      ASSERT_EQ(base::PLATFORM_FILE_OK, error);
      EXPECT_FALSE(created);
    }
    ASSERT_EQ(length, base::WritePlatformFile(file_handle, 0, data, length));
    EXPECT_TRUE(base::ClosePlatformFile(file_handle));

    base::PlatformFileInfo file_info1;
    EXPECT_EQ(length, GetSize(data_path));
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), url, &file_info1, &data_path));
    EXPECT_EQ(data_path, local_path);

    EXPECT_FALSE(file_info0.is_directory);
    EXPECT_FALSE(file_info1.is_directory);
    EXPECT_FALSE(file_info0.is_symbolic_link);
    EXPECT_FALSE(file_info1.is_symbolic_link);
    EXPECT_EQ(0, file_info0.size);
    EXPECT_EQ(length, file_info1.size);
    EXPECT_LE(file_info0.last_modified, file_info1.last_modified);

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
        context.get(), url, length * 2));
    EXPECT_EQ(length * 2, GetSize(data_path));

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
        context.get(), url, 0));
    EXPECT_EQ(0, GetSize(data_path));
  }

  void ValidateTestDirectory(
      const FileSystemURL& root_url,
      const std::set<base::FilePath::StringType>& files,
      const std::set<base::FilePath::StringType>& directories) {
    scoped_ptr<FileSystemOperationContext> context;
    std::set<base::FilePath::StringType>::const_iterator iter;
    for (iter = files.begin(); iter != files.end(); ++iter) {
      bool created = true;
      context.reset(NewContext(NULL));
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(
              context.get(), FileSystemURLAppend(root_url, *iter),
              &created));
      ASSERT_FALSE(created);
    }
    for (iter = directories.begin(); iter != directories.end(); ++iter) {
      context.reset(NewContext(NULL));
      EXPECT_TRUE(DirectoryExists(
          FileSystemURLAppend(root_url, *iter)));
    }
  }

  class UsageVerifyHelper {
   public:
    UsageVerifyHelper(scoped_ptr<FileSystemOperationContext> context,
                      SandboxFileSystemTestHelper* file_system,
                      int64 expected_usage)
        : context_(context.Pass()),
          sandbox_file_system_(file_system),
          expected_usage_(expected_usage) {}

    ~UsageVerifyHelper() {
      base::RunLoop().RunUntilIdle();
      Check();
    }

    FileSystemOperationContext* context() {
      return context_.get();
    }

   private:
    void Check() {
      ASSERT_EQ(expected_usage_,
                sandbox_file_system_->GetCachedOriginUsage());
    }

    scoped_ptr<FileSystemOperationContext> context_;
    SandboxFileSystemTestHelper* sandbox_file_system_;
    int64 expected_usage_;
  };

  scoped_ptr<UsageVerifyHelper> AllowUsageIncrease(int64 requested_growth) {
    int64 usage = sandbox_file_system_.GetCachedOriginUsage();
    return scoped_ptr<UsageVerifyHelper>(new UsageVerifyHelper(
        LimitedContext(requested_growth),
        &sandbox_file_system_, usage + requested_growth));
  }

  scoped_ptr<UsageVerifyHelper> DisallowUsageIncrease(int64 requested_growth) {
    int64 usage = sandbox_file_system_.GetCachedOriginUsage();
    return scoped_ptr<UsageVerifyHelper>(new UsageVerifyHelper(
        LimitedContext(requested_growth - 1), &sandbox_file_system_, usage));
  }

  void FillTestDirectory(
      const FileSystemURL& root_url,
      std::set<base::FilePath::StringType>* files,
      std::set<base::FilePath::StringType>* directories) {
    scoped_ptr<FileSystemOperationContext> context;
    std::vector<DirectoryEntry> entries;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              AsyncFileTestHelper::ReadDirectory(
                  file_system_context(), root_url, &entries));
    EXPECT_EQ(0UL, entries.size());

    files->clear();
    files->insert(FILE_PATH_LITERAL("first"));
    files->insert(FILE_PATH_LITERAL("second"));
    files->insert(FILE_PATH_LITERAL("third"));
    directories->clear();
    directories->insert(FILE_PATH_LITERAL("fourth"));
    directories->insert(FILE_PATH_LITERAL("fifth"));
    directories->insert(FILE_PATH_LITERAL("sixth"));
    std::set<base::FilePath::StringType>::iterator iter;
    for (iter = files->begin(); iter != files->end(); ++iter) {
      bool created = false;
      context.reset(NewContext(NULL));
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(
              context.get(),
              FileSystemURLAppend(root_url, *iter),
              &created));
      ASSERT_TRUE(created);
    }
    for (iter = directories->begin(); iter != directories->end(); ++iter) {
      bool exclusive = true;
      bool recursive = false;
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
          ofu()->CreateDirectory(
              context.get(),
              FileSystemURLAppend(root_url, *iter),
              exclusive, recursive));
    }
    ValidateTestDirectory(root_url, *files, *directories);
  }

  void TestReadDirectoryHelper(const FileSystemURL& root_url) {
    std::set<base::FilePath::StringType> files;
    std::set<base::FilePath::StringType> directories;
    FillTestDirectory(root_url, &files, &directories);

    scoped_ptr<FileSystemOperationContext> context;
    std::vector<DirectoryEntry> entries;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              AsyncFileTestHelper::ReadDirectory(
                  file_system_context(), root_url, &entries));
    std::vector<DirectoryEntry>::iterator entry_iter;
    EXPECT_EQ(files.size() + directories.size(), entries.size());
    EXPECT_TRUE(change_observer()->HasNoChange());
    for (entry_iter = entries.begin(); entry_iter != entries.end();
        ++entry_iter) {
      const DirectoryEntry& entry = *entry_iter;
      std::set<base::FilePath::StringType>::iterator iter =
          files.find(entry.name);
      if (iter != files.end()) {
        EXPECT_FALSE(entry.is_directory);
        files.erase(iter);
        continue;
      }
      iter = directories.find(entry.name);
      EXPECT_FALSE(directories.end() == iter);
      EXPECT_TRUE(entry.is_directory);
      directories.erase(iter);
    }
  }

  void TestTouchHelper(const FileSystemURL& url, bool is_file) {
    base::Time last_access_time = base::Time::Now();
    base::Time last_modified_time = base::Time::Now();

    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Touch(
                  context.get(), url, last_access_time, last_modified_time));
    // Currently we fire no change notifications for Touch.
    EXPECT_TRUE(change_observer()->HasNoChange());
    base::FilePath local_path;
    base::PlatformFileInfo file_info;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), url, &file_info, &local_path));
    // We compare as time_t here to lower our resolution, to avoid false
    // negatives caused by conversion to the local filesystem's native
    // representation and back.
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());

    context.reset(NewContext(NULL));
    last_modified_time += base::TimeDelta::FromHours(1);
    last_access_time += base::TimeDelta::FromHours(14);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Touch(
                  context.get(), url, last_access_time, last_modified_time));
    EXPECT_TRUE(change_observer()->HasNoChange());
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), url, &file_info, &local_path));
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());
    if (is_file)  // Directories in OFU don't support atime.
      EXPECT_EQ(file_info.last_accessed.ToTimeT(), last_access_time.ToTimeT());
  }

  void TestCopyInForeignFileHelper(bool overwrite) {
    base::ScopedTempDir source_dir;
    ASSERT_TRUE(source_dir.CreateUniqueTempDir());
    base::FilePath root_file_path = source_dir.path();
    base::FilePath src_file_path = root_file_path.AppendASCII("file_name");
    FileSystemURL dest_url = CreateURLFromUTF8("new file");
    int64 src_file_length = 87;

    base::PlatformFileError error_code;
    bool created = false;
    int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
    base::PlatformFile file_handle =
        base::CreatePlatformFile(
            src_file_path, file_flags, &created, &error_code);
    EXPECT_TRUE(created);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);
    ASSERT_NE(base::kInvalidPlatformFileValue, file_handle);
    ASSERT_TRUE(base::TruncatePlatformFile(file_handle, src_file_length));
    EXPECT_TRUE(base::ClosePlatformFile(file_handle));

    scoped_ptr<FileSystemOperationContext> context;

    if (overwrite) {
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(context.get(), dest_url, &created));
      EXPECT_TRUE(created);

      // We must have observed one (and only one) create_file_count.
      EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
      EXPECT_TRUE(change_observer()->HasNoChange());
    }

    const int64 path_cost =
        ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path());
    if (!overwrite) {
      // Verify that file creation requires sufficient quota for the path.
      context.reset(NewContext(NULL));
      context->set_allowed_bytes_growth(path_cost + src_file_length - 1);
      EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
                ofu()->CopyInForeignFile(context.get(),
                                         src_file_path, dest_url));
    }

    context.reset(NewContext(NULL));
    context->set_allowed_bytes_growth(path_cost + src_file_length);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CopyInForeignFile(context.get(),
                                       src_file_path, dest_url));

    EXPECT_TRUE(PathExists(dest_url));
    EXPECT_FALSE(DirectoryExists(dest_url));

    context.reset(NewContext(NULL));
    base::PlatformFileInfo file_info;
    base::FilePath data_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), dest_url, &file_info, &data_path));
    EXPECT_NE(data_path, src_file_path);
    EXPECT_TRUE(FileExists(data_path));
    EXPECT_EQ(src_file_length, GetSize(data_path));

    EXPECT_EQ(base::PLATFORM_FILE_OK,
        ofu()->DeleteFile(context.get(), dest_url));
  }

  void ClearTimestamp(const FileSystemURL& url) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Touch(context.get(), url, base::Time(), base::Time()));
    EXPECT_EQ(base::Time(), GetModifiedTime(url));
  }

  base::Time GetModifiedTime(const FileSystemURL& url) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
    base::FilePath data_path;
    base::PlatformFileInfo file_info;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->GetFileInfo(context.get(), url, &file_info, &data_path));
    EXPECT_TRUE(change_observer()->HasNoChange());
    return file_info.last_modified;
  }

  void TestDirectoryTimestampHelper(const FileSystemURL& base_dir,
                                    bool copy,
                                    bool overwrite) {
    scoped_ptr<FileSystemOperationContext> context;
    const FileSystemURL src_dir_url(
        FileSystemURLAppendUTF8(base_dir, "foo_dir"));
    const FileSystemURL dest_dir_url(
        FileSystemURLAppendUTF8(base_dir, "bar_dir"));

    const FileSystemURL src_file_url(
        FileSystemURLAppendUTF8(src_dir_url, "hoge"));
    const FileSystemURL dest_file_url(
        FileSystemURLAppendUTF8(dest_dir_url, "fuga"));

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CreateDirectory(context.get(), src_dir_url, true, true));
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CreateDirectory(context.get(), dest_dir_url, true, true));

    bool created = false;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->EnsureFileExists(context.get(), src_file_url, &created));
    if (overwrite) {
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(context.get(),
                                        dest_file_url, &created));
    }

    ClearTimestamp(src_dir_url);
    ClearTimestamp(dest_dir_url);
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CopyOrMoveFile(context.get(),
                                    src_file_url, dest_file_url,
                                    copy));
    if (copy)
      EXPECT_EQ(base::Time(), GetModifiedTime(src_dir_url));
    else
      EXPECT_NE(base::Time(), GetModifiedTime(src_dir_url));
    EXPECT_NE(base::Time(), GetModifiedTime(dest_dir_url));
  }

  int64 ComputeCurrentUsage() {
    return sandbox_file_system_.ComputeCurrentOriginUsage() -
        sandbox_file_system_.ComputeCurrentDirectoryDatabaseUsage();
  }

  FileSystemContext* file_system_context() {
    return sandbox_file_system_.file_system_context();
  }

  const base::FilePath& data_dir_path() const {
    return data_dir_.path();
  }

 protected:
  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<FileSystemContext> file_system_context_;
  GURL origin_;
  fileapi::FileSystemType type_;
  base::WeakPtrFactory<ObfuscatedFileUtilTest> weak_factory_;
  SandboxFileSystemTestHelper sandbox_file_system_;
  quota::QuotaStatusCode quota_status_;
  int64 usage_;
  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtilTest);
};

TEST_F(ObfuscatedFileUtilTest, TestCreateAndDeleteFile) {
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  bool created;
  FileSystemURL url = CreateURLFromUTF8("fake/file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->CreateOrOpen(
                context.get(), url, file_flags, &file_handle,
                &created));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->DeleteFile(context.get(), url));

  url = CreateURLFromUTF8("test file");

  EXPECT_TRUE(change_observer()->HasNoChange());

  // Verify that file creation requires sufficient quota for the path.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()) - 1);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->CreateOrOpen(
                context.get(), url, file_flags, &file_handle, &created));

  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), url, file_flags, &file_handle, &created));
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  CheckFileAndCloseHandle(url, file_handle);

  context.reset(NewContext(NULL));
  base::FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
      context.get(), url, &local_path));
  EXPECT_TRUE(base::PathExists(local_path));

  // Verify that deleting a file isn't stopped by zero quota, and that it frees
  // up quote from its path.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), url));
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_FALSE(base::PathExists(local_path));
  EXPECT_EQ(ObfuscatedFileUtil::ComputeFilePathCost(url.path()),
      context->allowed_bytes_growth());

  context.reset(NewContext(NULL));
  bool exclusive = true;
  bool recursive = true;
  FileSystemURL directory_url = CreateURLFromUTF8(
      "series/of/directories");
  url = FileSystemURLAppendUTF8(directory_url, "file name");
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), directory_url, exclusive, recursive));
  // The oepration created 3 directories recursively.
  EXPECT_EQ(3, change_observer()->get_and_reset_create_directory_count());

  context.reset(NewContext(NULL));
  file_handle = base::kInvalidPlatformFileValue;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), url, file_flags, &file_handle, &created));
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  CheckFileAndCloseHandle(url, file_handle);

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
      context.get(), url, &local_path));
  EXPECT_TRUE(base::PathExists(local_path));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), url));
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_file_count());
  EXPECT_FALSE(base::PathExists(local_path));

  // Make sure we have no unexpected changes.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(ObfuscatedFileUtilTest, TestTruncate) {
  bool created = false;
  FileSystemURL url = CreateURLFromUTF8("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Truncate(context.get(), url, 4));

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());

  context.reset(NewContext(NULL));
  base::FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetLocalFilePath(
      context.get(), url, &local_path));
  EXPECT_EQ(0, GetSize(local_path));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
      context.get(), url, 10));
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
  EXPECT_EQ(10, GetSize(local_path));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->Truncate(
      context.get(), url, 1));
  EXPECT_EQ(1, GetSize(local_path));
  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());

  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  // Make sure we have no unexpected changes.
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnTruncation) {
  bool created = false;
  FileSystemURL url = CreateURLFromUTF8("file");

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(url))->context(),
                url, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1020)->context(),
                url, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(-1020)->context(),
                url, 0));
  ASSERT_EQ(0, ComputeTotalFileSize());

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->Truncate(
                DisallowUsageIncrease(1021)->context(),
                url, 1021));
  ASSERT_EQ(0, ComputeTotalFileSize());

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1020)->context(),
                url, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(0)->context(),
                url, 1020));
  ASSERT_EQ(1020, ComputeTotalFileSize());

  // quota exceeded
  {
    scoped_ptr<UsageVerifyHelper> helper = AllowUsageIncrease(-1);
    helper->context()->set_allowed_bytes_growth(
        helper->context()->allowed_bytes_growth() - 1);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Truncate(helper->context(), url, 1019));
    ASSERT_EQ(1019, ComputeTotalFileSize());
  }

  // Delete backing file to make following truncation fail.
  base::FilePath local_path;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(
                UnlimitedContext().get(),
                url, &local_path));
  ASSERT_FALSE(local_path.empty());
  ASSERT_TRUE(base::DeleteFile(local_path, false));

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Truncate(
                LimitedContext(1234).get(),
                url, 1234));
  ASSERT_EQ(0, ComputeTotalFileSize());
}

TEST_F(ObfuscatedFileUtilTest, TestEnsureFileExists) {
  FileSystemURL url = CreateURLFromUTF8("fake/file");
  bool created = false;
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->EnsureFileExists(
                context.get(), url, &created));
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Verify that file creation requires sufficient quota for the path.
  context.reset(NewContext(NULL));
  url = CreateURLFromUTF8("test file");
  created = false;
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()) - 1);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_FALSE(created);
  EXPECT_TRUE(change_observer()->HasNoChange());

  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  EXPECT_EQ(1, change_observer()->get_and_reset_create_file_count());

  CheckFileAndCloseHandle(url, base::kInvalidPlatformFileValue);

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_FALSE(created);
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Also test in a subdirectory.
  url = CreateURLFromUTF8("path/to/file.txt");
  context.reset(NewContext(NULL));
  bool exclusive = true;
  bool recursive = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(),
      FileSystemURLDirName(url),
      exclusive, recursive));
  // 2 directories: path/ and path/to.
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryOps) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  bool exclusive = false;
  bool recursive = false;
  FileSystemURL url = CreateURLFromUTF8("foo/bar");
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->DeleteDirectory(context.get(), url));

  FileSystemURL root = CreateURLFromUTF8(std::string());
  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_FALSE(PathExists(url));
  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), root));

  context.reset(NewContext(NULL));
  exclusive = false;
  recursive = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());

  EXPECT_TRUE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->IsDirectoryEmpty(context.get(), root));
  EXPECT_TRUE(DirectoryExists(FileSystemURLDirName(url)));

  context.reset(NewContext(NULL));
  EXPECT_FALSE(ofu()->IsDirectoryEmpty(context.get(),
                                       FileSystemURLDirName(url)));

  // Can't remove a non-empty directory.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
      ofu()->DeleteDirectory(context.get(),
                             FileSystemURLDirName(url)));
  EXPECT_TRUE(change_observer()->HasNoChange());

  base::PlatformFileInfo file_info;
  base::FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
      context.get(), url, &file_info, &local_path));
  EXPECT_TRUE(local_path.empty());
  EXPECT_TRUE(file_info.is_directory);
  EXPECT_FALSE(file_info.is_symbolic_link);

  // Same create again should succeed, since exclusive is false.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  exclusive = true;
  recursive = true;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  // Verify that deleting a directory isn't stopped by zero quota, and that it
  // frees up quota from its path.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->DeleteDirectory(context.get(), url));
  EXPECT_EQ(1, change_observer()->get_and_reset_remove_directory_count());
  EXPECT_EQ(ObfuscatedFileUtil::ComputeFilePathCost(url.path()),
      context->allowed_bytes_growth());

  url = CreateURLFromUTF8("foo/bop");

  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_FALSE(PathExists(url));

  context.reset(NewContext(NULL));
  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), url));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofu()->GetFileInfo(
      context.get(), url, &file_info, &local_path));

  // Verify that file creation requires sufficient quota for the path.
  exclusive = true;
  recursive = false;
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()) - 1);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(url.path()));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());

  EXPECT_TRUE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  exclusive = true;
  recursive = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  exclusive = true;
  recursive = false;
  url = CreateURLFromUTF8("foo");
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());

  url = CreateURLFromUTF8("blah");

  EXPECT_FALSE(DirectoryExists(url));
  EXPECT_FALSE(PathExists(url));

  exclusive = true;
  recursive = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_EQ(1, change_observer()->get_and_reset_create_directory_count());

  EXPECT_TRUE(DirectoryExists(url));
  EXPECT_TRUE(PathExists(url));

  exclusive = true;
  recursive = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(ObfuscatedFileUtilTest, TestReadDirectory) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool exclusive = true;
  bool recursive = true;
  FileSystemURL url = CreateURLFromUTF8("directory/to/use");
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  TestReadDirectoryHelper(url);
}

TEST_F(ObfuscatedFileUtilTest, TestReadRootWithSlash) {
  TestReadDirectoryHelper(CreateURLFromUTF8(std::string()));
}

TEST_F(ObfuscatedFileUtilTest, TestReadRootWithEmptyString) {
  TestReadDirectoryHelper(CreateURLFromUTF8("/"));
}

TEST_F(ObfuscatedFileUtilTest, TestReadDirectoryOnFile) {
  FileSystemURL url = CreateURLFromUTF8("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);

  std::vector<DirectoryEntry> entries;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            AsyncFileTestHelper::ReadDirectory(
                file_system_context(), url, &entries));

  EXPECT_TRUE(ofu()->IsDirectoryEmpty(context.get(), url));
}

TEST_F(ObfuscatedFileUtilTest, TestTouch) {
  FileSystemURL url = CreateURLFromUTF8("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  base::Time last_access_time = base::Time::Now();
  base::Time last_modified_time = base::Time::Now();

  // It's not there yet.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Touch(
                context.get(), url, last_access_time, last_modified_time));

  // OK, now create it.
  context.reset(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  ASSERT_TRUE(created);
  TestTouchHelper(url, true);

  // Now test a directory:
  context.reset(NewContext(NULL));
  bool exclusive = true;
  bool recursive = false;
  url = CreateURLFromUTF8("dir");
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(context.get(),
      url, exclusive, recursive));
  TestTouchHelper(url, false);
}

TEST_F(ObfuscatedFileUtilTest, TestPathQuotas) {
  FileSystemURL url = CreateURLFromUTF8("fake/file");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  url = CreateURLFromUTF8("file name");
  context->set_allowed_bytes_growth(5);
  bool created = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_FALSE(created);
  context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);
  int64 path_cost = ObfuscatedFileUtil::ComputeFilePathCost(url.path());
  EXPECT_EQ(1024 - path_cost, context->allowed_bytes_growth());

  context->set_allowed_bytes_growth(1024);
  bool exclusive = true;
  bool recursive = true;
  url = CreateURLFromUTF8("directory/to/use");
  std::vector<base::FilePath::StringType> components;
  url.path().GetComponents(&components);
  path_cost = 0;
  typedef std::vector<base::FilePath::StringType>::iterator iterator;
  for (iterator iter = components.begin();
       iter != components.end(); ++iter) {
    path_cost += ObfuscatedFileUtil::ComputeFilePathCost(
        base::FilePath(*iter));
  }
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(1024);
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), url, exclusive, recursive));
  EXPECT_EQ(1024 - path_cost, context->allowed_bytes_growth());
}

TEST_F(ObfuscatedFileUtilTest, TestCopyOrMoveFileNotFound) {
  FileSystemURL source_url = CreateURLFromUTF8("path0.txt");
  FileSystemURL dest_url = CreateURLFromUTF8("path1.txt");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  bool is_copy_not_move = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
          is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
  context.reset(NewContext(NULL));
  is_copy_not_move = true;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
          is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
  source_url = CreateURLFromUTF8("dir/dir/file");
  bool exclusive = true;
  bool recursive = true;
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(),
      FileSystemURLDirName(source_url),
      exclusive, recursive));
  EXPECT_EQ(2, change_observer()->get_and_reset_create_directory_count());
  is_copy_not_move = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
          is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
  context.reset(NewContext(NULL));
  is_copy_not_move = true;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofu()->CopyOrMoveFile(context.get(), source_url, dest_url,
          is_copy_not_move));
  EXPECT_TRUE(change_observer()->HasNoChange());
}

TEST_F(ObfuscatedFileUtilTest, TestCopyOrMoveFileSuccess) {
  const int64 kSourceLength = 5;
  const int64 kDestLength = 50;

  for (size_t i = 0; i < arraysize(kCopyMoveTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "kCopyMoveTestCase " << i);
    const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
    SCOPED_TRACE(testing::Message() << "\t is_copy_not_move " <<
      test_case.is_copy_not_move);
    SCOPED_TRACE(testing::Message() << "\t source_path " <<
      test_case.source_path);
    SCOPED_TRACE(testing::Message() << "\t dest_path " <<
      test_case.dest_path);
    SCOPED_TRACE(testing::Message() << "\t cause_overwrite " <<
      test_case.cause_overwrite);
    scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

    bool exclusive = false;
    bool recursive = true;
    FileSystemURL source_url = CreateURLFromUTF8(test_case.source_path);
    FileSystemURL dest_url = CreateURLFromUTF8(test_case.dest_path);

    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
        context.get(),
        FileSystemURLDirName(source_url),
        exclusive, recursive));
    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
        context.get(),
        FileSystemURLDirName(dest_url),
        exclusive, recursive));

    bool created = false;
    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofu()->EnsureFileExists(context.get(), source_url, &created));
    ASSERT_TRUE(created);
    context.reset(NewContext(NULL));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofu()->Truncate(context.get(), source_url, kSourceLength));

    if (test_case.cause_overwrite) {
      context.reset(NewContext(NULL));
      created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(context.get(), dest_url, &created));
      ASSERT_TRUE(created);
      context.reset(NewContext(NULL));
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->Truncate(context.get(), dest_url, kDestLength));
    }

    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->CopyOrMoveFile(context.get(),
        source_url, dest_url, test_case.is_copy_not_move));

    if (test_case.is_copy_not_move) {
      base::PlatformFileInfo file_info;
      base::FilePath local_path;
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
          context.get(), source_url, &file_info, &local_path));
      EXPECT_EQ(kSourceLength, file_info.size);
      EXPECT_EQ(base::PLATFORM_FILE_OK,
                ofu()->DeleteFile(context.get(), source_url));
    } else {
      base::PlatformFileInfo file_info;
      base::FilePath local_path;
      context.reset(NewContext(NULL));
      EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofu()->GetFileInfo(
          context.get(), source_url, &file_info, &local_path));
    }
    base::PlatformFileInfo file_info;
    base::FilePath local_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->GetFileInfo(
        context.get(), dest_url, &file_info, &local_path));
    EXPECT_EQ(kSourceLength, file_info.size);

    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->DeleteFile(context.get(), dest_url));
  }
}

TEST_F(ObfuscatedFileUtilTest, TestCopyPathQuotas) {
  FileSystemURL src_url = CreateURLFromUTF8("src path");
  FileSystemURL dest_url = CreateURLFromUTF8("destination path");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_url, &created));

  bool is_copy = true;
  // Copy, no overwrite.
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()) - 1);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));

  // Copy, with overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));
}

TEST_F(ObfuscatedFileUtilTest, TestMovePathQuotasWithRename) {
  FileSystemURL src_url = CreateURLFromUTF8("src path");
  FileSystemURL dest_url = CreateURLFromUTF8("destination path");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_url, &created));

  bool is_copy = false;
  // Move, rename, no overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()) -
      ObfuscatedFileUtil::ComputeFilePathCost(src_url.path()) - 1);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(
      ObfuscatedFileUtil::ComputeFilePathCost(dest_url.path()) -
      ObfuscatedFileUtil::ComputeFilePathCost(src_url.path()));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));

  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_url, &created));

  // Move, rename, with overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(0);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));
}

TEST_F(ObfuscatedFileUtilTest, TestMovePathQuotasWithoutRename) {
  FileSystemURL src_url = CreateURLFromUTF8("src path");
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_url, &created));

  bool exclusive = true;
  bool recursive = false;
  FileSystemURL dir_url = CreateURLFromUTF8("directory path");
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), dir_url, exclusive, recursive));

  FileSystemURL dest_url = FileSystemURLAppend(
      dir_url, src_url.path().value());

  bool is_copy = false;
  int64 allowed_bytes_growth = -1000;  // Over quota, this should still work.
  // Move, no rename, no overwrite.
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(allowed_bytes_growth);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));
  EXPECT_EQ(allowed_bytes_growth, context->allowed_bytes_growth());

  // Move, no rename, with overwrite.
  context.reset(NewContext(NULL));
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->EnsureFileExists(
      context.get(), src_url, &created));
  context.reset(NewContext(NULL));
  context->set_allowed_bytes_growth(allowed_bytes_growth);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofu()->CopyOrMoveFile(context.get(), src_url, dest_url, is_copy));
  EXPECT_EQ(
      allowed_bytes_growth +
          ObfuscatedFileUtil::ComputeFilePathCost(src_url.path()),
      context->allowed_bytes_growth());
}

TEST_F(ObfuscatedFileUtilTest, TestCopyInForeignFile) {
  TestCopyInForeignFileHelper(false /* overwrite */);
  TestCopyInForeignFileHelper(true /* overwrite */);
}

TEST_F(ObfuscatedFileUtilTest, TestEnumerator) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  FileSystemURL src_url = CreateURLFromUTF8("source dir");
  bool exclusive = true;
  bool recursive = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofu()->CreateDirectory(
      context.get(), src_url, exclusive, recursive));

  std::set<base::FilePath::StringType> files;
  std::set<base::FilePath::StringType> directories;
  FillTestDirectory(src_url, &files, &directories);

  FileSystemURL dest_url = CreateURLFromUTF8("destination dir");

  EXPECT_FALSE(DirectoryExists(dest_url));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            AsyncFileTestHelper::Copy(
                file_system_context(), src_url, dest_url));

  ValidateTestDirectory(dest_url, files, directories);
  EXPECT_TRUE(DirectoryExists(src_url));
  EXPECT_TRUE(DirectoryExists(dest_url));
  recursive = true;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            AsyncFileTestHelper::Remove(
                file_system_context(), dest_url, recursive));
  EXPECT_FALSE(DirectoryExists(dest_url));
}

TEST_F(ObfuscatedFileUtilTest, TestOriginEnumerator) {
  scoped_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator>
      enumerator(ofu()->CreateOriginEnumerator());
  // The test helper starts out with a single filesystem.
  EXPECT_TRUE(enumerator.get());
  EXPECT_EQ(origin(), enumerator->Next());
  ASSERT_TRUE(type() == kFileSystemTypeTemporary);
  EXPECT_TRUE(enumerator->HasFileSystemType(kFileSystemTypeTemporary));
  EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypePersistent));
  EXPECT_EQ(GURL(), enumerator->Next());
  EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypeTemporary));
  EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypePersistent));

  std::set<GURL> origins_expected;
  origins_expected.insert(origin());

  for (size_t i = 0; i < arraysize(kOriginEnumerationTestRecords); ++i) {
    SCOPED_TRACE(testing::Message() <<
        "Validating kOriginEnumerationTestRecords " << i);
    const OriginEnumerationTestRecord& record =
        kOriginEnumerationTestRecords[i];
    GURL origin_url(record.origin_url);
    origins_expected.insert(origin_url);
    if (record.has_temporary) {
      scoped_ptr<SandboxFileSystemTestHelper> file_system(
          NewFileSystem(origin_url, kFileSystemTypeTemporary));
      scoped_ptr<FileSystemOperationContext> context(
          NewContext(file_system.get()));
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(
                    context.get(),
                    file_system->CreateURLFromUTF8("file"),
                    &created));
      EXPECT_TRUE(created);
    }
    if (record.has_persistent) {
      scoped_ptr<SandboxFileSystemTestHelper> file_system(
          NewFileSystem(origin_url, kFileSystemTypePersistent));
      scoped_ptr<FileSystemOperationContext> context(
          NewContext(file_system.get()));
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofu()->EnsureFileExists(
                    context.get(),
                    file_system->CreateURLFromUTF8("file"),
                    &created));
      EXPECT_TRUE(created);
    }
  }
  enumerator.reset(ofu()->CreateOriginEnumerator());
  EXPECT_TRUE(enumerator.get());
  std::set<GURL> origins_found;
  GURL origin_url;
  while (!(origin_url = enumerator->Next()).is_empty()) {
    origins_found.insert(origin_url);
    SCOPED_TRACE(testing::Message() << "Handling " << origin_url.spec());
    bool found = false;
    for (size_t i = 0; !found && i < arraysize(kOriginEnumerationTestRecords);
        ++i) {
      const OriginEnumerationTestRecord& record =
          kOriginEnumerationTestRecords[i];
      if (GURL(record.origin_url) != origin_url)
        continue;
      found = true;
      EXPECT_EQ(record.has_temporary,
          enumerator->HasFileSystemType(kFileSystemTypeTemporary));
      EXPECT_EQ(record.has_persistent,
          enumerator->HasFileSystemType(kFileSystemTypePersistent));
    }
    // Deal with the default filesystem created by the test helper.
    if (!found && origin_url == origin()) {
      ASSERT_TRUE(type() == kFileSystemTypeTemporary);
      EXPECT_EQ(true,
          enumerator->HasFileSystemType(kFileSystemTypeTemporary));
      EXPECT_FALSE(enumerator->HasFileSystemType(kFileSystemTypePersistent));
      found = true;
    }
    EXPECT_TRUE(found);
  }

  std::set<GURL> diff;
  std::set_symmetric_difference(origins_expected.begin(),
      origins_expected.end(), origins_found.begin(), origins_found.end(),
      inserter(diff, diff.begin()));
  EXPECT_TRUE(diff.empty());
}

TEST_F(ObfuscatedFileUtilTest, TestRevokeUsageCache) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));

  int64 expected_quota = 0;

  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kRegularTestCase " << i);
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    base::FilePath file_path(test_case.path);
    expected_quota += ObfuscatedFileUtil::ComputeFilePathCost(file_path);
    if (test_case.is_directory) {
      bool exclusive = true;
      bool recursive = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->CreateDirectory(context.get(), CreateURL(file_path),
                                 exclusive, recursive));
    } else {
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->EnsureFileExists(context.get(), CreateURL(file_path),
                                  &created));
      ASSERT_TRUE(created);
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofu()->Truncate(context.get(),
                          CreateURL(file_path),
                          test_case.data_file_size));
      expected_quota += test_case.data_file_size;
    }
  }

  // Usually raw size in usage cache and the usage returned by QuotaUtil
  // should be same.
  EXPECT_EQ(expected_quota, SizeInUsageFile());
  EXPECT_EQ(expected_quota, SizeByQuotaUtil());

  RevokeUsageCache();
  EXPECT_EQ(-1, SizeInUsageFile());
  EXPECT_EQ(expected_quota, SizeByQuotaUtil());

  // This should reconstruct the cache.
  GetUsageFromQuotaManager();
  EXPECT_EQ(expected_quota, SizeInUsageFile());
  EXPECT_EQ(expected_quota, SizeByQuotaUtil());
  EXPECT_EQ(expected_quota, usage());
}

TEST_F(ObfuscatedFileUtilTest, TestInconsistency) {
  const FileSystemURL kPath1 = CreateURLFromUTF8("hoge");
  const FileSystemURL kPath2 = CreateURLFromUTF8("fuga");

  scoped_ptr<FileSystemOperationContext> context;
  base::PlatformFile file;
  base::PlatformFileInfo file_info;
  base::FilePath data_path;
  bool created = false;

  // Create a non-empty file.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(context.get(), kPath1, 10));
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetFileInfo(
                context.get(), kPath1, &file_info, &data_path));
  EXPECT_EQ(10, file_info.size);

  // Destroy database to make inconsistency between database and filesystem.
  ofu()->DestroyDirectoryDatabase(origin(), type());

  // Try to get file info of broken file.
  EXPECT_FALSE(PathExists(kPath1));
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetFileInfo(
                context.get(), kPath1, &file_info, &data_path));
  EXPECT_EQ(0, file_info.size);

  // Make another broken file to |kPath2|.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath2, &created));
  EXPECT_TRUE(created);

  // Destroy again.
  ofu()->DestroyDirectoryDatabase(origin(), type());

  // Repair broken |kPath1|.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->Touch(context.get(), kPath1, base::Time::Now(),
                           base::Time::Now()));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), kPath1, &created));
  EXPECT_TRUE(created);

  // Copy from sound |kPath1| to broken |kPath2|.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(context.get(), kPath1, kPath2,
                                  true /* copy */));

  ofu()->DestroyDirectoryDatabase(origin(), type());
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), kPath1,
                base::PLATFORM_FILE_READ | base::PLATFORM_FILE_CREATE,
                &file, &created));
  EXPECT_TRUE(created);
  EXPECT_TRUE(base::GetPlatformFileInfo(file, &file_info));
  EXPECT_EQ(0, file_info.size);
  EXPECT_TRUE(base::ClosePlatformFile(file));
}

TEST_F(ObfuscatedFileUtilTest, TestIncompleteDirectoryReading) {
  const FileSystemURL kPath[] = {
    CreateURLFromUTF8("foo"),
    CreateURLFromUTF8("bar"),
    CreateURLFromUTF8("baz")
  };
  const FileSystemURL empty_path = CreateURL(base::FilePath());
  scoped_ptr<FileSystemOperationContext> context;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kPath); ++i) {
    bool created = false;
    context.reset(NewContext(NULL));
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->EnsureFileExists(context.get(), kPath[i], &created));
    EXPECT_TRUE(created);
  }

  std::vector<DirectoryEntry> entries;
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            AsyncFileTestHelper::ReadDirectory(
                file_system_context(), empty_path, &entries));
  EXPECT_EQ(3u, entries.size());

  base::FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(context.get(), kPath[0], &local_path));
  EXPECT_TRUE(base::DeleteFile(local_path, false));

  entries.clear();
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            AsyncFileTestHelper::ReadDirectory(
                file_system_context(), empty_path, &entries));
  EXPECT_EQ(ARRAYSIZE_UNSAFE(kPath) - 1, entries.size());
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryTimestampForCreation) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  const FileSystemURL dir_url = CreateURLFromUTF8("foo_dir");

  // Create working directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), dir_url, false, false));

  // EnsureFileExists, create case.
  FileSystemURL url(FileSystemURLAppendUTF8(
          dir_url, "EnsureFileExists_file"));
  bool created = false;
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // non create case.
  created = true;
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_FALSE(created);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // fail case.
  url = FileSystemURLAppendUTF8(dir_url, "EnsureFileExists_dir");
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), url, false, false));

  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // CreateOrOpen, create case.
  url = FileSystemURLAppendUTF8(dir_url, "CreateOrOpen_file");
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  created = false;
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), url,
                base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);
  EXPECT_TRUE(created);
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // open case.
  file_handle = base::kInvalidPlatformFileValue;
  created = true;
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                context.get(), url,
                base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);
  EXPECT_FALSE(created);
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // fail case
  file_handle = base::kInvalidPlatformFileValue;
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS,
            ofu()->CreateOrOpen(
                context.get(), url,
                base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  EXPECT_EQ(base::kInvalidPlatformFileValue, file_handle);
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // CreateDirectory, create case.
  // Creating CreateDirectory_dir and CreateDirectory_dir/subdir.
  url = FileSystemURLAppendUTF8(dir_url, "CreateDirectory_dir");
  FileSystemURL subdir_url(FileSystemURLAppendUTF8(url, "subdir"));
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), subdir_url,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // create subdir case.
  // Creating CreateDirectory_dir/subdir2.
  subdir_url = FileSystemURLAppendUTF8(url, "subdir2");
  ClearTimestamp(dir_url);
  ClearTimestamp(url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), subdir_url,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));
  EXPECT_NE(base::Time(), GetModifiedTime(url));

  // fail case.
  url = FileSystemURLAppendUTF8(dir_url, "CreateDirectory_dir");
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS,
            ofu()->CreateDirectory(context.get(), url,
                                   true /* exclusive */, true /* recursive */));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // CopyInForeignFile, create case.
  url = FileSystemURLAppendUTF8(dir_url, "CopyInForeignFile_file");
  FileSystemURL src_path = FileSystemURLAppendUTF8(
      dir_url, "CopyInForeignFile_src_file");
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), src_path, &created));
  EXPECT_TRUE(created);
  base::FilePath src_local_path;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(context.get(), src_path, &src_local_path));

  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyInForeignFile(context.get(),
                                     src_local_path,
                                     url));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryTimestampForDeletion) {
  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  const FileSystemURL dir_url = CreateURLFromUTF8("foo_dir");

  // Create working directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), dir_url, false, false));

  // DeleteFile, delete case.
  FileSystemURL url = FileSystemURLAppendUTF8(
      dir_url, "DeleteFile_file");
  bool created = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), url));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));

  // fail case.
  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofu()->DeleteFile(context.get(), url));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // DeleteDirectory, fail case.
  url = FileSystemURLAppendUTF8(dir_url, "DeleteDirectory_dir");
  FileSystemURL file_path(FileSystemURLAppendUTF8(url, "pakeratta"));
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), url, true, true));
  created = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), file_path, &created));
  EXPECT_TRUE(created);

  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
            ofu()->DeleteDirectory(context.get(), url));
  EXPECT_EQ(base::Time(), GetModifiedTime(dir_url));

  // delete case.
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(context.get(), file_path));

  ClearTimestamp(dir_url);
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofu()->DeleteDirectory(context.get(), url));
  EXPECT_NE(base::Time(), GetModifiedTime(dir_url));
}

TEST_F(ObfuscatedFileUtilTest, TestDirectoryTimestampForCopyAndMove) {
  TestDirectoryTimestampHelper(
      CreateURLFromUTF8("copy overwrite"), true, true);
  TestDirectoryTimestampHelper(
      CreateURLFromUTF8("copy non-overwrite"), true, false);
  TestDirectoryTimestampHelper(
      CreateURLFromUTF8("move overwrite"), false, true);
  TestDirectoryTimestampHelper(
      CreateURLFromUTF8("move non-overwrite"), false, false);
}

TEST_F(ObfuscatedFileUtilTest, TestFileEnumeratorTimestamp) {
  FileSystemURL dir = CreateURLFromUTF8("foo");
  FileSystemURL url1 = FileSystemURLAppendUTF8(dir, "bar");
  FileSystemURL url2 = FileSystemURLAppendUTF8(dir, "baz");

  scoped_ptr<FileSystemOperationContext> context(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), dir, false, false));

  bool created = false;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(context.get(), url1, &created));
  EXPECT_TRUE(created);

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(context.get(), url2, false, false));

  base::FilePath file_path;
  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->GetLocalFilePath(context.get(), url1, &file_path));
  EXPECT_FALSE(file_path.empty());

  context.reset(NewContext(NULL));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Touch(context.get(), url1,
                         base::Time::Now() + base::TimeDelta::FromHours(1),
                         base::Time()));

  context.reset(NewContext(NULL));
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      ofu()->CreateFileEnumerator(context.get(), dir, false));

  int count = 0;
  base::FilePath file_path_each;
  while (!(file_path_each = file_enum->Next()).empty()) {
    context.reset(NewContext(NULL));
    base::PlatformFileInfo file_info;
    base::FilePath file_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofu()->GetFileInfo(context.get(),
                                 FileSystemURL::CreateForTest(
                                     dir.origin(),
                                     dir.mount_type(),
                                     file_path_each),
                                 &file_info, &file_path));
    EXPECT_EQ(file_info.is_directory, file_enum->IsDirectory());
    EXPECT_EQ(file_info.last_modified, file_enum->LastModifiedTime());
    EXPECT_EQ(file_info.size, file_enum->Size());
    ++count;
  }
  EXPECT_EQ(2, count);
}

// crbug.com/176470
#if defined(OS_WIN) || defined(OS_ANDROID)
#define MAYBE_TestQuotaOnCopyFile DISABLED_TestQuotaOnCopyFile
#else
#define MAYBE_TestQuotaOnCopyFile TestQuotaOnCopyFile
#endif
TEST_F(ObfuscatedFileUtilTest, MAYBE_TestQuotaOnCopyFile) {
  FileSystemURL from_file(CreateURLFromUTF8("fromfile"));
  FileSystemURL obstacle_file(CreateURLFromUTF8("obstaclefile"));
  FileSystemURL to_file1(CreateURLFromUTF8("tofile1"));
  FileSystemURL to_file2(CreateURLFromUTF8("tofile2"));
  bool created;

  int64 expected_total_file_size = 0;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(obstacle_file))->context(),
                obstacle_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 obstacle_file_size = 1;
  expected_total_file_size += obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(obstacle_file_size)->context(),
                obstacle_file, obstacle_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 to_file1_size = from_file_size;
  expected_total_file_size += to_file1_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(
                    PathCost(to_file1) + to_file1_size)->context(),
                from_file, to_file1, true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            ofu()->CopyOrMoveFile(
                DisallowUsageIncrease(
                    PathCost(to_file2) + from_file_size)->context(),
                from_file, to_file2, true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  expected_total_file_size += obstacle_file_size - old_obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(
                    obstacle_file_size - old_obstacle_file_size)->context(),
                from_file, obstacle_file, true /* copy */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 old_from_file_size = from_file_size;
  from_file_size = old_from_file_size - 1;
  expected_total_file_size += from_file_size - old_from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(
                    from_file_size - old_from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  // quota exceeded
  {
    old_obstacle_file_size = obstacle_file_size;
    obstacle_file_size = from_file_size;
    expected_total_file_size += obstacle_file_size - old_obstacle_file_size;
    scoped_ptr<UsageVerifyHelper> helper = AllowUsageIncrease(
        obstacle_file_size - old_obstacle_file_size);
    helper->context()->set_allowed_bytes_growth(
        helper->context()->allowed_bytes_growth() - 1);
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofu()->CopyOrMoveFile(
                  helper->context(),
                  from_file, obstacle_file, true /* copy */));
    ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());
  }
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnMoveFile) {
  FileSystemURL from_file(CreateURLFromUTF8("fromfile"));
  FileSystemURL obstacle_file(CreateURLFromUTF8("obstaclefile"));
  FileSystemURL to_file(CreateURLFromUTF8("tofile"));
  bool created;

  int64 expected_total_file_size = 0;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 to_file_size ALLOW_UNUSED = from_file_size;
  from_file_size = 0;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(-PathCost(from_file) +
                                   PathCost(to_file))->context(),
                from_file, to_file, false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(obstacle_file))->context(),
                obstacle_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 1020;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 obstacle_file_size = 1;
  expected_total_file_size += obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1)->context(),
                obstacle_file, obstacle_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  int64 old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  from_file_size = 0;
  expected_total_file_size -= old_obstacle_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                AllowUsageIncrease(
                    -old_obstacle_file_size - PathCost(from_file))->context(),
                from_file, obstacle_file,
                false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(from_file))->context(),
                from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  from_file_size = 10;
  expected_total_file_size += from_file_size;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(from_file_size)->context(),
                from_file, from_file_size));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());

  // quota exceeded even after operation
  old_obstacle_file_size = obstacle_file_size;
  obstacle_file_size = from_file_size;
  from_file_size = 0;
  expected_total_file_size -= old_obstacle_file_size;
  scoped_ptr<FileSystemOperationContext> context =
      LimitedContext(-old_obstacle_file_size - PathCost(from_file) - 1);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CopyOrMoveFile(
                context.get(), from_file, obstacle_file, false /* move */));
  ASSERT_EQ(expected_total_file_size, ComputeTotalFileSize());
  context.reset();
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnRemove) {
  FileSystemURL dir(CreateURLFromUTF8("dir"));
  FileSystemURL file(CreateURLFromUTF8("file"));
  FileSystemURL dfile1(CreateURLFromUTF8("dir/dfile1"));
  FileSystemURL dfile2(CreateURLFromUTF8("dir/dfile2"));
  bool created;

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(file))->context(),
                file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateDirectory(
                AllowUsageIncrease(PathCost(dir))->context(),
                dir, false, false));
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(dfile1))->context(),
                dfile1, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(dfile2))->context(),
                dfile2, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(340)->context(),
                file, 340));
  ASSERT_EQ(340, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(1020)->context(),
                dfile1, 1020));
  ASSERT_EQ(1360, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(120)->context(),
                dfile2, 120));
  ASSERT_EQ(1480, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->DeleteFile(
                AllowUsageIncrease(-PathCost(file) - 340)->context(),
                file));
  ASSERT_EQ(1140, ComputeTotalFileSize());

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            AsyncFileTestHelper::Remove(
                file_system_context(), dir, true /* recursive */));
  ASSERT_EQ(0, ComputeTotalFileSize());
}

TEST_F(ObfuscatedFileUtilTest, TestQuotaOnOpen) {
  FileSystemURL file(CreateURLFromUTF8("file"));
  base::PlatformFile file_handle;
  bool created;

  // Creating a file.
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(
                AllowUsageIncrease(PathCost(file))->context(),
                file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(0, ComputeTotalFileSize());

  // Opening it, which shouldn't change the usage.
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                AllowUsageIncrease(0)->context(), file,
                base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  ASSERT_EQ(0, ComputeTotalFileSize());
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));

  const int length = 33;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(length)->context(), file, length));
  ASSERT_EQ(length, ComputeTotalFileSize());

  // Opening it with CREATE_ALWAYS flag, which should truncate the file size.
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                AllowUsageIncrease(-length)->context(), file,
                base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  ASSERT_EQ(0, ComputeTotalFileSize());
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));

  // Extending the file again.
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->Truncate(
                AllowUsageIncrease(length)->context(), file, length));
  ASSERT_EQ(length, ComputeTotalFileSize());

  // Opening it with TRUNCATED flag, which should truncate the file size.
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->CreateOrOpen(
                AllowUsageIncrease(-length)->context(), file,
                base::PLATFORM_FILE_OPEN_TRUNCATED | base::PLATFORM_FILE_WRITE,
                &file_handle, &created));
  ASSERT_EQ(0, ComputeTotalFileSize());
  EXPECT_TRUE(base::ClosePlatformFile(file_handle));
}

TEST_F(ObfuscatedFileUtilTest, MaybeDropDatabasesAliveCase) {
  ObfuscatedFileUtil file_util(NULL,
                               data_dir_path(),
                               base::MessageLoopProxy::current().get());
  file_util.InitOriginDatabase(true /*create*/);
  ASSERT_TRUE(file_util.origin_database_ != NULL);

  // Callback to Drop DB is called while ObfuscatedFileUtilTest is still alive.
  file_util.db_flush_delay_seconds_ = 0;
  file_util.MarkUsed();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(file_util.origin_database_ == NULL);
}

TEST_F(ObfuscatedFileUtilTest, MaybeDropDatabasesAlreadyDeletedCase) {
  // Run message loop after OFU is already deleted to make sure callback doesn't
  // cause a crash for use after free.
  {
    ObfuscatedFileUtil file_util(NULL,
                                 data_dir_path(),
                                 base::MessageLoopProxy::current().get());
    file_util.InitOriginDatabase(true /*create*/);
    file_util.db_flush_delay_seconds_ = 0;
    file_util.MarkUsed();
  }

  // At this point the callback is still in the message queue but OFU is gone.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ObfuscatedFileUtilTest, DestroyDirectoryDatabase_Isolated) {
  storage_policy_->AddIsolated(origin_);
  ObfuscatedFileUtil file_util(
      storage_policy_.get(), data_dir_path(),
      base::MessageLoopProxy::current().get());

  // Create DirectoryDatabase for isolated origin.
  SandboxDirectoryDatabase* db = file_util.GetDirectoryDatabase(
      origin_, kFileSystemTypePersistent, true /* create */);
  ASSERT_TRUE(db != NULL);

  // Destory it.
  ASSERT_TRUE(
      file_util.DestroyDirectoryDatabase(origin_, kFileSystemTypePersistent));
  ASSERT_TRUE(file_util.directories_.empty());
}

TEST_F(ObfuscatedFileUtilTest, GetDirectoryDatabase_Isolated) {
  storage_policy_->AddIsolated(origin_);
  ObfuscatedFileUtil file_util(
      storage_policy_.get(), data_dir_path(),
      base::MessageLoopProxy::current().get());

  // Create DirectoryDatabase for isolated origin.
  SandboxDirectoryDatabase* db = file_util.GetDirectoryDatabase(
      origin_, kFileSystemTypePersistent, true /* create */);
  ASSERT_TRUE(db != NULL);
  ASSERT_EQ(1U, file_util.directories_.size());

  // Remove isolated.
  storage_policy_->RemoveIsolated(origin_);

  // This should still get the same database.
  SandboxDirectoryDatabase* db2 = file_util.GetDirectoryDatabase(
      origin_, kFileSystemTypePersistent, false /* create */);
  ASSERT_EQ(db, db2);
}

TEST_F(ObfuscatedFileUtilTest, MigrationBackFromIsolated) {
  std::string kFakeDirectoryData("0123456789");
  base::FilePath old_directory_db_path;

  // Initialize the directory with one origin using
  // SandboxIsolatedOriginDatabase.
  {
    std::string origin_string =
        webkit_database::GetIdentifierFromOrigin(origin_);
    SandboxIsolatedOriginDatabase database_old(origin_string, data_dir_path());
    base::FilePath path;
    EXPECT_TRUE(database_old.GetPathForOrigin(origin_string, &path));
    EXPECT_FALSE(path.empty());

    // Populate the origin directory with some fake data.
    old_directory_db_path = data_dir_path().Append(path);
    ASSERT_TRUE(file_util::CreateDirectory(old_directory_db_path));
    EXPECT_EQ(static_cast<int>(kFakeDirectoryData.size()),
              file_util::WriteFile(old_directory_db_path.AppendASCII("dummy"),
                                   kFakeDirectoryData.data(),
                                   kFakeDirectoryData.size()));
  }

  storage_policy_->AddIsolated(origin_);
  ObfuscatedFileUtil file_util(
      storage_policy_.get(), data_dir_path(),
      base::MessageLoopProxy::current().get());
  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::FilePath origin_directory = file_util.GetDirectoryForOrigin(
      origin_, true /* create */, &error);
  EXPECT_EQ(base::PLATFORM_FILE_OK, error);

  // The database is migrated from the old one.
  EXPECT_TRUE(base::DirectoryExists(origin_directory));
  EXPECT_FALSE(base::DirectoryExists(old_directory_db_path));

  // Check we see the same contents in the new origin directory.
  std::string origin_db_data;
  EXPECT_TRUE(base::PathExists(origin_directory.AppendASCII("dummy")));
  EXPECT_TRUE(base::ReadFileToString(
      origin_directory.AppendASCII("dummy"), &origin_db_data));
  EXPECT_EQ(kFakeDirectoryData, origin_db_data);
}

TEST_F(ObfuscatedFileUtilTest, OpenPathInNonDirectory) {
  FileSystemURL file(CreateURLFromUTF8("file"));
  FileSystemURL path_in_file(CreateURLFromUTF8("file/file"));
  bool created;

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofu()->EnsureFileExists(UnlimitedContext().get(), file, &created));
  ASSERT_TRUE(created);

  created = false;
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            ofu()->CreateOrOpen(UnlimitedContext().get(),
                                path_in_file,
                                file_flags,
                                &file_handle,
                                &created));
  ASSERT_FALSE(created);
  ASSERT_EQ(base::kInvalidPlatformFileValue, file_handle);

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            ofu()->CreateDirectory(UnlimitedContext().get(),
                                   path_in_file,
                                   false /* exclusive */,
                                   false /* recursive */));
}

}  // namespace fileapi
