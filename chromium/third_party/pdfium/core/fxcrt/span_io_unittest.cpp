// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/span_io.h"

#include <stdio.h>

#include <string>
#include <string_view>

#include "build/build_config.h"
#include "core/fxcrt/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/file_util.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

TEST(Spanread, ReadNormal) {
  pdfium::ScopedFILE scoped_f(tmpfile());
  FILE* f = scoped_f.get();
  ASSERT_TRUE(f);
  const char kData[] = "hello world";
  ASSERT_EQ(UNSAFE_BUFFERS(fwrite(kData, 1, sizeof(kData), f)), sizeof(kData));
  rewind(f);

  char buffer[100];
  pdfium::span<char> read_span = fxcrt::spanread(buffer, f);
  EXPECT_EQ(read_span.size(), sizeof(kData));
  EXPECT_STREQ(read_span.data(), kData);
}

TEST(Spanread, ReadTruncated) {
  pdfium::ScopedFILE scoped_f(tmpfile());
  FILE* f = scoped_f.get();
  ASSERT_TRUE(f);
  const char kData[] = "hello world";
  ASSERT_EQ(UNSAFE_BUFFERS(fwrite(kData, 1, sizeof(kData), f)), sizeof(kData));
  rewind(f);

  char buffer[5];
  pdfium::span<char> read_span = fxcrt::spanread(buffer, f);
  EXPECT_EQ(read_span.size(), 5u);
  EXPECT_EQ(std::string_view(read_span.data(), read_span.size()), "hello");
}

TEST(Spanread, ReadEmpty) {
  pdfium::ScopedFILE scoped_f(tmpfile());
  FILE* f = scoped_f.get();
  ASSERT_TRUE(f);
  pdfium::span<char> empty_span;
  pdfium::span<char> read_span = fxcrt::spanread(empty_span, f);
  EXPECT_TRUE(read_span.empty());
}

TEST(Spanwrite, WriteNormal) {
  pdfium::ScopedFILE scoped_f(tmpfile());
  FILE* f = scoped_f.get();
  ASSERT_TRUE(f);
  const char kData[] = "hello world";

  // Also test that raw array is acceptable (implicit conversion).
  EXPECT_EQ(fxcrt::spanwrite(kData, f), sizeof(kData));

  rewind(f);
  char buffer[100];
  size_t read = UNSAFE_BUFFERS(fread(buffer, 1, sizeof(kData), f));
  EXPECT_EQ(read, sizeof(kData));
  EXPECT_STREQ(buffer, kData);
}

TEST(Spanwrite, WriteEmpty) {
  pdfium::ScopedFILE scoped_f(tmpfile());
  FILE* f = scoped_f.get();
  ASSERT_TRUE(f);
  pdfium::span<const char> empty_span;
  EXPECT_EQ(fxcrt::spanwrite(empty_span, f), 0u);
}

#if BUILDFLAG(IS_POSIX)
TEST(SpanreadFD, ReadNormal) {
  std::string path = testing::TempDir() + "/pdfium_span_unittest_XXXXXX";
  pdfium::ScopedFD scoped_fd(mkstemp(path.data()));
  int fd = scoped_fd.get();
  ASSERT_GE(fd, 0);

  const char kData[] = "hello world";
  ASSERT_EQ(write(fd, kData, sizeof(kData)),
            static_cast<ssize_t>(sizeof(kData)));
  lseek(fd, 0, SEEK_SET);

  char buffer[100];
  pdfium::span<char> read_span = fxcrt::spanread(buffer, fd);
  EXPECT_EQ(read_span.size(), sizeof(kData));
  EXPECT_STREQ(read_span.data(), kData);

  unlink(path.c_str());
}

TEST(SpanreadFD, ReadEmpty) {
  std::string path = testing::TempDir() + "/pdfium_span_unittest_XXXXXX";
  pdfium::ScopedFD scoped_fd(mkstemp(path.data()));
  int fd = scoped_fd.get();
  ASSERT_GE(fd, 0);
  pdfium::span<char> empty_span;
  pdfium::span<char> read_span = fxcrt::spanread(empty_span, fd);
  EXPECT_TRUE(read_span.empty());
  unlink(path.c_str());
}

