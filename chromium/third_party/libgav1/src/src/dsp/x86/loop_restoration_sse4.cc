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

#if LIBGAV1_ENABLE_SSE4_1
#include <smmintrin.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "src/dsp/common.h"
#include "src/dsp/constants.h"
#include "src/dsp/dsp.h"
#include "src/dsp/x86/common_sse4.h"
#include "src/utils/common.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

inline void WienerHorizontalTap7Kernel(const __m128i s[2],
                                       const __m128i filter[4],
                                       int16_t* const wiener_buffer) {
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsHorizontal - 1));
  const auto s01 = _mm_alignr_epi8(s[1], s[0], 1);
  const auto s23 = _mm_alignr_epi8(s[1], s[0], 5);
  const auto s45 = _mm_alignr_epi8(s[1], s[0], 9);
  const auto s67 = _mm_alignr_epi8(s[1], s[0], 13);
  const __m128i madd01 = _mm_maddubs_epi16(s01, filter[0]);
  const __m128i madd23 = _mm_maddubs_epi16(s23, filter[1]);
  const __m128i madd45 = _mm_maddubs_epi16(s45, filter[2]);
  const __m128i madd67 = _mm_maddubs_epi16(s67, filter[3]);
  const __m128i madd0123 = _mm_add_epi16(madd01, madd23);
  const __m128i madd4567 = _mm_add_epi16(madd45, madd67);
  // The sum range here is [-128 * 255, 90 * 255].
  const __m128i madd = _mm_add_epi16(madd0123, madd4567);
  const __m128i sum = _mm_add_epi16(madd, round);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  const __m128i s_3x128 =
      _mm_slli_epi16(_mm_srli_epi16(s23, 8), 7 - kInterRoundBitsHorizontal);
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap5Kernel(const __m128i s[2],
                                       const __m128i filter[3],
                                       int16_t* const wiener_buffer) {
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsHorizontal - 1));
  const auto s01 = _mm_alignr_epi8(s[1], s[0], 1);
  const auto s23 = _mm_alignr_epi8(s[1], s[0], 5);
  const auto s45 = _mm_alignr_epi8(s[1], s[0], 9);
  const __m128i madd01 = _mm_maddubs_epi16(s01, filter[0]);
  const __m128i madd23 = _mm_maddubs_epi16(s23, filter[1]);
  const __m128i madd45 = _mm_maddubs_epi16(s45, filter[2]);
  const __m128i madd0123 = _mm_add_epi16(madd01, madd23);
  // The sum range here is [-128 * 255, 90 * 255].
  const __m128i madd = _mm_add_epi16(madd0123, madd45);
  const __m128i sum = _mm_add_epi16(madd, round);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  const __m128i s_3x128 =
      _mm_srli_epi16(_mm_slli_epi16(s23, 8), kInterRoundBitsHorizontal + 1);
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap3Kernel(const __m128i s[2],
                                       const __m128i filter[2],
                                       int16_t* const wiener_buffer) {
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const int offset =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i offsets = _mm_set1_epi16(-offset);
  const __m128i limits = _mm_set1_epi16(limit - offset);
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsHorizontal - 1));
  const auto s01 = _mm_alignr_epi8(s[1], s[0], 1);
  const auto s23 = _mm_alignr_epi8(s[1], s[0], 5);
  const __m128i madd01 = _mm_maddubs_epi16(s01, filter[0]);
  const __m128i madd23 = _mm_maddubs_epi16(s23, filter[1]);
  // The sum range here is [-128 * 255, 90 * 255].
  const __m128i madd = _mm_add_epi16(madd01, madd23);
  const __m128i sum = _mm_add_epi16(madd, round);
  const __m128i rounded_sum0 = _mm_srai_epi16(sum, kInterRoundBitsHorizontal);
  // Calculate scaled down offset correction, and add to sum here to prevent
  // signed 16 bit outranging.
  const __m128i s_3x128 =
      _mm_slli_epi16(_mm_srli_epi16(s01, 8), 7 - kInterRoundBitsHorizontal);
  const __m128i rounded_sum1 = _mm_add_epi16(rounded_sum0, s_3x128);
  const __m128i d0 = _mm_max_epi16(rounded_sum1, offsets);
  const __m128i d1 = _mm_min_epi16(d0, limits);
  StoreAligned16(wiener_buffer, d1);
}

inline void WienerHorizontalTap7(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  __m128i filter[4];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0200));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0604));
  filter[2] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0204));
  filter[3] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x8000));
  int y = height;
  do {
    const __m128i s0 = LoadUnaligned16(src);
    __m128i ss[4];
    ss[0] = _mm_unpacklo_epi8(s0, s0);
    ss[1] = _mm_unpackhi_epi8(s0, s0);
    ptrdiff_t x = 0;
    do {
      const __m128i s1 = LoadUnaligned16(src + x + 16);
      ss[2] = _mm_unpacklo_epi8(s1, s1);
      ss[3] = _mm_unpackhi_epi8(s1, s1);
      WienerHorizontalTap7Kernel(ss + 0, filter, *wiener_buffer + x + 0);
      WienerHorizontalTap7Kernel(ss + 1, filter, *wiener_buffer + x + 8);
      ss[0] = ss[2];
      ss[1] = ss[3];
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  } while (--y != 0);
}

inline void WienerHorizontalTap5(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  __m128i filter[3];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0402));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0406));
  filter[2] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x8002));
  int y = height;
  do {
    const __m128i s0 = LoadUnaligned16(src);
    __m128i ss[4];
    ss[0] = _mm_unpacklo_epi8(s0, s0);
    ss[1] = _mm_unpackhi_epi8(s0, s0);
    ptrdiff_t x = 0;
    do {
      const __m128i s1 = LoadUnaligned16(src + x + 16);
      ss[2] = _mm_unpacklo_epi8(s1, s1);
      ss[3] = _mm_unpackhi_epi8(s1, s1);
      WienerHorizontalTap5Kernel(ss + 0, filter, *wiener_buffer + x + 0);
      WienerHorizontalTap5Kernel(ss + 1, filter, *wiener_buffer + x + 8);
      ss[0] = ss[2];
      ss[1] = ss[3];
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  } while (--y != 0);
}

inline void WienerHorizontalTap3(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 const __m128i coefficients,
                                 int16_t** const wiener_buffer) {
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x0604));
  filter[1] = _mm_shuffle_epi8(coefficients, _mm_set1_epi16(0x8004));
  int y = height;
  do {
    const __m128i s0 = LoadUnaligned16(src);
    __m128i ss[4];
    ss[0] = _mm_unpacklo_epi8(s0, s0);
    ss[1] = _mm_unpackhi_epi8(s0, s0);
    ptrdiff_t x = 0;
    do {
      const __m128i s1 = LoadUnaligned16(src + x + 16);
      ss[2] = _mm_unpacklo_epi8(s1, s1);
      ss[3] = _mm_unpackhi_epi8(s1, s1);
      WienerHorizontalTap3Kernel(ss + 0, filter, *wiener_buffer + x + 0);
      WienerHorizontalTap3Kernel(ss + 1, filter, *wiener_buffer + x + 8);
      ss[0] = ss[2];
      ss[1] = ss[3];
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  } while (--y != 0);
}

inline void WienerHorizontalTap1(const uint8_t* src, const ptrdiff_t src_stride,
                                 const ptrdiff_t width, const int height,
                                 int16_t** const wiener_buffer) {
  int y = height;
  do {
    ptrdiff_t x = 0;
    do {
      const __m128i s = LoadUnaligned16(src + x);
      const __m128i s0 = _mm_unpacklo_epi8(s, _mm_setzero_si128());
      const __m128i s1 = _mm_unpackhi_epi8(s, _mm_setzero_si128());
      const __m128i d0 = _mm_slli_epi16(s0, 4);
      const __m128i d1 = _mm_slli_epi16(s1, 4);
      StoreAligned16(*wiener_buffer + x + 0, d0);
      StoreAligned16(*wiener_buffer + x + 8, d1);
      x += 16;
    } while (x < width);
    src += src_stride;
    *wiener_buffer += width;
  } while (--y != 0);
}

inline __m128i WienerVertical7(const __m128i a[2], const __m128i filter[2]) {
  const __m128i round = _mm_set1_epi32(1 << (kInterRoundBitsVertical - 1));
  const __m128i madd0 = _mm_madd_epi16(a[0], filter[0]);
  const __m128i madd1 = _mm_madd_epi16(a[1], filter[1]);
  const __m128i sum0 = _mm_add_epi32(round, madd0);
  const __m128i sum1 = _mm_add_epi32(sum0, madd1);
  return _mm_srai_epi32(sum1, kInterRoundBitsVertical);
}

