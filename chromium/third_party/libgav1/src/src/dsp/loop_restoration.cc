// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/dsp/loop_restoration.h"

#include <algorithm>  // std::max
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/common.h"
#include "src/dsp/dsp.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {

// Section 7.17.3.
// a2: range [1, 256].
// if (z >= 255)
//   a2 = 256;
// else if (z == 0)
//   a2 = 1;
// else
//   a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
const int kXByXPlus1[256] = {
    1,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
    240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
    248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
    250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    256};

// a2 = ((z << kSgrProjSgrBits) + (z >> 1)) / (z + 1);
// sgr_ma2 = 256 - a2
const uint8_t kSgrMa2Lookup[256] = {
    255, 128, 85, 64, 51, 43, 37, 32, 28, 26, 23, 21, 20, 18, 17, 16, 15, 14,
    13,  13,  12, 12, 11, 11, 10, 10, 9,  9,  9,  9,  8,  8,  8,  8,  7,  7,
    7,   7,   7,  6,  6,  6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  5,  5,  5,
    5,   5,   4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,   3,   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,   3,   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,   2,   2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,   1,   1,  0};

namespace {

constexpr ptrdiff_t kIntermediateStride = kRestorationUnitWidth + 2;

struct SgrIntermediateBuffer {
  uint16_t a;  // [1, 256]
  uint32_t b;  // < 2^20. 32-bit is required for bitdepth 10 and up.
};

struct SgrBuffer {
  // Circular buffer to save memory.
  // The 2d arrays A and B in Section 7.17.3, the intermediate results in the
  // box filter process. Reused for pass 0 and pass 1. Pass 0 uses 2 rows. Pass
  // 1 uses 3 or 4 rows.
  SgrIntermediateBuffer intermediate[6 * kIntermediateStride];
};

constexpr int kOneByX[25] = {
    4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
    293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164,
};

template <int bitdepth, typename Pixel>
struct LoopRestorationFuncs_C {
  LoopRestorationFuncs_C() = delete;

  static void SelfGuidedFilter(const void* source, void* dest,
                               const RestorationUnitInfo& restoration_info,
                               ptrdiff_t source_stride, ptrdiff_t dest_stride,
                               int width, int height,
                               RestorationBuffer* buffer);
  static void WienerFilter(const void* source, void* dest,
                           const RestorationUnitInfo& restoration_info,
                           ptrdiff_t source_stride, ptrdiff_t dest_stride,
                           int width, int height, RestorationBuffer* buffer);
  static void BoxFilterProcess(const RestorationUnitInfo& restoration_info,
                               const Pixel* src, ptrdiff_t src_stride,
                               int width, int height, SgrBuffer* buffer,
                               Pixel* dst, ptrdiff_t dst_stride);
  static void BoxFilterProcessPass1(const RestorationUnitInfo& restoration_info,
                                    const Pixel* src, ptrdiff_t src_stride,
                                    int width, int height, SgrBuffer* buffer,
                                    Pixel* dst, ptrdiff_t dst_stride);
  static void BoxFilterProcessPass2(const RestorationUnitInfo& restoration_info,
                                    const Pixel* src, ptrdiff_t src_stride,
                                    int width, int height, SgrBuffer* buffer,
                                    Pixel* dst, ptrdiff_t dst_stride);
};

// Note: range of wiener filter coefficients.
// Wiener filter coefficients are symmetric, and their sum is 1 (128).
// The range of each coefficient:
// filter[0] = filter[6], 4 bits, min = -5, max = 10.
// filter[1] = filter[5], 5 bits, min = -23, max = 8.
// filter[2] = filter[4], 6 bits, min = -17, max = 46.
// filter[3] = 128 - (filter[0] + filter[1] + filter[2]) * 2.
// The difference from libaom is that in libaom:
// filter[3] = 0 - (filter[0] + filter[1] + filter[2]) * 2.
// Thus in libaom's computation, an offset of 128 is needed for filter[3].
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, const int direction,
    int16_t* const filter) {
  filter[3] = 128;
  for (int i = 0; i < 3; ++i) {
    const int16_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter[3] -= MultiplyBy2(coeff);
  }
}

inline int CountZeroCoefficients(const int16_t* const filter) {
  int number_zero_coefficients = 0;
  if (filter[0] == 0) {
    number_zero_coefficients++;
    if (filter[1] == 0) {
      number_zero_coefficients++;
      if (filter[2] == 0) {
        number_zero_coefficients++;
      }
    }
  }
  return number_zero_coefficients;
}

