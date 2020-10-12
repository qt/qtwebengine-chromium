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
#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON
#include <arm_neon.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/dsp/arm/common_neon.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

template <int bytes>
inline uint16x4_t VshrU128(const uint16x8_t a) {
  return vext_u16(vget_low_u16(a), vget_high_u16(a), bytes / 2);
}

// Wiener

// Must make a local copy of coefficients to help compiler know that they have
// no overlap with other buffers. Using 'const' keyword is not enough. Actually
// compiler doesn't make a copy, since there is enough registers in this case.
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, const int direction,
    int16_t filter[4]) {
  // In order to keep the horizontal pass intermediate values within 16 bits we
  // initialize |filter[3]| to 0 instead of 128.
  int filter_3;
  if (direction == WienerInfo::kHorizontal) {
    filter_3 = 0;
  } else {
    assert(direction == WienerInfo::kVertical);
    filter_3 = 128;
  }
  for (int i = 0; i < 3; ++i) {
    const int16_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter_3 -= coeff * 2;
  }
  filter[3] = filter_3;
}

inline int CountZeroCoefficients(const int16_t filter[2][kSubPixelTaps]) {
  int number_zero_coefficients = 0;
  if ((filter[WienerInfo::kHorizontal][0] | filter[WienerInfo::kVertical][0]) ==
      0) {
    number_zero_coefficients++;
    if ((filter[WienerInfo::kHorizontal][1] |
         filter[WienerInfo::kVertical][1]) == 0) {
      number_zero_coefficients++;
      if ((filter[WienerInfo::kHorizontal][2] |
           filter[WienerInfo::kVertical][2]) == 0) {
        number_zero_coefficients++;
      }
    }
  }
  return number_zero_coefficients;
}

inline int16x8_t HorizontalSum(const uint8x8_t a[3], const int16_t filter[2],
                               int16x8_t sum) {
  const int16x8_t a_0_2 = vreinterpretq_s16_u16(vaddl_u8(a[0], a[2]));
  sum = vmlaq_n_s16(sum, a_0_2, filter[0]);
  sum = vmlaq_n_s16(sum, vreinterpretq_s16_u16(vmovl_u8(a[1])), filter[1]);
  sum = vrshrq_n_s16(sum, kInterRoundBitsHorizontal);
  // Delaying |horizontal_rounding| until after down shifting allows the sum to
  // stay in 16 bits.
  // |horizontal_rounding| = 1 << (bitdepth + kWienerFilterBits - 1)
  //                         1 << (       8 +                 7 - 1)
  // Plus |kInterRoundBitsHorizontal| and it works out to 1 << 11.
  sum = vaddq_s16(sum, vdupq_n_s16(1 << 11));
  // Just like |horizontal_rounding|, adding |filter[3]| at this point allows
  // the sum to stay in 16 bits.
  // But wait! We *did* calculate |filter[3]| and used it in the sum! But it was
  // offset by 128. Fix that here:
  // |src[3]| * 128 >> 3 == |src[3]| << 4
  sum = vaddq_s16(sum, vreinterpretq_s16_u16(vshll_n_u8(a[1], 4)));
  // Saturate to
  // [0,
  // (1 << (bitdepth + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1)]
  // (1 << (       8 + 1 +                 7 -                         3)) - 1)
  sum = vminq_s16(sum, vdupq_n_s16((1 << 13) - 1));
  sum = vmaxq_s16(sum, vdupq_n_s16(0));
  return sum;
}

inline uint8x8_t WienerVertical(const int16x8_t a[3], const int16_t filter[2],
                                int32x4_t sum[2]) {
  // -(1 << (bitdepth + kInterRoundBitsVertical - 1))
  // -(1 << (       8 +                      11 - 1))
  constexpr int vertical_rounding = -(1 << 18);
  const int32x4_t rounding = vdupq_n_s32(vertical_rounding);
  const int16x8_t a_0_2 = vaddq_s16(a[0], a[2]);
  sum[0] = vaddq_s32(sum[0], rounding);
  sum[1] = vaddq_s32(sum[1], rounding);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_0_2), filter[0]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_0_2), filter[0]);
  sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a[1]), filter[1]);
  sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a[1]), filter[1]);
  const uint16x4_t sum_lo_16 = vqrshrun_n_s32(sum[0], 11);
  const uint16x4_t sum_hi_16 = vqrshrun_n_s32(sum[1], 11);
  return vqmovn_u16(vcombine_u16(sum_lo_16, sum_hi_16));
}

// For width 16 and up, store the horizontal results, and then do the vertical
// filter row by row. This is faster than doing it column by column when
// considering cache issues.
void WienerFilter_NEON(const void* const source, void* const dest,
                       const RestorationUnitInfo& restoration_info,
                       const ptrdiff_t source_stride,
                       const ptrdiff_t dest_stride, const int width,
                       const int height, RestorationBuffer* const buffer) {
  constexpr int kCenterTap = (kSubPixelTaps - 1) / 2;
  const int number_zero_coefficients =
      CountZeroCoefficients(restoration_info.wiener_info.filter);
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  // Casting once here saves a lot of vreinterpret() calls. The values are
  // saturated to 13 bits before storing.
  int16_t* wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);
  int16_t filter_horizontal[kSubPixelTaps / 2];
  int16_t filter_vertical[kSubPixelTaps / 2];
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal,
                             filter_horizontal);
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical,
                             filter_vertical);
  if (number_zero_coefficients == 0) {
    // 7-tap
    src -= (kCenterTap - 1) * source_stride + kCenterTap;
    int y = height + kSubPixelTaps - 4;
    do {
      wiener_buffer += width;
      int x = 0;
      do {
        // This is just as fast as an 8x8 transpose but avoids over-reading
        // extra rows. It always over-reads by at least 1 value. On small widths
        // (4xH) it over-reads by 9 values.
        const uint8x16_t r = vld1q_u8(src + x);
        uint8x8_t s[7];
        s[0] = vget_low_u8(r);
        s[1] = vext_u8(s[0], vget_high_u8(r), 1);
        s[2] = vext_u8(s[0], vget_high_u8(r), 2);
        s[3] = vext_u8(s[0], vget_high_u8(r), 3);
        s[4] = vext_u8(s[0], vget_high_u8(r), 4);
        s[5] = vext_u8(s[0], vget_high_u8(r), 5);
        s[6] = vext_u8(s[0], vget_high_u8(r), 6);
        const int16x8_t s_0_6 = vreinterpretq_s16_u16(vaddl_u8(s[0], s[6]));
        const int16x8_t s_1_5 = vreinterpretq_s16_u16(vaddl_u8(s[1], s[5]));
        int16x8_t sum = vmulq_n_s16(s_0_6, filter_horizontal[0]);
        sum = vmlaq_n_s16(sum, s_1_5, filter_horizontal[1]);
        const int16x8_t a = HorizontalSum(s + 2, filter_horizontal + 2, sum);
        vst1q_s16(wiener_buffer + x, a);
        x += 8;
      } while (x < width);
      src += source_stride;
    } while (--y != 0);
    // Because the top row of |source| is a duplicate of the second row, and the
    // bottom row of |source| is a duplicate of its above row, we can duplicate
    // the top and bottom row of |wiener_buffer| accordingly.
    memcpy(wiener_buffer + width, wiener_buffer,
           sizeof(*wiener_buffer) * width);
    wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);
    memcpy(wiener_buffer, wiener_buffer + width,
           sizeof(*wiener_buffer) * width);

    y = height;
    do {
      int x = 0;
      do {
        int16x8_t a[7];
        a[0] = vld1q_s16(wiener_buffer + x + 0 * width);
        a[1] = vld1q_s16(wiener_buffer + x + 1 * width);
        a[2] = vld1q_s16(wiener_buffer + x + 2 * width);
        a[3] = vld1q_s16(wiener_buffer + x + 3 * width);
        a[4] = vld1q_s16(wiener_buffer + x + 4 * width);
        a[5] = vld1q_s16(wiener_buffer + x + 5 * width);
        a[6] = vld1q_s16(wiener_buffer + x + 6 * width);
        const int16x8_t a_0_6 = vaddq_s16(a[0], a[6]);
        const int16x8_t a_1_5 = vaddq_s16(a[1], a[5]);
        int32x4_t sum[2];
        sum[0] = sum[1] = vdupq_n_s32(0);
        sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_0_6), filter_vertical[0]);
        sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_0_6), filter_vertical[0]);
        sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_1_5), filter_vertical[1]);
        sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_1_5), filter_vertical[1]);
        const uint8x8_t r = WienerVertical(a + 2, filter_vertical + 2, sum);
        vst1_u8(dst + x, r);
        x += 8;
      } while (x < width);
      wiener_buffer += width;
      dst += dest_stride;
    } while (--y != 0);
  } else if (number_zero_coefficients == 1) {
    // 5-tap
    src -= (kCenterTap - 1) * source_stride + kCenterTap - 1;
    int y = height + kSubPixelTaps - 4;
    do {
      int x = 0;
      do {
        const uint8x16_t r = vld1q_u8(src + x);
        uint8x8_t s[5];
        s[0] = vget_low_u8(r);
        s[1] = vext_u8(s[0], vget_high_u8(r), 1);
        s[2] = vext_u8(s[0], vget_high_u8(r), 2);
        s[3] = vext_u8(s[0], vget_high_u8(r), 3);
        s[4] = vext_u8(s[0], vget_high_u8(r), 4);
        const int16x8_t s_0_4 = vreinterpretq_s16_u16(vaddl_u8(s[0], s[4]));
        const int16x8_t sum = vmulq_n_s16(s_0_4, filter_horizontal[1]);
        const int16x8_t a = HorizontalSum(s + 1, filter_horizontal + 2, sum);
        vst1q_s16(wiener_buffer + x, a);
        x += 8;
      } while (x < width);
      src += source_stride;
      wiener_buffer += width;
    } while (--y != 0);

    wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);
    y = height;
    do {
      int x = 0;
      do {
        int16x8_t a[5];
        a[0] = vld1q_s16(wiener_buffer + x + 0 * width);
        a[1] = vld1q_s16(wiener_buffer + x + 1 * width);
        a[2] = vld1q_s16(wiener_buffer + x + 2 * width);
        a[3] = vld1q_s16(wiener_buffer + x + 3 * width);
        a[4] = vld1q_s16(wiener_buffer + x + 4 * width);
        const int16x8_t a_0_4 = vaddq_s16(a[0], a[4]);
        int32x4_t sum[2];
        sum[0] = sum[1] = vdupq_n_s32(0);
        sum[0] = vmlal_n_s16(sum[0], vget_low_s16(a_0_4), filter_vertical[1]);
        sum[1] = vmlal_n_s16(sum[1], vget_high_s16(a_0_4), filter_vertical[1]);
        const uint8x8_t r = WienerVertical(a + 1, filter_vertical + 2, sum);
        vst1_u8(dst + x, r);
        x += 8;
      } while (x < width);
      wiener_buffer += width;
      dst += dest_stride;
    } while (--y != 0);
  } else {
    // 3-tap
    src -= (kCenterTap - 2) * source_stride + kCenterTap - 2;
    int y = height + kSubPixelTaps - 6;
    do {
      int x = 0;
      do {
        const uint8x16_t r = vld1q_u8(src + x);
        uint8x8_t s[3];
        s[0] = vget_low_u8(r);
        s[1] = vext_u8(s[0], vget_high_u8(r), 1);
        s[2] = vext_u8(s[0], vget_high_u8(r), 2);
        const int16x8_t a =
            HorizontalSum(s, filter_horizontal + 2, vdupq_n_s16(0));
        vst1q_s16(wiener_buffer + x, a);
        x += 8;
      } while (x < width);
      src += source_stride;
      wiener_buffer += width;
    } while (--y != 0);

    wiener_buffer = reinterpret_cast<int16_t*>(buffer->wiener_buffer);
    y = height;
    do {
      int x = 0;
      do {
        int16x8_t a[3];
        a[0] = vld1q_s16(wiener_buffer + x + 0 * width);
        a[1] = vld1q_s16(wiener_buffer + x + 1 * width);
        a[2] = vld1q_s16(wiener_buffer + x + 2 * width);
        int32x4_t sum[2];
        sum[0] = sum[1] = vdupq_n_s32(0);
        const uint8x8_t r = WienerVertical(a, filter_vertical + 2, sum);
        vst1_u8(dst + x, r);
        x += 8;
      } while (x < width);
      wiener_buffer += width;
      dst += dest_stride;
    } while (--y != 0);
  }
}

