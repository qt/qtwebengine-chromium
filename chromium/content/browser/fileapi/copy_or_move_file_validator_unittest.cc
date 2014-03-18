// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "content/public/test/test_file_system_backend.h"
#include "content/public/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_test_helper.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

namespace {

const FileSystemType kNoValidatorType = kFileSystemTypeTemporary;
const FileSystemType kWithValidatorType = kFileSystemTypeTest;

void ExpectOk(const GURL& origin_url,
              const std::string& name,
              base::PlatformFileError error) {
  ASSERT_EQ(base::PLATFORM_FILE_OK, error);
}

class CopyOrMoveFileValidatorTestHelper {
 public:
  CopyOrMoveFileValidatorTestHelper(
      const GURL& origin,
      FileSystemType src_type,
      FileSystemType dest_type)
      : origin_(origin),
        src_type_(src_type),
        dest_type_(dest_type) {}

  ~CopyOrMoveFileValidatorTestHelper() {
    file_system_context_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.path();

    file_system_context_ = CreateFileSystemContextForTesting(NULL, base_dir);

    // Set up TestFileSystemBackend to require CopyOrMoveFileValidator.
    FileSystemBackend* test_file_system_backend =
        file_system_context_->GetFileSystemBackend(kWithValidatorType);
    static_cast<TestFileSystemBackend*>(test_file_system_backend)->
        set_require_copy_or_move_validator(true);

    // Sets up source.
    FileSystemBackend* src_file_system_backend =
        file_system_context_->GetFileSystemBackend(src_type_);
    src_file_system_backend->OpenFileSystem(
        origin_, src_type_,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::Bind(&ExpectOk));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(base::PLATFORM_FILE_OK, CreateDirectory(SourceURL("")));

    // Sets up dest.
    DCHECK_EQ(kWithValidatorType, dest_type_);
    ASSERT_EQ(base::PLATFORM_FILE_OK, CreateDirectory(DestURL("")));

    copy_src_ = SourceURL("copy_src.jpg");
    move_src_ = SourceURL("move_src.jpg");
    copy_dest_ = DestURL("copy_dest.jpg");
    move_dest_ = DestURL("move_dest.jpg");

    ASSERT_EQ(base::PLATFORM_FILE_OK, CreateFile(copy_src_, 10));
    ASSERT_EQ(base::PLATFORM_FILE_OK, CreateFile(move_src_, 10));

    ASSERT_TRUE(FileExists(copy_src_, 10));
    ASSERT_TRUE(FileExists(move_src_, 10));
    ASSERT_FALSE(FileExists(copy_dest_, 10));
    ASSERT_FALSE(FileExists(move_dest_, 10));
  }

  void SetMediaCopyOrMoveFileValidatorFactory(
      scoped_ptr<CopyOrMoveFileValidatorFactory> factory) {
    TestFileSystemBackend* backend = static_cast<TestFileSystemBackend*>(
        file_system_context_->GetFileSystemBackend(kWithValidatorType));
    backend->InitializeCopyOrMoveFileValidatorFactory(factory.Pass());
  }

  void CopyTest(base::PlatformFileError expected) {
    ASSERT_TRUE(FileExists(copy_src_, 10));
    ASSERT_FALSE(FileExists(copy_dest_, 10));

    EXPECT_EQ(expected,
              AsyncFileTestHelper::Copy(
                  file_system_context_.get(), copy_src_, copy_dest_));

    EXPECT_TRUE(FileExists(copy_src_, 10));
    if (expected == base::PLATFORM_FILE_OK)
      EXPECT_TRUE(FileExists(copy_dest_, 10));
    else
      EXPECT_FALSE(FileExists(copy_dest_, 10));
  };

  void MoveTest(base::PlatformFileError expected) {
    ASSERT_TRUE(FileExists(move_src_, 10));
    ASSERT_FALSE(FileExists(move_dest_, 10));

    EXPECT_EQ(expected,
              AsyncFileTestHelper::Move(
                  file_system_context_.get(), move_src_, move_dest_));

    if (expected == base::PLATFORM_FILE_OK) {
      EXPECT_FALSE(FileExists(move_src_, 10));
      EXPECT_TRUE(FileExists(move_dest_, 10));
    } else {
      EXPECT_TRUE(FileExists(move_src_, 10));
      EXPECT_FALSE(FileExists(move_dest_, 10));
    }
  };

 private:
  FileSystemURL SourceURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, src_type_,
        base::FilePath().AppendASCII("src").AppendASCII(path));
  }

  FileSystemURL DestURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, dest_type_,
        base::FilePath().AppendASCII("dest").AppendASCII(path));
  }

  base::PlatformFileError CreateFile(const FileSystemURL& url, size_t size) {
    base::PlatformFileError result =
        AsyncFileTestHelper::CreateFile(file_system_context_.get(), url);
    if (result != base::PLATFORM_FILE_OK)
      return result;
    return AsyncFileTestHelper::TruncateFile(
        file_system_context_.get(), url, size);
  }

  base::PlatformFileError CreateDirectory(const FileSystemURL& url) {
    return AsyncFileTestHelper::CreateDirectory(file_system_context_.get(),
                                                url);
  }

  bool FileExists(const FileSystemURL& url, int64 expected_size) {
    return AsyncFileTestHelper::FileExists(
        file_system_context_.get(), url, expected_size);
  }

  base::ScopedTempDir base_;

  const GURL origin_;

  const FileSystemType src_type_;
  const FileSystemType dest_type_;
  std::string src_fsid_;
  std::string dest_fsid_;

  base::MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> file_system_context_;

  FileSystemURL copy_src_;
  FileSystemURL copy_dest_;
  FileSystemURL move_src_;
  FileSystemURL move_dest_;

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveFileValidatorTestHelper);
};