template <int bitdepth, typename Pixel>
inline void WienerHorizontal(const Pixel* source, const ptrdiff_t source_stride,
                             const int width, const int height,
                             const int16_t* const filter,
                             const int number_zero_coefficients,
                             uint16_t** wiener_buffer) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  constexpr int limit =
      (1 << (bitdepth + 1 + kWienerFilterBits - kRoundBitsHorizontal)) - 1;
  constexpr int horizontal_rounding = 1 << (bitdepth + kWienerFilterBits - 1);
  int y = height;
  do {
    int x = 0;
    do {
      // sum fits into 16 bits only when bitdepth = 8.
      int sum = horizontal_rounding;
      for (int k = number_zero_coefficients; k < kCenterTap; ++k) {
        sum += filter[k] * (source[x + k] + source[x + kSubPixelTaps - 2 - k]);
      }
      sum += filter[kCenterTap] * source[x + kCenterTap];
      const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsHorizontal);
      (*wiener_buffer)[x] = static_cast<uint16_t>(Clip3(rounded_sum, 0, limit));
    } while (++x < width);
    source += source_stride;
    *wiener_buffer += width;
  } while (--y != 0);
}

template <int bitdepth, typename Pixel>
inline void WienerVertical(const uint16_t* wiener_buffer, const int width,
                           const int height, const int16_t* const filter,
                           const int number_zero_coefficients, void* const dest,
                           const ptrdiff_t dest_stride) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  constexpr int kRoundBitsVertical =
      (bitdepth == 12) ? kInterRoundBitsVertical12bpp : kInterRoundBitsVertical;
  constexpr int vertical_rounding = -(1 << (bitdepth + kRoundBitsVertical - 1));
  auto* dst = static_cast<Pixel*>(dest);
  int y = height;
  do {
    int x = 0;
    do {
      // sum needs 32 bits.
      int sum = vertical_rounding;
      for (int k = number_zero_coefficients; k < kCenterTap; ++k) {
        sum += filter[k] * (wiener_buffer[k * width + x] +
                            wiener_buffer[(kSubPixelTaps - 2 - k) * width + x]);
      }
      sum += filter[kCenterTap] * wiener_buffer[kCenterTap * width + x];
      const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
      dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
    } while (++x < width);
    wiener_buffer += width;
    dst += dest_stride;
  } while (--y != 0);
}

// Note: bit range for wiener filter.
// Wiener filter process first applies horizontal filtering to input pixels,
// followed by rounding with predefined bits (dependent on bitdepth).
// Then vertical filtering is applied, followed by rounding (dependent on
// bitdepth).
// The process is the same as convolution:
// <input> --> <horizontal filter> --> <rounding 0> --> <vertical filter>
// --> <rounding 1>
// By design:
// (a). horizontal/vertical filtering adds 7 bits to input.
// (b). The output of first rounding fits into 16 bits.
// (c). The output of second rounding fits into 16 bits.
// If input bitdepth > 8, the accumulator of the horizontal filter is larger
// than 16 bit and smaller than 32 bits.
// The accumulator of the vertical filter is larger than 16 bits and smaller
// than 32 bits.
template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::WienerFilter(
    const void* const source, void* const dest,
    const RestorationUnitInfo& restoration_info, ptrdiff_t source_stride,
    ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* const buffer) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  int16_t filter_horizontal[kSubPixelTaps / 2];
  int16_t filter_vertical[kSubPixelTaps / 2];
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal,
                             filter_horizontal);
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical,
                             filter_vertical);
  const int number_zero_coefficients_horizontal =
      CountZeroCoefficients(filter_horizontal);
  const int number_zero_coefficients_vertical =
      CountZeroCoefficients(filter_vertical);
  const int number_rows_to_skip =
      std::max(number_zero_coefficients_vertical, 1);

  // horizontal filtering.
  const auto* src = static_cast<const Pixel*>(source);
  src -= (kCenterTap - number_rows_to_skip) * source_stride + kCenterTap;
  auto* wiener_buffer = buffer->wiener_buffer + number_rows_to_skip * width;
  const int height_horizontal =
      height + kSubPixelTaps - 2 - 2 * number_rows_to_skip;

  if (number_zero_coefficients_horizontal == 0) {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 0,
                                      &wiener_buffer);
  } else if (number_zero_coefficients_horizontal == 1) {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 1,
                                      &wiener_buffer);
  } else if (number_zero_coefficients_horizontal == 2) {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 2,
                                      &wiener_buffer);
  } else {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 3,
                                      &wiener_buffer);
  }

  // vertical filtering.
  if (number_zero_coefficients_vertical == 0) {
    // Because the top row of |source| is a duplicate of the second row, and the
    // bottom row of |source| is a duplicate of its above row, we can duplicate
    // the top and bottom row of |wiener_buffer| accordingly.
    memcpy(wiener_buffer, wiener_buffer - width,
           sizeof(*wiener_buffer) * width);
    memcpy(buffer->wiener_buffer, buffer->wiener_buffer + width,
           sizeof(*wiener_buffer) * width);
    WienerVertical<bitdepth, Pixel>(buffer->wiener_buffer, width, height,
                                    filter_vertical, 0, dest, dest_stride);
  } else if (number_zero_coefficients_vertical == 1) {
    WienerVertical<bitdepth, Pixel>(buffer->wiener_buffer, width, height,
                                    filter_vertical, 1, dest, dest_stride);
  } else if (number_zero_coefficients_vertical == 2) {
    WienerVertical<bitdepth, Pixel>(buffer->wiener_buffer, width, height,
                                    filter_vertical, 2, dest, dest_stride);
  } else {
    WienerVertical<bitdepth, Pixel>(buffer->wiener_buffer, width, height,
                                    filter_vertical, 3, dest, dest_stride);
  }
}