//------------------------------------------------------------------------------
// SGR

template <int n>
inline uint16x4_t CalculateSgrMA2(const uint32x4_t sum_sq, const uint16x4_t sum,
                                  const uint32_t s) {
  // a = |sum_sq|
  // d = |sum|
  // p = (a * n < d * d) ? 0 : a * n - d * d;
  const uint32x4_t dxd = vmull_u16(sum, sum);
  const uint32x4_t axn = vmulq_n_u32(sum_sq, n);
  // Ensure |p| does not underflow by using saturating subtraction.
  const uint32x4_t p = vqsubq_u32(axn, dxd);

  // z = RightShiftWithRounding(p * s, kSgrProjScaleBits);
  const uint32x4_t pxs = vmulq_n_u32(p, s);
  // vrshrn_n_u32() (narrowing shift) can only shift by 16 and kSgrProjScaleBits
  // is 20.
  const uint32x4_t shifted = vrshrq_n_u32(pxs, kSgrProjScaleBits);
  return vmovn_u32(shifted);
}

inline uint16x4_t CalculateIntermediate4(const uint8x8_t sgr_ma2,
                                         const uint16x4_t sum,
                                         const uint32_t one_over_n) {
  // b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n
  // 1 << kSgrProjSgrBits = 256
  // |a2| = [1, 256]
  // |sgr_ma2| max value = 255
  // |sum| is a box sum with radius 1 or 2.
  // For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
  // For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
  // |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
  // When radius is 2 |n| is 25. |one_over_n| is 164.
  // When radius is 1 |n| is 9. |one_over_n| is 455.
  const uint16x8_t sgr_ma2q = vmovl_u8(sgr_ma2);
  const uint32x4_t m = vmull_u16(vget_high_u16(sgr_ma2q), sum);
  const uint32x4_t b2 = vmulq_n_u32(m, one_over_n);
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  return vrshrn_n_u32(b2, kSgrProjReciprocalBits);
}

inline uint16x8_t CalculateIntermediate8(const uint8x8_t sgr_ma2,
                                         const uint16x8_t sum,
                                         const uint32_t one_over_n) {
  // b2 = ((1 << kSgrProjSgrBits) - a2) * b * one_over_n
  // 1 << kSgrProjSgrBits = 256
  // |a2| = [1, 256]
  // |sgr_ma2| max value = 255
  // |sum| is a box sum with radius 1 or 2.
  // For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
  // For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
  // |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
  // When radius is 2 |n| is 25. |one_over_n| is 164.
  // When radius is 1 |n| is 9. |one_over_n| is 455.
  const uint16x8_t sgr_ma2q = vmovl_u8(sgr_ma2);
  const uint32x4_t m0 = vmull_u16(vget_low_u16(sgr_ma2q), vget_low_u16(sum));
  const uint32x4_t m1 = vmull_u16(vget_high_u16(sgr_ma2q), vget_high_u16(sum));
  const uint32x4_t m2 = vmulq_n_u32(m0, one_over_n);
  const uint32x4_t m3 = vmulq_n_u32(m1, one_over_n);
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  const uint16x4_t b2_lo = vrshrn_n_u32(m2, kSgrProjReciprocalBits);
  const uint16x4_t b2_hi = vrshrn_n_u32(m3, kSgrProjReciprocalBits);
  return vcombine_u16(b2_lo, b2_hi);
}

inline uint16x4_t Sum3(const uint16x4_t left, const uint16x4_t middle,
                       const uint16x4_t right) {
  const uint16x4_t sum = vadd_u16(left, middle);
  return vadd_u16(sum, right);
}

inline uint16x8_t Sum3_16(const uint16x8_t left, const uint16x8_t middle,
                          const uint16x8_t right) {
  const uint16x8_t sum = vaddq_u16(left, middle);
  return vaddq_u16(sum, right);
}

inline uint32x4_t Sum3_32(const uint32x4_t left, const uint32x4_t middle,
                          const uint32x4_t right) {
  const uint32x4_t sum = vaddq_u32(left, middle);
  return vaddq_u32(sum, right);
}

inline uint16x8_t Sum3W_16(const uint8x8_t left, const uint8x8_t middle,
                           const uint8x8_t right) {
  const uint16x8_t sum = vaddl_u8(left, middle);
  return vaddw_u8(sum, right);
}

inline uint16x8_t Sum3W_16(const uint8x8_t a[3]) {
  return Sum3W_16(a[0], a[1], a[2]);
}

inline uint32x4_t Sum3W_32(const uint16x4_t left, const uint16x4_t middle,
                           const uint16x4_t right) {
  const uint32x4_t sum = vaddl_u16(left, middle);
  return vaddw_u16(sum, right);
}