// For TestCopyOrMoveFileValidatorFactory
enum Validity {
  VALID,
  PRE_WRITE_INVALID,
  POST_WRITE_INVALID
};

class TestCopyOrMoveFileValidatorFactory
    : public CopyOrMoveFileValidatorFactory {
 public:
  // A factory that creates validators that accept everything or nothing.
  // TODO(gbillock): switch args to enum or something
  explicit TestCopyOrMoveFileValidatorFactory(Validity validity)
      : validity_(validity) {}
  virtual ~TestCopyOrMoveFileValidatorFactory() {}

  virtual CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& /*src_url*/,
      const base::FilePath& /*platform_path*/) OVERRIDE {
    return new TestCopyOrMoveFileValidator(validity_);
  }

 private:
  class TestCopyOrMoveFileValidator : public CopyOrMoveFileValidator {
   public:
    explicit TestCopyOrMoveFileValidator(Validity validity)
        : result_(validity == VALID || validity == POST_WRITE_INVALID
                  ? base::PLATFORM_FILE_OK
                  : base::PLATFORM_FILE_ERROR_SECURITY),
          write_result_(validity == VALID || validity == PRE_WRITE_INVALID
                        ? base::PLATFORM_FILE_OK
                        : base::PLATFORM_FILE_ERROR_SECURITY) {
    }
    virtual ~TestCopyOrMoveFileValidator() {}

    virtual void StartPreWriteValidation(
        const ResultCallback& result_callback) OVERRIDE {
      // Post the result since a real validator must do work asynchronously.
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(result_callback, result_));
    }

    virtual void StartPostWriteValidation(
        const base::FilePath& dest_platform_path,
        const ResultCallback& result_callback) OVERRIDE {
      // Post the result since a real validator must do work asynchronously.
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(result_callback, write_result_));
    }

   private:
    base::PlatformFileError result_;
    base::PlatformFileError write_result_;

    DISALLOW_COPY_AND_ASSIGN(TestCopyOrMoveFileValidator);
  };

  Validity validity_;

  DISALLOW_COPY_AND_ASSIGN(TestCopyOrMoveFileValidatorFactory);
};

}  // namespace

TEST(CopyOrMoveFileValidatorTest, NoValidatorWithinSameFSType) {
  // Within a file system type, validation is not expected, so it should
  // work for kWithValidatorType without a validator set.
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"),
                                           kWithValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  helper.CopyTest(base::PLATFORM_FILE_OK);
  helper.MoveTest(base::PLATFORM_FILE_OK);
}

TEST(CopyOrMoveFileValidatorTest, MissingValidator) {
  // Copying or moving into a kWithValidatorType requires a file
  // validator.  An error is expected if copy is attempted without a validator.
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"),
                                           kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  helper.CopyTest(base::PLATFORM_FILE_ERROR_SECURITY);
  helper.MoveTest(base::PLATFORM_FILE_ERROR_SECURITY);
}

TEST(CopyOrMoveFileValidatorTest, AcceptAll) {
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"),
                                           kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  scoped_ptr<CopyOrMoveFileValidatorFactory> factory(
      new TestCopyOrMoveFileValidatorFactory(VALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(factory.Pass());

  helper.CopyTest(base::PLATFORM_FILE_OK);
  helper.MoveTest(base::PLATFORM_FILE_OK);
}

TEST(CopyOrMoveFileValidatorTest, AcceptNone) {
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"),
                                           kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  scoped_ptr<CopyOrMoveFileValidatorFactory> factory(
      new TestCopyOrMoveFileValidatorFactory(PRE_WRITE_INVALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(factory.Pass());

  helper.CopyTest(base::PLATFORM_FILE_ERROR_SECURITY);
  helper.MoveTest(base::PLATFORM_FILE_ERROR_SECURITY);
}

TEST(CopyOrMoveFileValidatorTest, OverrideValidator) {
  // Once set, you can not override the validator.
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"),
                                           kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  scoped_ptr<CopyOrMoveFileValidatorFactory> reject_factory(
      new TestCopyOrMoveFileValidatorFactory(PRE_WRITE_INVALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(reject_factory.Pass());

  scoped_ptr<CopyOrMoveFileValidatorFactory> accept_factory(
      new TestCopyOrMoveFileValidatorFactory(VALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(accept_factory.Pass());

  helper.CopyTest(base::PLATFORM_FILE_ERROR_SECURITY);
  helper.MoveTest(base::PLATFORM_FILE_ERROR_SECURITY);
}

TEST(CopyOrMoveFileValidatorTest, RejectPostWrite) {
  CopyOrMoveFileValidatorTestHelper helper(GURL("http://foo"),
                                           kNoValidatorType,
                                           kWithValidatorType);
  helper.SetUp();
  scoped_ptr<CopyOrMoveFileValidatorFactory> factory(
      new TestCopyOrMoveFileValidatorFactory(POST_WRITE_INVALID));
  helper.SetMediaCopyOrMoveFileValidatorFactory(factory.Pass());

  helper.CopyTest(base::PLATFORM_FILE_ERROR_SECURITY);
  helper.MoveTest(base::PLATFORM_FILE_ERROR_SECURITY);
}

}  // namespace fileapi
