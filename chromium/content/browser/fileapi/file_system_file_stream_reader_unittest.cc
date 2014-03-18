// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_file_stream_reader.h"

#include <limits>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/run_loop.h"
#include "content/public/test/test_file_system_context.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_test_helper.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"

namespace fileapi {

namespace {

const char kURLOrigin[] = "http://remote/";
const char kTestFileName[] = "test.dat";
const char kTestData[] = "0123456789";
const int kTestDataSize = arraysize(kTestData) - 1;

void ReadFromReader(FileSystemFileStreamReader* reader,
                    std::string* data,
                    size_t size,
                    int* result) {
  ASSERT_TRUE(reader != NULL);
  ASSERT_TRUE(result != NULL);
  *result = net::OK;
  net::TestCompletionCallback callback;
  size_t total_bytes_read = 0;
  while (total_bytes_read < size) {
    scoped_refptr<net::IOBufferWithSize> buf(
        new net::IOBufferWithSize(size - total_bytes_read));
    int rv = reader->Read(buf.get(), buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      *result = rv;
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data->append(buf->data(), rv);
  }
}

void NeverCalled(int unused) { ADD_FAILURE(); }

}  // namespace

class FileSystemFileStreamReaderTest : public testing::Test {
 public:
  FileSystemFileStreamReaderTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = CreateFileSystemContextForTesting(
        NULL, temp_dir_.path());

    file_system_context_->OpenFileSystem(
        GURL(kURLOrigin), kFileSystemTypeTemporary,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::Bind(&OnOpenFileSystem));
    base::RunLoop().RunUntilIdle();

    WriteFile(kTestFileName, kTestData, kTestDataSize,
              &test_file_modification_time_);
  }

  virtual void TearDown() OVERRIDE {
    base::RunLoop().RunUntilIdle();
  }

 protected:
  FileSystemFileStreamReader* CreateFileReader(
      const std::string& file_name,
      int64 initial_offset,
      const base::Time& expected_modification_time) {
    return new FileSystemFileStreamReader(file_system_context_.get(),
                                          GetFileSystemURL(file_name),
                                          initial_offset,
                                          expected_modification_time);
  }

  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

  void WriteFile(const std::string& file_name,
                 const char* buf,
                 int buf_size,
                 base::Time* modification_time) {
    FileSystemURL url = GetFileSystemURL(file_name);

    ASSERT_EQ(base::PLATFORM_FILE_OK,
              fileapi::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_, url, buf, buf_size));

    base::PlatformFileInfo file_info;
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              AsyncFileTestHelper::GetMetadata(
                  file_system_context_, url, &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

 private:
  static void OnOpenFileSystem(const GURL& root_url,
                               const std::string& name,
                               base::PlatformFileError result) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
  }

  FileSystemURL GetFileSystemURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL(kURLOrigin),
        kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(file_name));
  }

  base::MessageLoopForIO message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::Time test_file_modification_time_;
};

TEST_F(FileSystemFileStreamReaderTest, NonExistent) {
  const char kFileName[] = "nonexistent";
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kFileName, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, result);
  ASSERT_EQ(0U, data.size());
}

TEST_F(FileSystemFileStreamReaderTest, Empty) {
  const char kFileName[] = "empty";
  WriteFile(kFileName, NULL, 0, NULL);

  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kFileName, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(0U, data.size());

  net::TestInt64CompletionCallback callback;
  int64 length_result = reader->GetLength(callback.callback());
  if (length_result == net::ERR_IO_PENDING)
    length_result = callback.WaitForResult();
  ASSERT_EQ(0, length_result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthNormal) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback;
  int64 result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthAfterModified) {
  // Pass a fake expected modifictaion time so that the expectation fails.
  base::Time fake_expected_modification_time =
      test_file_modification_time() - base::TimeDelta::FromSeconds(10);

  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, fake_expected_modification_time));
  net::TestInt64CompletionCallback callback;
  int64 result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);

  // With NULL expected modification time this should work.
  reader.reset(CreateFileReader(kTestFileName, 0, base::Time()));
  result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthWithOffset) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 3, base::Time()));
  net::TestInt64CompletionCallback callback;
  int64 result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  // Initial offset does not affect the result of GetLength.
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, ReadNormal) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(FileSystemFileStreamReaderTest, ReadAfterModified) {
  // Pass a fake expected modifictaion time so that the expectation fails.
  base::Time fake_expected_modification_time =
      test_file_modification_time() - base::TimeDelta::FromSeconds(10);

  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, fake_expected_modification_time));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
  ASSERT_EQ(0U, data.size());

  // With NULL expected modification time this should work.
  data.clear();
  reader.reset(CreateFileReader(kTestFileName, 0, base::Time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(FileSystemFileStreamReaderTest, ReadWithOffset) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 3, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(&kTestData[3], data);
}

TEST_F(FileSystemFileStreamReaderTest, DeleteWithUnfinishedRead) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, base::Time()));

  net::TestCompletionCallback callback;
  scoped_refptr<net::IOBufferWithSize> buf(
      new net::IOBufferWithSize(kTestDataSize));
  int rv = reader->Read(buf.get(), buf->size(), base::Bind(&NeverCalled));
  ASSERT_TRUE(rv == net::ERR_IO_PENDING || rv >= 0);

  // Delete immediately.
  // Should not crash; nor should NeverCalled be callback.
  reader.reset();
}

}  // namespace fileapi