//------------------------------------------------------------------------------
// SGR

template <int bitdepth>
inline void CalculateIntermediate(const uint32_t s, uint32_t a,
                                  const uint32_t b, const uint32_t n,
                                  SgrIntermediateBuffer* const intermediate) {
  // a: before shift, max is 25 * (2^(bitdepth) - 1) * (2^(bitdepth) - 1).
  // since max bitdepth = 12, max < 2^31.
  // after shift, a < 2^16 * n < 2^22 regardless of bitdepth
  a = RightShiftWithRounding(a, (bitdepth - 8) << 1);
  // b: max is 25 * (2^(bitdepth) - 1). If bitdepth = 12, max < 2^19.
  // d < 2^8 * n < 2^14 regardless of bitdepth
  const uint32_t d = RightShiftWithRounding(b, bitdepth - 8);
  // p: Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
  // and p itself satisfies p < 2^14 * n^2 < 2^26.
  // This bound on p is due to:
  // https://en.wikipedia.org/wiki/Popoviciu's_inequality_on_variances
  // Note: Sometimes, in high bitdepth, we can end up with a*n < b*b.
  // This is an artifact of rounding, and can only happen if all pixels
  // are (almost) identical, so in this case we saturate to p=0.
  const uint32_t p = (a * n < d * d) ? 0 : a * n - d * d;
  // p * s < (2^14 * n^2) * round(2^20 / (n^2 * scale)) < 2^34 / scale <
  // 2^32 as long as scale >= 4. So p * s fits into a uint32_t, and z < 2^12
  // (this holds even after accounting for the rounding in s)
  const uint32_t z = RightShiftWithRounding(p * s, kSgrProjScaleBits);
  // a2: range [1, 256].
  uint32_t a2 = kXByXPlus1[std::min(z, 255u)];
  const uint32_t one_over_n = kOneByX[n - 1];
  // (kSgrProjSgrBits - a2) < 2^8, b < 2^(bitdepth) * n,
  // one_over_n = round(2^12 / n)
  // => the product here is < 2^(20 + bitdepth) <= 2^32,
  // and b is set to a value < 2^(8 + bitdepth).
  // This holds even with the rounding in one_over_n and in the overall
  // result, as long as (kSgrProjSgrBits - a2) is strictly less than 2^8.
  const uint32_t b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n;
  intermediate->a = a2;
  intermediate->b = RightShiftWithRounding(b2, kSgrProjReciprocalBits);
}