inline __m128i WienerVertical5(const __m128i a[2], const __m128i filter[2]) {
  const __m128i madd0 = _mm_madd_epi16(a[0], filter[0]);
  const __m128i madd1 = _mm_madd_epi16(a[1], filter[1]);
  const __m128i sum = _mm_add_epi32(madd0, madd1);
  return _mm_srai_epi32(sum, kInterRoundBitsVertical);
}

inline __m128i WienerVertical3(const __m128i a, const __m128i filter) {
  const __m128i round = _mm_set1_epi32(1 << (kInterRoundBitsVertical - 1));
  const __m128i madd = _mm_madd_epi16(a, filter);
  const __m128i sum = _mm_add_epi32(round, madd);
  return _mm_srai_epi32(sum, kInterRoundBitsVertical);
}

inline __m128i WienerVerticalFilter7(const __m128i a[7],
                                     const __m128i filter[2]) {
  __m128i b[2];
  const __m128i a06 = _mm_add_epi16(a[0], a[6]);
  const __m128i a15 = _mm_add_epi16(a[1], a[5]);
  const __m128i a24 = _mm_add_epi16(a[2], a[4]);
  b[0] = _mm_unpacklo_epi16(a06, a15);
  b[1] = _mm_unpacklo_epi16(a24, a[3]);
  const __m128i sum0 = WienerVertical7(b, filter);
  b[0] = _mm_unpackhi_epi16(a06, a15);
  b[1] = _mm_unpackhi_epi16(a24, a[3]);
  const __m128i sum1 = WienerVertical7(b, filter);
  return _mm_packs_epi32(sum0, sum1);
}

inline __m128i WienerVerticalFilter5(const __m128i a[5],
                                     const __m128i filter[2]) {
  const __m128i round = _mm_set1_epi16(1 << (kInterRoundBitsVertical - 1));
  __m128i b[2];
  const __m128i a04 = _mm_add_epi16(a[0], a[4]);
  const __m128i a13 = _mm_add_epi16(a[1], a[3]);
  b[0] = _mm_unpacklo_epi16(a04, a13);
  b[1] = _mm_unpacklo_epi16(a[2], round);
  const __m128i sum0 = WienerVertical5(b, filter);
  b[0] = _mm_unpackhi_epi16(a04, a13);
  b[1] = _mm_unpackhi_epi16(a[2], round);
  const __m128i sum1 = WienerVertical5(b, filter);
  return _mm_packs_epi32(sum0, sum1);
}

inline __m128i WienerVerticalFilter3(const __m128i a[3], const __m128i filter) {
  __m128i b;
  const __m128i a02 = _mm_add_epi16(a[0], a[2]);
  b = _mm_unpacklo_epi16(a02, a[1]);
  const __m128i sum0 = WienerVertical3(b, filter);
  b = _mm_unpackhi_epi16(a02, a[1]);
  const __m128i sum1 = WienerVertical3(b, filter);
  return _mm_packs_epi32(sum0, sum1);
}

inline __m128i WienerVerticalTap7Kernel(const int16_t* wiener_buffer,
                                        const ptrdiff_t wiener_stride,
                                        const __m128i filter[2], __m128i a[7]) {
  a[0] = LoadAligned16(wiener_buffer + 0 * wiener_stride);
  a[1] = LoadAligned16(wiener_buffer + 1 * wiener_stride);
  a[2] = LoadAligned16(wiener_buffer + 2 * wiener_stride);
  a[3] = LoadAligned16(wiener_buffer + 3 * wiener_stride);
  a[4] = LoadAligned16(wiener_buffer + 4 * wiener_stride);
  a[5] = LoadAligned16(wiener_buffer + 5 * wiener_stride);
  a[6] = LoadAligned16(wiener_buffer + 6 * wiener_stride);
  return WienerVerticalFilter7(a, filter);
}

inline __m128i WienerVerticalTap5Kernel(const int16_t* wiener_buffer,
                                        const ptrdiff_t wiener_stride,
                                        const __m128i filter[2], __m128i a[5]) {
  a[0] = LoadAligned16(wiener_buffer + 0 * wiener_stride);
  a[1] = LoadAligned16(wiener_buffer + 1 * wiener_stride);
  a[2] = LoadAligned16(wiener_buffer + 2 * wiener_stride);
  a[3] = LoadAligned16(wiener_buffer + 3 * wiener_stride);
  a[4] = LoadAligned16(wiener_buffer + 4 * wiener_stride);
  return WienerVerticalFilter5(a, filter);
}

inline __m128i WienerVerticalTap3Kernel(const int16_t* wiener_buffer,
                                        const ptrdiff_t wiener_stride,
                                        const __m128i filter, __m128i a[3]) {
  a[0] = LoadAligned16(wiener_buffer + 0 * wiener_stride);
  a[1] = LoadAligned16(wiener_buffer + 1 * wiener_stride);
  a[2] = LoadAligned16(wiener_buffer + 2 * wiener_stride);
  return WienerVerticalFilter3(a, filter);
}

inline void WienerVerticalTap7Kernel2(const int16_t* wiener_buffer,
                                      const ptrdiff_t wiener_stride,
                                      const __m128i filter[2], __m128i d[2]) {
  __m128i a[8];
  d[0] = WienerVerticalTap7Kernel(wiener_buffer, wiener_stride, filter, a);
  a[7] = LoadAligned16(wiener_buffer + 7 * wiener_stride);
  d[1] = WienerVerticalFilter7(a + 1, filter);
}

inline void WienerVerticalTap5Kernel2(const int16_t* wiener_buffer,
                                      const ptrdiff_t wiener_stride,
                                      const __m128i filter[2], __m128i d[2]) {
  __m128i a[6];
  d[0] = WienerVerticalTap5Kernel(wiener_buffer, wiener_stride, filter, a);
  a[5] = LoadAligned16(wiener_buffer + 5 * wiener_stride);
  d[1] = WienerVerticalFilter5(a + 1, filter);
}

inline void WienerVerticalTap3Kernel2(const int16_t* wiener_buffer,
                                      const ptrdiff_t wiener_stride,
                                      const __m128i filter, __m128i d[2]) {
  __m128i a[4];
  d[0] = WienerVerticalTap3Kernel(wiener_buffer, wiener_stride, filter, a);
  a[3] = LoadAligned16(wiener_buffer + 3 * wiener_stride);
  d[1] = WienerVerticalFilter3(a + 1, filter);
}

inline void WienerVerticalTap7(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t coefficients[4], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  const __m128i c = LoadLo8(coefficients);
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi32(c, 0x0);
  filter[1] = _mm_shuffle_epi32(c, 0x55);
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i d[2][2];
      WienerVerticalTap7Kernel2(wiener_buffer + x + 0, width, filter, d[0]);
      WienerVerticalTap7Kernel2(wiener_buffer + x + 8, width, filter, d[1]);
      StoreAligned16(dst + x, _mm_packus_epi16(d[0][0], d[1][0]));
      StoreAligned16(dst + dst_stride + x, _mm_packus_epi16(d[0][1], d[1][1]));
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      __m128i a[7];
      const __m128i d0 =
          WienerVerticalTap7Kernel(wiener_buffer + x + 0, width, filter, a);
      const __m128i d1 =
          WienerVerticalTap7Kernel(wiener_buffer + x + 8, width, filter, a);
      StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
      x += 16;
    } while (x < width);
  }
}

inline void WienerVerticalTap5(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t coefficients[3], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  const __m128i c = Load4(coefficients);
  __m128i filter[2];
  filter[0] = _mm_shuffle_epi32(c, 0);
  filter[1] =
      _mm_set1_epi32((1 << 16) | static_cast<uint16_t>(coefficients[2]));
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i d[2][2];
      WienerVerticalTap5Kernel2(wiener_buffer + x + 0, width, filter, d[0]);
      WienerVerticalTap5Kernel2(wiener_buffer + x + 8, width, filter, d[1]);
      StoreAligned16(dst + x, _mm_packus_epi16(d[0][0], d[1][0]));
      StoreAligned16(dst + dst_stride + x, _mm_packus_epi16(d[0][1], d[1][1]));
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      __m128i a[5];
      const __m128i d0 =
          WienerVerticalTap5Kernel(wiener_buffer + x + 0, width, filter, a);
      const __m128i d1 =
          WienerVerticalTap5Kernel(wiener_buffer + x + 8, width, filter, a);
      StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
      x += 16;
    } while (x < width);
  }
}

inline void WienerVerticalTap3(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               const int16_t coefficients[2], uint8_t* dst,
                               const ptrdiff_t dst_stride) {
  const __m128i filter =
      _mm_set1_epi32(*reinterpret_cast<const int32_t*>(coefficients));
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      __m128i d[2][2];
      WienerVerticalTap3Kernel2(wiener_buffer + x + 0, width, filter, d[0]);
      WienerVerticalTap3Kernel2(wiener_buffer + x + 8, width, filter, d[1]);
      StoreAligned16(dst + x, _mm_packus_epi16(d[0][0], d[1][0]));
      StoreAligned16(dst + dst_stride + x, _mm_packus_epi16(d[0][1], d[1][1]));
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      __m128i a[3];
      const __m128i d0 =
          WienerVerticalTap3Kernel(wiener_buffer + x + 0, width, filter, a);
      const __m128i d1 =
          WienerVerticalTap3Kernel(wiener_buffer + x + 8, width, filter, a);
      StoreAligned16(dst + x, _mm_packus_epi16(d0, d1));
      x += 16;
    } while (x < width);
  }
}

