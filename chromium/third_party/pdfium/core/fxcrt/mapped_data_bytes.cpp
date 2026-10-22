// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/mapped_data_bytes.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

#include "build/build_config.h"
#include "core/fxcrt/check_op.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/ptr_util.h"

#if !BUILDFLAG(IS_POSIX)
#error "Built on wrong platform"
#endif

namespace fxcrt {

// static
std::unique_ptr<MappedDataBytes> MappedDataBytes::Create(
    const ByteString& file_name) {
  int fd = open(file_name.c_str(), O_RDONLY);
  if (fd < 0) {
    return nullptr;
  }
  auto result = pdfium::WrapUnique(new MappedDataBytes(fd));
  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    return nullptr;
  }
  if (sb.st_size == 0) {
    return result;
  }
  void* ptr = mmap(nullptr, static_cast<size_t>(sb.st_size), PROT_READ,
                   MAP_PRIVATE, fd, 0);
  if (ptr == MAP_FAILED) {
    return nullptr;
  }
  result->mapping_ = UNSAFE_BUFFERS(pdfium::span(
      static_cast<const uint8_t*>(ptr), static_cast<size_t>(sb.st_size)));
  return result;
}

MappedDataBytes::MappedDataBytes(int fd) : fd_(fd) {
  CHECK_GE(fd, 0);
}

MappedDataBytes::~MappedDataBytes() {
  if (!mapping_.empty()) {
    munmap(const_cast<uint8_t*>(mapping_.data()), mapping_.size());
  }
  close(fd_);
}

}  // namespace fxcrt