template <int bitdepth, typename Pixel>
LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessTop(
    const Pixel* src, const ptrdiff_t stride, const int width, const uint32_t s,
    SgrIntermediateBuffer* intermediate) {
  uint32_t a = 0;
  uint32_t b = 0;
  for (int dx = 0; dx < 5; ++dx) {
    const Pixel source = src[dx];
    a += source * source;
    b += source;
  }
  a += a;
  b += b;
  for (int dy = 1; dy < 4; ++dy) {
    for (int dx = 0; dx < 5; ++dx) {
      const Pixel source = src[dy * stride + dx];
      a += source * source;
      b += source;
    }
  }
  CalculateIntermediate<bitdepth>(s, a, b, 25, intermediate);
  int x = width - 1;
  do {
    {
      const Pixel source0 = src[0];
      const Pixel source1 = src[5];
      a += 2 * (source1 * source1 - source0 * source0);
      b += 2 * (source1 - source0);
    }
    int dy = 1;
    do {
      const Pixel source0 = src[dy * stride];
      const Pixel source1 = src[dy * stride + 5];
      a -= source0 * source0;
      a += source1 * source1;
      b -= source0;
      b += source1;
    } while (++dy < 4);
    src++;
    CalculateIntermediate<bitdepth>(s, a, b, 25, ++intermediate);
  } while (--x != 0);
}

template <int bitdepth, typename Pixel, int size>
LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess(
    const Pixel* src, const ptrdiff_t stride, const int width, const uint32_t s,
    SgrIntermediateBuffer* intermediate) {
  const int n = size * size;
  uint32_t a = 0;
  uint32_t b = 0;
  for (int dy = 0; dy < size; ++dy) {
    for (int dx = 0; dx < size; ++dx) {
      const Pixel source = src[dy * stride + dx];
      a += source * source;
      b += source;
    }
  }
  CalculateIntermediate<bitdepth>(s, a, b, n, intermediate);
  int x = width - 1;
  do {
    int dy = 0;
    do {
      const Pixel source0 = src[dy * stride];
      const Pixel source1 = src[dy * stride + size];
      a -= source0 * source0;
      a += source1 * source1;
      b -= source0;
      b += source1;
    } while (++dy < size);
    src++;
    CalculateIntermediate<bitdepth>(s, a, b, n, ++intermediate);
  } while (--x != 0);
}

template <int bitdepth, typename Pixel>
LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcessBottom(
    const Pixel* src, const ptrdiff_t stride, const int width, const uint32_t s,
    SgrIntermediateBuffer* intermediate) {
  uint32_t a = 0;
  uint32_t b = 0;
  for (int dx = 0; dx < 5; ++dx) {
    const Pixel source = src[3 * stride + dx];
    a += source * source;
    b += source;
  }
  a += a;
  b += b;
  for (int dy = 0; dy < 3; ++dy) {
    for (int dx = 0; dx < 5; ++dx) {
      const Pixel source = src[dy * stride + dx];
      a += source * source;
      b += source;
    }
  }
  CalculateIntermediate<bitdepth>(s, a, b, 25, intermediate);
  int x = width - 1;
  do {
    {
      const Pixel source0 = src[3 * stride + 0];
      const Pixel source1 = src[3 * stride + 5];
      a += 2 * (source1 * source1 - source0 * source0);
      b += 2 * (source1 - source0);
    }
    int dy = 0;
    do {
      const Pixel source0 = src[dy * stride];
      const Pixel source1 = src[dy * stride + 5];
      a -= source0 * source0;
      a += source1 * source1;
      b -= source0;
      b += source1;
    } while (++dy < 3);
    src++;
    CalculateIntermediate<bitdepth>(s, a, b, 25, ++intermediate);
  } while (--x != 0);
}

inline void Sum565(const SgrIntermediateBuffer* const intermediate,
                   uint16_t* const a, uint32_t* const b) {
  *a = 5 * (intermediate[0].a + intermediate[2].a) + 6 * intermediate[1].a;
  *b = 5 * (intermediate[0].b + intermediate[2].b) + 6 * intermediate[1].b;
}