inline void WienerVerticalTap1Kernel(const int16_t* const wiener_buffer,
                                     uint8_t* const dst) {
  const __m128i a0 = LoadAligned16(wiener_buffer + 0);
  const __m128i a1 = LoadAligned16(wiener_buffer + 8);
  const __m128i b0 = _mm_add_epi16(a0, _mm_set1_epi16(8));
  const __m128i b1 = _mm_add_epi16(a1, _mm_set1_epi16(8));
  const __m128i c0 = _mm_srai_epi16(b0, 4);
  const __m128i c1 = _mm_srai_epi16(b1, 4);
  const __m128i d = _mm_packus_epi16(c0, c1);
  StoreAligned16(dst, d);
}

inline void WienerVerticalTap1(const int16_t* wiener_buffer,
                               const ptrdiff_t width, const int height,
                               uint8_t* dst, const ptrdiff_t dst_stride) {
  for (int y = height >> 1; y > 0; --y) {
    ptrdiff_t x = 0;
    do {
      WienerVerticalTap1Kernel(wiener_buffer + x, dst + x);
      WienerVerticalTap1Kernel(wiener_buffer + width + x, dst + dst_stride + x);
      x += 16;
    } while (x < width);
    dst += 2 * dst_stride;
    wiener_buffer += 2 * width;
  }

  if ((height & 1) != 0) {
    ptrdiff_t x = 0;
    do {
      WienerVerticalTap1Kernel(wiener_buffer + x, dst + x);
      x += 16;
    } while (x < width);
  }
}

void WienerFilter_SSE4_1(const void* const source, void* const dest,
                         const RestorationUnitInfo& restoration_info,
                         const ptrdiff_t source_stride,
                         const ptrdiff_t dest_stride, const int width,
                         const int height, RestorationBuffer* const buffer) {
  constexpr int kCenterTap = kWienerFilterTaps / 2;
  const int16_t* const number_leading_zero_coefficients =
      restoration_info.wiener_info.number_leading_zero_coefficients;
  const int number_rows_to_skip = std::max(
      static_cast<int>(number_leading_zero_coefficients[WienerInfo::kVertical]),
      1);
  const ptrdiff_t wiener_stride = Align(width, 16);
  int16_t* const wiener_buffer_vertical = buffer->wiener_buffer;
  // The values are saturated to 13 bits before storing.
  int16_t* wiener_buffer_horizontal =
      wiener_buffer_vertical + number_rows_to_skip * wiener_stride;

  // horizontal filtering.
  // Over-reads up to 15 - |kRestorationHorizontalBorder| values.
  const int height_horizontal =
      height + kWienerFilterTaps - 1 - 2 * number_rows_to_skip;
  const auto* const src = static_cast<const uint8_t*>(source) -
                          (kCenterTap - number_rows_to_skip) * source_stride;
  const __m128i c =
      LoadLo8(restoration_info.wiener_info.filter[WienerInfo::kHorizontal]);
  // In order to keep the horizontal pass intermediate values within 16 bits we
  // offset |filter[3]| by 128. The 128 offset will be added back in the loop.
  const __m128i coefficients_horizontal =
      _mm_sub_epi16(c, _mm_setr_epi16(0, 0, 0, 128, 0, 0, 0, 0));
  if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 0) {
    WienerHorizontalTap7(src - 3, source_stride, wiener_stride,
                         height_horizontal, coefficients_horizontal,
                         &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 1) {
    WienerHorizontalTap5(src - 2, source_stride, wiener_stride,
                         height_horizontal, coefficients_horizontal,
                         &wiener_buffer_horizontal);
  } else if (number_leading_zero_coefficients[WienerInfo::kHorizontal] == 2) {
    // The maximum over-reads happen here.
    WienerHorizontalTap3(src - 1, source_stride, wiener_stride,
                         height_horizontal, coefficients_horizontal,
                         &wiener_buffer_horizontal);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kHorizontal] == 3);
    WienerHorizontalTap1(src, source_stride, wiener_stride, height_horizontal,
                         &wiener_buffer_horizontal);
  }

  // vertical filtering.
  // Over-writes up to 15 values.
  const int16_t* const filter_vertical =
      restoration_info.wiener_info.filter[WienerInfo::kVertical];
  auto* dst = static_cast<uint8_t*>(dest);
  if (number_leading_zero_coefficients[WienerInfo::kVertical] == 0) {
    // Because the top row of |source| is a duplicate of the second row, and the
    // bottom row of |source| is a duplicate of its above row, we can duplicate
    // the top and bottom row of |wiener_buffer| accordingly.
    memcpy(wiener_buffer_horizontal, wiener_buffer_horizontal - wiener_stride,
           sizeof(*wiener_buffer_horizontal) * wiener_stride);
    memcpy(buffer->wiener_buffer, buffer->wiener_buffer + wiener_stride,
           sizeof(*buffer->wiener_buffer) * wiener_stride);
    WienerVerticalTap7(wiener_buffer_vertical, wiener_stride, height,
                       filter_vertical, dst, dest_stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 1) {
    WienerVerticalTap5(wiener_buffer_vertical + wiener_stride, wiener_stride,
                       height, filter_vertical + 1, dst, dest_stride);
  } else if (number_leading_zero_coefficients[WienerInfo::kVertical] == 2) {
    WienerVerticalTap3(wiener_buffer_vertical + 2 * wiener_stride,
                       wiener_stride, height, filter_vertical + 2, dst,
                       dest_stride);
  } else {
    assert(number_leading_zero_coefficients[WienerInfo::kVertical] == 3);
    WienerVerticalTap1(wiener_buffer_vertical + 3 * wiener_stride,
                       wiener_stride, height, dst, dest_stride);
  }
}

//------------------------------------------------------------------------------
// SGR

// Don't use _mm_cvtepu8_epi16() or _mm_cvtepu16_epi32() in the following
// functions. Some compilers may generate super inefficient code and the whole
// decoder could be 15% slower.

inline __m128i VaddlLo8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(s0, s1);
}

inline __m128i VaddlHi8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(s0, s1);
}

inline __m128i VaddlLo16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(s0, s1);
}

inline __m128i VaddlHi16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(s0, s1);
}

inline __m128i VaddwLo8(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(src0, s1);
}

inline __m128i VaddwHi8(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
  return _mm_add_epi16(src0, s1);
}

inline __m128i VaddwLo16(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpacklo_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(src0, s1);
}

inline __m128i VaddwHi16(const __m128i src0, const __m128i src1) {
  const __m128i s1 = _mm_unpackhi_epi16(src1, _mm_setzero_si128());
  return _mm_add_epi32(src0, s1);
}

// Using VgetLane16() can save a sign extension instruction.
template <int n>
inline int VgetLane16(const __m128i src) {
  return _mm_extract_epi16(src, n);
}

inline __m128i VmullLo8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi8(src1, _mm_setzero_si128());
  return _mm_mullo_epi16(s0, s1);
}

inline __m128i VmullHi8(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi8(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi8(src1, _mm_setzero_si128());
  return _mm_mullo_epi16(s0, s1);
}

inline __m128i VmullNLo8(const __m128i src0, const int src1) {
  const __m128i s0 = _mm_unpacklo_epi16(src0, _mm_setzero_si128());
  return _mm_madd_epi16(s0, _mm_set1_epi32(src1));
}

inline __m128i VmullNHi8(const __m128i src0, const int src1) {
  const __m128i s0 = _mm_unpackhi_epi16(src0, _mm_setzero_si128());
  return _mm_madd_epi16(s0, _mm_set1_epi32(src1));
}

inline __m128i VmullLo16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpacklo_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpacklo_epi16(src1, _mm_setzero_si128());
  return _mm_madd_epi16(s0, s1);
}

inline __m128i VmullHi16(const __m128i src0, const __m128i src1) {
  const __m128i s0 = _mm_unpackhi_epi16(src0, _mm_setzero_si128());
  const __m128i s1 = _mm_unpackhi_epi16(src1, _mm_setzero_si128());
  return _mm_madd_epi16(s0, s1);
}

inline __m128i VrshrS32(const __m128i src0, const int src1) {
  const __m128i sum = _mm_add_epi32(src0, _mm_set1_epi32(1 << (src1 - 1)));
  return _mm_srai_epi32(sum, src1);
}

inline __m128i VrshrU32(const __m128i src0, const int src1) {
  const __m128i sum = _mm_add_epi32(src0, _mm_set1_epi32(1 << (src1 - 1)));
  return _mm_srli_epi32(sum, src1);
}