inline uint16x8x2_t Sum3W_16x2(const uint8x16_t a[3]) {
  const uint8x8_t low0 = vget_low_u8(a[0]);
  const uint8x8_t low1 = vget_low_u8(a[1]);
  const uint8x8_t low2 = vget_low_u8(a[2]);
  const uint8x8_t high0 = vget_high_u8(a[0]);
  const uint8x8_t high1 = vget_high_u8(a[1]);
  const uint8x8_t high2 = vget_high_u8(a[2]);
  uint16x8x2_t sum;
  sum.val[0] = Sum3W_16(low0, low1, low2);
  sum.val[1] = Sum3W_16(high0, high1, high2);
  return sum;
}

inline uint32x4x2_t Sum3W(const uint16x8_t a[3]) {
  const uint16x4_t low0 = vget_low_u16(a[0]);
  const uint16x4_t low1 = vget_low_u16(a[1]);
  const uint16x4_t low2 = vget_low_u16(a[2]);
  const uint16x4_t high0 = vget_high_u16(a[0]);
  const uint16x4_t high1 = vget_high_u16(a[1]);
  const uint16x4_t high2 = vget_high_u16(a[2]);
  uint32x4x2_t sum;
  sum.val[0] = Sum3W_32(low0, low1, low2);
  sum.val[1] = Sum3W_32(high0, high1, high2);
  return sum;
}

template <int index>
inline uint32x4_t Sum3WLo(const uint16x8x2_t a[3]) {
  const uint16x4_t low0 = vget_low_u16(a[0].val[index]);
  const uint16x4_t low1 = vget_low_u16(a[1].val[index]);
  const uint16x4_t low2 = vget_low_u16(a[2].val[index]);
  return Sum3W_32(low0, low1, low2);
}

inline uint32x4_t Sum3WHi(const uint16x8x2_t a[3]) {
  const uint16x4_t high0 = vget_high_u16(a[0].val[0]);
  const uint16x4_t high1 = vget_high_u16(a[1].val[0]);
  const uint16x4_t high2 = vget_high_u16(a[2].val[0]);
  return Sum3W_32(high0, high1, high2);
}

inline uint32x4x3_t Sum3W(const uint16x8x2_t a[3]) {
  uint32x4x3_t sum;
  sum.val[0] = Sum3WLo<0>(a);
  sum.val[1] = Sum3WHi(a);
  sum.val[2] = Sum3WLo<1>(a);
  return sum;
}

inline uint16x4_t Sum5(const uint16x4_t a[5]) {
  const uint16x4_t sum01 = vadd_u16(a[0], a[1]);
  const uint16x4_t sum23 = vadd_u16(a[2], a[3]);
  const uint16x4_t sum = vadd_u16(sum01, sum23);
  return vadd_u16(sum, a[4]);
}

inline uint16x8_t Sum5_16(const uint16x8_t a[5]) {
  const uint16x8_t sum01 = vaddq_u16(a[0], a[1]);
  const uint16x8_t sum23 = vaddq_u16(a[2], a[3]);
  const uint16x8_t sum = vaddq_u16(sum01, sum23);
  return vaddq_u16(sum, a[4]);
}

inline uint32x4_t Sum5_32(const uint32x4_t a[5]) {
  const uint32x4_t sum01 = vaddq_u32(a[0], a[1]);
  const uint32x4_t sum23 = vaddq_u32(a[2], a[3]);
  const uint32x4_t sum = vaddq_u32(sum01, sum23);
  return vaddq_u32(sum, a[4]);
}

inline uint16x8_t Sum5W_16(const uint8x8_t a[5]) {
  const uint16x8_t sum01 = vaddl_u8(a[0], a[1]);
  const uint16x8_t sum23 = vaddl_u8(a[2], a[3]);
  const uint16x8_t sum = vaddq_u16(sum01, sum23);
  return vaddw_u8(sum, a[4]);
}

inline uint32x4_t Sum5W_32(const uint16x4_t a[5]) {
  const uint32x4_t sum01 = vaddl_u16(a[0], a[1]);
  const uint32x4_t sum23 = vaddl_u16(a[2], a[3]);
  const uint32x4_t sum0123 = vaddq_u32(sum01, sum23);
  return vaddw_u16(sum0123, a[4]);
}

inline uint16x8x2_t Sum5W_16D(const uint8x16_t a[5]) {
  uint16x8x2_t sum;
  uint8x8_t low[5], high[5];
  low[0] = vget_low_u8(a[0]);
  low[1] = vget_low_u8(a[1]);
  low[2] = vget_low_u8(a[2]);
  low[3] = vget_low_u8(a[3]);
  low[4] = vget_low_u8(a[4]);
  high[0] = vget_high_u8(a[0]);
  high[1] = vget_high_u8(a[1]);
  high[2] = vget_high_u8(a[2]);
  high[3] = vget_high_u8(a[3]);
  high[4] = vget_high_u8(a[4]);
  sum.val[0] = Sum5W_16(low);
  sum.val[1] = Sum5W_16(high);
  return sum;
}

inline uint32x4x2_t Sum5W_32x2(const uint16x8_t a[5]) {
  uint32x4x2_t sum;
  uint16x4_t low[5], high[5];
  low[0] = vget_low_u16(a[0]);
  low[1] = vget_low_u16(a[1]);
  low[2] = vget_low_u16(a[2]);
  low[3] = vget_low_u16(a[3]);
  low[4] = vget_low_u16(a[4]);
  high[0] = vget_high_u16(a[0]);
  high[1] = vget_high_u16(a[1]);
  high[2] = vget_high_u16(a[2]);
  high[3] = vget_high_u16(a[3]);
  high[4] = vget_high_u16(a[4]);
  sum.val[0] = Sum5W_32(low);
  sum.val[1] = Sum5W_32(high);
  return sum;
}

template <int index>
inline uint32x4_t Sum5WLo(const uint16x8x2_t a[5]) {
  uint16x4_t low[5];
  low[0] = vget_low_u16(a[0].val[index]);
  low[1] = vget_low_u16(a[1].val[index]);
  low[2] = vget_low_u16(a[2].val[index]);
  low[3] = vget_low_u16(a[3].val[index]);
  low[4] = vget_low_u16(a[4].val[index]);
  return Sum5W_32(low);
}

inline uint32x4_t Sum5WHi(const uint16x8x2_t a[5]) {
  uint16x4_t high[5];
  high[0] = vget_high_u16(a[0].val[0]);
  high[1] = vget_high_u16(a[1].val[0]);
  high[2] = vget_high_u16(a[2].val[0]);
  high[3] = vget_high_u16(a[3].val[0]);
  high[4] = vget_high_u16(a[4].val[0]);
  return Sum5W_32(high);
}

inline uint32x4x3_t Sum5W_32x3(const uint16x8x2_t a[5]) {
  uint32x4x3_t sum;
  sum.val[0] = Sum5WLo<0>(a);
  sum.val[1] = Sum5WHi(a);
  sum.val[2] = Sum5WLo<1>(a);
  return sum;
}

inline uint16x4_t Sum3Horizontal(const uint16x8_t a) {
  const uint16x4_t left = vget_low_u16(a);
  const uint16x4_t middle = VshrU128<2>(a);
  const uint16x4_t right = VshrU128<4>(a);
  return Sum3(left, middle, right);
}

inline uint16x8_t Sum3Horizontal_16(const uint16x8x2_t a) {
  const uint16x8_t left = a.val[0];
  const uint16x8_t middle = vextq_u16(a.val[0], a.val[1], 1);
  const uint16x8_t right = vextq_u16(a.val[0], a.val[1], 2);
  return Sum3_16(left, middle, right);
}

inline uint32x4_t Sum3Horizontal_32(const uint32x4x2_t a) {
  const uint32x4_t left = a.val[0];
  const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 1);
  const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 2);
  return Sum3_32(left, middle, right);
}

inline uint32x4x2_t Sum3Horizontal_32x2(const uint32x4x3_t a) {
  uint32x4x2_t sum;
  {
    const uint32x4_t left = a.val[0];
    const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 1);
    const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 2);
    sum.val[0] = Sum3_32(left, middle, right);
  }
  {
    const uint32x4_t left = a.val[1];
    const uint32x4_t middle = vextq_u32(a.val[1], a.val[2], 1);
    const uint32x4_t right = vextq_u32(a.val[1], a.val[2], 2);
    sum.val[1] = Sum3_32(left, middle, right);
  }
  return sum;
}

inline uint16x4_t Sum3HorizontalOffset1(const uint16x8_t a) {
  const uint16x4_t left = VshrU128<2>(a);
  const uint16x4_t middle = VshrU128<4>(a);
  const uint16x4_t right = VshrU128<6>(a);
  return Sum3(left, middle, right);
}

