// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_dir_url_request_job.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/platform_file.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/test_file_system_context.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"

namespace fileapi {
namespace {

// We always use the TEMPORARY FileSystem in this test.
static const char kFileSystemURLPrefix[] =
    "filesystem:http://remote/temporary/";

}  // namespace

class FileSystemDirURLRequestJobTest : public testing::Test {
 protected:
  FileSystemDirURLRequestJobTest()
    : weak_factory_(this) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    special_storage_policy_ = new quota::MockSpecialStoragePolicy;
    file_system_context_ = CreateFileSystemContextForTesting(
        NULL, temp_dir_.path());

    file_system_context_->OpenFileSystem(
        GURL("http://remote/"), kFileSystemTypeTemporary,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::Bind(&FileSystemDirURLRequestJobTest::OnOpenFileSystem,
                   weak_factory_.GetWeakPtr()));
    base::RunLoop().RunUntilIdle();

    net::URLRequest::Deprecated::RegisterProtocolFactory(
        "filesystem", &FileSystemDirURLRequestJobFactory);
  }

  virtual void TearDown() OVERRIDE {
    // NOTE: order matters, request must die before delegate
    request_.reset(NULL);
    delegate_.reset(NULL);

    net::URLRequest::Deprecated::RegisterProtocolFactory("filesystem", NULL);
    ClearUnusedJob();
  }

  void OnOpenFileSystem(const GURL& root_url,
                        const std::string& name,
                        base::PlatformFileError result) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
  }

  void TestRequestHelper(const GURL& url, bool run_to_completion,
                         FileSystemContext* file_system_context) {
    delegate_.reset(new net::TestDelegate());
    delegate_->set_quit_on_redirect(true);
    request_ = empty_context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, delegate_.get());
    job_ = new FileSystemDirURLRequestJob(
        request_.get(), NULL, file_system_context);

    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    if (run_to_completion)
      base::MessageLoop::current()->Run();
  }

  void TestRequest(const GURL& url) {
    TestRequestHelper(url, true, file_system_context_.get());
  }

  void TestRequestWithContext(const GURL& url,
                              FileSystemContext* file_system_context) {
    TestRequestHelper(url, true, file_system_context);
  }

  void TestRequestNoRun(const GURL& url) {
    TestRequestHelper(url, false, file_system_context_.get());
  }

  FileSystemURL CreateURL(const base::FilePath& file_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://remote"),
        fileapi::kFileSystemTypeTemporary,
        file_path);
  }

  FileSystemOperationContext* NewOperationContext() {
    FileSystemOperationContext* context(
        new FileSystemOperationContext(file_system_context_.get()));
    context->set_allowed_bytes_growth(1024);
    return context;
  }

  void CreateDirectory(const base::StringPiece& dir_name) {
    base::FilePath path = base::FilePath().AppendASCII(dir_name);
    scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util()->CreateDirectory(
        context.get(),
        CreateURL(path),
        false /* exclusive */,
        false /* recursive */));
  }

  void EnsureFileExists(const base::StringPiece file_name) {
    base::FilePath path = base::FilePath().AppendASCII(file_name);
    scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util()->EnsureFileExists(
        context.get(), CreateURL(path), NULL));
  }

  void TruncateFile(const base::StringPiece file_name, int64 length) {
    base::FilePath path = base::FilePath().AppendASCII(file_name);
    scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util()->Truncate(
        context.get(), CreateURL(path), length));
  }

  base::PlatformFileError GetFileInfo(const base::FilePath& path,
                                      base::PlatformFileInfo* file_info,
                                      base::FilePath* platform_file_path) {
    scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
    return file_util()->GetFileInfo(context.get(),
                                    CreateURL(path),
                                    file_info, platform_file_path);
  }

  void VerifyListingEntry(const std::string& entry_line,
                          const std::string& name,
                          const std::string& url,
                          bool is_directory,
                          int64 size) {
#define STR "([^\"]*)"
    icu::UnicodeString pattern("^<script>addRow\\(\"" STR "\",\"" STR
                               "\",(0|1),\"" STR "\",\"" STR "\"\\);</script>");
#undef STR
    icu::UnicodeString input(entry_line.c_str());

    UErrorCode status = U_ZERO_ERROR;
    icu::RegexMatcher match(pattern, input, 0, status);

    EXPECT_TRUE(match.find());
    EXPECT_EQ(5, match.groupCount());
    EXPECT_EQ(icu::UnicodeString(name.c_str()), match.group(1, status));
    EXPECT_EQ(icu::UnicodeString(url.c_str()), match.group(2, status));
    EXPECT_EQ(icu::UnicodeString(is_directory ? "1" : "0"),
              match.group(3, status));
    icu::UnicodeString size_string(FormatBytesUnlocalized(size).c_str());
    EXPECT_EQ(size_string, match.group(4, status));

    base::Time date;
    icu::UnicodeString date_ustr(match.group(5, status));
    std::string date_str;
    UTF16ToUTF8(date_ustr.getBuffer(), date_ustr.length(), &date_str);
    EXPECT_TRUE(base::Time::FromString(date_str.c_str(), &date));
    EXPECT_FALSE(date.is_null());
  }

  GURL CreateFileSystemURL(const std::string path) {
    return GURL(kFileSystemURLPrefix + path);
  }

  static net::URLRequestJob* FileSystemDirURLRequestJobFactory(
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

  FileSystemFileUtil* file_util() {
    return file_system_context_->sandbox_delegate()->sync_file_util();
  }

  // Put the message loop at the top, so that it's the last thing deleted.
  // Delete all MessageLoopProxy objects before the MessageLoop, to help prevent
  // leaks caused by tasks posted during shutdown.
  base::MessageLoopForIO message_loop_;

  base::ScopedTempDir temp_dir_;
  net::URLRequestContext empty_context_;
  scoped_ptr<net::TestDelegate> delegate_;
  scoped_ptr<net::URLRequest> request_;
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::WeakPtrFactory<FileSystemDirURLRequestJobTest> weak_factory_;

  static net::URLRequestJob* job_;
};

