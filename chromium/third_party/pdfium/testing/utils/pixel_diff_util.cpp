// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/utils/pixel_diff_util.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct UnpackedPixel {
  explicit UnpackedPixel(uint32_t packed)
      : red(packed & 0xff),
        green((packed >> 8) & 0xff),
        blue((packed >> 16) & 0xff),
        alpha((packed >> 24) & 0xff) {}

  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
};

uint8_t ChannelDelta(uint8_t baseline_channel, uint8_t actual_channel) {
  // No casts are necessary because arithmetic operators implicitly convert
  // `uint8_t` to `int` first. The final delta is always in the range 0 to 255.
  return std::abs(baseline_channel - actual_channel);
}

}  // namespace

uint8_t MaxPixelPerChannelDelta(uint32_t baseline_pixel,
                                uint32_t actual_pixel) {
  UnpackedPixel baseline_unpacked(baseline_pixel);
  UnpackedPixel actual_unpacked(actual_pixel);
  return std::max(
      {ChannelDelta(baseline_unpacked.red, actual_unpacked.red),
       ChannelDelta(baseline_unpacked.green, actual_unpacked.green),
       ChannelDelta(baseline_unpacked.blue, actual_unpacked.blue),
       ChannelDelta(baseline_unpacked.alpha, actual_unpacked.alpha)});
}
