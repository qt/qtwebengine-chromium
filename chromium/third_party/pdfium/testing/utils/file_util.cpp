// Copyright 2019 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/utils/file_util.h"

#include <stdio.h>

#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/numerics/safe_conversions.h"
#include "core/fxcrt/span.h"
#include "core/fxcrt/span_io.h"
#include "core/fxcrt/stl_util.h"
#include "testing/utils/path_service.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace pdfium {

void FileCloser::operator()(FILE* f) const {
  if (f) {
    fclose(f);
  }
}

#if BUILDFLAG(IS_POSIX)
ScopedFD::ScopedFD() = default;
ScopedFD::ScopedFD(int fd) : fd_(fd) {}
ScopedFD::~ScopedFD() {
  if (fd_ >= 0) {
    close(fd_);
  }
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
ScopedHandle::ScopedHandle() = default;
ScopedHandle::ScopedHandle(HANDLE handle) : handle_(handle) {}
ScopedHandle::~ScopedHandle() {
  if (handle_ && handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace pdfium

bool CanReadFile(const char* filename) {
  pdfium::ScopedFILE file(fopen(filename, "rb"));
  return !!file;
}

std::vector<uint8_t> GetFileContents(const char* filename) {
  pdfium::ScopedFILE file(fopen(filename, "rb"));
  if (!file) {
    UNSAFE_TODO(fprintf(stderr, "Failed to open: %s\n", filename));
    return {};
  }
  (void)fseek(file.get(), 0, SEEK_END);
  size_t file_length = ftell(file.get());
  if (!file_length) {
    return {};
  }
  (void)fseek(file.get(), 0, SEEK_SET);
  std::vector<uint8_t> buffer(file_length);
  pdfium::span<uint8_t> items_read = fxcrt::spanread(buffer, file.get());
  size_t bytes_read = items_read.size();
  if (bytes_read != file_length) {
    UNSAFE_TODO(fprintf(stderr, "Failed to read: %s\n", filename));
    return {};
  }
  return buffer;
}

FileAccessForTesting::FileAccessForTesting(const std::string& file_name) {
  std::string file_path = PathService::GetTestFilePath(file_name);
  if (file_path.empty()) {
    return;
  }

  file_contents_ = GetFileContents(file_path.c_str());
  if (file_contents_.empty()) {
    return;
  }

  m_FileLen = pdfium::checked_cast<unsigned long>(file_contents_.size());
  m_GetBlock = SGetBlock;
  m_Param = this;
}

int FileAccessForTesting::GetBlockImpl(unsigned long pos,
                                       unsigned char* pBuf,
                                       unsigned long size) {
  fxcrt::Copy(pdfium::span(file_contents_).subspan(pos, size),
              UNSAFE_TODO(pdfium::span(pBuf, size)));
  return size ? 1 : 0;
}

// static
int FileAccessForTesting::SGetBlock(void* param,
                                    unsigned long pos,
                                    unsigned char* pBuf,
                                    unsigned long size) {
  auto* file_access = static_cast<FileAccessForTesting*>(param);
  return file_access->GetBlockImpl(pos, pBuf, size);
}
