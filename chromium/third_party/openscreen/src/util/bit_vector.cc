// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/bit_vector.h"

#include <algorithm>

namespace openscreen {

BitVector::BitVector(size_t size, Fill fill) {
  Resize(size, fill);
}

void BitVector::Resize(size_t size, Fill fill) {
  size_ = size;
  v_.assign((size + kBitsPerWord - 1) / kBitsPerWord,
            fill ? ~uint64_t{0} : uint64_t{0});
  if (fill && size % kBitsPerWord != 0) {
    v_.back() &= (uint64_t{1} << (size % kBitsPerWord)) - 1;
  }
}

size_t BitVector::FindFirstSet() const {
  for (size_t i = 0; i < v_.size(); ++i) {
    if (v_[i] != 0) {
      size_t pos = i * kBitsPerWord + std::countr_zero(v_[i]);
      return (pos < size_) ? pos : size_;
    }
  }
  return size_;
}

}  // namespace openscreen
