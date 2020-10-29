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

#include <algorithm>
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
// ma = 256 - a2;
const uint8_t kSgrMaLookup[256] = {
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

template <int bitdepth, typename Pixel>
inline void WienerHorizontal(const Pixel* source, const ptrdiff_t source_stride,
                             const int width, const int height,
                             const int16_t* const filter,
                             const int number_zero_coefficients,
                             int16_t** wiener_buffer) {
  constexpr int kCenterTap = kWienerFilterTaps / 2;
  constexpr int kRoundBitsHorizontal = (bitdepth == 12)
                                           ? kInterRoundBitsHorizontal12bpp
                                           : kInterRoundBitsHorizontal;
  constexpr int offset =
      1 << (bitdepth + kWienerFilterBits - kRoundBitsHorizontal - 1);
  constexpr int limit = (offset << 2) - 1;
  int y = height;
  do {
    int x = 0;
    do {
      // sum fits into 16 bits only when bitdepth = 8.
      int sum = 0;
      for (int k = number_zero_coefficients; k < kCenterTap; ++k) {
        sum +=
            filter[k] * (source[x + k] + source[x + kWienerFilterTaps - 1 - k]);
      }
      sum += filter[kCenterTap] * source[x + kCenterTap];
      const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsHorizontal);
      (*wiener_buffer)[x] = Clip3(rounded_sum, -offset, limit - offset);
    } while (++x != width);
    source += source_stride;
    *wiener_buffer += width;
  } while (--y != 0);
}

template <int bitdepth, typename Pixel>
inline void WienerVertical(const int16_t* wiener_buffer, const int width,
                           const int height, const int16_t* const filter,
                           const int number_zero_coefficients, void* const dest,
                           const ptrdiff_t dest_stride) {
  constexpr int kCenterTap = kWienerFilterTaps / 2;
  constexpr int kRoundBitsVertical =
      (bitdepth == 12) ? kInterRoundBitsVertical12bpp : kInterRoundBitsVertical;
  auto* dst = static_cast<Pixel*>(dest);
  int y = height;
  do {
    int x = 0;
    do {
      // sum needs 32 bits.
      int sum = 0;
      for (int k = number_zero_coefficients; k < kCenterTap; ++k) {
        sum += filter[k] *
               (wiener_buffer[k * width + x] +
                wiener_buffer[(kWienerFilterTaps - 1 - k) * width + x]);
      }
      sum += filter[kCenterTap] * wiener_buffer[kCenterTap * width + x];
      const int rounded_sum = RightShiftWithRounding(sum, kRoundBitsVertical);
      dst[x] = static_cast<Pixel>(Clip3(rounded_sum, 0, (1 << bitdepth) - 1));
    } while (++x != width);
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
// Note: range of wiener filter coefficients.
// Wiener filter coefficients are symmetric, and their sum is 1 (128).
// The range of each coefficient:
// filter[0] = filter[6], 4 bits, min = -5, max = 10.
// filter[1] = filter[5], 5 bits, min = -23, max = 8.
// filter[2] = filter[4], 6 bits, min = -17, max = 46.
// filter[3] = 128 - 2 * (filter[0] + filter[1] + filter[2]).
// The difference from libaom is that in libaom:
// filter[3] = 0 - 2 * (filter[0] + filter[1] + filter[2]).
// Thus in libaom's computation, an offset of 128 is needed for filter[3].
template <int bitdepth, typename Pixel>
void WienerFilter_C(const void* const source, void* const dest,
                    const RestorationUnitInfo& restoration_info,
                    ptrdiff_t source_stride, ptrdiff_t dest_stride, int width,
                    int height, RestorationBuffer* const restoration_buffer) {
  constexpr int kCenterTap = kWienerFilterTaps / 2;
  const int16_t* const number_leading_zero_coefficients =
      restoration_info.wiener_info.number_leading_zero_coefficients;
  const int number_rows_to_skip = std::max(
      static_cast<int>(number_leading_zero_coefficients[WienerInfo::kVertical]),
      1);
  int16_t* const wiener_buffer_org = restoration_buffer->wiener_buffer;

  // horizontal filtering.
  const int height_horizontal =
      height + kWienerFilterTaps - 1 - 2 * number_rows_to_skip;
  const int16_t* const filter_horizontal =
      restoration_info.wiener_info.filter[WienerInfo::kHorizontal];
  const auto* src = static_cast<const Pixel*>(source);
  src -= (kCenterTap - number_rows_to_skip) * source_stride + kCenterTap;
  auto* wiener_buffer = wiener_buffer_org + number_rows_to_skip * width;

  if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 0) {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 0,
                                      &wiener_buffer);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 1) {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 1,
                                      &wiener_buffer);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 2) {
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 2,
                                      &wiener_buffer);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kHorizontal] == 3);
    WienerHorizontal<bitdepth, Pixel>(src, source_stride, width,
                                      height_horizontal, filter_horizontal, 3,
                                      &wiener_buffer);
  }

  // vertical filtering.
  const int16_t* const filter_vertical =
      restoration_info.wiener_info.filter[WienerInfo::kVertical];
  if (number_leading_zero_coefficients[WienerInfo::kVertical] == 0) {
    // Because the top row of |source| is a duplicate of the second row, and the
    // bottom row of |source| is a duplicate of its above row, we can duplicate
    // the top and bottom row of |wiener_buffer| accordingly.
    memcpy(wiener_buffer, wiener_buffer - width,
           sizeof(*wiener_buffer) * width);
    memcpy(wiener_buffer_org, wiener_buffer_org + width,
           sizeof(*wiener_buffer) * width);
    WienerVertical<bitdepth, Pixel>(wiener_buffer_org, width, height,
                                    filter_vertical, 0, dest, dest_stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 1) {
    WienerVertical<bitdepth, Pixel>(wiener_buffer_org, width, height,
                                    filter_vertical, 1, dest, dest_stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 2) {
    WienerVertical<bitdepth, Pixel>(wiener_buffer_org, width, height,
                                    filter_vertical, 2, dest, dest_stride);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kVertical] == 3);
    WienerVertical<bitdepth, Pixel>(wiener_buffer_org, width, height,
                                    filter_vertical, 3, dest, dest_stride);
  }
}