inline uint16x8_t Sum3HorizontalOffset1_16(const uint16x8x2_t a) {
  const uint16x8_t left = vextq_u16(a.val[0], a.val[1], 1);
  const uint16x8_t middle = vextq_u16(a.val[0], a.val[1], 2);
  const uint16x8_t right = vextq_u16(a.val[0], a.val[1], 3);
  return Sum3_16(left, middle, right);
}

inline uint32x4_t Sum3HorizontalOffset1_32(const uint32x4x2_t a) {
  const uint32x4_t left = vextq_u32(a.val[0], a.val[1], 1);
  const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 2);
  const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 3);
  return Sum3_32(left, middle, right);
}

inline uint32x4x2_t Sum3HorizontalOffset1_32x2(const uint32x4x3_t a) {
  uint32x4x2_t sum;
  {
    const uint32x4_t left = vextq_u32(a.val[0], a.val[1], 1);
    const uint32x4_t middle = vextq_u32(a.val[0], a.val[1], 2);
    const uint32x4_t right = vextq_u32(a.val[0], a.val[1], 3);
    sum.val[0] = Sum3_32(left, middle, right);
  }
  {
    const uint32x4_t left = vextq_u32(a.val[1], a.val[2], 1);
    const uint32x4_t middle = vextq_u32(a.val[1], a.val[2], 2);
    const uint32x4_t right = vextq_u32(a.val[1], a.val[2], 3);
    sum.val[1] = Sum3_32(left, middle, right);
  }
  return sum;
}

inline uint16x4_t Sum5Horizontal(const uint16x8_t a) {
  uint16x4_t s[5];
  s[0] = vget_low_u16(a);
  s[1] = VshrU128<2>(a);
  s[2] = VshrU128<4>(a);
  s[3] = VshrU128<6>(a);
  s[4] = vget_high_u16(a);
  return Sum5(s);
}

inline uint16x8_t Sum5Horizontal_16(const uint16x8x2_t a) {
  uint16x8_t s[5];
  s[0] = a.val[0];
  s[1] = vextq_u16(a.val[0], a.val[1], 1);
  s[2] = vextq_u16(a.val[0], a.val[1], 2);
  s[3] = vextq_u16(a.val[0], a.val[1], 3);
  s[4] = vextq_u16(a.val[0], a.val[1], 4);
  return Sum5_16(s);
}

inline uint32x4_t Sum5Horizontal_32(const uint32x4x2_t a) {
  uint32x4_t s[5];
  s[0] = a.val[0];
  s[1] = vextq_u32(a.val[0], a.val[1], 1);
  s[2] = vextq_u32(a.val[0], a.val[1], 2);
  s[3] = vextq_u32(a.val[0], a.val[1], 3);
  s[4] = a.val[1];
  return Sum5_32(s);
}

inline uint32x4x2_t Sum5Horizontal_32x2(const uint32x4x3_t a) {
  uint32x4x2_t sum;
  uint32x4_t s[5];
  s[0] = a.val[0];
  s[1] = vextq_u32(a.val[0], a.val[1], 1);
  s[2] = vextq_u32(a.val[0], a.val[1], 2);
  s[3] = vextq_u32(a.val[0], a.val[1], 3);
  s[4] = a.val[1];
  sum.val[0] = Sum5_32(s);
  s[0] = a.val[1];
  s[1] = vextq_u32(a.val[1], a.val[2], 1);
  s[2] = vextq_u32(a.val[1], a.val[2], 2);
  s[3] = vextq_u32(a.val[1], a.val[2], 3);
  s[4] = a.val[2];
  sum.val[1] = Sum5_32(s);
  return sum;
}

template <int size, int offset>
inline void BoxFilterPreProcess4(const uint8x8_t* const row,
                                 const uint16x8_t* const row_sq,
                                 const uint32_t s, uint16_t* const dst) {
  static_assert(offset == 0 || offset == 1, "");
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  uint16x4_t sum;
  uint32x4_t sum_sq;
  if (size == 3) {
    if (offset == 0) {
      sum = Sum3Horizontal(Sum3W_16(row));
      sum_sq = Sum3Horizontal_32(Sum3W(row_sq));
    } else {
      sum = Sum3HorizontalOffset1(Sum3W_16(row));
      sum_sq = Sum3HorizontalOffset1_32(Sum3W(row_sq));
    }
  }
  if (size == 5) {
    sum = Sum5Horizontal(Sum5W_16(row));
    sum_sq = Sum5Horizontal_32(Sum5W_32x2(row_sq));
  }
  const uint16x4_t z0 = CalculateSgrMA2<n>(sum_sq, sum, s);
  const uint16x4_t z = vmin_u16(z0, vdup_n_u16(255));
  // Using vget_lane_s16() can save a sign extension instruction.
  // Add 4 0s for memory initialization purpose only.
  const uint8_t lookup[8] = {
      0,
      0,
      0,
      0,
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 0)],
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 1)],
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 2)],
      kSgrMa2Lookup[vget_lane_s16(vreinterpret_s16_u16(z), 3)]};
  const uint8x8_t sgr_ma2 = vld1_u8(lookup);
  const uint16x4_t b2 = CalculateIntermediate4(sgr_ma2, sum, one_over_n);
  const uint16x8_t sgr_ma2_b2 = vcombine_u16(vreinterpret_u16_u8(sgr_ma2), b2);
  vst1q_u16(dst, sgr_ma2_b2);
}

template <int size, int offset>
inline void BoxFilterPreProcess8(const uint8x16_t* const row,
                                 const uint16x8x2_t* const row_sq,
                                 const uint32_t s, uint8x8_t* const sgr_ma2,
                                 uint16x8_t* const b2, uint16_t* const dst) {
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  uint16x8_t sum;
  uint32x4x2_t sum_sq;
  if (size == 3) {
    if (offset == 0) {
      sum = Sum3Horizontal_16(Sum3W_16x2(row));
      sum_sq = Sum3Horizontal_32x2(Sum3W(row_sq));
    } else /* if (offset == 1) */ {
      sum = Sum3HorizontalOffset1_16(Sum3W_16x2(row));
      sum_sq = Sum3HorizontalOffset1_32x2(Sum3W(row_sq));
    }
  }
  if (size == 5) {
    sum = Sum5Horizontal_16(Sum5W_16D(row));
    sum_sq = Sum5Horizontal_32x2(Sum5W_32x3(row_sq));
  }
  const uint16x4_t z0 = CalculateSgrMA2<n>(sum_sq.val[0], vget_low_u16(sum), s);
  const uint16x4_t z1 =
      CalculateSgrMA2<n>(sum_sq.val[1], vget_high_u16(sum), s);
  const uint16x8_t z01 = vcombine_u16(z0, z1);
  // Using vqmovn_u16() needs an extra sign extension instruction.
  const uint16x8_t z = vminq_u16(z01, vdupq_n_u16(255));
  // Using vgetq_lane_s16() can save the sign extension instruction.
  const uint8_t lookup[8] = {
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 0)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 1)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 2)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 3)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 4)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 5)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 6)],
      kSgrMa2Lookup[vgetq_lane_s16(vreinterpretq_s16_u16(z), 7)]};
  *sgr_ma2 = vld1_u8(lookup);
  *b2 = CalculateIntermediate8(*sgr_ma2, sum, one_over_n);
  const uint16x8_t sgr_ma2_b2 =
      vcombine_u16(vreinterpret_u16_u8(*sgr_ma2), vget_high_u16(*b2));
  vst1q_u16(dst, sgr_ma2_b2);
}

inline void Prepare3_8(const uint8x8_t a[2], uint8x8_t* const left,
                       uint8x8_t* const middle, uint8x8_t* const right) {
  *left = vext_u8(a[0], a[1], 4);
  *middle = vext_u8(a[0], a[1], 5);
  *right = vext_u8(a[0], a[1], 6);
}

inline void Prepare3_16(const uint16x8_t a[2], uint16x8_t* const left,
                        uint16x8_t* const middle, uint16x8_t* const right) {
  *left = vextq_u16(a[0], a[1], 4);
  *middle = vextq_u16(a[0], a[1], 5);
  *right = vextq_u16(a[0], a[1], 6);
}

inline uint16x8_t Sum343(const uint8x8_t a[2]) {
  uint8x8_t left, middle, right;
  Prepare3_8(a, &left, &middle, &right);
  const uint16x8_t sum = Sum3W_16(left, middle, right);
  const uint16x8_t sum3 = Sum3_16(sum, sum, sum);
  return vaddw_u8(sum3, middle);
}

inline void Sum343_444(const uint8x8_t a[2], uint16x8_t* const sum343,
                       uint16x8_t* const sum444) {
  uint8x8_t left, middle, right;
  Prepare3_8(a, &left, &middle, &right);
  const uint16x8_t sum = Sum3W_16(left, middle, right);
  const uint16x8_t sum3 = Sum3_16(sum, sum, sum);
  *sum343 = vaddw_u8(sum3, middle);
  *sum444 = vshlq_n_u16(sum, 2);
}

