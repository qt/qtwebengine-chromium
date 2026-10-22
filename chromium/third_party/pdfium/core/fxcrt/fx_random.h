// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_RANDOM_H_
#define CORE_FXCRT_FX_RANDOM_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "core/fxcrt/span.h"

// A Mersenne Twister (MT) pseudo-random number generator.
class FX_Random {
 public:
  static constexpr size_t kStateSize = 848;

  explicit FX_Random(uint32_t seed);

  FX_Random(const FX_Random&) = delete;
  FX_Random& operator=(const FX_Random&) = delete;

  ~FX_Random();

  // Using a temporary MT generator, fills `buffer` with random 32-bit unsigned
  // integers.
  static void Fill(pdfium::span<uint32_t> buffer);

  // Returns a single random 32-bit unsigned integer.
  uint32_t Generate();

 private:
  uint32_t next_index_;
  std::array<uint32_t, kStateSize> state_;
};

#endif  // CORE_FXCRT_FX_RANDOM_H_