//------------------------------------------------------------------------------
// SGR

template <typename Pixel, int size>
LIBGAV1_ALWAYS_INLINE void BoxSum(const Pixel* src, const ptrdiff_t src_stride,
                                  const int height, const int width,
                                  uint16_t* sums, uint32_t* square_sums,
                                  const ptrdiff_t sum_stride) {
  int y = height;
  do {
    uint32_t sum = 0;
    uint32_t square_sum = 0;
    for (int dx = 0; dx < size; ++dx) {
      const Pixel source = src[dx];
      sum += source;
      square_sum += source * source;
    }
    sums[0] = sum;
    square_sums[0] = square_sum;
    int x = 1;
    do {
      const Pixel source0 = src[x - 1];
      const Pixel source1 = src[x - 1 + size];
      sum -= source0;
      sum += source1;
      square_sum -= source0 * source0;
      square_sum += source1 * source1;
      sums[x] = sum;
      square_sums[x] = square_sum;
    } while (++x != width);
    src += src_stride;
    sums += sum_stride;
    square_sums += sum_stride;
  } while (--y != 0);
}

template <typename Pixel>
LIBGAV1_ALWAYS_INLINE void BoxSum(const Pixel* src, const ptrdiff_t src_stride,
                                  const int height, const int width,
                                  uint16_t* sum3, uint16_t* sum5,
                                  uint32_t* square_sum3, uint32_t* square_sum5,
                                  const ptrdiff_t sum_stride) {
  int y = height;
  do {
    uint32_t sum = 0;
    uint32_t square_sum = 0;
    for (int dx = 0; dx < 4; ++dx) {
      const Pixel source = src[dx];
      sum += source;
      square_sum += source * source;
    }
    int x = 0;
    do {
      const Pixel source0 = src[x];
      const Pixel source1 = src[x + 4];
      sum -= source0;
      square_sum -= source0 * source0;
      sum3[x] = sum;
      square_sum3[x] = square_sum;
      sum += source1;
      square_sum += source1 * source1;
      sum5[x] = sum + source0;
      square_sum5[x] = square_sum + source0 * source0;
    } while (++x != width);
    src += src_stride;
    sum3 += sum_stride;
    sum5 += sum_stride;
    square_sum3 += sum_stride;
    square_sum5 += sum_stride;
  } while (--y != 0);
}

