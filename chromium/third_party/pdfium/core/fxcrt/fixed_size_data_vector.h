// Copyright 2022 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_FIXED_SIZE_DATA_VECTOR_H_
#define CORE_FXCRT_FIXED_SIZE_DATA_VECTOR_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "core/fxcrt/fx_memory_wrappers.h"
#include "third_party/base/containers/span.h"

namespace fxcrt {

// A simple data container that has a fixed size.
// Unlike std::vector, it cannot be implicitly copied and its data is only
// accessible using spans.
// It can either initialize its data with zeros, or leave its data
// uninitialized.
template <typename T>
class FixedSizeDataVector {
 public:
  FixedSizeDataVector() = default;

  // Allocates a vector of the given size with uninitialized memory.
  // A CHECK() failure occurs when insufficient memory.
  static FixedSizeDataVector Uninit(size_t size) {
    if (size == 0) {
      return FixedSizeDataVector();
    }
    return FixedSizeDataVector(FX_AllocUninit(T, size), size);
  }

  // Allocates a vector of the given size with zeroed memory.
  // A CHECK() failure occurs when insufficient memory.
  static FixedSizeDataVector Zeroed(size_t size) {
    if (size == 0) {
      return FixedSizeDataVector();
    }
    return FixedSizeDataVector(FX_Alloc(T, size), size);
  }

  // Same as above, but return an empty vector when insufficient memory.
  static FixedSizeDataVector TryZeroed(size_t size) {
    if (size == 0) {
      return FixedSizeDataVector();
    }
    T* ptr = FX_TryAlloc(T, size);
    return FixedSizeDataVector(ptr, ptr ? size : 0u);
  }

  FixedSizeDataVector(const FixedSizeDataVector&) = delete;
  FixedSizeDataVector& operator=(const FixedSizeDataVector&) = delete;

  FixedSizeDataVector(FixedSizeDataVector<T>&& that) noexcept
      : data_(std::move(that.data_)), size_(std::exchange(that.size_, 0)) {}

  FixedSizeDataVector& operator=(FixedSizeDataVector<T>&& that) noexcept {
    data_ = std::move(that.data_);
    size_ = std::exchange(that.size_, 0);
    return *this;
  }

  ~FixedSizeDataVector() = default;

  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }

  // Implicit access to data via span.
  operator pdfium::span<T>() { return span(); }
  operator pdfium::span<const T>() const { return span(); }

  // Explicit access to data via span.
  pdfium::span<T> span() { return pdfium::span<T>(data_.get(), size_); }
  pdfium::span<const T> span() const {
    return pdfium::span<const T>(data_.get(), size_);
  }

  // Convenience methods to slice the vector into spans.
  pdfium::span<T> subspan(size_t offset,
                          size_t count = pdfium::dynamic_extent) {
    return span().subspan(offset, count);
  }
  pdfium::span<const T> subspan(size_t offset,
                                size_t count = pdfium::dynamic_extent) const {
    return span().subspan(offset, count);
  }

  pdfium::span<T> first(size_t count) { return span().first(count); }
  pdfium::span<const T> first(size_t count) const {
    return span().first(count);
  }

  pdfium::span<T> last(size_t count) { return span().last(count); }
  pdfium::span<const T> last(size_t count) const { return span().last(count); }

 private:
  FixedSizeDataVector(T* ptr, size_t size) : data_(ptr), size_(size) {}

  std::unique_ptr<T, FxFreeDeleter> data_;
  size_t size_ = 0;
};

}  // namespace fxcrt

using fxcrt::FixedSizeDataVector;

#endif  // CORE_FXCRT_FIXED_SIZE_DATA_VECTOR_H_