// static
net::URLRequestJob* FileSystemDirURLRequestJobTest::job_ = NULL;

namespace {

TEST_F(FileSystemDirURLRequestJobTest, DirectoryListing) {
  CreateDirectory("foo");
  CreateDirectory("foo/bar");
  CreateDirectory("foo/bar/baz");

  EnsureFileExists("foo/bar/hoge");
  TruncateFile("foo/bar/hoge", 10);

  TestRequest(CreateFileSystemURL("foo/bar/"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_GT(delegate_->bytes_received(), 0);

  std::istringstream in(delegate_->data_received());
  std::string line;
  EXPECT_TRUE(std::getline(in, line));

#if defined(OS_WIN)
  EXPECT_EQ("<script>start(\"foo\\\\bar\");</script>", line);
#elif defined(OS_POSIX)
  EXPECT_EQ("<script>start(\"/foo/bar\");</script>", line);
#endif

  EXPECT_TRUE(std::getline(in, line));
  VerifyListingEntry(line, "hoge", "hoge", false, 10);

  EXPECT_TRUE(std::getline(in, line));
  VerifyListingEntry(line, "baz", "baz", true, 0);
}

TEST_F(FileSystemDirURLRequestJobTest, InvalidURL) {
  TestRequest(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(net::ERR_INVALID_URL, request_->status().error());
}

TEST_F(FileSystemDirURLRequestJobTest, NoSuchRoot) {
  TestRequest(GURL("filesystem:http://remote/persistent/somedir/"));
  ASSERT_FALSE(request_->is_pending());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

TEST_F(FileSystemDirURLRequestJobTest, NoSuchDirectory) {
  TestRequest(CreateFileSystemURL("somedir/"));
  ASSERT_FALSE(request_->is_pending());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

TEST_F(FileSystemDirURLRequestJobTest, Cancel) {
  CreateDirectory("foo");
  TestRequestNoRun(CreateFileSystemURL("foo/"));
  // Run StartAsync() and only StartAsync().
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, request_.release());
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

TEST_F(FileSystemDirURLRequestJobTest, Incognito) {
  CreateDirectory("foo");

  scoped_refptr<FileSystemContext> file_system_context =
      CreateIncognitoFileSystemContextForTesting(NULL, temp_dir_.path());

  TestRequestWithContext(CreateFileSystemURL("/"),
                         file_system_context.get());
  ASSERT_FALSE(request_->is_pending());
  ASSERT_TRUE(request_->status().is_success());

  std::istringstream in(delegate_->data_received());
  std::string line;
  EXPECT_TRUE(std::getline(in, line));
  EXPECT_FALSE(std::getline(in, line));

  TestRequestWithContext(CreateFileSystemURL("foo"),
                         file_system_context.get());
  ASSERT_FALSE(request_->is_pending());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

}  // namespace (anonymous)
}  // namespace fileapi
