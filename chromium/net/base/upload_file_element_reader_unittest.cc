// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class UploadFileElementReaderTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();
    // Some tests (*.ReadPartially) rely on bytes_.size() being even.
    const char kData[] = "123456789abcdefghi";
    bytes_.assign(kData, kData + arraysize(kData) - 1);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(),
                                               &temp_file_path_));
    ASSERT_EQ(
        static_cast<int>(bytes_.size()),
        file_util::WriteFile(temp_file_path_, &bytes_[0], bytes_.size()));

    reader_.reset(
        new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                    temp_file_path_,
                                    0,
                                    kuint64max,
                                    base::Time()));
    TestCompletionCallback callback;
    ASSERT_EQ(ERR_IO_PENDING, reader_->Init(callback.callback()));
    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_FALSE(reader_->IsInMemory());
  }

  virtual ~UploadFileElementReaderTest() {
    reader_.reset();
    base::RunLoop().RunUntilIdle();
  }

  std::vector<char> bytes_;
  scoped_ptr<UploadElementReader> reader_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
};

TEST_F(UploadFileElementReaderTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  ASSERT_EQ(bytes_.size(), kHalfSize * 2);
  std::vector<char> buf(kHalfSize);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  TestCompletionCallback read_callback1;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback1.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback1.WaitForResult());
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin(), bytes_.begin() + kHalfSize), buf);

  TestCompletionCallback read_callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback2.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin() + kHalfSize, bytes_.end()), buf);
}

TEST_F(UploadFileElementReaderTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  TestCompletionCallback read_callback;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(0,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback.callback()));
}

TEST_F(UploadFileElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  TestCompletionCallback read_callback;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback.callback()));
  EXPECT_EQ(static_cast<int>(bytes_.size()), read_callback.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadFileElementReaderTest, MultipleInit) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);

  // Read all.
  TestCompletionCallback read_callback1;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback1.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback1.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);

  // Call Init() again to reset the state.
  TestCompletionCallback init_callback;
  ASSERT_EQ(ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(OK, init_callback.WaitForResult());
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read again.
  TestCompletionCallback read_callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback2.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadFileElementReaderTest, InitDuringAsyncOperation) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);

  // Start reading all.
  TestCompletionCallback read_callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer.get(), buf.size(), read_callback1.callback()));

  // Call Init to cancel the previous read.
  TestCompletionCallback init_callback1;
  EXPECT_EQ(ERR_IO_PENDING, reader_->Init(init_callback1.callback()));

  // Call Init again to cancel the previous init.
  TestCompletionCallback init_callback2;
  EXPECT_EQ(ERR_IO_PENDING, reader_->Init(init_callback2.callback()));
  EXPECT_EQ(OK, init_callback2.WaitForResult());
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read half.
  std::vector<char> buf2(bytes_.size() / 2);
  scoped_refptr<IOBuffer> wrapped_buffer2 = new WrappedIOBuffer(&buf2[0]);
  TestCompletionCallback read_callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            reader_->Read(
                wrapped_buffer2.get(), buf2.size(), read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf2.size()), read_callback2.WaitForResult());
  EXPECT_EQ(bytes_.size() - buf2.size(), reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin(), bytes_.begin() + buf2.size()),
            buf2);

  // Make sure callbacks are not called for cancelled operations.
  EXPECT_FALSE(read_callback1.have_result());
  EXPECT_FALSE(init_callback1.have_result());
}

TEST_F(UploadFileElementReaderTest, Range) {
  const uint64 kOffset = 2;
  const uint64 kLength = bytes_.size() - kOffset * 3;
  reader_.reset(
      new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                  temp_file_path_,
                                  kOffset,
                                  kLength,
                                  base::Time()));
  TestCompletionCallback init_callback;
  ASSERT_EQ(ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(OK, init_callback.WaitForResult());
  EXPECT_EQ(kLength, reader_->GetContentLength());
  EXPECT_EQ(kLength, reader_->BytesRemaining());
  std::vector<char> buf(kLength);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  TestCompletionCallback read_callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      reader_->Read(wrapped_buffer.get(), kLength, read_callback.callback()));
  EXPECT_EQ(static_cast<int>(kLength), read_callback.WaitForResult());
  const std::vector<char> expected(bytes_.begin() + kOffset,
                                   bytes_.begin() + kOffset + kLength);
  EXPECT_EQ(expected, buf);
}