inline uint32x4x2_t Sum343W(const uint16x8_t a[2]) {
  uint16x8_t left, middle, right;
  uint32x4x2_t d;
  Prepare3_16(a, &left, &middle, &right);
  d.val[0] =
      Sum3W_32(vget_low_u16(left), vget_low_u16(middle), vget_low_u16(right));
  d.val[1] = Sum3W_32(vget_high_u16(left), vget_high_u16(middle),
                      vget_high_u16(right));
  d.val[0] = Sum3_32(d.val[0], d.val[0], d.val[0]);
  d.val[1] = Sum3_32(d.val[1], d.val[1], d.val[1]);
  d.val[0] = vaddw_u16(d.val[0], vget_low_u16(middle));
  d.val[1] = vaddw_u16(d.val[1], vget_high_u16(middle));
  return d;
}

inline void Sum343_444W(const uint16x8_t a[2], uint32x4x2_t* const sum343,
                        uint32x4x2_t* const sum444) {
  uint16x8_t left, middle, right;
  Prepare3_16(a, &left, &middle, &right);
  sum444->val[0] =
      Sum3W_32(vget_low_u16(left), vget_low_u16(middle), vget_low_u16(right));
  sum444->val[1] = Sum3W_32(vget_high_u16(left), vget_high_u16(middle),
                            vget_high_u16(right));
  sum343->val[0] = Sum3_32(sum444->val[0], sum444->val[0], sum444->val[0]);
  sum343->val[1] = Sum3_32(sum444->val[1], sum444->val[1], sum444->val[1]);
  sum343->val[0] = vaddw_u16(sum343->val[0], vget_low_u16(middle));
  sum343->val[1] = vaddw_u16(sum343->val[1], vget_high_u16(middle));
  sum444->val[0] = vshlq_n_u32(sum444->val[0], 2);
  sum444->val[1] = vshlq_n_u32(sum444->val[1], 2);
}

inline uint16x8_t Sum565(const uint8x8_t a[2]) {
  uint8x8_t left, middle, right;
  Prepare3_8(a, &left, &middle, &right);
  const uint16x8_t sum = Sum3W_16(left, middle, right);
  const uint16x8_t sum4 = vshlq_n_u16(sum, 2);
  const uint16x8_t sum5 = vaddq_u16(sum4, sum);
  return vaddw_u8(sum5, middle);
}

inline uint32x4_t Sum565W(const uint16x8_t a) {
  const uint16x4_t left = vget_low_u16(a);
  const uint16x4_t middle = VshrU128<2>(a);
  const uint16x4_t right = VshrU128<4>(a);
  const uint32x4_t sum = Sum3W_32(left, middle, right);
  const uint32x4_t sum4 = vshlq_n_u32(sum, 2);
  const uint32x4_t sum5 = vaddq_u32(sum4, sum);
  return vaddw_u16(sum5, middle);
}

