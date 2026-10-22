// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_BIT_VECTOR_H_
#define UTIL_BIT_VECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <limits>
#include <vector>

#include "util/osp_logging.h"

namespace openscreen {

// A simple dynamic bit vector implementation using C++20 <bit> and std::vector.
// This is used for tracking packet transmission flags in the Sender.
class BitVector {
 public:
  enum Fill : bool { SET = true, CLEARED = false };

  BitVector() noexcept = default;
  BitVector(size_t size, Fill fill);

  ~BitVector() = default;

  BitVector(BitVector&& other) noexcept = default;
  BitVector& operator=(BitVector&& other) noexcept = default;

  BitVector(const BitVector& other) = default;
  BitVector& operator=(const BitVector& other) = default;

  [[nodiscard]] size_t size() const noexcept { return size_; }

  void Resize(size_t size, Fill fill);

  void Set(size_t pos) {
    OSP_CHECK_LT(pos, size_);
    v_[pos / kBitsPerWord] |= (uint64_t{1} << (pos % kBitsPerWord));
  }

  void Clear(size_t pos) {
    OSP_CHECK_LT(pos, size_);
    v_[pos / kBitsPerWord] &= ~(uint64_t{1} << (pos % kBitsPerWord));
  }

  [[nodiscard]] bool IsSet(size_t pos) const {
    OSP_CHECK_LT(pos, size_);
    return (v_[pos / kBitsPerWord] >> (pos % kBitsPerWord)) & 1;
  }

  [[nodiscard]] size_t FindFirstSet() const;

 private:
  static constexpr size_t kBitsPerWord = std::numeric_limits<uint64_t>::digits;

  std::vector<uint64_t> v_;
  size_t size_ = 0;
};

}  // namespace openscreen

#endif  // UTIL_BIT_VECTOR_H_
