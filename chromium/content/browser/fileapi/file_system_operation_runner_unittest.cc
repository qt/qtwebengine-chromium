// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/run_loop.h"
#include "content/public/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

namespace fileapi {

void GetStatus(bool* done,
               base::PlatformFileError *status_out,
               base::PlatformFileError status) {
  ASSERT_FALSE(*done);
  *done = true;
  *status_out = status;
}

void GetCancelStatus(bool* operation_done,
                     bool* cancel_done,
                     base::PlatformFileError *status_out,
                     base::PlatformFileError status) {
  // Cancel callback must be always called after the operation's callback.
  ASSERT_TRUE(*operation_done);
  ASSERT_FALSE(*cancel_done);
  *cancel_done = true;
  *status_out = status;
}

class FileSystemOperationRunnerTest : public testing::Test {
 protected:
  FileSystemOperationRunnerTest() {}
  virtual ~FileSystemOperationRunnerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base_.CreateUniqueTempDir());
    base::FilePath base_dir = base_.path();
    file_system_context_ =
        CreateFileSystemContextForTesting(NULL, base_dir);
  }

  virtual void TearDown() OVERRIDE {
    file_system_context_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://example.com"), kFileSystemTypeTemporary,
        base::FilePath::FromUTF8Unsafe(path));
  }

  FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

 private:
  base::ScopedTempDir base_;
  base::MessageLoop message_loop_;
  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationRunnerTest);
};

TEST_F(FileSystemOperationRunnerTest, NotFoundError) {
  bool done = false;
  base::PlatformFileError status = base::PLATFORM_FILE_ERROR_FAILED;

  // Regular NOT_FOUND error, which is called asynchronously.
  operation_runner()->Truncate(URL("foo"), 0,
                               base::Bind(&GetStatus, &done, &status));
  ASSERT_FALSE(done);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(done);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status);
}

TEST_F(FileSystemOperationRunnerTest, InvalidURLError) {
  bool done = false;
  base::PlatformFileError status = base::PLATFORM_FILE_ERROR_FAILED;

  // Invalid URL error, which calls DidFinish synchronously.
  operation_runner()->Truncate(FileSystemURL(), 0,
                               base::Bind(&GetStatus, &done, &status));
  // The error call back shouldn't be fired synchronously.
  ASSERT_FALSE(done);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(done);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_URL, status);
}

TEST_F(FileSystemOperationRunnerTest, NotFoundErrorAndCancel) {
  bool done = false;
  bool cancel_done = false;
  base::PlatformFileError status = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFileError cancel_status = base::PLATFORM_FILE_ERROR_FAILED;

  // Call Truncate with non-existent URL, and try to cancel it immediately
  // after that (before its callback is fired).
  FileSystemOperationRunner::OperationID id =
      operation_runner()->Truncate(URL("foo"), 0,
                                   base::Bind(&GetStatus, &done, &status));
  operation_runner()->Cancel(id, base::Bind(&GetCancelStatus,
                                            &done, &cancel_done,
                                            &cancel_status));

  ASSERT_FALSE(done);
  ASSERT_FALSE(cancel_done);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(done);
  ASSERT_TRUE(cancel_done);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, cancel_status);
}

TEST_F(FileSystemOperationRunnerTest, InvalidURLErrorAndCancel) {
  bool done = false;
  bool cancel_done = false;
  base::PlatformFileError status = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFileError cancel_status = base::PLATFORM_FILE_ERROR_FAILED;

  // Call Truncate with invalid URL, and try to cancel it immediately
  // after that (before its callback is fired).
  FileSystemOperationRunner::OperationID id =
      operation_runner()->Truncate(FileSystemURL(), 0,
                                  base::Bind(&GetStatus, &done, &status));
  operation_runner()->Cancel(id, base::Bind(&GetCancelStatus,
                                            &done, &cancel_done,
                                            &cancel_status));

  ASSERT_FALSE(done);
  ASSERT_FALSE(cancel_done);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(done);
  ASSERT_TRUE(cancel_done);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_URL, status);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, cancel_status);
}

TEST_F(FileSystemOperationRunnerTest, CancelWithInvalidId) {
  const FileSystemOperationRunner::OperationID kInvalidId = -1;
  bool done = true;  // The operation is not running.
  bool cancel_done = false;
  base::PlatformFileError cancel_status = base::PLATFORM_FILE_ERROR_FAILED;
  operation_runner()->Cancel(kInvalidId, base::Bind(&GetCancelStatus,
                                                    &done, &cancel_done,
                                                    &cancel_status));

  ASSERT_TRUE(cancel_done);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, cancel_status);
}

}  // namespace fileapi