template <typename Pixel>
inline int CalculateFilteredOutput(const Pixel src, const uint32_t a,
                                   const uint32_t b, const int shift) {
  // v < 2^32. All intermediate calculations are positive.
  const uint32_t v = a * src + b;
  return RightShiftWithRounding(v,
                                kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <typename Pixel>
inline void BoxFilterPass1(const Pixel src0, const Pixel src1,
                           const SgrIntermediateBuffer* const intermediate[2],
                           const ptrdiff_t x, int p[2]) {
  uint16_t a[2];
  uint32_t b[2];
  Sum565(intermediate[0] + x, &a[0], &b[0]);
  Sum565(intermediate[1] + x, &a[1], &b[1]);
  p[0] = CalculateFilteredOutput<Pixel>(src0, a[0] + a[1], b[0] + b[1], 5);
  p[1] = CalculateFilteredOutput<Pixel>(src1, a[1], b[1], 4);
}

template <typename Pixel>
inline int BoxFilterPass2(const Pixel src,
                          const SgrIntermediateBuffer* const intermediate[3],
                          const ptrdiff_t x) {
  const uint32_t a = 3 * (intermediate[0][x + 0].a + intermediate[0][x + 2].a +
                          intermediate[2][x + 0].a + intermediate[2][x + 2].a) +
                     4 * (intermediate[0][x + 1].a + intermediate[1][x + 0].a +
                          intermediate[1][x + 1].a + intermediate[1][x + 2].a +
                          intermediate[2][x + 1].a);
  const uint32_t b = 3 * (intermediate[0][x + 0].b + intermediate[0][x + 2].b +
                          intermediate[2][x + 0].b + intermediate[2][x + 2].b) +
                     4 * (intermediate[0][x + 1].b + intermediate[1][x + 0].b +
                          intermediate[1][x + 1].b + intermediate[1][x + 2].b +
                          intermediate[2][x + 1].b);
  return CalculateFilteredOutput<Pixel>(src, a, b, 5);
}

template <int bitdepth, typename Pixel>
inline Pixel SelfGuidedDoubleMultiplier(const int src,
                                        const int box_filter_process_output0,
                                        const int box_filter_process_output1,
                                        const int16_t w0, const int16_t w1,
                                        const int16_t w2) {
  const int v = w1 * (src << kSgrProjRestoreBits) +
                w0 * box_filter_process_output0 +
                w2 * box_filter_process_output1;
  // if radius_pass_0 == 0 and radius_pass_1 == 0, the range of v is:
  // bits(u) + bits(w0/w1/w2) + 2 = bitdepth + 13.
  // Then, range of s is bitdepth + 2. This is a rough estimation, taking
  // the maximum value of each element.
  const int s =
      RightShiftWithRounding(v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  return static_cast<Pixel>(Clip3(s, 0, (1 << bitdepth) - 1));
}

template <int bitdepth, typename Pixel>
inline Pixel SelfGuidedSingleMultiplier(const int src,
                                        const int box_filter_process_output,
                                        const int16_t w0, const int16_t w1) {
  const int v =
      w1 * (src << kSgrProjRestoreBits) + w0 * box_filter_process_output;
  // if radius_pass_0 == 0 and radius_pass_1 == 0, the range of v is:
  // bits(u) + bits(w0/w1/w2) + 2 = bitdepth + 13.
  // Then, range of s is bitdepth + 2. This is a rough estimation, taking
  // the maximum value of each element.
  const int s =
      RightShiftWithRounding(v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  return static_cast<Pixel>(Clip3(s, 0, (1 << bitdepth) - 1));
}

template <int bitdepth, typename Pixel>
inline void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcess(
    const RestorationUnitInfo& restoration_info, const Pixel* src,
    const ptrdiff_t src_stride, const int width, const int height,
    SgrBuffer* const buffer, Pixel* dst, const ptrdiff_t dst_stride) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t s0 = kSgrScaleParameter[sgr_proj_index][0];  // s0 < 2^12.
  const uint32_t s1 = kSgrScaleParameter[sgr_proj_index][1];  // s1 < 2^12.
  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  SgrIntermediateBuffer *intermediate0[2], *intermediate1[4];
  assert(s0 != 0);
  assert(s1 != 0);
  intermediate0[0] = buffer->intermediate;
  intermediate0[1] = intermediate0[0] + kIntermediateStride;
  intermediate1[0] = intermediate0[1] + kIntermediateStride;
  intermediate1[1] = intermediate1[0] + kIntermediateStride,
  intermediate1[2] = intermediate1[1] + kIntermediateStride,
  intermediate1[3] = intermediate1[2] + kIntermediateStride;
  BoxFilterPreProcessTop<bitdepth, Pixel>(src - 2 * src_stride - 3, src_stride,
                                          width + 2, s0, intermediate0[0]);
  BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 2 * src_stride - 2, src_stride,
                                          width + 2, s1, intermediate1[0]);
  BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 1 * src_stride - 2, src_stride,
                                          width + 2, s1, intermediate1[1]);
  for (int y = height >> 1; y != 0; --y) {
    BoxFilterPreProcess<bitdepth, Pixel, 5>(src - src_stride - 3, src_stride,
                                            width + 2, s0, intermediate0[1]);
    BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 2, src_stride, width + 2, s1,
                                            intermediate1[2]);
    BoxFilterPreProcess<bitdepth, Pixel, 3>(src + src_stride - 2, src_stride,
                                            width + 2, s1, intermediate1[3]);
    int x = 0;
    do {
      int p[2][2];
      BoxFilterPass1<Pixel>(src[x], src[src_stride + x], intermediate0, x,
                            p[0]);
      p[1][0] = BoxFilterPass2<Pixel>(src[x], intermediate1, x);
      p[1][1] =
          BoxFilterPass2<Pixel>(src[src_stride + x], intermediate1 + 1, x);
      dst[x] = SelfGuidedDoubleMultiplier<bitdepth, Pixel>(src[x], p[0][0],
                                                           p[1][0], w0, w1, w2);
      dst[dst_stride + x] = SelfGuidedDoubleMultiplier<bitdepth, Pixel>(
          src[src_stride + x], p[0][1], p[1][1], w0, w1, w2);
    } while (++x < width);
    src += 2 * src_stride;
    dst += 2 * dst_stride;
    std::swap(intermediate0[0], intermediate0[1]);
    std::swap(intermediate1[0], intermediate1[2]);
    std::swap(intermediate1[1], intermediate1[3]);
  }
  if ((height & 1) != 0) {
    BoxFilterPreProcessBottom<bitdepth, Pixel>(src - src_stride - 3, src_stride,
                                               width + 2, s0, intermediate0[1]);
    BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 2, src_stride, width + 2, s1,
                                            intermediate1[2]);
    int x = 0;
    do {
      int p[2][2];
      BoxFilterPass1<Pixel>(src[x], src[src_stride + x], intermediate0, x,
                            p[0]);
      p[1][0] = BoxFilterPass2<Pixel>(src[x], intermediate1, x);
      dst[x] = SelfGuidedDoubleMultiplier<bitdepth, Pixel>(src[x], p[0][0],
                                                           p[1][0], w0, w1, w2);
    } while (++x < width);
  }
}