// RightShiftWithRounding(
//   (a * src_ptr[x] + b), kSgrProjSgrBits + shift - kSgrProjRestoreBits);
template <int shift>
inline uint16x4_t FilterOutput(const uint16x4_t src, const uint16x4_t a,
                               const uint32x4_t b) {
  // a: 256 * 32 = 8192 (14 bits)
  // b: 65088 * 32 = 2082816 (21 bits)
  const uint32x4_t axsrc = vmull_u16(a, src);
  // v: 8192 * 255 + 2082816 = 4171876 (22 bits)
  const uint32x4_t v = vaddq_u32(axsrc, b);

  // kSgrProjSgrBits = 8
  // kSgrProjRestoreBits = 4
  // shift = 4 or 5
  // v >> 8 or 9
  // 22 bits >> 8 = 14 bits
  return vrshrn_n_u32(v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <int shift>
inline int16x8_t CalculateFilteredOutput(const uint8x8_t src,
                                         const uint16x8_t a,
                                         const uint32x4x2_t b) {
  const uint16x8_t src_u16 = vmovl_u8(src);
  const uint16x4_t dst_lo =
      FilterOutput<shift>(vget_low_u16(src_u16), vget_low_u16(a), b.val[0]);
  const uint16x4_t dst_hi =
      FilterOutput<shift>(vget_high_u16(src_u16), vget_high_u16(a), b.val[1]);
  return vreinterpretq_s16_u16(vcombine_u16(dst_lo, dst_hi));  // 14 bits
}

inline int16x8_t BoxFilterPass1(const uint8x8_t src_u8, const uint8x8_t a2[2],
                                const uint16x8_t b2[2], uint16x8_t sum565_a[2],
                                uint32x4x2_t sum565_b[2]) {
  uint32x4x2_t b_v;
  sum565_a[1] = Sum565(a2);
  sum565_a[1] = vsubq_u16(vdupq_n_u16((5 + 6 + 5) * 256), sum565_a[1]);
  sum565_b[1].val[0] = Sum565W(vextq_u16(b2[0], b2[1], 4));
  sum565_b[1].val[1] = Sum565W(b2[1]);

  uint16x8_t a_v = vaddq_u16(sum565_a[0], sum565_a[1]);
  b_v.val[0] = vaddq_u32(sum565_b[0].val[0], sum565_b[1].val[0]);
  b_v.val[1] = vaddq_u32(sum565_b[0].val[1], sum565_b[1].val[1]);
  return CalculateFilteredOutput<5>(src_u8, a_v, b_v);  // 14 bits
}

inline int16x8_t BoxFilterPass2(const uint8x8_t src_u8, const uint8x8_t a2[2],
                                const uint16x8_t b2[2], uint16x8_t sum343_a[4],
                                uint16x8_t sum444_a[3],
                                uint32x4x2_t sum343_b[4],
                                uint32x4x2_t sum444_b[3]) {
  uint32x4x2_t b_v;
  Sum343_444(a2, &sum343_a[2], &sum444_a[1]);
  sum343_a[2] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[2]);
  sum444_a[1] = vsubq_u16(vdupq_n_u16((4 + 4 + 4) * 256), sum444_a[1]);
  uint16x8_t a_v = Sum3_16(sum343_a[0], sum444_a[0], sum343_a[2]);
  Sum343_444W(b2, &sum343_b[2], &sum444_b[1]);
  b_v.val[0] =
      Sum3_32(sum343_b[0].val[0], sum444_b[0].val[0], sum343_b[2].val[0]);
  b_v.val[1] =
      Sum3_32(sum343_b[0].val[1], sum444_b[0].val[1], sum343_b[2].val[1]);
  return CalculateFilteredOutput<5>(src_u8, a_v, b_v);  // 14 bits
}

inline void SelfGuidedDoubleMultiplier(
    const uint8x8_t src, const int16x8_t box_filter_process_output[2],
    const int16x4_t w0, const int16x4_t w1, const int16x4_t w2,
    uint8_t* const dst) {
  // |wN| values are signed. |src| values can be treated as int16_t.
  const int16x8_t u =
      vreinterpretq_s16_u16(vshll_n_u8(src, kSgrProjRestoreBits));
  int32x4_t v_lo = vmull_s16(vget_low_s16(u), w1);
  v_lo = vmlal_s16(v_lo, vget_low_s16(box_filter_process_output[0]), w0);
  v_lo = vmlal_s16(v_lo, vget_low_s16(box_filter_process_output[1]), w2);
  int32x4_t v_hi = vmull_s16(vget_high_s16(u), w1);
  v_hi = vmlal_s16(v_hi, vget_high_s16(box_filter_process_output[0]), w0);
  v_hi = vmlal_s16(v_hi, vget_high_s16(box_filter_process_output[1]), w2);
  // |s| is saturated to uint8_t.
  const int16x4_t s_lo =
      vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const int16x4_t s_hi =
      vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  vst1_u8(dst, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
}

inline void SelfGuidedSingleMultiplier(
    const uint8x8_t src, const int16x8_t box_filter_process_output,
    const int16_t w0, const int16_t w1, uint8_t* dst) {
  // weight: -96 to 96 (Sgrproj_Xqd_Min/Max)
  const int16x8_t u =
      vreinterpretq_s16_u16(vshll_n_u8(src, kSgrProjRestoreBits));
  // u * w1 + u * wN == u * (w1 + wN)
  int32x4_t v_lo = vmull_n_s16(vget_low_s16(u), w1);
  v_lo = vmlal_n_s16(v_lo, vget_low_s16(box_filter_process_output), w0);
  int32x4_t v_hi = vmull_n_s16(vget_high_s16(u), w1);
  v_hi = vmlal_n_s16(v_hi, vget_high_s16(box_filter_process_output), w0);
  const int16x4_t s_lo =
      vrshrn_n_s32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const int16x4_t s_hi =
      vrshrn_n_s32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  vst1_u8(dst, vqmovun_s16(vcombine_s16(s_lo, s_hi)));
}

inline void BoxFilterProcess(const uint8_t* const src,
                             const ptrdiff_t src_stride,
                             const RestorationUnitInfo& restoration_info,
                             const int width, const int height,
                             const uint16_t s[2], uint16_t* const temp,
                             uint8_t* const dst, const ptrdiff_t dst_stride) {
  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |a2| region. The values stored are one vertical
  // column of interleaved |a2| and |b2| values and consume 8 * |height| values.
  // This is |height| and not |height| * 2 because PreProcess only generates
  // output for every other row. When processing the next column we write the
  // new scratch values right after reading the previously saved ones.

  // The PreProcess phase calculates a 5x5 box sum for every other row
  //
  // PreProcess and Process have been combined into the same step. We need 12
  // input values to generate 8 output values for PreProcess:
  // 0 1 2 3 4 5 6 7 8 9 10 11
  // 2 = 0 + 1 + 2 +  3 +  4
  // 3 = 1 + 2 + 3 +  4 +  5
  // 4 = 2 + 3 + 4 +  5 +  6
  // 5 = 3 + 4 + 5 +  6 +  7
  // 6 = 4 + 5 + 6 +  7 +  8
  // 7 = 5 + 6 + 7 +  8 +  9
  // 8 = 6 + 7 + 8 +  9 + 10
  // 9 = 7 + 8 + 9 + 10 + 11
  //
  // and then we need 10 input values to generate 8 output values for Process:
  // 0 1 2 3 4 5 6 7 8 9
  // 1 = 0 + 1 + 2
  // 2 = 1 + 2 + 3
  // 3 = 2 + 3 + 4
  // 4 = 3 + 4 + 5
  // 5 = 4 + 5 + 6
  // 6 = 5 + 6 + 7
  // 7 = 6 + 7 + 8
  // 8 = 7 + 8 + 9
  //
  // To avoid re-calculating PreProcess values over and over again we will do a
  // single column of 8 output values and store the second half of them
  // interleaved in |temp|. The first half is not stored, since it is used
  // immediately and becomes useless for the next column. Next we will start the
  // second column. When 2 rows have been calculated we can calculate Process
  // and output the results.

  // Calculate and store a single column. Scope so we can re-use the variable
  // names for the next step.
  uint16_t* ab_ptr = temp;

  const uint8_t* const src_pre_process = src - 2 * src_stride - 3;
  // Calculate intermediate results, including two-pixel border, for example, if
  // unit size is 64x64, we calculate 68x68 pixels.
  {
    const uint8_t* column = src_pre_process;
    uint8x8_t row[5];
    uint16x8_t row_sq[5];
    row[0] = row[1] = vld1_u8(column);
    column += src_stride;
    row[2] = vld1_u8(column);

    row_sq[0] = row_sq[1] = vmull_u8(row[1], row[1]);
    row_sq[2] = vmull_u8(row[2], row[2]);

    int y = (height + 2) >> 1;
    do {
      column += src_stride;
      row[3] = vld1_u8(column);
      column += src_stride;
      row[4] = vld1_u8(column);

      row_sq[3] = vmull_u8(row[3], row[3]);
      row_sq[4] = vmull_u8(row[4], row[4]);

      BoxFilterPreProcess4<5, 0>(row + 0, row_sq + 0, s[0], ab_ptr + 0);
      BoxFilterPreProcess4<3, 1>(row + 1, row_sq + 1, s[1], ab_ptr + 8);
      BoxFilterPreProcess4<3, 1>(row + 2, row_sq + 2, s[1], ab_ptr + 16);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];
      ab_ptr += 24;
    } while (--y != 0);

    if ((height & 1) != 0) {
      column += src_stride;
      row[3] = row[4] = vld1_u8(column);
      row_sq[3] = row_sq[4] = vmull_u8(row[3], row[3]);
      BoxFilterPreProcess4<5, 0>(row + 0, row_sq + 0, s[0], ab_ptr + 0);
      BoxFilterPreProcess4<3, 1>(row + 1, row_sq + 1, s[1], ab_ptr + 8);
    }
  }

  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  const int16x4_t w0_v = vdup_n_s16(w0);
  const int16x4_t w1_v = vdup_n_s16(w1);
  const int16x4_t w2_v = vdup_n_s16(w2);
  int x = 0;
  do {
    // |src_pre_process| is X but we already processed the first column of 4
    // values so we want to start at Y and increment from there.
    // X s s s Y s s
    // s s s s s s s
    // s s i i i i i
    // s s i o o o o
    // s s i o o o o

    // Seed the loop with one line of output. Then, inside the loop, for each
    // iteration we can output one even row and one odd row and carry the new
    // line to the next iteration. In the diagram below 'i' values are
    // intermediary values from the first step and '-' values are empty.
    // iiii
    // ---- > even row
    // iiii - odd row
    // ---- > even row
    // iiii
    uint8x8_t a2[2][2];
    uint16x8_t b2[2][2], sum565_a[2], sum343_a[4], sum444_a[3];
    uint32x4x2_t sum565_b[2], sum343_b[4], sum444_b[3];
    ab_ptr = temp;
    b2[0][0] = vld1q_u16(ab_ptr);
    a2[0][0] = vget_low_u8(vreinterpretq_u8_u16(b2[0][0]));
    b2[1][0] = vld1q_u16(ab_ptr + 8);
    a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

    const uint8_t* column = src_pre_process + x + 4;
    uint8x16_t row[5];
    uint16x8x2_t row_sq[5];
    row[0] = row[1] = vld1q_u8(column);
    column += src_stride;
    row[2] = vld1q_u8(column);
    column += src_stride;
    row[3] = vld1q_u8(column);
    column += src_stride;
    row[4] = vld1q_u8(column);

    row_sq[0].val[0] = row_sq[1].val[0] =
        vmull_u8(vget_low_u8(row[1]), vget_low_u8(row[1]));
    row_sq[0].val[1] = row_sq[1].val[1] =
        vmull_u8(vget_high_u8(row[1]), vget_high_u8(row[1]));
    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));
    row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
    row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
    row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
    row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

    BoxFilterPreProcess8<5, 0>(row, row_sq, s[0], &a2[0][1], &b2[0][1], ab_ptr);
    BoxFilterPreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1][1], &b2[1][1],
                               ab_ptr + 8);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    sum565_a[0] = Sum565(a2[0]);
    sum565_a[0] = vsubq_u16(vdupq_n_u16((5 + 6 + 5) * 256), sum565_a[0]);
    sum565_b[0].val[0] = Sum565W(vextq_u16(b2[0][0], b2[0][1], 4));
    sum565_b[0].val[1] = Sum565W(b2[0][1]);

    sum343_a[0] = Sum343(a2[1]);
    sum343_a[0] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[0]);
    sum343_b[0] = Sum343W(b2[1]);

    b2[1][0] = vld1q_u16(ab_ptr + 16);
    a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

    BoxFilterPreProcess8<3, 1>(row + 2, row_sq + 2, s[1], &a2[1][1], &b2[1][1],
                               ab_ptr + 16);

    Sum343_444(a2[1], &sum343_a[1], &sum444_a[0]);
    sum343_a[1] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[1]);
    sum444_a[0] = vsubq_u16(vdupq_n_u16((4 + 4 + 4) * 256), sum444_a[0]);
    Sum343_444W(b2[1], &sum343_b[1], &sum444_b[0]);

    const uint8_t* src_ptr = src + x;
    uint8_t* dst_ptr = dst + x;

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    for (int y = height >> 1; y != 0; --y) {
      ab_ptr += 24;
      b2[0][0] = vld1q_u16(ab_ptr);
      a2[0][0] = vget_low_u8(vreinterpretq_u8_u16(b2[0][0]));
      b2[1][0] = vld1q_u16(ab_ptr + 8);
      a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      column += src_stride;
      row[3] = vld1q_u8(column);
      column += src_stride;
      row[4] = vld1q_u8(column);

      row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
      row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
      row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
      row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

      BoxFilterPreProcess8<5, 0>(row, row_sq, s[0], &a2[0][1], &b2[0][1],
                                 ab_ptr);
      BoxFilterPreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1][1],
                                 &b2[1][1], ab_ptr + 8);

      int16x8_t p[2];
      const uint8x8_t src0 = vld1_u8(src_ptr);
      p[0] = BoxFilterPass1(src0, a2[0], b2[0], sum565_a, sum565_b);
      p[1] = BoxFilterPass2(src0, a2[1], b2[1], sum343_a, sum444_a, sum343_b,
                            sum444_b);
      SelfGuidedDoubleMultiplier(src0, p, w0_v, w1_v, w2_v, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      const uint8x8_t src1 = vld1_u8(src_ptr);
      p[0] = CalculateFilteredOutput<4>(src1, sum565_a[1], sum565_b[1]);
      b2[1][0] = vld1q_u16(ab_ptr + 16);
      a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));
      BoxFilterPreProcess8<3, 1>(row + 2, row_sq + 2, s[1], &a2[1][1],
                                 &b2[1][1], ab_ptr + 16);
      p[1] = BoxFilterPass2(src1, a2[1], b2[1], sum343_a + 1, sum444_a + 1,
                            sum343_b + 1, sum444_b + 1);
      SelfGuidedDoubleMultiplier(src1, p, w0_v, w1_v, w2_v, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      sum565_a[0] = sum565_a[1];
      sum565_b[0] = sum565_b[1];
      sum343_a[0] = sum343_a[2];
      sum343_a[1] = sum343_a[3];
      sum444_a[0] = sum444_a[2];
      sum343_b[0] = sum343_b[2];
      sum343_b[1] = sum343_b[3];
      sum444_b[0] = sum444_b[2];
    }
    if ((height & 1) != 0) {
      ab_ptr += 24;
      b2[0][0] = vld1q_u16(ab_ptr);
      a2[0][0] = vget_low_u8(vreinterpretq_u8_u16(b2[0][0]));
      b2[1][0] = vld1q_u16(ab_ptr + 8);
      a2[1][0] = vget_low_u8(vreinterpretq_u8_u16(b2[1][0]));

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      column += src_stride;
      row[3] = row[4] = vld1q_u8(column);

      row_sq[3].val[0] = row_sq[4].val[0] =
          vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
      row_sq[3].val[1] = row_sq[4].val[1] =
          vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));

      BoxFilterPreProcess8<5, 0>(row, row_sq, s[0], &a2[0][1], &b2[0][1],
                                 ab_ptr);
      BoxFilterPreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1][1],
                                 &b2[1][1], ab_ptr + 8);

      int16x8_t p[2];
      const uint8x8_t src0 = vld1_u8(src_ptr);
      p[0] = BoxFilterPass1(src0, a2[0], b2[0], sum565_a, sum565_b);
      p[1] = BoxFilterPass2(src0, a2[1], b2[1], sum343_a, sum444_a, sum343_b,
                            sum444_b);
      SelfGuidedDoubleMultiplier(src0, p, w0_v, w1_v, w2_v, dst_ptr);
    }
    x += 8;
  } while (x < width);
}

