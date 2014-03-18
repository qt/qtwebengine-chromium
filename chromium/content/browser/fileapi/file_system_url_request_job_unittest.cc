// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_url_request_job.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/test_file_system_context.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_test_helper.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"

namespace fileapi {
namespace {

// We always use the TEMPORARY FileSystem in this test.
const char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";
const char kTestFileData[] = "0123456789";

void FillBuffer(char* buffer, size_t len) {
  base::RandBytes(buffer, len);
}

}  // namespace

class FileSystemURLRequestJobTest : public testing::Test {
 protected:
  FileSystemURLRequestJobTest() : weak_factory_(this) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // We use the main thread so that we can get the root path synchronously.
    // TODO(adamk): Run this on the FILE thread we've created as well.
    file_system_context_ =
        CreateFileSystemContextForTesting(NULL, temp_dir_.path());

    file_system_context_->OpenFileSystem(
        GURL("http://remote/"), kFileSystemTypeTemporary,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::Bind(&FileSystemURLRequestJobTest::OnOpenFileSystem,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();

    net::URLRequest::Deprecated::RegisterProtocolFactory(
        "filesystem", &FileSystemURLRequestJobFactory);
  }

  virtual void TearDown() OVERRIDE {
    net::URLRequest::Deprecated::RegisterProtocolFactory("filesystem", NULL);
    ClearUnusedJob();
    if (pending_job_.get()) {
      pending_job_->Kill();
      pending_job_ = NULL;
    }
    // FileReader posts a task to close the file in destructor.
    base::RunLoop().RunUntilIdle();
  }

  void OnOpenFileSystem(const GURL& root_url,
                        const std::string& name,
                        base::PlatformFileError result) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
  }

  void TestRequestHelper(const GURL& url,
                         const net::HttpRequestHeaders* headers,
                         bool run_to_completion,
                         FileSystemContext* file_system_context) {
    delegate_.reset(new net::TestDelegate());
    // Make delegate_ exit the MessageLoop when the request is done.
    delegate_->set_quit_on_complete(true);
    delegate_->set_quit_on_redirect(true);
    request_ = empty_context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, delegate_.get());
    if (headers)
      request_->SetExtraRequestHeaders(*headers);
    ASSERT_TRUE(!job_);
    job_ = new FileSystemURLRequestJob(
        request_.get(), NULL, file_system_context);
    pending_job_ = job_;

    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    if (run_to_completion)
      base::MessageLoop::current()->Run();
  }

  void TestRequest(const GURL& url) {
    TestRequestHelper(url, NULL, true, file_system_context_.get());
  }

  void TestRequestWithContext(const GURL& url,
                              FileSystemContext* file_system_context) {
    TestRequestHelper(url, NULL, true, file_system_context);
  }

  void TestRequestWithHeaders(const GURL& url,
                              const net::HttpRequestHeaders* headers) {
    TestRequestHelper(url, headers, true, file_system_context_.get());
  }

  void TestRequestNoRun(const GURL& url) {
    TestRequestHelper(url, NULL, false, file_system_context_.get());
  }

  void CreateDirectory(const base::StringPiece& dir_name) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://remote"),
        kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(dir_name));
    ASSERT_EQ(base::PLATFORM_FILE_OK, AsyncFileTestHelper::CreateDirectory(
        file_system_context_, url));
  }

  void WriteFile(const base::StringPiece& file_name,
                 const char* buf, int buf_size) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://remote"),
        kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(file_name));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_, url, buf, buf_size));
  }

  GURL CreateFileSystemURL(const std::string& path) {
    return GURL(kFileSystemURLPrefix + path);
  }

  static net::URLRequestJob* FileSystemURLRequestJobFactory(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& scheme) {
    DCHECK(job_);
    net::URLRequestJob* temp = job_;
    job_ = NULL;
    return temp;
  }

  static void ClearUnusedJob() {
    if (job_) {
      scoped_refptr<net::URLRequestJob> deleter = job_;
      job_ = NULL;
    }
  }

  // Put the message loop at the top, so that it's the last thing deleted.
  base::MessageLoopForIO message_loop_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::WeakPtrFactory<FileSystemURLRequestJobTest> weak_factory_;

  net::URLRequestContext empty_context_;

  // NOTE: order matters, request must die before delegate
  scoped_ptr<net::TestDelegate> delegate_;
  scoped_ptr<net::URLRequest> request_;

  scoped_refptr<net::URLRequestJob> pending_job_;
  static net::URLRequestJob* job_;
};