template <int n>
inline __m128i CalcAxN(const __m128i a) {
  static_assert(n == 9 || n == 25, "");
  // _mm_mullo_epi32() has high latency. Using shifts and additions instead.
  // Some compilers could do this for us but we make this explicit.
  // return _mm_mullo_epi32(a, _mm_set1_epi32(n));
  const __m128i ax9 = _mm_add_epi32(a, _mm_slli_epi32(a, 3));
  if (n == 9) return ax9;
  if (n == 25) return _mm_add_epi32(ax9, _mm_slli_epi32(a, 4));
}

template <int n>
inline __m128i CalculateMa(const __m128i sum_sq, const __m128i sum,
                           const uint32_t s) {
  // a = |sum_sq|
  // d = |sum|
  // p = (a * n < d * d) ? 0 : a * n - d * d;
  const __m128i dxd = _mm_madd_epi16(sum, sum);
  const __m128i axn = CalcAxN<n>(sum_sq);
  const __m128i sub = _mm_sub_epi32(axn, dxd);
  const __m128i p = _mm_max_epi32(sub, _mm_setzero_si128());

  // z = RightShiftWithRounding(p * s, kSgrProjScaleBits);
  const __m128i pxs = _mm_mullo_epi32(p, _mm_set1_epi32(s));
  return VrshrU32(pxs, kSgrProjScaleBits);
}

// b = ma * b * one_over_n
// |ma| = [0, 255]
// |sum| is a box sum with radius 1 or 2.
// For the first pass radius is 2. Maximum value is 5x5x255 = 6375.
// For the second pass radius is 1. Maximum value is 3x3x255 = 2295.
// |one_over_n| = ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n
// When radius is 2 |n| is 25. |one_over_n| is 164.
// When radius is 1 |n| is 9. |one_over_n| is 455.
// |kSgrProjReciprocalBits| is 12.
// Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
// Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
inline __m128i CalculateIntermediate4(const __m128i ma, const __m128i sum,
                                      const uint32_t one_over_n) {
  const __m128i maq = _mm_unpacklo_epi8(ma, _mm_setzero_si128());
  const __m128i s = _mm_unpackhi_epi16(maq, _mm_setzero_si128());
  const __m128i m = _mm_madd_epi16(s, sum);
  const __m128i b = _mm_mullo_epi32(m, _mm_set1_epi32(one_over_n));
  const __m128i truncate_u32 = VrshrU32(b, kSgrProjReciprocalBits);
  return _mm_packus_epi32(truncate_u32, truncate_u32);
}

inline __m128i CalculateIntermediate8(const __m128i ma, const __m128i sum,
                                      const uint32_t one_over_n) {
  const __m128i maq = _mm_unpackhi_epi8(ma, _mm_setzero_si128());
  const __m128i m0 = VmullLo16(maq, sum);
  const __m128i m1 = VmullHi16(maq, sum);
  const __m128i m2 = _mm_mullo_epi32(m0, _mm_set1_epi32(one_over_n));
  const __m128i m3 = _mm_mullo_epi32(m1, _mm_set1_epi32(one_over_n));
  const __m128i b_lo = VrshrU32(m2, kSgrProjReciprocalBits);
  const __m128i b_hi = VrshrU32(m3, kSgrProjReciprocalBits);
  return _mm_packus_epi32(b_lo, b_hi);
}

inline __m128i Sum3_16(const __m128i left, const __m128i middle,
                       const __m128i right) {
  const __m128i sum = _mm_add_epi16(left, middle);
  return _mm_add_epi16(sum, right);
}

inline __m128i Sum3_32(const __m128i left, const __m128i middle,
                       const __m128i right) {
  const __m128i sum = _mm_add_epi32(left, middle);
  return _mm_add_epi32(sum, right);
}

inline __m128i Sum3W_16(const __m128i left, const __m128i middle,
                        const __m128i right) {
  const __m128i sum = VaddlLo8(left, middle);
  return VaddwLo8(sum, right);
}

inline __m128i Sum3WLo_16(const __m128i src[3]) {
  return Sum3W_16(src[0], src[1], src[2]);
}

inline __m128i Sum3WHi_16(const __m128i src[3]) {
  const __m128i sum = VaddlHi8(src[0], src[1]);
  return VaddwHi8(sum, src[2]);
}

inline __m128i Sum3WLo_32(const __m128i left, const __m128i middle,
                          const __m128i right) {
  const __m128i sum = VaddlLo16(left, middle);
  return VaddwLo16(sum, right);
}

inline __m128i Sum3WHi_32(const __m128i left, const __m128i middle,
                          const __m128i right) {
  const __m128i sum = VaddlHi16(left, middle);
  return VaddwHi16(sum, right);
}

inline __m128i* Sum3W_16x2(const __m128i src[3], __m128i sum[2]) {
  sum[0] = Sum3WLo_16(src);
  sum[1] = Sum3WHi_16(src);
  return sum;
}

inline __m128i* Sum3W(const __m128i src[3], __m128i sum[2]) {
  sum[0] = Sum3WLo_32(src[0], src[1], src[2]);
  sum[1] = Sum3WHi_32(src[0], src[1], src[2]);
  return sum;
}

template <int index>
inline __m128i Sum3WLo(const __m128i src[3][2]) {
  return Sum3WLo_32(src[0][index], src[1][index], src[2][index]);
}

inline __m128i Sum3WHi(const __m128i src[3][2]) {
  return Sum3WHi_32(src[0][0], src[1][0], src[2][0]);
}

inline __m128i* Sum3W(const __m128i src[3][2], __m128i sum[3]) {
  sum[0] = Sum3WLo<0>(src);
  sum[1] = Sum3WHi(src);
  sum[2] = Sum3WLo<1>(src);
  return sum;
}