template <int bitdepth, int n>
inline void CalculateIntermediate(const uint32_t s, uint32_t a,
                                  const uint32_t b, uint8_t* const ma_ptr,
                                  uint32_t* const b_ptr) {
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
  // ma: range [0, 255].
  const uint32_t ma = kSgrMaLookup[std::min(z, 255u)];
  const uint32_t one_over_n = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  // ma < 2^8, b < 2^(bitdepth) * n,
  // one_over_n = round(2^12 / n)
  // => the product here is < 2^(20 + bitdepth) <= 2^32,
  // and b is set to a value < 2^(8 + bitdepth).
  // This holds even with the rounding in one_over_n and in the overall result,
  // as long as ma is strictly less than 2^8.
  const uint32_t b2 = ma * b * one_over_n;
  *ma_ptr = ma;
  *b_ptr = RightShiftWithRounding(b2, kSgrProjReciprocalBits);
}

template <typename T>
inline uint32_t Sum343(const T* const src) {
  return 3 * (src[0] + src[2]) + 4 * src[1];
}

template <typename T>
inline uint32_t Sum444(const T* const src) {
  return 4 * (src[0] + src[1] + src[2]);
}

template <typename T>
inline uint32_t Sum565(const T* const src) {
  return 5 * (src[0] + src[2]) + 6 * src[1];
}

template <int bitdepth>
LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess5(
    const uint16_t* const sum5[5], const uint32_t* const square_sum5[5],
    const int width, const uint32_t s, SgrBuffer* const sgr_buffer,
    uint16_t* const ma565, uint32_t* const b565) {
  int x = 0;
  do {
    uint32_t a = 0;
    uint32_t b = 0;
    for (int dy = 0; dy < 5; ++dy) {
      a += square_sum5[dy][x];
      b += sum5[dy][x];
    }
    CalculateIntermediate<bitdepth, 25>(s, a, b, sgr_buffer->ma + x,
                                        sgr_buffer->b + x);
  } while (++x != width + 2);
  x = 0;
  do {
    ma565[x] = Sum565(sgr_buffer->ma + x);
    b565[x] = Sum565(sgr_buffer->b + x);
  } while (++x != width);
}

template <int bitdepth>
LIBGAV1_ALWAYS_INLINE void BoxFilterPreProcess3(
    const uint16_t* const sum3[3], const uint32_t* const square_sum3[3],
    const int width, const uint32_t s, const bool calculate444,
    SgrBuffer* const sgr_buffer, uint16_t* const ma343, uint32_t* const b343,
    uint16_t* const ma444, uint32_t* const b444) {
  int x = 0;
  do {
    uint32_t a = 0;
    uint32_t b = 0;
    for (int dy = 0; dy < 3; ++dy) {
      a += square_sum3[dy][x];
      b += sum3[dy][x];
    }
    CalculateIntermediate<bitdepth, 9>(s, a, b, sgr_buffer->ma + x,
                                       sgr_buffer->b + x);
  } while (++x != width + 2);
  x = 0;
  do {
    ma343[x] = Sum343(sgr_buffer->ma + x);
    b343[x] = Sum343(sgr_buffer->b + x);
  } while (++x != width);
  if (calculate444) {
    x = 0;
    do {
      ma444[x] = Sum444(sgr_buffer->ma + x);
      b444[x] = Sum444(sgr_buffer->b + x);
    } while (++x != width);
  }
}