template <int bitdepth, typename Pixel>
inline void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcessPass1(
    const RestorationUnitInfo& restoration_info, const Pixel* src,
    const ptrdiff_t src_stride, const int width, const int height,
    SgrBuffer* const buffer, Pixel* dst, const ptrdiff_t dst_stride) {
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t s = kSgrScaleParameter[sgr_proj_index][0];  // s < 2^12.
  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = (1 << kSgrProjPrecisionBits) - w0;
  SgrIntermediateBuffer* intermediate[2];
  assert(s != 0);
  intermediate[0] = buffer->intermediate;
  intermediate[1] = intermediate[0] + kIntermediateStride;
  BoxFilterPreProcessTop<bitdepth, Pixel>(src - 2 * src_stride - 3, src_stride,
                                          width + 2, s, intermediate[0]);
  for (int y = height >> 1; y != 0; --y) {
    BoxFilterPreProcess<bitdepth, Pixel, 5>(src - src_stride - 3, src_stride,
                                            width + 2, s, intermediate[1]);
    int x = 0;
    do {
      int p[2];
      BoxFilterPass1<Pixel>(src[x], src[src_stride + x], intermediate, x, p);
      dst[x] =
          SelfGuidedSingleMultiplier<bitdepth, Pixel>(src[x], p[0], w0, w1);
      dst[dst_stride + x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(
          src[src_stride + x], p[1], w0, w1);
    } while (++x < width);
    src += 2 * src_stride;
    dst += 2 * dst_stride;
    std::swap(intermediate[0], intermediate[1]);
  }
  if ((height & 1) != 0) {
    BoxFilterPreProcessBottom<bitdepth, Pixel>(src - src_stride - 3, src_stride,
                                               width + 2, s, intermediate[1]);
    int x = 0;
    do {
      int p[2];
      BoxFilterPass1<Pixel>(src[x], src[src_stride + x], intermediate, x, p);
      dst[x] =
          SelfGuidedSingleMultiplier<bitdepth, Pixel>(src[x], p[0], w0, w1);
      dst[dst_stride + x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(
          src[src_stride + x], p[1], w0, w1);
    } while (++x < width);
  }
}

template <int bitdepth, typename Pixel>
inline void LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcessPass2(
    const RestorationUnitInfo& restoration_info, const Pixel* src,
    const ptrdiff_t src_stride, const int width, const int height,
    SgrBuffer* const buffer, Pixel* dst, const ptrdiff_t dst_stride) {
  assert(restoration_info.sgr_proj_info.multiplier[0] == 0);
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w0 = (1 << kSgrProjPrecisionBits) - w1;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t s = kSgrScaleParameter[sgr_proj_index][1];  // s < 2^12.
  SgrIntermediateBuffer* intermediate[3];
  assert(s != 0);
  intermediate[0] = buffer->intermediate;
  intermediate[1] = intermediate[0] + kIntermediateStride;
  intermediate[2] = intermediate[1] + kIntermediateStride;
  BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 2 * src_stride - 2, src_stride,
                                          width + 2, s, intermediate[0]);
  BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 1 * src_stride - 2, src_stride,
                                          width + 2, s, intermediate[1]);
  int y = height;
  do {
    BoxFilterPreProcess<bitdepth, Pixel, 3>(src - 2, src_stride, width + 2, s,
                                            intermediate[2]);
    int x = 0;
    do {
      const int p = BoxFilterPass2<Pixel>(src[x], intermediate, x);
      dst[x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(src[x], p, w0, w1);
    } while (++x < width);
    src += src_stride;
    dst += dst_stride;
    SgrIntermediateBuffer* const intermediate0 = intermediate[0];
    intermediate[0] = intermediate[1];
    intermediate[1] = intermediate[2];
    intermediate[2] = intermediate0;
  } while (--y != 0);
}

template <int bitdepth, typename Pixel>
void LoopRestorationFuncs_C<bitdepth, Pixel>::SelfGuidedFilter(
    const void* const source, void* const dest,
    const RestorationUnitInfo& restoration_info, ptrdiff_t source_stride,
    ptrdiff_t dest_stride, int width, int height,
    RestorationBuffer* const /*buffer*/) {
  const int index = restoration_info.sgr_proj_info.index;
  const int radius_pass_0 = kSgrProjParams[index][0];  // 2 or 0
  const int radius_pass_1 = kSgrProjParams[index][2];  // 1 or 0
  const auto* src = static_cast<const Pixel*>(source);
  auto* dst = static_cast<Pixel*>(dest);
  SgrBuffer buffer;
  if (radius_pass_1 == 0) {
    // |radius_pass_0| and |radius_pass_1| cannot both be 0, so we have the
    // following assertion.
    assert(radius_pass_0 != 0);
    LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcessPass1(
        restoration_info, src, source_stride, width, height, &buffer, dst,
        dest_stride);
  } else if (radius_pass_0 == 0) {
    LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcessPass2(
        restoration_info, src, source_stride, width, height, &buffer, dst,
        dest_stride);
  } else {
    LoopRestorationFuncs_C<bitdepth, Pixel>::BoxFilterProcess(
        restoration_info, src, source_stride, width, height, &buffer, dst,
        dest_stride);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] = LoopRestorationFuncs_C<8, uint8_t>::WienerFilter;
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<8, uint8_t>::SelfGuidedFilter;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_WienerFilter
  dsp->loop_restorations[0] = LoopRestorationFuncs_C<8, uint8_t>::WienerFilter;
#endif
#ifndef LIBGAV1_Dsp8bpp_SelfGuidedFilter
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<8, uint8_t>::SelfGuidedFilter;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10

void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] =
      LoopRestorationFuncs_C<10, uint16_t>::WienerFilter;
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<10, uint16_t>::SelfGuidedFilter;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_WienerFilter
  dsp->loop_restorations[0] =
      LoopRestorationFuncs_C<10, uint16_t>::WienerFilter;
#endif
#ifndef LIBGAV1_Dsp10bpp_SelfGuidedFilter
  dsp->loop_restorations[1] =
      LoopRestorationFuncs_C<10, uint16_t>::SelfGuidedFilter;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#endif  // LIBGAV1_MAX_BITDEPTH >= 10
}  // namespace

void LoopRestorationInit_C() {
  Init8bpp();
#if LIBGAV1_MAX_BITDEPTH >= 10
  Init10bpp();
#endif
  // Local functions that may be unused depending on the optimizations
  // available.
  static_cast<void>(CountZeroCoefficients);
  static_cast<void>(PopulateWienerCoefficients);
  static_cast<void>(Sum565);
}

}  // namespace dsp
}  // namespace libgav1