inline __m128i Sum5_16(const __m128i src[5]) {
  const __m128i sum01 = _mm_add_epi16(src[0], src[1]);
  const __m128i sum23 = _mm_add_epi16(src[2], src[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return _mm_add_epi16(sum, src[4]);
}

inline __m128i Sum5_32(const __m128i src[5]) {
  const __m128i sum01 = _mm_add_epi32(src[0], src[1]);
  const __m128i sum23 = _mm_add_epi32(src[2], src[3]);
  const __m128i sum = _mm_add_epi32(sum01, sum23);
  return _mm_add_epi32(sum, src[4]);
}

inline __m128i Sum5WLo_16(const __m128i src[5]) {
  const __m128i sum01 = VaddlLo8(src[0], src[1]);
  const __m128i sum23 = VaddlLo8(src[2], src[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwLo8(sum, src[4]);
}

inline __m128i Sum5WHi_16(const __m128i src[5]) {
  const __m128i sum01 = VaddlHi8(src[0], src[1]);
  const __m128i sum23 = VaddlHi8(src[2], src[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwHi8(sum, src[4]);
}

inline __m128i Sum5WLo_32(const __m128i src[5]) {
  const __m128i sum01 = VaddlLo16(src[0], src[1]);
  const __m128i sum23 = VaddlLo16(src[2], src[3]);
  const __m128i sum0123 = _mm_add_epi32(sum01, sum23);
  return VaddwLo16(sum0123, src[4]);
}

inline __m128i Sum5WHi_32(const __m128i src[5]) {
  const __m128i sum01 = VaddlHi16(src[0], src[1]);
  const __m128i sum23 = VaddlHi16(src[2], src[3]);
  const __m128i sum0123 = _mm_add_epi32(sum01, sum23);
  return VaddwHi16(sum0123, src[4]);
}

inline __m128i* Sum5W_16D(const __m128i src[5], __m128i sum[2]) {
  sum[0] = Sum5WLo_16(src);
  sum[1] = Sum5WHi_16(src);
  return sum;
}

inline __m128i* Sum5W_32x2(const __m128i src[5], __m128i sum[2]) {
  sum[0] = Sum5WLo_32(src);
  sum[1] = Sum5WHi_32(src);
  return sum;
}

template <int index>
inline __m128i Sum5WLo(const __m128i src[5][2]) {
  __m128i s[5];
  s[0] = src[0][index];
  s[1] = src[1][index];
  s[2] = src[2][index];
  s[3] = src[3][index];
  s[4] = src[4][index];
  return Sum5WLo_32(s);
}

inline __m128i Sum5WHi(const __m128i src[5][2]) {
  __m128i s[5];
  s[0] = src[0][0];
  s[1] = src[1][0];
  s[2] = src[2][0];
  s[3] = src[3][0];
  s[4] = src[4][0];
  return Sum5WHi_32(s);
}

inline __m128i* Sum5W_32x3(const __m128i src[5][2], __m128i sum[3]) {
  sum[0] = Sum5WLo<0>(src);
  sum[1] = Sum5WHi(src);
  sum[2] = Sum5WLo<1>(src);
  return sum;
}

inline __m128i Sum3Horizontal(const __m128i src) {
  const auto left = src;
  const auto middle = _mm_srli_si128(src, 2);
  const auto right = _mm_srli_si128(src, 4);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3Horizontal_32(const __m128i src[2]) {
  const auto left = src[0];
  const auto middle = _mm_alignr_epi8(src[1], src[0], 4);
  const auto right = _mm_alignr_epi8(src[1], src[0], 8);
  return Sum3_32(left, middle, right);
}

inline __m128i Sum3HorizontalOffset1(const __m128i src) {
  const auto left = _mm_srli_si128(src, 2);
  const auto middle = _mm_srli_si128(src, 4);
  const auto right = _mm_srli_si128(src, 6);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3HorizontalOffset1_16(const __m128i src[2]) {
  const auto left = _mm_alignr_epi8(src[1], src[0], 2);
  const auto middle = _mm_alignr_epi8(src[1], src[0], 4);
  const auto right = _mm_alignr_epi8(src[1], src[0], 6);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3HorizontalOffset1_32(const __m128i src[2]) {
  const auto left = _mm_alignr_epi8(src[1], src[0], 4);
  const auto middle = _mm_alignr_epi8(src[1], src[0], 8);
  const auto right = _mm_alignr_epi8(src[1], src[0], 12);
  return Sum3_32(left, middle, right);
}

inline void Sum3HorizontalOffset1_32x2(const __m128i src[3], __m128i sum[2]) {
  sum[0] = Sum3HorizontalOffset1_32(src + 0);
  sum[1] = Sum3HorizontalOffset1_32(src + 1);
}

inline __m128i Sum5Horizontal(const __m128i src) {
  __m128i s[5];
  s[0] = src;
  s[1] = _mm_srli_si128(src, 2);
  s[2] = _mm_srli_si128(src, 4);
  s[3] = _mm_srli_si128(src, 6);
  s[4] = _mm_srli_si128(src, 8);
  return Sum5_16(s);
}

inline __m128i Sum5Horizontal_16(const __m128i src[2]) {
  __m128i s[5];
  s[0] = src[0];
  s[1] = _mm_alignr_epi8(src[1], src[0], 2);
  s[2] = _mm_alignr_epi8(src[1], src[0], 4);
  s[3] = _mm_alignr_epi8(src[1], src[0], 6);
  s[4] = _mm_alignr_epi8(src[1], src[0], 8);
  return Sum5_16(s);
}

inline __m128i Sum5Horizontal_32(const __m128i src[2]) {
  __m128i s[5];
  s[0] = src[0];
  s[1] = _mm_alignr_epi8(src[1], src[0], 4);
  s[2] = _mm_alignr_epi8(src[1], src[0], 8);
  s[3] = _mm_alignr_epi8(src[1], src[0], 12);
  s[4] = src[1];
  return Sum5_32(s);
}

inline __m128i* Sum5Horizontal_32x2(const __m128i src[3], __m128i sum[2]) {
  __m128i s[5];
  s[0] = src[0];
  s[1] = _mm_alignr_epi8(src[1], src[0], 4);
  s[2] = _mm_alignr_epi8(src[1], src[0], 8);
  s[3] = _mm_alignr_epi8(src[1], src[0], 12);
  s[4] = src[1];
  sum[0] = Sum5_32(s);
  s[0] = src[1];
  s[1] = _mm_alignr_epi8(src[2], src[1], 4);
  s[2] = _mm_alignr_epi8(src[2], src[1], 8);
  s[3] = _mm_alignr_epi8(src[2], src[1], 12);
  s[4] = src[2];
  sum[1] = Sum5_32(s);
  return sum;
}

template <int size, int offset>
inline void BoxFilterPreProcess4(const __m128i* const row,
                                 const __m128i* const row_sq, const uint32_t s,
                                 uint16_t* const dst) {
  static_assert(size == 3 || size == 5, "");
  static_assert(offset == 0 || offset == 1, "");
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  __m128i sum, sum_sq;
  if (size == 3) {
    __m128i temp32[2];
    if (offset == 0) {
      sum = Sum3Horizontal(Sum3WLo_16(row));
      sum_sq = Sum3Horizontal_32(Sum3W(row_sq, temp32));
    } else {
      sum = Sum3HorizontalOffset1(Sum3WLo_16(row));
      sum_sq = Sum3HorizontalOffset1_32(Sum3W(row_sq, temp32));
    }
  }
  if (size == 5) {
    __m128i temp[2];
    sum = Sum5Horizontal(Sum5WLo_16(row));
    sum_sq = Sum5Horizontal_32(Sum5W_32x2(row_sq, temp));
  }
  const __m128i sum_32 = _mm_unpacklo_epi16(sum, _mm_setzero_si128());
  const __m128i z0 = CalculateMa<n>(sum_sq, sum_32, s);
  const __m128i z1 = _mm_packus_epi32(z0, z0);
  const __m128i z = _mm_min_epu16(z1, _mm_set1_epi16(255));
  __m128i ma = _mm_setzero_si128();
  ma = _mm_insert_epi8(ma, kSgrMaLookup[VgetLane16<0>(z)], 4);
  ma = _mm_insert_epi8(ma, kSgrMaLookup[VgetLane16<1>(z)], 5);
  ma = _mm_insert_epi8(ma, kSgrMaLookup[VgetLane16<2>(z)], 6);
  ma = _mm_insert_epi8(ma, kSgrMaLookup[VgetLane16<3>(z)], 7);
  const __m128i b = CalculateIntermediate4(ma, sum_32, one_over_n);
  const __m128i ma_b = _mm_unpacklo_epi64(ma, b);
  StoreAligned16(dst, ma_b);
}

template <int size>
inline void BoxFilterPreProcess8(const __m128i* const row,
                                 const __m128i row_sq[][2], const uint32_t s,
                                 __m128i* const ma, __m128i* const b,
                                 uint16_t* const dst) {
  static_assert(size == 3 || size == 5, "");
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  __m128i sum, sum_sq[2];
  if (size == 3) {
    __m128i temp16[2], temp32[3];
    sum = Sum3HorizontalOffset1_16(Sum3W_16x2(row, temp16));
    Sum3HorizontalOffset1_32x2(Sum3W(row_sq, temp32), sum_sq);
  }
  if (size == 5) {
    __m128i temp16[2], temp32[3];
    sum = Sum5Horizontal_16(Sum5W_16D(row, temp16));
    Sum5Horizontal_32x2(Sum5W_32x3(row_sq, temp32), sum_sq);
  }
  const __m128i sum_lo = _mm_unpacklo_epi16(sum, _mm_setzero_si128());
  const __m128i z0 = CalculateMa<n>(sum_sq[0], sum_lo, s);
  const __m128i sum_hi = _mm_unpackhi_epi16(sum, _mm_setzero_si128());
  const __m128i z1 = CalculateMa<n>(sum_sq[1], sum_hi, s);
  const __m128i z01 = _mm_packus_epi32(z0, z1);
  const __m128i z = _mm_min_epu16(z01, _mm_set1_epi16(255));
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<0>(z)], 8);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<1>(z)], 9);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<2>(z)], 10);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<3>(z)], 11);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<4>(z)], 12);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<5>(z)], 13);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<6>(z)], 14);
  *ma = _mm_insert_epi8(*ma, kSgrMaLookup[VgetLane16<7>(z)], 15);
  *b = CalculateIntermediate8(*ma, sum, one_over_n);
  const __m128i ma_b = _mm_unpackhi_epi64(*ma, *b);
  StoreAligned16(dst, ma_b);
}

inline void Prepare3_8(const __m128i src, __m128i* const left,
                       __m128i* const middle, __m128i* const right) {
  *left = _mm_srli_si128(src, 5);
  *middle = _mm_srli_si128(src, 6);
  *right = _mm_srli_si128(src, 7);
}

inline void Prepare3_16(const __m128i src[2], __m128i* const left,
                        __m128i* const middle, __m128i* const right) {
  *left = _mm_alignr_epi8(src[1], src[0], 10);
  *middle = _mm_alignr_epi8(src[1], src[0], 12);
  *right = _mm_alignr_epi8(src[1], src[0], 14);
}

inline __m128i Sum343(const __m128i src) {
  __m128i left, middle, right;
  Prepare3_8(src, &left, &middle, &right);
  const auto sum = Sum3W_16(left, middle, right);
  const auto sum3 = Sum3_16(sum, sum, sum);
  return VaddwLo8(sum3, middle);
}

inline void Sum343_444(const __m128i src, __m128i* const sum343,
                       __m128i* const sum444) {
  __m128i left, middle, right;
  Prepare3_8(src, &left, &middle, &right);
  const auto sum111 = Sum3W_16(left, middle, right);
  *sum444 = _mm_slli_epi16(sum111, 2);
  const __m128i sum333 = _mm_sub_epi16(*sum444, sum111);
  *sum343 = VaddwLo8(sum333, middle);
}