template <typename Pixel>
inline int CalculateFilteredOutput(const Pixel src, const uint32_t ma,
                                   const uint32_t b, const int shift) {
  const int32_t v = b - ma * src;
  return RightShiftWithRounding(v,
                                kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <typename Pixel>
inline void BoxFilterPass(const Pixel src0, const Pixel src1,
                          const uint16_t* const ma565[2],
                          const uint32_t* const b565[2], const ptrdiff_t x,
                          int p[2]) {
  p[0] = CalculateFilteredOutput<Pixel>(src0, ma565[0][x] + ma565[1][x],
                                        b565[0][x] + b565[1][x], 5);
  p[1] = CalculateFilteredOutput<Pixel>(src1, ma565[1][x], b565[1][x], 4);
}

template <typename Pixel>
inline int BoxFilterPass2(const Pixel src, const uint16_t* const ma343[3],
                          const uint16_t* const ma444,
                          const uint32_t* const b343[3],
                          const uint32_t* const b444, const ptrdiff_t x) {
  const uint32_t ma = ma343[0][x] + ma444[x] + ma343[2][x];
  const uint32_t b = b343[0][x] + b444[x] + b343[2][x];
  return CalculateFilteredOutput<Pixel>(src, ma, b, 5);
}

template <int bitdepth, typename Pixel>
inline Pixel SelfGuidedFinal(const int src, const int v) {
  // if radius_pass_0 == 0 and radius_pass_1 == 0, the range of v is:
  // bits(u) + bits(w0/w1/w2) + 2 = bitdepth + 13.
  // Then, range of s is bitdepth + 2. This is a rough estimation, taking the
  // maximum value of each element.
  const int s = src + RightShiftWithRounding(
                          v, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  return static_cast<Pixel>(Clip3(s, 0, (1 << bitdepth) - 1));
}

template <int bitdepth, typename Pixel>
inline Pixel SelfGuidedDoubleMultiplier(const int src, const int filter0,
                                        const int filter1, const int16_t w0,
                                        const int16_t w2) {
  const int v = w0 * filter0 + w2 * filter1;
  return SelfGuidedFinal<bitdepth, Pixel>(src, v);
}

template <int bitdepth, typename Pixel>
inline Pixel SelfGuidedSingleMultiplier(const int src, const int filter,
                                        const int16_t w0) {
  const int v = w0 * filter;
  return SelfGuidedFinal<bitdepth, Pixel>(src, v);
}

template <typename T>
void Circulate3PointersBy1(T* p[3]) {
  T* const p0 = p[0];
  p[0] = p[1];
  p[1] = p[2];
  p[2] = p0;
}

template <typename T>
void Circulate4PointersBy2(T* p[4]) {
  std::swap(p[0], p[2]);
  std::swap(p[1], p[3]);
}

template <typename T>
void Circulate5PointersBy2(T* p[5]) {
  T* const p0 = p[0];
  T* const p1 = p[1];
  p[0] = p[2];
  p[1] = p[3];
  p[2] = p[4];
  p[3] = p0;
  p[4] = p1;
}

template <int bitdepth, typename Pixel>
inline void BoxFilterProcess(const RestorationUnitInfo& restoration_info,
                             const Pixel* src, const ptrdiff_t src_stride,
                             const int width, const int height,
                             SgrBuffer* const sgr_buffer, Pixel* dst,
                             const ptrdiff_t dst_stride) {
  const auto temp_stride = Align<ptrdiff_t>(width, 8);
  const ptrdiff_t sum_stride = temp_stride + 8;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint16_t* const scales = kSgrScaleParameter[sgr_proj_index];  // < 2^12.
  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  uint16_t *sum3[4], *sum5[5], *ma343[4], *ma444[3], *ma565[2];
  uint32_t *square_sum3[4], *square_sum5[5], *b343[4], *b444[3], *b565[2];
  sum3[0] = sgr_buffer->sum3;
  square_sum3[0] = sgr_buffer->square_sum3;
  ma343[0] = sgr_buffer->ma343;
  b343[0] = sgr_buffer->b343;
  for (int i = 1; i <= 3; ++i) {
    sum3[i] = sum3[i - 1] + sum_stride;
    square_sum3[i] = square_sum3[i - 1] + sum_stride;
    ma343[i] = ma343[i - 1] + temp_stride;
    b343[i] = b343[i - 1] + temp_stride;
  }
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;
  for (int i = 1; i <= 4; ++i) {
    sum5[i] = sum5[i - 1] + sum_stride;
    square_sum5[i] = square_sum5[i - 1] + sum_stride;
  }
  ma444[0] = sgr_buffer->ma444;
  b444[0] = sgr_buffer->b444;
  for (int i = 1; i <= 2; ++i) {
    ma444[i] = ma444[i - 1] + temp_stride;
    b444[i] = b444[i - 1] + temp_stride;
  }
  ma565[0] = sgr_buffer->ma565;
  ma565[1] = ma565[0] + temp_stride;
  b565[0] = sgr_buffer->b565;
  b565[1] = b565[0] + temp_stride;
  assert(scales[0] != 0);
  assert(scales[1] != 0);
  BoxSum<Pixel>(src - 2 * src_stride - 3, src_stride, 4, width + 2, sum3[0],
                sum5[1], square_sum3[0], square_sum5[1], sum_stride);
  memcpy(sum5[0], sum5[1], sizeof(**sum5) * sum_stride);
  memcpy(square_sum5[0], square_sum5[1], sizeof(**square_sum5) * sum_stride);
  BoxFilterPreProcess5<bitdepth>(sum5, square_sum5, width, scales[0],
                                 sgr_buffer, ma565[0], b565[0]);
  BoxFilterPreProcess3<bitdepth>(sum3, square_sum3, width, scales[1], false,
                                 sgr_buffer, ma343[0], b343[0], nullptr,
                                 nullptr);
  BoxFilterPreProcess3<bitdepth>(sum3 + 1, square_sum3 + 1, width, scales[1],
                                 true, sgr_buffer, ma343[1], b343[1], ma444[0],
                                 b444[0]);
  for (int y = height >> 1; y != 0; --y) {
    Circulate4PointersBy2<uint16_t>(sum3);
    Circulate4PointersBy2<uint32_t>(square_sum3);
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxSum<Pixel>(src + 2 * src_stride - 3, src_stride, 1, width + 2, sum3[2],
                  sum5[3], square_sum3[2], square_sum5[3], sum_stride);
    BoxSum<Pixel>(src + 3 * src_stride - 3, src_stride, 1, width + 2, sum3[3],
                  sum5[4], square_sum3[3], square_sum5[4], sum_stride);
    BoxFilterPreProcess5<bitdepth>(sum5, square_sum5, width, scales[0],
                                   sgr_buffer, ma565[1], b565[1]);
    BoxFilterPreProcess3<bitdepth>(sum3, square_sum3, width, scales[1], true,
                                   sgr_buffer, ma343[2], b343[2], ma444[1],
                                   b444[1]);
    BoxFilterPreProcess3<bitdepth>(sum3 + 1, square_sum3 + 1, width, scales[1],
                                   true, sgr_buffer, ma343[3], b343[3],
                                   ma444[2], b444[2]);
    int x = 0;
    do {
      int p[2][2];
      BoxFilterPass<Pixel>(src[x], src[src_stride + x], ma565, b565, x, p[0]);
      p[1][0] =
          BoxFilterPass2<Pixel>(src[x], ma343, ma444[0], b343, b444[0], x);
      p[1][1] = BoxFilterPass2<Pixel>(src[src_stride + x], ma343 + 1, ma444[1],
                                      b343 + 1, b444[1], x);
      dst[x] = SelfGuidedDoubleMultiplier<bitdepth, Pixel>(src[x], p[0][0],
                                                           p[1][0], w0, w2);
      dst[dst_stride + x] = SelfGuidedDoubleMultiplier<bitdepth, Pixel>(
          src[src_stride + x], p[0][1], p[1][1], w0, w2);
    } while (++x != width);
    src += 2 * src_stride;
    dst += 2 * dst_stride;
    Circulate4PointersBy2<uint16_t>(ma343);
    Circulate4PointersBy2<uint32_t>(b343);
    std::swap(ma444[0], ma444[2]);
    std::swap(b444[0], b444[2]);
    std::swap(ma565[0], ma565[1]);
    std::swap(b565[0], b565[1]);
  }
  if ((height & 1) != 0) {
    Circulate4PointersBy2<uint16_t>(sum3);
    Circulate4PointersBy2<uint32_t>(square_sum3);
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxSum<Pixel>(src + 2 * src_stride - 3, src_stride, 1, width + 2, sum3[2],
                  sum5[3], square_sum3[2], square_sum5[3], sum_stride);
    memcpy(sum5[4], sum5[3], sizeof(**sum5) * sum_stride);
    memcpy(square_sum5[4], square_sum5[3], sizeof(**square_sum5) * sum_stride);
    BoxFilterPreProcess5<bitdepth>(sum5, square_sum5, width, scales[0],
                                   sgr_buffer, ma565[1], b565[1]);
    BoxFilterPreProcess3<bitdepth>(sum3, square_sum3, width, scales[1], false,
                                   sgr_buffer, ma343[2], b343[2], nullptr,
                                   nullptr);
    int x = 0;
    do {
      const int p0 = CalculateFilteredOutput<Pixel>(
          src[x], ma565[0][x] + ma565[1][x], b565[0][x] + b565[1][x], 5);
      const int p1 =
          BoxFilterPass2<Pixel>(src[x], ma343, ma444[0], b343, b444[0], x);
      dst[x] =
          SelfGuidedDoubleMultiplier<bitdepth, Pixel>(src[x], p0, p1, w0, w2);
    } while (++x != width);
  }
}

template <int bitdepth, typename Pixel>
inline void BoxFilterProcessPass1(const RestorationUnitInfo& restoration_info,
                                  const Pixel* src, const ptrdiff_t src_stride,
                                  const int width, const int height,
                                  SgrBuffer* const sgr_buffer, Pixel* dst,
                                  const ptrdiff_t dst_stride) {
  const auto temp_stride = Align<ptrdiff_t>(width, 8);
  const ptrdiff_t sum_stride = temp_stride + 8;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t s = kSgrScaleParameter[sgr_proj_index][0];  // s < 2^12.
  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  uint16_t *sum5[5], *ma565[2];
  uint32_t *square_sum5[5], *b565[2];
  sum5[0] = sgr_buffer->sum5;
  square_sum5[0] = sgr_buffer->square_sum5;
  for (int i = 1; i <= 4; ++i) {
    sum5[i] = sum5[i - 1] + sum_stride;
    square_sum5[i] = square_sum5[i - 1] + sum_stride;
  }
  ma565[0] = sgr_buffer->ma565;
  ma565[1] = ma565[0] + temp_stride;
  b565[0] = sgr_buffer->b565;
  b565[1] = b565[0] + temp_stride;
  assert(s != 0);
  BoxSum<Pixel, 5>(src - 2 * src_stride - 3, src_stride, 4, width + 2, sum5[1],
                   square_sum5[1], sum_stride);
  memcpy(sum5[0], sum5[1], sizeof(**sum5) * sum_stride);
  memcpy(square_sum5[0], square_sum5[1], sizeof(**square_sum5) * sum_stride);
  BoxFilterPreProcess5<bitdepth>(sum5, square_sum5, width, s, sgr_buffer,
                                 ma565[0], b565[0]);
  for (int y = height >> 1; y != 0; --y) {
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxSum<Pixel, 5>(src + 2 * src_stride - 3, src_stride, 1, width + 2,
                     sum5[3], square_sum5[3], sum_stride);
    BoxSum<Pixel, 5>(src + 3 * src_stride - 3, src_stride, 1, width + 2,
                     sum5[4], square_sum5[4], sum_stride);
    BoxFilterPreProcess5<bitdepth>(sum5, square_sum5, width, s, sgr_buffer,
                                   ma565[1], b565[1]);
    int x = 0;
    do {
      int p[2];
      BoxFilterPass<Pixel>(src[x], src[src_stride + x], ma565, b565, x, p);
      dst[x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(src[x], p[0], w0);
      dst[dst_stride + x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(
          src[src_stride + x], p[1], w0);
    } while (++x != width);
    src += 2 * src_stride;
    dst += 2 * dst_stride;
    std::swap(ma565[0], ma565[1]);
    std::swap(b565[0], b565[1]);
  }
  if ((height & 1) != 0) {
    Circulate5PointersBy2<uint16_t>(sum5);
    Circulate5PointersBy2<uint32_t>(square_sum5);
    BoxSum<Pixel, 5>(src + 2 * src_stride - 3, src_stride, 1, width + 2,
                     sum5[3], square_sum5[3], sum_stride);
    memcpy(sum5[4], sum5[3], sizeof(**sum5) * sum_stride);
    memcpy(square_sum5[4], square_sum5[3], sizeof(**square_sum5) * sum_stride);
    BoxFilterPreProcess5<bitdepth>(sum5, square_sum5, width, s, sgr_buffer,
                                   ma565[1], b565[1]);
    int x = 0;
    do {
      const int p = CalculateFilteredOutput<Pixel>(
          src[x], ma565[0][x] + ma565[1][x], b565[0][x] + b565[1][x], 5);
      dst[x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(src[x], p, w0);
    } while (++x != width);
  }
}

template <int bitdepth, typename Pixel>
inline void BoxFilterProcessPass2(const RestorationUnitInfo& restoration_info,
                                  const Pixel* src, const ptrdiff_t src_stride,
                                  const int width, const int height,
                                  SgrBuffer* const sgr_buffer, Pixel* dst,
                                  const ptrdiff_t dst_stride) {
  assert(restoration_info.sgr_proj_info.multiplier[0] == 0);
  const auto temp_stride = Align<ptrdiff_t>(width, 8);
  const ptrdiff_t sum_stride = temp_stride + 8;
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w0 = (1 << kSgrProjPrecisionBits) - w1;
  const int sgr_proj_index = restoration_info.sgr_proj_info.index;
  const uint32_t s = kSgrScaleParameter[sgr_proj_index][1];  // s < 2^12.
  uint16_t *sum3[3], *ma343[3], *ma444[2];
  uint32_t *square_sum3[3], *b343[3], *b444[2];
  sum3[0] = sgr_buffer->sum3;
  square_sum3[0] = sgr_buffer->square_sum3;
  ma343[0] = sgr_buffer->ma343;
  b343[0] = sgr_buffer->b343;
  for (int i = 1; i <= 2; ++i) {
    sum3[i] = sum3[i - 1] + sum_stride;
    square_sum3[i] = square_sum3[i - 1] + sum_stride;
    ma343[i] = ma343[i - 1] + temp_stride;
    b343[i] = b343[i - 1] + temp_stride;
  }
  ma444[0] = sgr_buffer->ma444;
  ma444[1] = ma444[0] + temp_stride;
  b444[0] = sgr_buffer->b444;
  b444[1] = b444[0] + temp_stride;
  assert(s != 0);
  BoxSum<Pixel, 3>(src - 2 * src_stride - 2, src_stride, 3, width + 2, sum3[0],
                   square_sum3[0], sum_stride);
  BoxFilterPreProcess3<bitdepth>(sum3, square_sum3, width, s, false, sgr_buffer,
                                 ma343[0], b343[0], nullptr, nullptr);
  Circulate3PointersBy1<uint16_t>(sum3);
  Circulate3PointersBy1<uint32_t>(square_sum3);
  BoxSum<Pixel, 3>(src + src_stride - 2, src_stride, 1, width + 2, sum3[2],
                   square_sum3[2], sum_stride);
  BoxFilterPreProcess3<bitdepth>(sum3, square_sum3, width, s, true, sgr_buffer,
                                 ma343[1], b343[1], ma444[0], b444[0]);
  int y = height;
  do {
    Circulate3PointersBy1<uint16_t>(sum3);
    Circulate3PointersBy1<uint32_t>(square_sum3);
    BoxSum<Pixel, 3>(src + 2 * src_stride - 2, src_stride, 1, width + 2,
                     sum3[2], square_sum3[2], sum_stride);
    BoxFilterPreProcess3<bitdepth>(sum3, square_sum3, width, s, true,
                                   sgr_buffer, ma343[2], b343[2], ma444[1],
                                   b444[1]);
    int x = 0;
    do {
      const int p =
          BoxFilterPass2<Pixel>(src[x], ma343, ma444[0], b343, b444[0], x);
      dst[x] = SelfGuidedSingleMultiplier<bitdepth, Pixel>(src[x], p, w0);
    } while (++x != width);
    src += src_stride;
    dst += dst_stride;
    Circulate3PointersBy1<uint16_t>(ma343);
    Circulate3PointersBy1<uint32_t>(b343);
    std::swap(ma444[0], ma444[1]);
    std::swap(b444[0], b444[1]);
  } while (--y != 0);
}

template <int bitdepth, typename Pixel>
void SelfGuidedFilter_C(const void* const source, void* const dest,
                        const RestorationUnitInfo& restoration_info,
                        ptrdiff_t source_stride, ptrdiff_t dest_stride,
                        int width, int height,
                        RestorationBuffer* const restoration_buffer) {
  const int index = restoration_info.sgr_proj_info.index;
  const int radius_pass_0 = kSgrProjParams[index][0];  // 2 or 0
  const int radius_pass_1 = kSgrProjParams[index][2];  // 1 or 0
  const auto* src = static_cast<const Pixel*>(source);
  auto* dst = static_cast<Pixel*>(dest);
  SgrBuffer* const sgr_buffer = &restoration_buffer->sgr_buffer;
  if (radius_pass_1 == 0) {
    // |radius_pass_0| and |radius_pass_1| cannot both be 0, so we have the
    // following assertion.
    assert(radius_pass_0 != 0);
    BoxFilterProcessPass1<bitdepth, Pixel>(restoration_info, src, source_stride,
                                           width, height, sgr_buffer, dst,
                                           dest_stride);
  } else if (radius_pass_0 == 0) {
    BoxFilterProcessPass2<bitdepth, Pixel>(restoration_info, src, source_stride,
                                           width, height, sgr_buffer, dst,
                                           dest_stride);
  } else {
    BoxFilterProcess<bitdepth, Pixel>(restoration_info, src, source_stride,
                                      width, height, sgr_buffer, dst,
                                      dest_stride);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(8);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] = WienerFilter_C<8, uint8_t>;
  dsp->loop_restorations[1] = SelfGuidedFilter_C<8, uint8_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp8bpp_WienerFilter
  dsp->loop_restorations[0] = WienerFilter_C<8, uint8_t>;
#endif
#ifndef LIBGAV1_Dsp8bpp_SelfGuidedFilter
  dsp->loop_restorations[1] = SelfGuidedFilter_C<8, uint8_t>;
#endif
#endif  // LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
}

#if LIBGAV1_MAX_BITDEPTH >= 10

void Init10bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(10);
  assert(dsp != nullptr);
#if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  dsp->loop_restorations[0] = WienerFilter_C<10, uint16_t>;
  dsp->loop_restorations[1] = SelfGuidedFilter_C<10, uint16_t>;
#else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  static_cast<void>(dsp);
#ifndef LIBGAV1_Dsp10bpp_WienerFilter
  dsp->loop_restorations[0] = WienerFilter_C<10, uint16_t>;
#endif
#ifndef LIBGAV1_Dsp10bpp_SelfGuidedFilter
  dsp->loop_restorations[1] = SelfGuidedFilter_C<10, uint16_t>;
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
}

}  // namespace dsp
}  // namespace libgav1