// static
net::URLRequestJob* FileSystemURLRequestJobTest::job_ = NULL;

namespace {

TEST_F(FileSystemURLRequestJobTest, FileTest) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  TestRequest(CreateFileSystemURL("file1.dat"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_EQ(kTestFileData, delegate_->data_received());
  EXPECT_EQ(200, request_->GetResponseCode());
  std::string cache_control;
  request_->GetResponseHeaderByName("cache-control", &cache_control);
  EXPECT_EQ("no-cache", cache_control);
}

TEST_F(FileSystemURLRequestJobTest, FileTestFullSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);
  WriteFile("bigfile", buffer.get(), buffer_size);

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::Bounded(
          first_byte_position, last_byte_position).GetHeaderValue());
  TestRequestWithHeaders(CreateFileSystemURL("bigfile"), &headers);

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_TRUE(partial_buffer_string == delegate_->data_received());
}

TEST_F(FileSystemURLRequestJobTest, FileTestHalfSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_ptr<char[]> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);
  WriteFile("bigfile", buffer.get(), buffer_size);

  const size_t first_byte_position = 500;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + buffer_size);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::RightUnbounded(first_byte_position).GetHeaderValue());
  TestRequestWithHeaders(CreateFileSystemURL("bigfile"), &headers);
  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
  EXPECT_TRUE(partial_buffer_string == delegate_->data_received());
}


TEST_F(FileSystemURLRequestJobTest, FileTestMultipleRangesNotSupported) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    "bytes=0-5,10-200,200-300");
  TestRequestWithHeaders(CreateFileSystemURL("file1.dat"), &headers);
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, RangeOutOfBounds) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::Bounded(500, 1000).GetHeaderValue());
  TestRequestWithHeaders(CreateFileSystemURL("file1.dat"), &headers);

  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, FileDirRedirect) {
  CreateDirectory("dir");
  TestRequest(CreateFileSystemURL("dir"));

  EXPECT_EQ(1, delegate_->received_redirect_count());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_FALSE(delegate_->request_failed());

  // We've deferred the redirect; now cancel the request to avoid following it.
  request_->Cancel();
  base::MessageLoop::current()->Run();
}

TEST_F(FileSystemURLRequestJobTest, InvalidURL) {
  TestRequest(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_INVALID_URL, request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, NoSuchRoot) {
  TestRequest(GURL("filesystem:http://remote/persistent/somefile"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, NoSuchFile) {
  TestRequest(CreateFileSystemURL("somefile"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, Cancel) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  TestRequestNoRun(CreateFileSystemURL("file1.dat"));

  // Run StartAsync() and only StartAsync().
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, request_.release());
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

TEST_F(FileSystemURLRequestJobTest, GetMimeType) {
  const char kFilename[] = "hoge.html";

  std::string mime_type_direct;
  base::FilePath::StringType extension =
      base::FilePath().AppendASCII(kFilename).Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  EXPECT_TRUE(net::GetWellKnownMimeTypeFromExtension(
      extension, &mime_type_direct));

  TestRequest(CreateFileSystemURL(kFilename));

  std::string mime_type_from_job;
  request_->GetMimeType(&mime_type_from_job);
  EXPECT_EQ(mime_type_direct, mime_type_from_job);
}

TEST_F(FileSystemURLRequestJobTest, Incognito) {
  WriteFile("file", kTestFileData, arraysize(kTestFileData) - 1);

  // Creates a new filesystem context for incognito mode.
  scoped_refptr<FileSystemContext> file_system_context =
      CreateIncognitoFileSystemContextForTesting(NULL, temp_dir_.path());

  // The request should return NOT_FOUND error if it's in incognito mode.
  TestRequestWithContext(CreateFileSystemURL("file"),
                         file_system_context.get());
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());

  // Make sure it returns success with regular (non-incognito) context.
  TestRequest(CreateFileSystemURL("file"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(kTestFileData, delegate_->data_received());
  EXPECT_EQ(200, request_->GetResponseCode());
}

}  // namespace
}  // namespace fileapi