inline void BoxFilterProcessPass1(const uint8_t* const src,
                                  const ptrdiff_t src_stride,
                                  const RestorationUnitInfo& restoration_info,
                                  const int width, const int height,
                                  const uint32_t s, uint16_t* const temp,
                                  uint8_t* const dst,
                                  const ptrdiff_t dst_stride) {
  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |a2| region. The values stored are one vertical
  // column of interleaved |a2| and |b2| values and consume 8 * |height| values.
  // This is |height| and not |height| * 2 because PreProcess only generates
  // output for every other row. When processing the next column we write the
  // new scratch values right after reading the previously saved ones.

  // The PreProcess phase calculates a 5x5 box sum for every other row
  //
  // PreProcess and Process have been combined into the same step. We need 12
  // input values to generate 8 output values for PreProcess:
  // 0 1 2 3 4 5 6 7 8 9 10 11
  // 2 = 0 + 1 + 2 +  3 +  4
  // 3 = 1 + 2 + 3 +  4 +  5
  // 4 = 2 + 3 + 4 +  5 +  6
  // 5 = 3 + 4 + 5 +  6 +  7
  // 6 = 4 + 5 + 6 +  7 +  8
  // 7 = 5 + 6 + 7 +  8 +  9
  // 8 = 6 + 7 + 8 +  9 + 10
  // 9 = 7 + 8 + 9 + 10 + 11
  //
  // and then we need 10 input values to generate 8 output values for Process:
  // 0 1 2 3 4 5 6 7 8 9
  // 1 = 0 + 1 + 2
  // 2 = 1 + 2 + 3
  // 3 = 2 + 3 + 4
  // 4 = 3 + 4 + 5
  // 5 = 4 + 5 + 6
  // 6 = 5 + 6 + 7
  // 7 = 6 + 7 + 8
  // 8 = 7 + 8 + 9
  //
  // To avoid re-calculating PreProcess values over and over again we will do a
  // single column of 8 output values and store the second half of them
  // interleaved in |temp|. The first half is not stored, since it is used
  // immediately and becomes useless for the next column. Next we will start the
  // second column. When 2 rows have been calculated we can calculate Process
  // and output the results.

  // Calculate and store a single column. Scope so we can re-use the variable
  // names for the next step.
  uint16_t* ab_ptr = temp;

  const uint8_t* const src_pre_process = src - 2 * src_stride - 3;
  // Calculate intermediate results, including two-pixel border, for example, if
  // unit size is 64x64, we calculate 68x68 pixels.
  {
    const uint8_t* column = src_pre_process;
    uint8x8_t row[5];
    uint16x8_t row_sq[5];
    row[0] = row[1] = vld1_u8(column);
    column += src_stride;
    row[2] = vld1_u8(column);

    row_sq[0] = row_sq[1] = vmull_u8(row[1], row[1]);
    row_sq[2] = vmull_u8(row[2], row[2]);

    int y = (height + 2) >> 1;
    do {
      column += src_stride;
      row[3] = vld1_u8(column);
      column += src_stride;
      row[4] = vld1_u8(column);

      row_sq[3] = vmull_u8(row[3], row[3]);
      row_sq[4] = vmull_u8(row[4], row[4]);

      BoxFilterPreProcess4<5, 0>(row, row_sq, s, ab_ptr);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];
      ab_ptr += 8;
    } while (--y != 0);

    if ((height & 1) != 0) {
      column += src_stride;
      row[3] = row[4] = vld1_u8(column);
      row_sq[3] = row_sq[4] = vmull_u8(row[3], row[3]);
      BoxFilterPreProcess4<5, 0>(row, row_sq, s, ab_ptr);
    }
  }

  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = (1 << kSgrProjPrecisionBits) - w0;
  int x = 0;
  do {
    // |src_pre_process| is X but we already processed the first column of 4
    // values so we want to start at Y and increment from there.
    // X s s s Y s s
    // s s s s s s s
    // s s i i i i i
    // s s i o o o o
    // s s i o o o o

    // Seed the loop with one line of output. Then, inside the loop, for each
    // iteration we can output one even row and one odd row and carry the new
    // line to the next iteration. In the diagram below 'i' values are
    // intermediary values from the first step and '-' values are empty.
    // iiii
    // ---- > even row
    // iiii - odd row
    // ---- > even row
    // iiii
    uint8x8_t a2[2];
    uint16x8_t b2[2], sum565_a[2];
    uint32x4x2_t sum565_b[2];
    ab_ptr = temp;
    b2[0] = vld1q_u16(ab_ptr);
    a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

    const uint8_t* column = src_pre_process + x + 4;
    uint8x16_t row[5];
    uint16x8x2_t row_sq[5];
    row[0] = row[1] = vld1q_u8(column);
    column += src_stride;
    row[2] = vld1q_u8(column);
    column += src_stride;
    row[3] = vld1q_u8(column);
    column += src_stride;
    row[4] = vld1q_u8(column);

    row_sq[0].val[0] = row_sq[1].val[0] =
        vmull_u8(vget_low_u8(row[1]), vget_low_u8(row[1]));
    row_sq[0].val[1] = row_sq[1].val[1] =
        vmull_u8(vget_high_u8(row[1]), vget_high_u8(row[1]));
    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));
    row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
    row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
    row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
    row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

    BoxFilterPreProcess8<5, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    sum565_a[0] = Sum565(a2);
    sum565_a[0] = vsubq_u16(vdupq_n_u16((5 + 6 + 5) * 256), sum565_a[0]);
    sum565_b[0].val[0] = Sum565W(vextq_u16(b2[0], b2[1], 4));
    sum565_b[0].val[1] = Sum565W(b2[1]);

    const uint8_t* src_ptr = src + x;
    uint8_t* dst_ptr = dst + x;

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    for (int y = height >> 1; y != 0; --y) {
      ab_ptr += 8;
      b2[0] = vld1q_u16(ab_ptr);
      a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      column += src_stride;
      row[3] = vld1q_u8(column);
      column += src_stride;
      row[4] = vld1q_u8(column);

      row_sq[3].val[0] = vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
      row_sq[3].val[1] = vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));
      row_sq[4].val[0] = vmull_u8(vget_low_u8(row[4]), vget_low_u8(row[4]));
      row_sq[4].val[1] = vmull_u8(vget_high_u8(row[4]), vget_high_u8(row[4]));

      BoxFilterPreProcess8<5, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

      const uint8x8_t src0 = vld1_u8(src_ptr);
      const int16x8_t p0 = BoxFilterPass1(src0, a2, b2, sum565_a, sum565_b);
      SelfGuidedSingleMultiplier(src0, p0, w0, w1, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      const uint8x8_t src1 = vld1_u8(src_ptr);
      const int16x8_t p1 =
          CalculateFilteredOutput<4>(src1, sum565_a[1], sum565_b[1]);
      SelfGuidedSingleMultiplier(src1, p1, w0, w1, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      sum565_a[0] = sum565_a[1];
      sum565_b[0] = sum565_b[1];
    }
    if ((height & 1) != 0) {
      ab_ptr += 8;
      b2[0] = vld1q_u16(ab_ptr);
      a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0] = row_sq[2];
      row_sq[1] = row_sq[3];
      row_sq[2] = row_sq[4];

      column += src_stride;
      row[3] = row[4] = vld1q_u8(column);

      row_sq[3].val[0] = row_sq[4].val[0] =
          vmull_u8(vget_low_u8(row[3]), vget_low_u8(row[3]));
      row_sq[3].val[1] = row_sq[4].val[1] =
          vmull_u8(vget_high_u8(row[3]), vget_high_u8(row[3]));

      BoxFilterPreProcess8<5, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

      const uint8x8_t src0 = vld1_u8(src_ptr);
      const int16x8_t p0 = BoxFilterPass1(src0, a2, b2, sum565_a, sum565_b);
      SelfGuidedSingleMultiplier(src0, p0, w0, w1, dst_ptr);
    }
    x += 8;
  } while (x < width);
}