inline __m128i* Sum343W(const __m128i src[2], __m128i d[2]) {
  __m128i left, middle, right;
  Prepare3_16(src, &left, &middle, &right);
  d[0] = Sum3WLo_32(left, middle, right);
  d[1] = Sum3WHi_32(left, middle, right);
  d[0] = Sum3_32(d[0], d[0], d[0]);
  d[1] = Sum3_32(d[1], d[1], d[1]);
  d[0] = VaddwLo16(d[0], middle);
  d[1] = VaddwHi16(d[1], middle);
  return d;
}

inline void Sum343_444W(const __m128i src[2], __m128i sum343[2],
                        __m128i sum444[2]) {
  __m128i left, middle, right, sum111[2];
  Prepare3_16(src, &left, &middle, &right);
  sum111[0] = Sum3WLo_32(left, middle, right);
  sum111[1] = Sum3WHi_32(left, middle, right);
  sum444[0] = _mm_slli_epi32(sum111[0], 2);
  sum444[1] = _mm_slli_epi32(sum111[1], 2);
  sum343[0] = _mm_sub_epi32(sum444[0], sum111[0]);
  sum343[1] = _mm_sub_epi32(sum444[1], sum111[1]);
  sum343[0] = VaddwLo16(sum343[0], middle);
  sum343[1] = VaddwHi16(sum343[1], middle);
}

inline __m128i Sum565(const __m128i src) {
  __m128i left, middle, right;
  Prepare3_8(src, &left, &middle, &right);
  const auto sum = Sum3W_16(left, middle, right);
  const auto sum4 = _mm_slli_epi16(sum, 2);
  const auto sum5 = _mm_add_epi16(sum4, sum);
  return VaddwLo8(sum5, middle);
}

inline __m128i Sum565W(const __m128i src) {
  const auto left = _mm_srli_si128(src, 2);
  const auto middle = _mm_srli_si128(src, 4);
  const auto right = _mm_srli_si128(src, 6);
  const auto sum = Sum3WLo_32(left, middle, right);
  const auto sum4 = _mm_slli_epi32(sum, 2);
  const auto sum5 = _mm_add_epi32(sum4, sum);
  return VaddwLo16(sum5, middle);
}

template <int shift>
inline __m128i FilterOutput(const __m128i ma_x_src, const __m128i b) {
  // ma: 255 * 32 = 8160 (13 bits)
  // b: 65088 * 32 = 2082816 (21 bits)
  // v: b - ma * 255 (22 bits)
  const __m128i v = _mm_sub_epi32(b, ma_x_src);
  // kSgrProjSgrBits = 8
  // kSgrProjRestoreBits = 4
  // shift = 4 or 5
  // v >> 8 or 9 (13 bits)
  return VrshrS32(v, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
}

template <int shift>
inline __m128i CalculateFilteredOutput(const __m128i src, const __m128i a,
                                       const __m128i b[2]) {
  const __m128i src_u16 = _mm_unpacklo_epi8(src, _mm_setzero_si128());
  const __m128i ma_x_src_lo = VmullLo16(a, src_u16);
  const __m128i ma_x_src_hi = VmullHi16(a, src_u16);
  const __m128i dst_lo = FilterOutput<shift>(ma_x_src_lo, b[0]);
  const __m128i dst_hi = FilterOutput<shift>(ma_x_src_hi, b[1]);
  return _mm_packs_epi32(dst_lo, dst_hi);  // 13 bits
}

inline __m128i BoxFilterPass1(const __m128i src_u8, const __m128i ma,
                              const __m128i b[2], __m128i ma565[2],
                              __m128i b565[2][2]) {
  __m128i b_sum[2];
  ma565[1] = Sum565(ma);
  b565[1][0] = Sum565W(_mm_alignr_epi8(b[1], b[0], 8));
  b565[1][1] = Sum565W(b[1]);
  __m128i ma_sum = _mm_add_epi16(ma565[0], ma565[1]);
  b_sum[0] = _mm_add_epi32(b565[0][0], b565[1][0]);
  b_sum[1] = _mm_add_epi32(b565[0][1], b565[1][1]);
  return CalculateFilteredOutput<5>(src_u8, ma_sum, b_sum);  // 13 bits
}

inline __m128i BoxFilterPass2(const __m128i src_u8, const __m128i ma,
                              const __m128i b[2], __m128i ma343[4],
                              __m128i ma444[3], __m128i b343[4][2],
                              __m128i b444[3][2]) {
  __m128i b_sum[2];
  Sum343_444(ma, &ma343[2], &ma444[1]);
  __m128i ma_sum = Sum3_16(ma343[0], ma444[0], ma343[2]);
  Sum343_444W(b, b343[2], b444[1]);
  b_sum[0] = Sum3_32(b343[0][0], b444[0][0], b343[2][0]);
  b_sum[1] = Sum3_32(b343[0][1], b444[0][1], b343[2][1]);
  return CalculateFilteredOutput<5>(src_u8, ma_sum, b_sum);  // 13 bits
}

inline void SelfGuidedFinal(const __m128i src, const __m128i v[2],
                            uint8_t* const dst) {
  const __m128i v_lo =
      VrshrS32(v[0], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i v_hi =
      VrshrS32(v[1], kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i vv = _mm_packs_epi32(v_lo, v_hi);
  const __m128i s = _mm_unpacklo_epi8(src, _mm_setzero_si128());
  const __m128i d = _mm_add_epi16(s, vv);
  StoreLo8(dst, _mm_packus_epi16(d, d));
}

inline void SelfGuidedDoubleMultiplier(const __m128i src,
                                       const __m128i filter[2], const int w0,
                                       const int w2, uint8_t* const dst) {
  __m128i v[2];
  const __m128i w0_w2 = _mm_set1_epi32((w2 << 16) | static_cast<uint16_t>(w0));
  const __m128i f_lo = _mm_unpacklo_epi16(filter[0], filter[1]);
  const __m128i f_hi = _mm_unpackhi_epi16(filter[0], filter[1]);
  v[0] = _mm_madd_epi16(w0_w2, f_lo);
  v[1] = _mm_madd_epi16(w0_w2, f_hi);
  SelfGuidedFinal(src, v, dst);
}

inline void SelfGuidedSingleMultiplier(const __m128i src, const __m128i filter,
                                       const int w0, uint8_t* const dst) {
  // weight: -96 to 96 (Sgrproj_Xqd_Min/Max)
  __m128i v[2];
  v[0] = VmullNLo8(filter, w0);
  v[1] = VmullNHi8(filter, w0);
  SelfGuidedFinal(src, v, dst);
}

inline void BoxFilterProcess(const uint8_t* const src,
                             const ptrdiff_t src_stride,
                             const RestorationUnitInfo& restoration_info,
                             const int width, const int height,
                             const uint16_t scale[2], uint16_t* const temp,
                             uint8_t* const dst, const ptrdiff_t dst_stride) {
  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |ma| region. The values stored are one
  // vertical column of interleaved |ma| and |b| values and consume 8 *
  // |height| values. This is |height| and not |height| * 2 because PreProcess
  // only generates output for every other row. When processing the next column
  // we write the new scratch values right after reading the previously saved
  // ones.

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
  const uint8_t* const src_pre_process = src - 2 * src_stride;
  // Calculate intermediate results, including two-pixel border, for example, if
  // unit size is 64x64, we calculate 68x68 pixels.
  {
    const uint8_t* column = src_pre_process - 4;
    __m128i row[5], row_sq[5];
    row[0] = row[1] = LoadLo8(column);
    column += src_stride;
    row[2] = LoadLo8(column);
    row_sq[0] = row_sq[1] = VmullLo8(row[1], row[1]);
    row_sq[2] = VmullLo8(row[2], row[2]);

    int y = (height + 2) >> 1;
    do {
      column += src_stride;
      row[3] = LoadLo8(column);
      column += src_stride;
      row[4] = LoadLo8(column);
      row_sq[3] = VmullLo8(row[3], row[3]);
      row_sq[4] = VmullLo8(row[4], row[4]);
      BoxFilterPreProcess4<5, 1>(row + 0, row_sq + 0, scale[0], ab_ptr + 0);
      BoxFilterPreProcess4<3, 1>(row + 1, row_sq + 1, scale[1], ab_ptr + 8);
      BoxFilterPreProcess4<3, 1>(row + 2, row_sq + 2, scale[1], ab_ptr + 16);
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
      row[3] = row[4] = LoadLo8(column);
      row_sq[3] = row_sq[4] = VmullLo8(row[3], row[3]);
      BoxFilterPreProcess4<5, 1>(row + 0, row_sq + 0, scale[0], ab_ptr + 0);
      BoxFilterPreProcess4<3, 1>(row + 1, row_sq + 1, scale[1], ab_ptr + 8);
    }
  }

  const int w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
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
    __m128i ma[2], b[2][2], ma565[2], ma343[4], ma444[3];
    __m128i b565[2][2], b343[4][2], b444[3][2];
    ab_ptr = temp;
    ma[0] = b[0][0] = LoadAligned16(ab_ptr);
    ma[1] = b[1][0] = LoadAligned16(ab_ptr + 8);
    const uint8_t* column = src_pre_process + x;
    __m128i row[5], row_sq[5][2];
    // Need |width| + 3 pixels, but we read max(|x|) + 16 pixels.
    // Mask max(|x|) + 13 - |width| extra pixels.
    row[0] = row[1] = LoadUnaligned16Msan(column, x + 13 - width);
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 13 - width);
    column += src_stride;
    row[3] = LoadUnaligned16Msan(column, x + 13 - width);
    column += src_stride;
    row[4] = LoadUnaligned16Msan(column, x + 13 - width);
    row_sq[0][0] = row_sq[1][0] = VmullLo8(row[1], row[1]);
    row_sq[0][1] = row_sq[1][1] = VmullHi8(row[1], row[1]);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);
    row_sq[3][0] = VmullLo8(row[3], row[3]);
    row_sq[3][1] = VmullHi8(row[3], row[3]);
    row_sq[4][0] = VmullLo8(row[4], row[4]);
    row_sq[4][1] = VmullHi8(row[4], row[4]);
    BoxFilterPreProcess8<5>(row, row_sq, scale[0], &ma[0], &b[0][1], ab_ptr);
    BoxFilterPreProcess8<3>(row + 1, row_sq + 1, scale[1], &ma[1], &b[1][1],
                            ab_ptr + 8);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    ma565[0] = Sum565(ma[0]);
    b565[0][0] = Sum565W(_mm_alignr_epi8(b[0][1], b[0][0], 8));
    b565[0][1] = Sum565W(b[0][1]);
    ma343[0] = Sum343(ma[1]);
    Sum343W(b[1], b343[0]);
    ma[1] = b[1][0] = LoadAligned16(ab_ptr + 16);
    BoxFilterPreProcess8<3>(row + 2, row_sq + 2, scale[1], &ma[1], &b[1][1],
                            ab_ptr + 16);
    Sum343_444(ma[1], &ma343[1], &ma444[0]);
    Sum343_444W(b[1], b343[1], b444[0]);

    uint8_t* dst_ptr = dst + x;
    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    for (int y = height >> 1; y != 0; --y) {
      ab_ptr += 24;
      ma[0] = b[0][0] = LoadAligned16(ab_ptr);
      ma[1] = b[1][0] = LoadAligned16(ab_ptr + 8);
      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];
      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];
      column += src_stride;
      row[3] = LoadUnaligned16Msan(column, x + 13 - width);
      column += src_stride;
      row[4] = LoadUnaligned16Msan(column, x + 13 - width);
      row_sq[3][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = VmullHi8(row[3], row[3]);
      row_sq[4][0] = VmullLo8(row[4], row[4]);
      row_sq[4][1] = VmullHi8(row[4], row[4]);
      BoxFilterPreProcess8<5>(row, row_sq, scale[0], &ma[0], &b[0][1], ab_ptr);
      BoxFilterPreProcess8<3>(row + 1, row_sq + 1, scale[1], &ma[1], &b[1][1],
                              ab_ptr + 8);
      __m128i p[2];
      p[0] = BoxFilterPass1(row[1], ma[0], b[0], ma565, b565);
      p[1] = BoxFilterPass2(row[1], ma[1], b[1], ma343, ma444, b343, b444);
      SelfGuidedDoubleMultiplier(row[1], p, w0, w2, dst_ptr);
      dst_ptr += dst_stride;
      p[0] = CalculateFilteredOutput<4>(row[2], ma565[1], b565[1]);
      ma[1] = b[1][0] = LoadAligned16(ab_ptr + 16);
      BoxFilterPreProcess8<3>(row + 2, row_sq + 2, scale[1], &ma[1], &b[1][1],
                              ab_ptr + 16);
      p[1] = BoxFilterPass2(row[2], ma[1], b[1], ma343 + 1, ma444 + 1, b343 + 1,
                            b444 + 1);
      SelfGuidedDoubleMultiplier(row[2], p, w0, w2, dst_ptr);
      dst_ptr += dst_stride;
      ma565[0] = ma565[1];
      b565[0][0] = b565[1][0], b565[0][1] = b565[1][1];
      ma343[0] = ma343[2];
      ma343[1] = ma343[3];
      ma444[0] = ma444[2];
      b343[0][0] = b343[2][0], b343[0][1] = b343[2][1];
      b343[1][0] = b343[3][0], b343[1][1] = b343[3][1];
      b444[0][0] = b444[2][0], b444[0][1] = b444[2][1];
    }

    if ((height & 1) != 0) {
      ab_ptr += 24;
      ma[0] = b[0][0] = LoadAligned16(ab_ptr);
      ma[1] = b[1][0] = LoadAligned16(ab_ptr + 8);
      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];
      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];
      column += src_stride;
      row[3] = row[4] = LoadUnaligned16Msan(column, x + 13 - width);
      row_sq[3][0] = row_sq[4][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = row_sq[4][1] = VmullHi8(row[3], row[3]);
      BoxFilterPreProcess8<5>(row, row_sq, scale[0], &ma[0], &b[0][1], ab_ptr);
      BoxFilterPreProcess8<3>(row + 1, row_sq + 1, scale[1], &ma[1], &b[1][1],
                              ab_ptr + 8);
      __m128i p[2];
      p[0] = BoxFilterPass1(row[1], ma[0], b[0], ma565, b565);
      p[1] = BoxFilterPass2(row[1], ma[1], b[1], ma343, ma444, b343, b444);
      SelfGuidedDoubleMultiplier(row[1], p, w0, w2, dst_ptr);
    }
    x += 8;
  } while (x < width);
}