TEST_F(UploadFileElementReaderTest, FileChanged) {
  base::PlatformFileInfo info;
  ASSERT_TRUE(base::GetFileInfo(temp_file_path_, &info));

  // Expect one second before the actual modification time to simulate change.
  const base::Time expected_modification_time =
      info.last_modified - base::TimeDelta::FromSeconds(1);
  reader_.reset(
      new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                  temp_file_path_,
                                  0,
                                  kuint64max,
                                  expected_modification_time));
  TestCompletionCallback init_callback;
  ASSERT_EQ(ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(ERR_UPLOAD_FILE_CHANGED, init_callback.WaitForResult());
}

TEST_F(UploadFileElementReaderTest, WrongPath) {
  const base::FilePath wrong_path(FILE_PATH_LITERAL("wrong_path"));
  reader_.reset(
      new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                  wrong_path,
                                  0,
                                  kuint64max,
                                  base::Time()));
  TestCompletionCallback init_callback;
  ASSERT_EQ(ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(ERR_FILE_NOT_FOUND, init_callback.WaitForResult());
}


class UploadFileElementReaderSyncTest : public PlatformTest {
 protected:
  virtual void SetUp() OVERRIDE {
    // Some tests (*.ReadPartially) rely on bytes_.size() being even.
    const char kData[] = "123456789abcdefghi";
    bytes_.assign(kData, kData + arraysize(kData) - 1);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(),
                                               &temp_file_path_));
    ASSERT_EQ(
        static_cast<int>(bytes_.size()),
        file_util::WriteFile(temp_file_path_, &bytes_[0], bytes_.size()));

    reader_.reset(new UploadFileElementReaderSync(
        temp_file_path_, 0, kuint64max, base::Time()));
    ASSERT_EQ(OK, reader_->Init(CompletionCallback()));
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_FALSE(reader_->IsInMemory());
  }

  std::vector<char> bytes_;
  scoped_ptr<UploadElementReader> reader_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
};

TEST_F(UploadFileElementReaderSyncTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  ASSERT_EQ(bytes_.size(), kHalfSize * 2);
  std::vector<char> buf(kHalfSize);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin(), bytes_.begin() + kHalfSize), buf);

  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(std::vector<char>(bytes_.begin() + kHalfSize, bytes_.end()), buf);
}

TEST_F(UploadFileElementReaderSyncTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(
      0, reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
}

TEST_F(UploadFileElementReaderSyncTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(
      static_cast<int>(bytes_.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadFileElementReaderSyncTest, MultipleInit) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);

  // Read all.
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);

  // Call Init() again to reset the state.
  ASSERT_EQ(OK, reader_->Init(CompletionCallback()));
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read again.
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadFileElementReaderSyncTest, Range) {
  const uint64 kOffset = 2;
  const uint64 kLength = bytes_.size() - kOffset * 3;
  reader_.reset(new UploadFileElementReaderSync(
      temp_file_path_, kOffset, kLength, base::Time()));
  ASSERT_EQ(OK, reader_->Init(CompletionCallback()));
  EXPECT_EQ(kLength, reader_->GetContentLength());
  EXPECT_EQ(kLength, reader_->BytesRemaining());
  std::vector<char> buf(kLength);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(static_cast<int>(kLength),
            reader_->Read(wrapped_buffer.get(), kLength, CompletionCallback()));
  const std::vector<char> expected(bytes_.begin() + kOffset,
                                   bytes_.begin() + kOffset + kLength);
  EXPECT_EQ(expected, buf);
}

TEST_F(UploadFileElementReaderSyncTest, FileChanged) {
  base::PlatformFileInfo info;
  ASSERT_TRUE(base::GetFileInfo(temp_file_path_, &info));

  // Expect one second before the actual modification time to simulate change.
  const base::Time expected_modification_time =
      info.last_modified - base::TimeDelta::FromSeconds(1);
  reader_.reset(new UploadFileElementReaderSync(
      temp_file_path_, 0, kuint64max, expected_modification_time));
  EXPECT_EQ(ERR_UPLOAD_FILE_CHANGED, reader_->Init(CompletionCallback()));
}

TEST_F(UploadFileElementReaderSyncTest, WrongPath) {
  const base::FilePath wrong_path(FILE_PATH_LITERAL("wrong_path"));
  reader_.reset(new UploadFileElementReaderSync(
      wrong_path, 0, kuint64max, base::Time()));
  ASSERT_EQ(ERR_FILE_NOT_FOUND, reader_->Init(CompletionCallback()));
}

}  // namespace net