inline void BoxFilterProcessPass2(const uint8_t* src,
                                  const ptrdiff_t src_stride,
                                  const RestorationUnitInfo& restoration_info,
                                  const int width, const int height,
                                  const uint32_t s, uint16_t* const temp,
                                  uint8_t* const dst,
                                  const ptrdiff_t dst_stride) {
  uint16_t* ab_ptr = temp;

  // Calculate intermediate results, including one-pixel border, for example, if
  // unit size is 64x64, we calculate 66x66 pixels.
  // Because of the vectors this calculates start in blocks of 4 so we actually
  // get 68 values.
  const uint8_t* const src_top_left_corner = src - 2 - 2 * src_stride;
  {
    const uint8_t* column = src_top_left_corner;
    uint8x8_t row[3];
    uint16x8_t row_sq[3];
    row[0] = vld1_u8(column);
    column += src_stride;
    row[1] = vld1_u8(column);
    row_sq[0] = vmull_u8(row[0], row[0]);
    row_sq[1] = vmull_u8(row[1], row[1]);

    int y = height + 2;
    do {
      column += src_stride;
      row[2] = vld1_u8(column);
      row_sq[2] = vmull_u8(row[2], row[2]);

      BoxFilterPreProcess4<3, 0>(row, row_sq, s, ab_ptr);

      row[0] = row[1];
      row[1] = row[2];

      row_sq[0] = row_sq[1];
      row_sq[1] = row_sq[2];
      ab_ptr += 8;
    } while (--y != 0);
  }

  assert(restoration_info.sgr_proj_info.multiplier[0] == 0);
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w0 = (1 << kSgrProjPrecisionBits) - w1;
  int x = 0;
  do {
    ab_ptr = temp;

    uint8x8_t a2[2];
    uint16x8_t b2[2], sum343_a[3], sum444_a[2];
    uint32x4x2_t sum343_b[3], sum444_b[2];
    b2[0] = vld1q_u16(ab_ptr);
    a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

    const uint8_t* column = src_top_left_corner + x + 4;
    uint8x16_t row[3];
    uint16x8x2_t row_sq[3];
    row[0] = vld1q_u8(column);
    column += src_stride;
    row[1] = vld1q_u8(column);
    column += src_stride;
    row[2] = vld1q_u8(column);

    row_sq[0].val[0] = vmull_u8(vget_low_u8(row[0]), vget_low_u8(row[0]));
    row_sq[0].val[1] = vmull_u8(vget_high_u8(row[0]), vget_high_u8(row[0]));
    row_sq[1].val[0] = vmull_u8(vget_low_u8(row[1]), vget_low_u8(row[1]));
    row_sq[1].val[1] = vmull_u8(vget_high_u8(row[1]), vget_high_u8(row[1]));
    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));

    BoxFilterPreProcess8<3, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

    sum343_a[0] = Sum343(a2);
    sum343_a[0] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[0]);
    sum343_b[0] = Sum343W(b2);

    ab_ptr += 8;
    b2[0] = vld1q_u16(ab_ptr);
    a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

    row[0] = row[1];
    row[1] = row[2];

    row_sq[0] = row_sq[1];
    row_sq[1] = row_sq[2];
    column += src_stride;
    row[2] = vld1q_u8(column);

    row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
    row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));

    BoxFilterPreProcess8<3, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

    Sum343_444(a2, &sum343_a[1], &sum444_a[0]);
    sum343_a[1] = vsubq_u16(vdupq_n_u16((3 + 4 + 3) * 256), sum343_a[1]);
    sum444_a[0] = vsubq_u16(vdupq_n_u16((4 + 4 + 4) * 256), sum444_a[0]);
    Sum343_444W(b2, &sum343_b[1], &sum444_b[0]);

    const uint8_t* src_ptr = src + x;
    uint8_t* dst_ptr = dst + x;
    int y = height;
    do {
      ab_ptr += 8;
      b2[0] = vld1q_u16(ab_ptr);
      a2[0] = vget_low_u8(vreinterpretq_u8_u16(b2[0]));

      row[0] = row[1];
      row[1] = row[2];

      row_sq[0] = row_sq[1];
      row_sq[1] = row_sq[2];
      column += src_stride;
      row[2] = vld1q_u8(column);

      row_sq[2].val[0] = vmull_u8(vget_low_u8(row[2]), vget_low_u8(row[2]));
      row_sq[2].val[1] = vmull_u8(vget_high_u8(row[2]), vget_high_u8(row[2]));

      BoxFilterPreProcess8<3, 0>(row, row_sq, s, &a2[1], &b2[1], ab_ptr);

      uint8x8_t src_u8 = vld1_u8(src_ptr);
      int16x8_t p = BoxFilterPass2(src_u8, a2, b2, sum343_a, sum444_a, sum343_b,
                                   sum444_b);
      SelfGuidedSingleMultiplier(src_u8, p, w0, w1, dst_ptr);
      sum343_a[0] = sum343_a[1];
      sum343_a[1] = sum343_a[2];
      sum444_a[0] = sum444_a[1];
      sum343_b[0] = sum343_b[1];
      sum343_b[1] = sum343_b[2];
      sum444_b[0] = sum444_b[1];
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--y != 0);
    x += 8;
  } while (x < width);
}

// If |width| is non-multiple of 8, up to 7 more pixels are written to |dest| in
// the end of each row. It is safe to overwrite the output as it will not be
// part of the visible frame.
void SelfGuidedFilter_NEON(const void* const source, void* const dest,
                           const RestorationUnitInfo& restoration_info,
                           const ptrdiff_t source_stride,
                           const ptrdiff_t dest_stride, const int width,
                           const int height, RestorationBuffer* const buffer) {
  const int index = restoration_info.sgr_proj_info.index;
  const int radius_pass_0 = kSgrProjParams[index][0];  // 2 or 0
  const int radius_pass_1 = kSgrProjParams[index][2];  // 1 or 0
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  if (radius_pass_1 == 0) {
    // |radius_pass_0| and |radius_pass_1| cannot both be 0, so we have the
    // following assertion.
    assert(radius_pass_0 != 0);
    BoxFilterProcessPass1(src, source_stride, restoration_info, width, height,
                          kSgrScaleParameter[index][0], buffer->sgf_buffer, dst,
                          dest_stride);
  } else if (radius_pass_0 == 0) {
    BoxFilterProcessPass2(src, source_stride, restoration_info, width, height,
                          kSgrScaleParameter[index][1], buffer->sgf_buffer, dst,
                          dest_stride);
  } else {
    BoxFilterProcess(src, source_stride, restoration_info, width, height,
                     kSgrScaleParameter[index], buffer->sgf_buffer, dst,
                     dest_stride);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
  dsp->loop_restorations[0] = WienerFilter_NEON;
  dsp->loop_restorations[1] = SelfGuidedFilter_NEON;
}

}  // namespace
}  // namespace low_bitdepth

void LoopRestorationInit_NEON() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else   // !LIBGAV1_ENABLE_NEON
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_NEON() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_NEON
