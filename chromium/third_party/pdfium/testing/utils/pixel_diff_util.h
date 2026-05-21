// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_UTILS_PIXEL_DIFF_UTIL_H_
#define TESTING_UTILS_PIXEL_DIFF_UTIL_H_

#include <cstdint>

// Returns the largest difference in pixel channels between `baseline_pixel` and
// `actual_pixel`. Pixels are expected to be in 32-bit ARGB format.
uint8_t MaxPixelPerChannelDelta(uint32_t baseline_pixel, uint32_t actual_pixel);

#endif  // TESTING_UTILS_PIXEL_DIFF_UTIL_H_