inline void BoxFilterProcessPass1(const uint8_t* const src,
                                  const ptrdiff_t src_stride,
                                  const RestorationUnitInfo& restoration_info,
                                  const int width, const int height,
                                  const uint32_t scale, uint16_t* const temp,
                                  uint8_t* const dst,
                                  const ptrdiff_t dst_stride) {
  // We have combined PreProcess and Process for the first pass by storing
  // intermediate values in the |ma| region. The values stored are one
  // vertical column of interleaved |ma| and |b| values and consume 8 *
  // |height| values. This is |height| and not |height| * 2 because PreProcess
  // only generates output for every other row. When processing the next column
  // we write the new scratch values right after reading the previously saved
  // ones.

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
  const uint8_t* const src_pre_process = src - 2 * src_stride;
  // Calculate intermediate results, including two-pixel border, for example, if
  // unit size is 64x64, we calculate 68x68 pixels.
  {
    const uint8_t* column = src_pre_process - 4;
    __m128i row[5], row_sq[5];
    row[0] = row[1] = LoadLo8(column);
    column += src_stride;
    row[2] = LoadLo8(column);
    row_sq[0] = row_sq[1] = VmullLo8(row[1], row[1]);
    row_sq[2] = VmullLo8(row[2], row[2]);

    int y = (height + 2) >> 1;
    do {
      column += src_stride;
      row[3] = LoadLo8(column);
      column += src_stride;
      row[4] = LoadLo8(column);
      row_sq[3] = VmullLo8(row[3], row[3]);
      row_sq[4] = VmullLo8(row[4], row[4]);
      BoxFilterPreProcess4<5, 1>(row, row_sq, scale, ab_ptr);
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
      row[3] = row[4] = LoadLo8(column);
      row_sq[3] = row_sq[4] = VmullLo8(row[3], row[3]);
      BoxFilterPreProcess4<5, 1>(row, row_sq, scale, ab_ptr);
    }
  }

  const int w0 = restoration_info.sgr_proj_info.multiplier[0];
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
    __m128i ma[2], b[2], ma565[2], b565[2][2];
    ab_ptr = temp;
    ma[0] = b[0] = LoadAligned16(ab_ptr);
    const uint8_t* column = src_pre_process + x;
    __m128i row[5], row_sq[5][2];
    // Need |width| + 3 pixels, but we read max(|x|) + 16 pixels.
    // Mask max(|x|) + 13 - |width| extra pixels.
    row[0] = row[1] = LoadUnaligned16Msan(column, x + 13 - width);
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 13 - width);
    column += src_stride;
    row[3] = LoadUnaligned16Msan(column, x + 13 - width);
    column += src_stride;
    row[4] = LoadUnaligned16Msan(column, x + 13 - width);
    row_sq[0][0] = row_sq[1][0] = VmullLo8(row[1], row[1]);
    row_sq[0][1] = row_sq[1][1] = VmullHi8(row[1], row[1]);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);
    row_sq[3][0] = VmullLo8(row[3], row[3]);
    row_sq[3][1] = VmullHi8(row[3], row[3]);
    row_sq[4][0] = VmullLo8(row[4], row[4]);
    row_sq[4][1] = VmullHi8(row[4], row[4]);
    BoxFilterPreProcess8<5>(row, row_sq, scale, &ma[0], &b[1], ab_ptr);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    ma565[0] = Sum565(ma[0]);
    b565[0][0] = Sum565W(_mm_alignr_epi8(b[1], b[0], 8));
    b565[0][1] = Sum565W(b[1]);
    uint8_t* dst_ptr = dst + x;
    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    for (int y = height >> 1; y != 0; --y) {
      ab_ptr += 8;
      ma[0] = b[0] = LoadAligned16(ab_ptr);
      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];
      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];
      column += src_stride;
      row[3] = LoadUnaligned16Msan(column, x + 13 - width);
      column += src_stride;
      row[4] = LoadUnaligned16Msan(column, x + 13 - width);
      row_sq[3][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = VmullHi8(row[3], row[3]);
      row_sq[4][0] = VmullLo8(row[4], row[4]);
      row_sq[4][1] = VmullHi8(row[4], row[4]);
      BoxFilterPreProcess8<5>(row, row_sq, scale, &ma[0], &b[1], ab_ptr);
      const __m128i p0 = BoxFilterPass1(row[1], ma[0], b, ma565, b565);
      SelfGuidedSingleMultiplier(row[1], p0, w0, dst_ptr);
      dst_ptr += dst_stride;
      const __m128i p1 = CalculateFilteredOutput<4>(row[2], ma565[1], b565[1]);
      SelfGuidedSingleMultiplier(row[2], p1, w0, dst_ptr);
      dst_ptr += dst_stride;
      ma565[0] = ma565[1];
      b565[0][0] = b565[1][0], b565[0][1] = b565[1][1];
    }

    if ((height & 1) != 0) {
      ab_ptr += 8;
      ma[0] = b[0] = LoadAligned16(ab_ptr);
      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];
      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];
      column += src_stride;
      row[3] = row[4] = LoadUnaligned16Msan(column, x + 13 - width);
      row_sq[3][0] = row_sq[4][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = row_sq[4][1] = VmullHi8(row[3], row[3]);
      BoxFilterPreProcess8<5>(row, row_sq, scale, &ma[0], &b[1], ab_ptr);
      const __m128i p0 = BoxFilterPass1(row[1], ma[0], b, ma565, b565);
      SelfGuidedSingleMultiplier(row[1], p0, w0, dst_ptr);
    }
    x += 8;
  } while (x < width);
}