TEST(SpanwriteFD, WriteNormal) {
  std::string path = testing::TempDir() + "/pdfium_span_unittest_XXXXXX";
  pdfium::ScopedFD scoped_fd(mkstemp(path.data()));
  int fd = scoped_fd.get();
  ASSERT_GE(fd, 0);

  const char kData[] = "hello world";
  EXPECT_EQ(fxcrt::spanwrite(kData, fd), sizeof(kData));

  lseek(fd, 0, SEEK_SET);
  char buffer[100];
  ssize_t bytes_read = read(fd, buffer, sizeof(kData));
  EXPECT_EQ(bytes_read, static_cast<ssize_t>(sizeof(kData)));
  EXPECT_STREQ(buffer, kData);

  unlink(path.c_str());
}

TEST(SpanwriteFD, WriteEmpty) {
  std::string path = testing::TempDir() + "/pdfium_span_unittest_XXXXXX";
  pdfium::ScopedFD scoped_fd(mkstemp(path.data()));
  int fd = scoped_fd.get();
  ASSERT_GE(fd, 0);
  pdfium::span<const char> empty_span;
  EXPECT_EQ(fxcrt::spanwrite(empty_span, fd), 0u);
  unlink(path.c_str());
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
TEST(SpanreadWin, ReadNormal) {
  std::string path = testing::TempDir() + "/pdfium_span_win_unittest.tmp";
  std::wstring wpath(path.begin(), path.end());

  pdfium::ScopedHandle scoped_handle(::CreateFileW(
      wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
  HANDLE handle = scoped_handle.get();
  ASSERT_NE(handle, INVALID_HANDLE_VALUE);

  const char kData[] = "hello world";
  DWORD bytes_written = 0;
  ASSERT_TRUE(
      ::WriteFile(handle, kData, sizeof(kData), &bytes_written, nullptr));
  ASSERT_EQ(bytes_written, sizeof(kData));

  // Seek to beginning
  ASSERT_NE(::SetFilePointer(handle, 0, nullptr, FILE_BEGIN),
            INVALID_SET_FILE_POINTER);

  char buffer[100];
  pdfium::span<char> read_span = fxcrt::spanread(buffer, handle);
  EXPECT_EQ(read_span.size(), sizeof(kData));
  EXPECT_STREQ(read_span.data(), kData);
}

TEST(SpanreadWin, ReadEmpty) {
  std::string path = testing::TempDir() + "/pdfium_span_win_unittest.tmp";
  std::wstring wpath(path.begin(), path.end());
  pdfium::ScopedHandle scoped_handle(::CreateFileW(
      wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
  HANDLE handle = scoped_handle.get();
  ASSERT_NE(handle, INVALID_HANDLE_VALUE);
  pdfium::span<char> empty_span;
  pdfium::span<char> read_span = fxcrt::spanread(empty_span, handle);
  EXPECT_TRUE(read_span.empty());
}

TEST(SpanwriteWin, WriteNormal) {
  std::string path = testing::TempDir() + "/pdfium_span_win_unittest.tmp";
  std::wstring wpath(path.begin(), path.end());

  pdfium::ScopedHandle scoped_handle(::CreateFileW(
      wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
  HANDLE handle = scoped_handle.get();
  ASSERT_NE(handle, INVALID_HANDLE_VALUE);

  const char kData[] = "hello world";
  EXPECT_EQ(fxcrt::spanwrite(kData, handle), sizeof(kData));

  // Seek to beginning
  ASSERT_NE(::SetFilePointer(handle, 0, nullptr, FILE_BEGIN),
            INVALID_SET_FILE_POINTER);

  char buffer[100];
  DWORD bytes_read = 0;
  ASSERT_TRUE(::ReadFile(handle, buffer, sizeof(kData), &bytes_read, nullptr));
  EXPECT_EQ(bytes_read, sizeof(kData));
  EXPECT_STREQ(buffer, kData);
}

TEST(SpanwriteWin, WriteEmpty) {
  std::string path = testing::TempDir() + "/pdfium_span_win_unittest.tmp";
  std::wstring wpath(path.begin(), path.end());
  pdfium::ScopedHandle scoped_handle(::CreateFileW(
      wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
  HANDLE handle = scoped_handle.get();
  ASSERT_NE(handle, INVALID_HANDLE_VALUE);
  pdfium::span<const char> empty_span;
  EXPECT_EQ(fxcrt::spanwrite(empty_span, handle), 0u);
}
#endif  // BUILDFLAG(IS_WIN)