inline void BoxFilterProcessPass2(const uint8_t* src,
                                  const ptrdiff_t src_stride,
                                  const RestorationUnitInfo& restoration_info,
                                  const int width, const int height,
                                  const uint32_t scale, uint16_t* const temp,
                                  uint8_t* const dst,
                                  const ptrdiff_t dst_stride) {
  // Calculate intermediate results, including one-pixel border, for example, if
  // unit size is 64x64, we calculate 66x66 pixels.
  // Because of the vectors this calculates start in blocks of 4 so we actually
  // get 68 values.
  uint16_t* ab_ptr = temp;
  const uint8_t* const src_pre_process = src - 2 * src_stride;
  {
    const uint8_t* column = src_pre_process - 3;
    __m128i row[3], row_sq[3];
    row[0] = LoadLo8(column);
    column += src_stride;
    row[1] = LoadLo8(column);
    row_sq[0] = VmullLo8(row[0], row[0]);
    row_sq[1] = VmullLo8(row[1], row[1]);
    int y = height + 2;
    do {
      column += src_stride;
      row[2] = LoadLo8(column);
      row_sq[2] = VmullLo8(row[2], row[2]);
      BoxFilterPreProcess4<3, 0>(row, row_sq, scale, ab_ptr);
      row[0] = row[1];
      row[1] = row[2];
      row_sq[0] = row_sq[1];
      row_sq[1] = row_sq[2];
      ab_ptr += 8;
    } while (--y != 0);
  }

  assert(restoration_info.sgr_proj_info.multiplier[0] == 0);
  const int w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int w0 = (1 << kSgrProjPrecisionBits) - w1;
  int x = 0;
  do {
    ab_ptr = temp;
    __m128i ma, b[2], ma343[3], ma444[2], b343[3][2], b444[2][2];
    ma = b[0] = LoadAligned16(ab_ptr);
    const uint8_t* column = src_pre_process + x;
    __m128i row[3], row_sq[3][2];
    // Need |width| + 2 pixels, but we read max(|x|) + 16 pixels.
    // Mask max(|x|) + 14 - |width| extra pixels.
    row[0] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[1] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 14 - width);
    row_sq[0][0] = VmullLo8(row[0], row[0]);
    row_sq[0][1] = VmullHi8(row[0], row[0]);
    row_sq[1][0] = VmullLo8(row[1], row[1]);
    row_sq[1][1] = VmullHi8(row[1], row[1]);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);
    BoxFilterPreProcess8<3>(row, row_sq, scale, &ma, &b[1], ab_ptr);
    ma343[0] = Sum343(ma);
    Sum343W(b, b343[0]);
    ab_ptr += 8;
    ma = b[0] = LoadAligned16(ab_ptr);
    row[0] = row[1];
    row[1] = row[2];
    row_sq[0][0] = row_sq[1][0], row_sq[0][1] = row_sq[1][1];
    row_sq[1][0] = row_sq[2][0], row_sq[1][1] = row_sq[2][1];
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 14 - width);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);
    BoxFilterPreProcess8<3>(row, row_sq, scale, &ma, &b[1], ab_ptr);
    Sum343_444(ma, &ma343[1], &ma444[0]);
    Sum343_444W(b, b343[1], b444[0]);

    uint8_t* dst_ptr = dst + x;
    int y = height;
    do {
      ab_ptr += 8;
      ma = b[0] = LoadAligned16(ab_ptr);
      row[0] = row[1];
      row[1] = row[2];
      row_sq[0][0] = row_sq[1][0], row_sq[0][1] = row_sq[1][1];
      row_sq[1][0] = row_sq[2][0], row_sq[1][1] = row_sq[2][1];
      column += src_stride;
      row[2] = LoadUnaligned16Msan(column, x + 14 - width);
      row_sq[2][0] = VmullLo8(row[2], row[2]);
      row_sq[2][1] = VmullHi8(row[2], row[2]);
      BoxFilterPreProcess8<3>(row, row_sq, scale, &ma, &b[1], ab_ptr);
      const __m128i p = BoxFilterPass2(row[0], ma, b, ma343, ma444, b343, b444);
      SelfGuidedSingleMultiplier(row[0], p, w0, dst_ptr);
      ma343[0] = ma343[1];
      ma343[1] = ma343[2];
      ma444[0] = ma444[1];
      b343[0][0] = b343[1][0], b343[0][1] = b343[1][1];
      b343[1][0] = b343[2][0], b343[1][1] = b343[2][1];
      b444[0][0] = b444[1][0], b444[0][1] = b444[1][1];
      dst_ptr += dst_stride;
    } while (--y != 0);
    x += 8;
  } while (x < width);
}

// If |width| is non-multiple of 8, up to 7 more pixels are written to |dest| in
// the end of each row. It is safe to overwrite the output as it will not be
// part of the visible frame.
void SelfGuidedFilter_SSE4_1(const void* const source, void* const dest,
                             const RestorationUnitInfo& restoration_info,
                             const ptrdiff_t source_stride,
                             const ptrdiff_t dest_stride, const int width,
                             const int height,
                             RestorationBuffer* const buffer) {
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
                          kSgrScaleParameter[index][0],
                          buffer->sgr_buffer.temp_buffer, dst, dest_stride);
  } else if (radius_pass_0 == 0) {
    BoxFilterProcessPass2(src, source_stride, restoration_info, width, height,
                          kSgrScaleParameter[index][1],
                          buffer->sgr_buffer.temp_buffer, dst, dest_stride);
  } else {
    BoxFilterProcess(src, source_stride, restoration_info, width, height,
                     kSgrScaleParameter[index], buffer->sgr_buffer.temp_buffer,
                     dst, dest_stride);
  }
}

void Init8bpp() {
  Dsp* const dsp = dsp_internal::GetWritableDspTable(kBitdepth8);
  assert(dsp != nullptr);
#if DSP_ENABLED_8BPP_SSE4_1(WienerFilter)
  dsp->loop_restorations[0] = WienerFilter_SSE4_1;
#endif
#if DSP_ENABLED_8BPP_SSE4_1(SelfGuidedFilter)
  dsp->loop_restorations[1] = SelfGuidedFilter_SSE4_1;
#endif
}

}  // namespace
}  // namespace low_bitdepth

void LoopRestorationInit_SSE4_1() { low_bitdepth::Init8bpp(); }

}  // namespace dsp
}  // namespace libgav1

#else  // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
