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
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace dsp {
namespace low_bitdepth {
namespace {

// Note: range of wiener filter coefficients.
// Wiener filter coefficients are symmetric, and their sum is 1 (128).
// The range of each coefficient:
// filter[0] = filter[6], 4 bits, min = -5, max = 10.
// filter[1] = filter[5], 5 bits, min = -23, max = 8.
// filter[2] = filter[4], 6 bits, min = -17, max = 46.
// filter[3] = 128 - (filter[0] + filter[1] + filter[2]) * 2.
// int8_t is used for the sse4 code, so in order to fit in an int8_t, the 128
// offset must be removed from filter[3].
// filter[3] = 0 - (filter[0] + filter[1] + filter[2]) * 2.
// The 128 offset will be added back in the loop.
inline void PopulateWienerCoefficients(
    const RestorationUnitInfo& restoration_info, int direction,
    int8_t* const filter) {
  filter[3] = 0;
  for (int i = 0; i < 3; ++i) {
    const int8_t coeff = restoration_info.wiener_info.filter[direction][i];
    filter[i] = coeff;
    filter[6 - i] = coeff;
    filter[3] -= MultiplyBy2(coeff);
  }

  // The Wiener filter has only 7 coefficients, but we run it as an 8-tap
  // filter in SIMD. The 8th coefficient of the filter must be set to 0.
  filter[7] = 0;
}

// This function calls LoadUnaligned16() to read 10 bytes from the |source|
// buffer. Since the LoadUnaligned16() call over-reads 6 bytes, the |source|
// buffer must be at least (height + kSubPixelTaps - 2) * source_stride + 6
// bytes long.
void WienerFilter_SSE4_1(const void* source, void* const dest,
                         const RestorationUnitInfo& restoration_info,
                         ptrdiff_t source_stride, ptrdiff_t dest_stride,
                         int width, int height,
                         RestorationBuffer* const buffer) {
  int8_t filter[kSubPixelTaps];
  const int limit =
      (1 << (8 + 1 + kWienerFilterBits - kInterRoundBitsHorizontal)) - 1;
  const auto* src = static_cast<const uint8_t*>(source);
  auto* dst = static_cast<uint8_t*>(dest);
  const ptrdiff_t buffer_stride = (width + 7) & ~7;
  auto* wiener_buffer = buffer->wiener_buffer + buffer_stride;
  // horizontal filtering.
  PopulateWienerCoefficients(restoration_info, WienerInfo::kHorizontal, filter);
  const int center_tap = 3;
  src -= (center_tap - 1) * source_stride + center_tap;

  const int horizontal_rounding =
      1 << (8 + kWienerFilterBits - kInterRoundBitsHorizontal - 1);
  const __m128i v_horizontal_rounding =
      _mm_shufflelo_epi16(_mm_cvtsi32_si128(horizontal_rounding), 0);
  const __m128i v_limit = _mm_shufflelo_epi16(_mm_cvtsi32_si128(limit), 0);
  const __m128i v_horizontal_filter = LoadLo8(filter);
  __m128i v_k1k0 = _mm_shufflelo_epi16(v_horizontal_filter, 0x0);
  __m128i v_k3k2 = _mm_shufflelo_epi16(v_horizontal_filter, 0x55);
  __m128i v_k5k4 = _mm_shufflelo_epi16(v_horizontal_filter, 0xaa);
  __m128i v_k7k6 = _mm_shufflelo_epi16(v_horizontal_filter, 0xff);
  const __m128i v_round_0 = _mm_shufflelo_epi16(
      _mm_cvtsi32_si128(1 << (kInterRoundBitsHorizontal - 1)), 0);
  const __m128i v_round_0_shift = _mm_cvtsi32_si128(kInterRoundBitsHorizontal);
  const __m128i v_offset_shift =
      _mm_cvtsi32_si128(7 - kInterRoundBitsHorizontal);

  int y = height + kSubPixelTaps - 4;
  do {
    int x = 0;
    do {
      // Run the Wiener filter on four sets of source samples at a time:
      //   src[x + 0] ... src[x + 6]
      //   src[x + 1] ... src[x + 7]
      //   src[x + 2] ... src[x + 8]
      //   src[x + 3] ... src[x + 9]

      // Read 10 bytes (from src[x] to src[x + 9]). We over-read 6 bytes but
      // their results are discarded.
      const __m128i v_src = LoadUnaligned16(&src[x]);
      const __m128i v_src_dup_lo = _mm_unpacklo_epi8(v_src, v_src);
      const __m128i v_src_dup_hi = _mm_unpackhi_epi8(v_src, v_src);
      const __m128i v_src_10 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 1);
      const __m128i v_src_32 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 5);
      const __m128i v_src_54 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 9);
      // Shift right by 12 bytes instead of 13 bytes so that src[x + 10] is not
      // shifted into the low 8 bytes of v_src_66.
      const __m128i v_src_66 = _mm_alignr_epi8(v_src_dup_hi, v_src_dup_lo, 12);
      const __m128i v_madd_10 = _mm_maddubs_epi16(v_src_10, v_k1k0);
      const __m128i v_madd_32 = _mm_maddubs_epi16(v_src_32, v_k3k2);
      const __m128i v_madd_54 = _mm_maddubs_epi16(v_src_54, v_k5k4);
      const __m128i v_madd_76 = _mm_maddubs_epi16(v_src_66, v_k7k6);
      const __m128i v_sum_3210 = _mm_add_epi16(v_madd_10, v_madd_32);
      const __m128i v_sum_7654 = _mm_add_epi16(v_madd_54, v_madd_76);
      // The sum range here is [-128 * 255, 90 * 255].
      const __m128i v_sum_76543210 = _mm_add_epi16(v_sum_7654, v_sum_3210);
      const __m128i v_sum = _mm_add_epi16(v_sum_76543210, v_round_0);
      const __m128i v_rounded_sum0 = _mm_sra_epi16(v_sum, v_round_0_shift);
      // Add scaled down horizontal round here to prevent signed 16 bit
      // outranging
      const __m128i v_rounded_sum1 =
          _mm_add_epi16(v_rounded_sum0, v_horizontal_rounding);
      // Zero out the even bytes, calculate scaled down offset correction, and
      // add to sum here to prevent signed 16 bit outranging.
      // (src[3] * 128) >> kInterRoundBitsHorizontal
      const __m128i v_src_3x128 =
          _mm_sll_epi16(_mm_srli_epi16(v_src_32, 8), v_offset_shift);
      const __m128i v_rounded_sum = _mm_add_epi16(v_rounded_sum1, v_src_3x128);
      const __m128i v_a = _mm_max_epi16(v_rounded_sum, _mm_setzero_si128());
      const __m128i v_b = _mm_min_epi16(v_a, v_limit);
      StoreLo8(&wiener_buffer[x], v_b);
      x += 4;
    } while (x < width);
    src += source_stride;
    wiener_buffer += buffer_stride;
  } while (--y != 0);
  // Because the top row of |source| is a duplicate of the second row, and the
  // bottom row of |source| is a duplicate of its above row, we can duplicate
  // the top and bottom row of |wiener_buffer| accordingly.
  memcpy(wiener_buffer, wiener_buffer - buffer_stride,
         sizeof(*wiener_buffer) * width);
  wiener_buffer = buffer->wiener_buffer;
  memcpy(wiener_buffer, wiener_buffer + buffer_stride,
         sizeof(*wiener_buffer) * width);

  // vertical filtering.
  PopulateWienerCoefficients(restoration_info, WienerInfo::kVertical, filter);

  const int vertical_rounding = -(1 << (8 + kInterRoundBitsVertical - 1));
  const __m128i v_vertical_rounding =
      _mm_shuffle_epi32(_mm_cvtsi32_si128(vertical_rounding), 0);
  const __m128i v_offset_correction = _mm_set_epi16(0, 0, 0, 0, 128, 0, 0, 0);
  const __m128i v_round_1 = _mm_shuffle_epi32(
      _mm_cvtsi32_si128(1 << (kInterRoundBitsVertical - 1)), 0);
  const __m128i v_round_1_shift = _mm_cvtsi32_si128(kInterRoundBitsVertical);
  const __m128i v_vertical_filter0 = _mm_cvtepi8_epi16(LoadLo8(filter));
  const __m128i v_vertical_filter =
      _mm_add_epi16(v_vertical_filter0, v_offset_correction);
  v_k1k0 = _mm_shuffle_epi32(v_vertical_filter, 0x0);
  v_k3k2 = _mm_shuffle_epi32(v_vertical_filter, 0x55);
  v_k5k4 = _mm_shuffle_epi32(v_vertical_filter, 0xaa);
  v_k7k6 = _mm_shuffle_epi32(v_vertical_filter, 0xff);
  y = 0;
  do {
    int x = 0;
    do {
      const __m128i v_wb_0 = LoadLo8(&wiener_buffer[0 * buffer_stride + x]);
      const __m128i v_wb_1 = LoadLo8(&wiener_buffer[1 * buffer_stride + x]);
      const __m128i v_wb_2 = LoadLo8(&wiener_buffer[2 * buffer_stride + x]);
      const __m128i v_wb_3 = LoadLo8(&wiener_buffer[3 * buffer_stride + x]);
      const __m128i v_wb_4 = LoadLo8(&wiener_buffer[4 * buffer_stride + x]);
      const __m128i v_wb_5 = LoadLo8(&wiener_buffer[5 * buffer_stride + x]);
      const __m128i v_wb_6 = LoadLo8(&wiener_buffer[6 * buffer_stride + x]);
      const __m128i v_wb_10 = _mm_unpacklo_epi16(v_wb_0, v_wb_1);
      const __m128i v_wb_32 = _mm_unpacklo_epi16(v_wb_2, v_wb_3);
      const __m128i v_wb_54 = _mm_unpacklo_epi16(v_wb_4, v_wb_5);
      const __m128i v_wb_76 = _mm_unpacklo_epi16(v_wb_6, _mm_setzero_si128());
      const __m128i v_madd_10 = _mm_madd_epi16(v_wb_10, v_k1k0);
      const __m128i v_madd_32 = _mm_madd_epi16(v_wb_32, v_k3k2);
      const __m128i v_madd_54 = _mm_madd_epi16(v_wb_54, v_k5k4);
      const __m128i v_madd_76 = _mm_madd_epi16(v_wb_76, v_k7k6);
      const __m128i v_sum_3210 = _mm_add_epi32(v_madd_10, v_madd_32);
      const __m128i v_sum_7654 = _mm_add_epi32(v_madd_54, v_madd_76);
      const __m128i v_sum_76543210 = _mm_add_epi32(v_sum_7654, v_sum_3210);
      const __m128i v_sum = _mm_add_epi32(v_sum_76543210, v_vertical_rounding);
      const __m128i v_rounded_sum =
          _mm_sra_epi32(_mm_add_epi32(v_sum, v_round_1), v_round_1_shift);
      const __m128i v_a = _mm_packs_epi32(v_rounded_sum, v_rounded_sum);
      const __m128i v_b = _mm_packus_epi16(v_a, v_a);
      Store4(&dst[x], v_b);
      x += 4;
    } while (x < width);
    dst += dest_stride;
    wiener_buffer += buffer_stride;
  } while (++y < height);
}

//------------------------------------------------------------------------------
// SGR

// Don't use _mm_cvtepu8_epi16() or _mm_cvtepu16_epi32() in the following
// functions. Some compilers may generate super inefficient code and the whole
// decoder could be 15% slower.

inline __m128i VaddlLo8(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpacklo_epi8(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpacklo_epi8(b, _mm_setzero_si128());
  return _mm_add_epi16(a0, b0);
}

inline __m128i VaddlHi8(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpackhi_epi8(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpackhi_epi8(b, _mm_setzero_si128());
  return _mm_add_epi16(a0, b0);
}

inline __m128i VaddlLo16(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpacklo_epi16(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpacklo_epi16(b, _mm_setzero_si128());
  return _mm_add_epi32(a0, b0);
}

inline __m128i VaddlHi16(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpackhi_epi16(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpackhi_epi16(b, _mm_setzero_si128());
  return _mm_add_epi32(a0, b0);
}

inline __m128i VaddwLo8(const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpacklo_epi8(b, _mm_setzero_si128());
  return _mm_add_epi16(a, b0);
}

inline __m128i VaddwHi8(const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpackhi_epi8(b, _mm_setzero_si128());
  return _mm_add_epi16(a, b0);
}

inline __m128i VaddwLo16(const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpacklo_epi16(b, _mm_setzero_si128());
  return _mm_add_epi32(a, b0);
}

inline __m128i VaddwHi16(const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpackhi_epi16(b, _mm_setzero_si128());
  return _mm_add_epi32(a, b0);
}

// Using VgetLane16() can save a sign extension instruction.
template <int n>
inline int16_t VgetLane16(const __m128i a) {
  return _mm_extract_epi16(a, n);
}

inline __m128i VmullLo8(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpacklo_epi8(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpacklo_epi8(b, _mm_setzero_si128());
  return _mm_mullo_epi16(a0, b0);
}

inline __m128i VmullHi8(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpackhi_epi8(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpackhi_epi8(b, _mm_setzero_si128());
  return _mm_mullo_epi16(a0, b0);
}

inline __m128i VmullNLo8(const __m128i a, const int16_t b) {
  const __m128i a0 = _mm_unpacklo_epi16(a, _mm_setzero_si128());
  return _mm_madd_epi16(a0, _mm_set1_epi32(b));
}

inline __m128i VmullNHi8(const __m128i a, const int16_t b) {
  const __m128i a0 = _mm_unpackhi_epi16(a, _mm_setzero_si128());
  return _mm_madd_epi16(a0, _mm_set1_epi32(b));
}

inline __m128i VmullLo16(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpacklo_epi16(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpacklo_epi16(b, _mm_setzero_si128());
  return _mm_madd_epi16(a0, b0);
}

inline __m128i VmullHi16(const __m128i a, const __m128i b) {
  const __m128i a0 = _mm_unpackhi_epi16(a, _mm_setzero_si128());
  const __m128i b0 = _mm_unpackhi_epi16(b, _mm_setzero_si128());
  return _mm_madd_epi16(a0, b0);
}

inline __m128i VmulwLo16(const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpacklo_epi16(b, _mm_setzero_si128());
  return _mm_madd_epi16(a, b0);
}

inline __m128i VmulwHi16(const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpackhi_epi16(b, _mm_setzero_si128());
  return _mm_madd_epi16(a, b0);
}

inline __m128i VmlalNLo16(const __m128i sum, const __m128i a, const int16_t b) {
  return _mm_add_epi32(sum, VmullNLo8(a, b));
}

inline __m128i VmlalNHi16(const __m128i sum, const __m128i a, const int16_t b) {
  return _mm_add_epi32(sum, VmullNHi8(a, b));
}

inline __m128i VmlawLo16(const __m128i sum, const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpacklo_epi16(b, _mm_setzero_si128());
  return _mm_add_epi32(sum, _mm_madd_epi16(a, b0));
}

inline __m128i VmlawHi16(const __m128i sum, const __m128i a, const __m128i b) {
  const __m128i b0 = _mm_unpackhi_epi16(b, _mm_setzero_si128());
  return _mm_add_epi32(sum, _mm_madd_epi16(a, b0));
}

inline __m128i VrshrNS32(const __m128i a, const int b) {
  const __m128i sum = _mm_add_epi32(a, _mm_set1_epi32(1 << (b - 1)));
  return _mm_srai_epi32(sum, b);
}

inline __m128i VrshrN32(const __m128i a, const int b) {
  const __m128i sum = _mm_add_epi32(a, _mm_set1_epi32(1 << (b - 1)));
  return _mm_srli_epi32(sum, b);
}

inline __m128i VshllN8(const __m128i a, const int b) {
  const __m128i a0 = _mm_unpacklo_epi8(a, _mm_setzero_si128());
  return _mm_slli_epi16(a0, b);
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
inline __m128i CalculateSgrMA2(const __m128i sum_sq, const __m128i sum,
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
  return VrshrN32(pxs, kSgrProjScaleBits);
}

inline __m128i CalculateIntermediate4(const __m128i sgr_ma2, const __m128i sum,
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
  const __m128i sgr_ma2q = _mm_unpacklo_epi8(sgr_ma2, _mm_setzero_si128());
  const __m128i s = _mm_unpackhi_epi16(sgr_ma2q, _mm_setzero_si128());
  const __m128i m = _mm_madd_epi16(s, sum);
  const __m128i b2 = _mm_mullo_epi32(m, _mm_set1_epi32(one_over_n));
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  const __m128i truncate_u32 = VrshrN32(b2, kSgrProjReciprocalBits);
  return _mm_packus_epi32(truncate_u32, truncate_u32);
}

inline __m128i CalculateIntermediate8(const __m128i sgr_ma2, const __m128i sum,
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
  const __m128i sgr_ma2q = _mm_unpackhi_epi8(sgr_ma2, _mm_setzero_si128());
  const __m128i m0 = VmullLo16(sgr_ma2q, sum);
  const __m128i m1 = VmullHi16(sgr_ma2q, sum);
  const __m128i m2 = _mm_mullo_epi32(m0, _mm_set1_epi32(one_over_n));
  const __m128i m3 = _mm_mullo_epi32(m1, _mm_set1_epi32(one_over_n));
  // static_cast<int>(RightShiftWithRounding(b2, kSgrProjReciprocalBits));
  // |kSgrProjReciprocalBits| is 12.
  // Radius 2: 255 * 6375 * 164 >> 12 = 65088 (16 bits).
  // Radius 1: 255 * 2295 * 455 >> 12 = 65009 (16 bits).
  const __m128i b2_lo = VrshrN32(m2, kSgrProjReciprocalBits);
  const __m128i b2_hi = VrshrN32(m3, kSgrProjReciprocalBits);
  return _mm_packus_epi32(b2_lo, b2_hi);
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

inline __m128i Sum3WLo_16(const __m128i a[3]) {
  return Sum3W_16(a[0], a[1], a[2]);
}

inline __m128i Sum3WHi_16(const __m128i a[3]) {
  const __m128i sum = VaddlHi8(a[0], a[1]);
  return VaddwHi8(sum, a[2]);
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

inline __m128i* Sum3W_16x2(const __m128i a[3], __m128i sum[2]) {
  sum[0] = Sum3WLo_16(a);
  sum[1] = Sum3WHi_16(a);
  return sum;
}

inline __m128i* Sum3W(const __m128i a[3], __m128i sum[2]) {
  sum[0] = Sum3WLo_32(a[0], a[1], a[2]);
  sum[1] = Sum3WHi_32(a[0], a[1], a[2]);
  return sum;
}

template <int index>
inline __m128i Sum3WLo(const __m128i a[3][2]) {
  const __m128i b0 = a[0][index];
  const __m128i b1 = a[1][index];
  const __m128i b2 = a[2][index];
  return Sum3WLo_32(b0, b1, b2);
}

inline __m128i Sum3WHi(const __m128i a[3][2]) {
  const __m128i b0 = a[0][0];
  const __m128i b1 = a[1][0];
  const __m128i b2 = a[2][0];
  return Sum3WHi_32(b0, b1, b2);
}

inline __m128i* Sum3W(const __m128i a[3][2], __m128i sum[3]) {
  sum[0] = Sum3WLo<0>(a);
  sum[1] = Sum3WHi(a);
  sum[2] = Sum3WLo<1>(a);
  return sum;
}

inline __m128i Sum5_16(const __m128i a[5]) {
  const __m128i sum01 = _mm_add_epi16(a[0], a[1]);
  const __m128i sum23 = _mm_add_epi16(a[2], a[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return _mm_add_epi16(sum, a[4]);
}

inline __m128i Sum5_32(const __m128i a[5]) {
  const __m128i sum01 = _mm_add_epi32(a[0], a[1]);
  const __m128i sum23 = _mm_add_epi32(a[2], a[3]);
  const __m128i sum = _mm_add_epi32(sum01, sum23);
  return _mm_add_epi32(sum, a[4]);
}

inline __m128i Sum5WLo_16(const __m128i a[5]) {
  const __m128i sum01 = VaddlLo8(a[0], a[1]);
  const __m128i sum23 = VaddlLo8(a[2], a[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwLo8(sum, a[4]);
}

inline __m128i Sum5WHi_16(const __m128i a[5]) {
  const __m128i sum01 = VaddlHi8(a[0], a[1]);
  const __m128i sum23 = VaddlHi8(a[2], a[3]);
  const __m128i sum = _mm_add_epi16(sum01, sum23);
  return VaddwHi8(sum, a[4]);
}

inline __m128i Sum5WLo_32(const __m128i a[5]) {
  const __m128i sum01 = VaddlLo16(a[0], a[1]);
  const __m128i sum23 = VaddlLo16(a[2], a[3]);
  const __m128i sum0123 = _mm_add_epi32(sum01, sum23);
  return VaddwLo16(sum0123, a[4]);
}

inline __m128i Sum5WHi_32(const __m128i a[5]) {
  const __m128i sum01 = VaddlHi16(a[0], a[1]);
  const __m128i sum23 = VaddlHi16(a[2], a[3]);
  const __m128i sum0123 = _mm_add_epi32(sum01, sum23);
  return VaddwHi16(sum0123, a[4]);
}

inline __m128i* Sum5W_16D(const __m128i a[5], __m128i sum[2]) {
  sum[0] = Sum5WLo_16(a);
  sum[1] = Sum5WHi_16(a);
  return sum;
}

inline __m128i* Sum5W_32x2(const __m128i a[5], __m128i sum[2]) {
  sum[0] = Sum5WLo_32(a);
  sum[1] = Sum5WHi_32(a);
  return sum;
}

template <int index>
inline __m128i Sum5WLo(const __m128i a[5][2]) {
  __m128i b[5];
  b[0] = a[0][index];
  b[1] = a[1][index];
  b[2] = a[2][index];
  b[3] = a[3][index];
  b[4] = a[4][index];
  return Sum5WLo_32(b);
}

inline __m128i Sum5WHi(const __m128i a[5][2]) {
  __m128i b[5];
  b[0] = a[0][0];
  b[1] = a[1][0];
  b[2] = a[2][0];
  b[3] = a[3][0];
  b[4] = a[4][0];
  return Sum5WHi_32(b);
}

inline __m128i* Sum5W_32x3(const __m128i a[5][2], __m128i sum[3]) {
  sum[0] = Sum5WLo<0>(a);
  sum[1] = Sum5WHi(a);
  sum[2] = Sum5WLo<1>(a);
  return sum;
}

inline __m128i Sum3Horizontal(const __m128i a) {
  const auto left = a;
  const auto middle = _mm_srli_si128(a, 2);
  const auto right = _mm_srli_si128(a, 4);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3Horizontal_16(const __m128i a[2]) {
  const auto left = a[0];
  const auto middle = _mm_alignr_epi8(a[1], a[0], 2);
  const auto right = _mm_alignr_epi8(a[1], a[0], 4);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3Horizontal_32(const __m128i a[2]) {
  const auto left = a[0];
  const auto middle = _mm_alignr_epi8(a[1], a[0], 4);
  const auto right = _mm_alignr_epi8(a[1], a[0], 8);
  return Sum3_32(left, middle, right);
}

inline __m128i* Sum3Horizontal_32x2(const __m128i a[3], __m128i sum[2]) {
  {
    const auto left = a[0];
    const auto middle = _mm_alignr_epi8(a[1], a[0], 4);
    const auto right = _mm_alignr_epi8(a[1], a[0], 8);
    sum[0] = Sum3_32(left, middle, right);
  }
  {
    const auto left = a[1];
    const auto middle = _mm_alignr_epi8(a[2], a[1], 4);
    const auto right = _mm_alignr_epi8(a[2], a[1], 8);
    sum[1] = Sum3_32(left, middle, right);
  }
  return sum;
}

inline __m128i Sum3HorizontalOffset1(const __m128i a) {
  const auto left = _mm_srli_si128(a, 2);
  const auto middle = _mm_srli_si128(a, 4);
  const auto right = _mm_srli_si128(a, 6);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3HorizontalOffset1_16(const __m128i a[2]) {
  const auto left = _mm_alignr_epi8(a[1], a[0], 2);
  const auto middle = _mm_alignr_epi8(a[1], a[0], 4);
  const auto right = _mm_alignr_epi8(a[1], a[0], 6);
  return Sum3_16(left, middle, right);
}

inline __m128i Sum3HorizontalOffset1_32(const __m128i a[2]) {
  const auto left = _mm_alignr_epi8(a[1], a[0], 4);
  const auto middle = _mm_alignr_epi8(a[1], a[0], 8);
  const auto right = _mm_alignr_epi8(a[1], a[0], 12);
  return Sum3_32(left, middle, right);
}

inline void Sum3HorizontalOffset1_32x2(const __m128i a[3], __m128i sum[2]) {
  sum[0] = Sum3HorizontalOffset1_32(a + 0);
  sum[1] = Sum3HorizontalOffset1_32(a + 1);
}

inline __m128i Sum5Horizontal(const __m128i a) {
  __m128i s[5];
  s[0] = a;
  s[1] = _mm_srli_si128(a, 2);
  s[2] = _mm_srli_si128(a, 4);
  s[3] = _mm_srli_si128(a, 6);
  s[4] = _mm_srli_si128(a, 8);
  return Sum5_16(s);
}

inline __m128i Sum5Horizontal_16(const __m128i a[2]) {
  __m128i s[5];
  s[0] = a[0];
  s[1] = _mm_alignr_epi8(a[1], a[0], 2);
  s[2] = _mm_alignr_epi8(a[1], a[0], 4);
  s[3] = _mm_alignr_epi8(a[1], a[0], 6);
  s[4] = _mm_alignr_epi8(a[1], a[0], 8);
  return Sum5_16(s);
}

inline __m128i Sum5Horizontal_32(const __m128i a[2]) {
  __m128i s[5];
  s[0] = a[0];
  s[1] = _mm_alignr_epi8(a[1], a[0], 4);
  s[2] = _mm_alignr_epi8(a[1], a[0], 8);
  s[3] = _mm_alignr_epi8(a[1], a[0], 12);
  s[4] = a[1];
  return Sum5_32(s);
}

inline __m128i* Sum5Horizontal_32x2(const __m128i a[3], __m128i sum[2]) {
  __m128i s[5];
  s[0] = a[0];
  s[1] = _mm_alignr_epi8(a[1], a[0], 4);
  s[2] = _mm_alignr_epi8(a[1], a[0], 8);
  s[3] = _mm_alignr_epi8(a[1], a[0], 12);
  s[4] = a[1];
  sum[0] = Sum5_32(s);
  s[0] = a[1];
  s[1] = _mm_alignr_epi8(a[2], a[1], 4);
  s[2] = _mm_alignr_epi8(a[2], a[1], 8);
  s[3] = _mm_alignr_epi8(a[2], a[1], 12);
  s[4] = a[2];
  sum[1] = Sum5_32(s);
  return sum;
}

template <int size, int offset>
inline void BoxFilterPreProcess4(const __m128i* const row,
                                 const __m128i* const row_sq, const uint32_t s,
                                 uint16_t* const dst) {
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
  const __m128i z0 = CalculateSgrMA2<n>(sum_sq, sum_32, s);
  const __m128i z1 = _mm_packus_epi32(z0, z0);
  const __m128i z = _mm_min_epu16(z1, _mm_set1_epi16(255));
  __m128i sgr_ma2 = _mm_setzero_si128();
  sgr_ma2 = _mm_insert_epi8(sgr_ma2, kSgrMa2Lookup[VgetLane16<0>(z)], 4);
  sgr_ma2 = _mm_insert_epi8(sgr_ma2, kSgrMa2Lookup[VgetLane16<1>(z)], 5);
  sgr_ma2 = _mm_insert_epi8(sgr_ma2, kSgrMa2Lookup[VgetLane16<2>(z)], 6);
  sgr_ma2 = _mm_insert_epi8(sgr_ma2, kSgrMa2Lookup[VgetLane16<3>(z)], 7);
  const __m128i b2 = CalculateIntermediate4(sgr_ma2, sum_32, one_over_n);
  const __m128i sgr_ma2_b2 = _mm_unpacklo_epi64(sgr_ma2, b2);
  StoreAligned16(dst, sgr_ma2_b2);
}

template <int size, int offset>
inline void BoxFilterPreProcess8(const __m128i* const row,
                                 const __m128i row_sq[][2], const uint32_t s,
                                 __m128i* const sgr_ma2, __m128i* const b2,
                                 uint16_t* const dst) {
  // Number of elements in the box being summed.
  constexpr uint32_t n = size * size;
  constexpr uint32_t one_over_n =
      ((1 << kSgrProjReciprocalBits) + (n >> 1)) / n;
  __m128i sum, sum_sq[2];
  if (size == 3) {
    __m128i temp16[2], temp32[3];
    if (offset == 0) {
      sum = Sum3Horizontal_16(Sum3W_16x2(row, temp16));
      Sum3Horizontal_32x2(Sum3W(row_sq, temp32), sum_sq);
    } else /* if (offset == 1) */ {
      sum = Sum3HorizontalOffset1_16(Sum3W_16x2(row, temp16));
      Sum3HorizontalOffset1_32x2(Sum3W(row_sq, temp32), sum_sq);
    }
  }
  if (size == 5) {
    __m128i temp16[2], temp32[3];
    sum = Sum5Horizontal_16(Sum5W_16D(row, temp16));
    Sum5Horizontal_32x2(Sum5W_32x3(row_sq, temp32), sum_sq);
  }
  const __m128i sum_lo = _mm_unpacklo_epi16(sum, _mm_setzero_si128());
  const __m128i z0 = CalculateSgrMA2<n>(sum_sq[0], sum_lo, s);
  const __m128i sum_hi = _mm_unpackhi_epi16(sum, _mm_setzero_si128());
  const __m128i z1 = CalculateSgrMA2<n>(sum_sq[1], sum_hi, s);
  const __m128i z01 = _mm_packus_epi32(z0, z1);
  const __m128i z = _mm_min_epu16(z01, _mm_set1_epi16(255));
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<0>(z)], 8);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<1>(z)], 9);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<2>(z)], 10);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<3>(z)], 11);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<4>(z)], 12);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<5>(z)], 13);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<6>(z)], 14);
  *sgr_ma2 = _mm_insert_epi8(*sgr_ma2, kSgrMa2Lookup[VgetLane16<7>(z)], 15);
  *b2 = CalculateIntermediate8(*sgr_ma2, sum, one_over_n);
  const __m128i sgr_ma2_b2 = _mm_unpackhi_epi64(*sgr_ma2, *b2);
  StoreAligned16(dst, sgr_ma2_b2);
}

inline void Prepare3_8(const __m128i a, __m128i* const left,
                       __m128i* const middle, __m128i* const right) {
  *left = _mm_srli_si128(a, 4);
  *middle = _mm_srli_si128(a, 5);
  *right = _mm_srli_si128(a, 6);
}

inline void Prepare3_16(const __m128i a[2], __m128i* const left,
                        __m128i* const middle, __m128i* const right) {
  *left = _mm_alignr_epi8(a[1], a[0], 8);
  *middle = _mm_alignr_epi8(a[1], a[0], 10);
  *right = _mm_alignr_epi8(a[1], a[0], 12);
}

inline __m128i Sum343(const __m128i a) {
  __m128i left, middle, right;
  Prepare3_8(a, &left, &middle, &right);
  const auto sum = Sum3W_16(left, middle, right);
  const auto sum3 = Sum3_16(sum, sum, sum);
  return VaddwLo8(sum3, middle);
}

inline void Sum343_444(const __m128i a, __m128i* const sum343,
                       __m128i* const sum444) {
  __m128i left, middle, right;
  Prepare3_8(a, &left, &middle, &right);
  const auto sum = Sum3W_16(left, middle, right);
  const auto sum3 = Sum3_16(sum, sum, sum);
  *sum343 = VaddwLo8(sum3, middle);
  *sum444 = _mm_slli_epi16(sum, 2);
}

inline __m128i* Sum343W(const __m128i a[2], __m128i d[2]) {
  __m128i left, middle, right;
  Prepare3_16(a, &left, &middle, &right);
  d[0] = Sum3WLo_32(left, middle, right);
  d[1] = Sum3WHi_32(left, middle, right);
  d[0] = Sum3_32(d[0], d[0], d[0]);
  d[1] = Sum3_32(d[1], d[1], d[1]);
  d[0] = VaddwLo16(d[0], middle);
  d[1] = VaddwHi16(d[1], middle);
  return d;
}

inline void Sum343_444W(const __m128i a[2], __m128i sum343[2],
                        __m128i sum444[2]) {
  __m128i left, middle, right;
  Prepare3_16(a, &left, &middle, &right);
  sum444[0] = Sum3WLo_32(left, middle, right);
  sum444[1] = Sum3WHi_32(left, middle, right);
  sum343[0] = Sum3_32(sum444[0], sum444[0], sum444[0]);
  sum343[1] = Sum3_32(sum444[1], sum444[1], sum444[1]);
  sum343[0] = VaddwLo16(sum343[0], middle);
  sum343[1] = VaddwHi16(sum343[1], middle);
  sum444[0] = _mm_slli_epi32(sum444[0], 2);
  sum444[1] = _mm_slli_epi32(sum444[1], 2);
}

inline __m128i Sum565(const __m128i a) {
  __m128i left, middle, right;
  Prepare3_8(a, &left, &middle, &right);
  const auto sum = Sum3W_16(left, middle, right);
  const auto sum4 = _mm_slli_epi16(sum, 2);
  const auto sum5 = _mm_add_epi16(sum4, sum);
  return VaddwLo8(sum5, middle);
}

inline __m128i Sum565W(const __m128i a) {
  const auto left = a;
  const auto middle = _mm_srli_si128(a, 2);
  const auto right = _mm_srli_si128(a, 4);
  const auto sum = Sum3WLo_32(left, middle, right);
  const auto sum4 = _mm_slli_epi32(sum, 2);
  const auto sum5 = _mm_add_epi32(sum4, sum);
  return VaddwLo16(sum5, middle);
}

// RightShiftWithRounding(
//   (a * src_ptr[x] + b), kSgrProjSgrBits + shift - kSgrProjRestoreBits);
template <int shift>
inline __m128i CalculateFilteredOutput(const __m128i src, const __m128i a,
                                       const __m128i b[2]) {
  const __m128i src_u16 = _mm_unpacklo_epi8(src, _mm_setzero_si128());
  // a: 256 * 32 = 8192 (14 bits)
  // b: 65088 * 32 = 2082816 (21 bits)
  const __m128i axsrc_lo = VmullLo16(a, src_u16);
  const __m128i axsrc_hi = VmullHi16(a, src_u16);
  // v: 8192 * 255 + 2082816 = 4171876 (22 bits)
  const __m128i v_lo = _mm_add_epi32(axsrc_lo, b[0]);
  const __m128i v_hi = _mm_add_epi32(axsrc_hi, b[1]);

  // kSgrProjSgrBits = 8
  // kSgrProjRestoreBits = 4
  // shift = 4 or 5
  // v >> 8 or 9
  // 22 bits >> 8 = 14 bits
  const __m128i dst_lo =
      VrshrN32(v_lo, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
  const __m128i dst_hi =
      VrshrN32(v_hi, kSgrProjSgrBits + shift - kSgrProjRestoreBits);
  return _mm_packus_epi32(dst_lo, dst_hi);  // 14 bits
}

inline __m128i BoxFilterPass1(const __m128i src_u8, const __m128i a2,
                              const __m128i b2[2], __m128i sum565_a[2],
                              __m128i sum565_b[2][2]) {
  __m128i b_v[2];
  sum565_a[1] = Sum565(a2);
  sum565_a[1] = _mm_sub_epi16(_mm_set1_epi16((5 + 6 + 5) * 256), sum565_a[1]);
  sum565_b[1][0] = Sum565W(_mm_alignr_epi8(b2[1], b2[0], 8));
  sum565_b[1][1] = Sum565W(b2[1]);

  __m128i a_v = _mm_add_epi16(sum565_a[0], sum565_a[1]);
  b_v[0] = _mm_add_epi32(sum565_b[0][0], sum565_b[1][0]);
  b_v[1] = _mm_add_epi32(sum565_b[0][1], sum565_b[1][1]);
  return CalculateFilteredOutput<5>(src_u8, a_v, b_v);  // 14 bits
}

inline __m128i BoxFilterPass2(const __m128i src_u8, const __m128i a2,
                              const __m128i b2[2], __m128i sum343_a[4],
                              __m128i sum444_a[3], __m128i sum343_b[4][2],
                              __m128i sum444_b[3][2]) {
  __m128i b_v[2];
  Sum343_444(a2, &sum343_a[2], &sum444_a[1]);
  sum343_a[2] = _mm_sub_epi16(_mm_set1_epi16((3 + 4 + 3) * 256), sum343_a[2]);
  sum444_a[1] = _mm_sub_epi16(_mm_set1_epi16((4 + 4 + 4) * 256), sum444_a[1]);
  __m128i a_v = Sum3_16(sum343_a[0], sum444_a[0], sum343_a[2]);
  Sum343_444W(b2, sum343_b[2], sum444_b[1]);
  b_v[0] = Sum3_32(sum343_b[0][0], sum444_b[0][0], sum343_b[2][0]);
  b_v[1] = Sum3_32(sum343_b[0][1], sum444_b[0][1], sum343_b[2][1]);
  return CalculateFilteredOutput<5>(src_u8, a_v, b_v);  // 14 bits
}

inline void SelfGuidedDoubleMultiplier(
    const __m128i src, const __m128i box_filter_process_output[2],
    const __m128i w0, const __m128i w1, const __m128i w2, uint8_t* const dst) {
  // |wN| values are signed. |src| values can be treated as int16_t.
  const __m128i u = VshllN8(src, kSgrProjRestoreBits);
  __m128i v_lo = VmulwLo16(w1, u);
  v_lo = VmlawLo16(v_lo, w0, box_filter_process_output[0]);
  v_lo = VmlawLo16(v_lo, w2, box_filter_process_output[1]);
  __m128i v_hi = VmulwHi16(w1, u);
  v_hi = VmlawHi16(v_hi, w0, box_filter_process_output[0]);
  v_hi = VmlawHi16(v_hi, w2, box_filter_process_output[1]);
  // |s| is saturated to uint8_t.
  const __m128i s_lo =
      VrshrNS32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i s_hi =
      VrshrNS32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i s = _mm_packs_epi32(s_lo, s_hi);
  StoreLo8(dst, _mm_packus_epi16(s, s));
}

inline void SelfGuidedSingleMultiplier(const __m128i src,
                                       const __m128i box_filter_process_output,
                                       const int16_t w0, const int16_t w1,
                                       uint8_t* const dst) {
  // weight: -96 to 96 (Sgrproj_Xqd_Min/Max)
  const __m128i u = VshllN8(src, kSgrProjRestoreBits);
  // u * w1 + u * wN == u * (w1 + wN)
  __m128i v_lo = VmullNLo8(u, w1);
  v_lo = VmlalNLo16(v_lo, box_filter_process_output, w0);
  __m128i v_hi = VmullNHi8(u, w1);
  v_hi = VmlalNHi16(v_hi, box_filter_process_output, w0);
  const __m128i s_lo =
      VrshrNS32(v_lo, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i s_hi =
      VrshrNS32(v_hi, kSgrProjRestoreBits + kSgrProjPrecisionBits);
  const __m128i s = _mm_packs_epi32(s_lo, s_hi);
  StoreLo8(dst, _mm_packus_epi16(s, s));
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
    __m128i row[5], row_sq[5];
    row[0] = row[1] = LoadLo8Msan(column, 2 - width);
    column += src_stride;
    row[2] = LoadLo8Msan(column, 2 - width);

    row_sq[0] = row_sq[1] = VmullLo8(row[1], row[1]);
    row_sq[2] = VmullLo8(row[2], row[2]);

    int y = (height + 2) >> 1;
    do {
      column += src_stride;
      row[3] = LoadLo8Msan(column, 2 - width);
      column += src_stride;
      row[4] = LoadLo8Msan(column, 2 - width);

      row_sq[3] = VmullLo8(row[3], row[3]);
      row_sq[4] = VmullLo8(row[4], row[4]);

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
      row[3] = row[4] = LoadLo8Msan(column, 2 - width);
      row_sq[3] = row_sq[4] = VmullLo8(row[3], row[3]);
      BoxFilterPreProcess4<5, 0>(row + 0, row_sq + 0, s[0], ab_ptr + 0);
      BoxFilterPreProcess4<3, 1>(row + 1, row_sq + 1, s[1], ab_ptr + 8);
    }
  }

  const int16_t w0 = restoration_info.sgr_proj_info.multiplier[0];
  const int16_t w1 = restoration_info.sgr_proj_info.multiplier[1];
  const int16_t w2 = (1 << kSgrProjPrecisionBits) - w0 - w1;
  const __m128i w0_v = _mm_set1_epi32(w0);
  const __m128i w1_v = _mm_set1_epi32(w1);
  const __m128i w2_v = _mm_set1_epi32(w2);
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
    __m128i a2[2], b2[2][2], sum565_a[2], sum343_a[4], sum444_a[3];
    __m128i sum565_b[2][2], sum343_b[4][2], sum444_b[3][2];
    ab_ptr = temp;
    a2[0] = b2[0][0] = LoadAligned16(ab_ptr);
    a2[1] = b2[1][0] = LoadAligned16(ab_ptr + 8);

    const uint8_t* column = src_pre_process + x + 4;
    __m128i row[5], row_sq[5][2];
    row[0] = row[1] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[3] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[4] = LoadUnaligned16Msan(column, x + 14 - width);

    row_sq[0][0] = row_sq[1][0] = VmullLo8(row[1], row[1]);
    row_sq[0][1] = row_sq[1][1] = VmullHi8(row[1], row[1]);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);
    row_sq[3][0] = VmullLo8(row[3], row[3]);
    row_sq[3][1] = VmullHi8(row[3], row[3]);
    row_sq[4][0] = VmullLo8(row[4], row[4]);
    row_sq[4][1] = VmullHi8(row[4], row[4]);

    BoxFilterPreProcess8<5, 0>(row, row_sq, s[0], &a2[0], &b2[0][1], ab_ptr);
    BoxFilterPreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1], &b2[1][1],
                               ab_ptr + 8);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    sum565_a[0] = Sum565(a2[0]);
    sum565_a[0] = _mm_sub_epi16(_mm_set1_epi16((5 + 6 + 5) * 256), sum565_a[0]);
    sum565_b[0][0] = Sum565W(_mm_alignr_epi8(b2[0][1], b2[0][0], 8));
    sum565_b[0][1] = Sum565W(b2[0][1]);

    sum343_a[0] = Sum343(a2[1]);
    sum343_a[0] = _mm_sub_epi16(_mm_set1_epi16((3 + 4 + 3) * 256), sum343_a[0]);
    Sum343W(b2[1], sum343_b[0]);

    a2[1] = b2[1][0] = LoadAligned16(ab_ptr + 16);

    BoxFilterPreProcess8<3, 1>(row + 2, row_sq + 2, s[1], &a2[1], &b2[1][1],
                               ab_ptr + 16);

    Sum343_444(a2[1], &sum343_a[1], &sum444_a[0]);
    sum343_a[1] = _mm_sub_epi16(_mm_set1_epi16((3 + 4 + 3) * 256), sum343_a[1]);
    sum444_a[0] = _mm_sub_epi16(_mm_set1_epi16((4 + 4 + 4) * 256), sum444_a[0]);
    Sum343_444W(b2[1], sum343_b[1], sum444_b[0]);

    const uint8_t* src_ptr = src + x;
    uint8_t* dst_ptr = dst + x;

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    for (int y = height >> 1; y != 0; --y) {
      ab_ptr += 24;
      a2[0] = b2[0][0] = LoadAligned16(ab_ptr);
      a2[1] = b2[1][0] = LoadAligned16(ab_ptr + 8);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];

      column += src_stride;
      row[3] = LoadUnaligned16Msan(column, x + 14 - width);
      column += src_stride;
      row[4] = LoadUnaligned16Msan(column, x + 14 - width);

      row_sq[3][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = VmullHi8(row[3], row[3]);
      row_sq[4][0] = VmullLo8(row[4], row[4]);
      row_sq[4][1] = VmullHi8(row[4], row[4]);

      BoxFilterPreProcess8<5, 0>(row, row_sq, s[0], &a2[0], &b2[0][1], ab_ptr);
      BoxFilterPreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1], &b2[1][1],
                                 ab_ptr + 8);

      __m128i p[2];
      const __m128i src0 = LoadLo8(src_ptr);
      p[0] = BoxFilterPass1(src0, a2[0], b2[0], sum565_a, sum565_b);
      p[1] = BoxFilterPass2(src0, a2[1], b2[1], sum343_a, sum444_a, sum343_b,
                            sum444_b);
      SelfGuidedDoubleMultiplier(src0, p, w0_v, w1_v, w2_v, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      const __m128i src1 = LoadLo8(src_ptr);
      p[0] = CalculateFilteredOutput<4>(src1, sum565_a[1], sum565_b[1]);
      a2[1] = b2[1][0] = LoadAligned16(ab_ptr + 16);
      BoxFilterPreProcess8<3, 1>(row + 2, row_sq + 2, s[1], &a2[1], &b2[1][1],
                                 ab_ptr + 16);
      p[1] = BoxFilterPass2(src1, a2[1], b2[1], sum343_a + 1, sum444_a + 1,
                            sum343_b + 1, sum444_b + 1);
      SelfGuidedDoubleMultiplier(src1, p, w0_v, w1_v, w2_v, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      sum565_a[0] = sum565_a[1];
      sum565_b[0][0] = sum565_b[1][0], sum565_b[0][1] = sum565_b[1][1];
      sum343_a[0] = sum343_a[2];
      sum343_a[1] = sum343_a[3];
      sum444_a[0] = sum444_a[2];
      sum343_b[0][0] = sum343_b[2][0], sum343_b[0][1] = sum343_b[2][1];
      sum343_b[1][0] = sum343_b[3][0], sum343_b[1][1] = sum343_b[3][1];
      sum444_b[0][0] = sum444_b[2][0], sum444_b[0][1] = sum444_b[2][1];
    }
    if ((height & 1) != 0) {
      ab_ptr += 24;
      a2[0] = b2[0][0] = LoadAligned16(ab_ptr);
      a2[1] = b2[1][0] = LoadAligned16(ab_ptr + 8);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];

      column += src_stride;
      row[3] = row[4] = LoadUnaligned16Msan(column, x + 14 - width);

      row_sq[3][0] = row_sq[4][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = row_sq[4][1] = VmullHi8(row[3], row[3]);

      BoxFilterPreProcess8<5, 0>(row, row_sq, s[0], &a2[0], &b2[0][1], ab_ptr);
      BoxFilterPreProcess8<3, 1>(row + 1, row_sq + 1, s[1], &a2[1], &b2[1][1],
                                 ab_ptr + 8);

      __m128i p[2];
      const __m128i src0 = LoadLo8(src_ptr);
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
    __m128i row[5], row_sq[5];
    row[0] = row[1] = LoadLo8Msan(column, 2 - width);
    column += src_stride;
    row[2] = LoadLo8Msan(column, 2 - width);

    row_sq[0] = row_sq[1] = VmullLo8(row[1], row[1]);
    row_sq[2] = VmullLo8(row[2], row[2]);

    int y = (height + 2) >> 1;
    do {
      column += src_stride;
      row[3] = LoadLo8Msan(column, 2 - width);
      column += src_stride;
      row[4] = LoadLo8Msan(column, 2 - width);

      row_sq[3] = VmullLo8(row[3], row[3]);
      row_sq[4] = VmullLo8(row[4], row[4]);

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
      row[3] = row[4] = LoadLo8Msan(column, 2 - width);
      row_sq[3] = row_sq[4] = VmullLo8(row[3], row[3]);
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
    __m128i a2[2], b2[2], sum565_a[2], sum565_b[2][2];
    ab_ptr = temp;
    a2[0] = b2[0] = LoadAligned16(ab_ptr);

    const uint8_t* column = src_pre_process + x + 4;
    __m128i row[5], row_sq[5][2];
    row[0] = row[1] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[3] = LoadUnaligned16Msan(column, x + 14 - width);
    column += src_stride;
    row[4] = LoadUnaligned16Msan(column, x + 14 - width);

    row_sq[0][0] = row_sq[1][0] = VmullLo8(row[1], row[1]);
    row_sq[0][1] = row_sq[1][1] = VmullHi8(row[1], row[1]);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);
    row_sq[3][0] = VmullLo8(row[3], row[3]);
    row_sq[3][1] = VmullHi8(row[3], row[3]);
    row_sq[4][0] = VmullLo8(row[4], row[4]);
    row_sq[4][1] = VmullHi8(row[4], row[4]);

    BoxFilterPreProcess8<5, 0>(row, row_sq, s, &a2[0], &b2[1], ab_ptr);

    // Pass 1 Process. These are the only values we need to propagate between
    // rows.
    sum565_a[0] = Sum565(a2[0]);
    sum565_a[0] = _mm_sub_epi16(_mm_set1_epi16((5 + 6 + 5) * 256), sum565_a[0]);
    sum565_b[0][0] = Sum565W(_mm_alignr_epi8(b2[1], b2[0], 8));
    sum565_b[0][1] = Sum565W(b2[1]);

    const uint8_t* src_ptr = src + x;
    uint8_t* dst_ptr = dst + x;

    // Calculate one output line. Add in the line from the previous pass and
    // output one even row. Sum the new line and output the odd row. Carry the
    // new row into the next pass.
    for (int y = height >> 1; y != 0; --y) {
      ab_ptr += 8;
      a2[0] = b2[0] = LoadAligned16(ab_ptr);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];

      column += src_stride;
      row[3] = LoadUnaligned16Msan(column, x + 14 - width);
      column += src_stride;
      row[4] = LoadUnaligned16Msan(column, x + 14 - width);

      row_sq[3][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = VmullHi8(row[3], row[3]);
      row_sq[4][0] = VmullLo8(row[4], row[4]);
      row_sq[4][1] = VmullHi8(row[4], row[4]);

      BoxFilterPreProcess8<5, 0>(row, row_sq, s, &a2[0], &b2[1], ab_ptr);

      const __m128i src0 = LoadLo8(src_ptr);
      const __m128i p0 = BoxFilterPass1(src0, a2[0], b2, sum565_a, sum565_b);
      SelfGuidedSingleMultiplier(src0, p0, w0, w1, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      const __m128i src1 = LoadLo8(src_ptr);
      const __m128i p1 =
          CalculateFilteredOutput<4>(src1, sum565_a[1], sum565_b[1]);
      SelfGuidedSingleMultiplier(src1, p1, w0, w1, dst_ptr);
      src_ptr += src_stride;
      dst_ptr += dst_stride;

      sum565_a[0] = sum565_a[1];
      sum565_b[0][0] = sum565_b[1][0], sum565_b[0][1] = sum565_b[1][1];
    }
    if ((height & 1) != 0) {
      ab_ptr += 8;
      a2[0] = b2[0] = LoadAligned16(ab_ptr);

      row[0] = row[2];
      row[1] = row[3];
      row[2] = row[4];

      row_sq[0][0] = row_sq[2][0], row_sq[0][1] = row_sq[2][1];
      row_sq[1][0] = row_sq[3][0], row_sq[1][1] = row_sq[3][1];
      row_sq[2][0] = row_sq[4][0], row_sq[2][1] = row_sq[4][1];

      column += src_stride;
      row[3] = row[4] = LoadUnaligned16Msan(column, x + 14 - width);

      row_sq[3][0] = row_sq[4][0] = VmullLo8(row[3], row[3]);
      row_sq[3][1] = row_sq[4][1] = VmullHi8(row[3], row[3]);

      BoxFilterPreProcess8<5, 0>(row, row_sq, s, &a2[0], &b2[1], ab_ptr);

      const __m128i src0 = LoadLo8(src_ptr);
      const __m128i p0 = BoxFilterPass1(src0, a2[0], b2, sum565_a, sum565_b);
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
  const uint8_t* const src_top_left_corner = src - 2 * src_stride - 2;
  {
    const uint8_t* column = src_top_left_corner;
    __m128i row[3], row_sq[3];
    row[0] = LoadLo8Msan(column, 4 - width);
    column += src_stride;
    row[1] = LoadLo8Msan(column, 4 - width);
    row_sq[0] = VmullLo8(row[0], row[0]);
    row_sq[1] = VmullLo8(row[1], row[1]);

    int y = height + 2;
    do {
      column += src_stride;
      row[2] = LoadLo8Msan(column, 4 - width);
      row_sq[2] = VmullLo8(row[2], row[2]);

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

    __m128i a2, b2[2], sum343_a[3], sum444_a[2], sum343_b[3][2], sum444_b[2][2];
    a2 = b2[0] = LoadAligned16(ab_ptr);

    const uint8_t* column = src_top_left_corner + x + 4;
    __m128i row[3], row_sq[3][2];
    row[0] = LoadUnaligned16Msan(column, x + 16 - width);
    column += src_stride;
    row[1] = LoadUnaligned16Msan(column, x + 16 - width);
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 16 - width);

    row_sq[0][0] = VmullLo8(row[0], row[0]);
    row_sq[0][1] = VmullHi8(row[0], row[0]);
    row_sq[1][0] = VmullLo8(row[1], row[1]);
    row_sq[1][1] = VmullHi8(row[1], row[1]);
    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);

    BoxFilterPreProcess8<3, 0>(row, row_sq, s, &a2, &b2[1], ab_ptr);

    sum343_a[0] = Sum343(a2);
    sum343_a[0] = _mm_sub_epi16(_mm_set1_epi16((3 + 4 + 3) * 256), sum343_a[0]);
    Sum343W(b2, sum343_b[0]);

    ab_ptr += 8;
    a2 = b2[0] = LoadAligned16(ab_ptr);

    row[0] = row[1];
    row[1] = row[2];

    row_sq[0][0] = row_sq[1][0], row_sq[0][1] = row_sq[1][1];
    row_sq[1][0] = row_sq[2][0], row_sq[1][1] = row_sq[2][1];
    column += src_stride;
    row[2] = LoadUnaligned16Msan(column, x + 16 - width);

    row_sq[2][0] = VmullLo8(row[2], row[2]);
    row_sq[2][1] = VmullHi8(row[2], row[2]);

    BoxFilterPreProcess8<3, 0>(row, row_sq, s, &a2, &b2[1], ab_ptr);

    Sum343_444(a2, &sum343_a[1], &sum444_a[0]);
    sum343_a[1] = _mm_sub_epi16(_mm_set1_epi16((3 + 4 + 3) * 256), sum343_a[1]);
    sum444_a[0] = _mm_sub_epi16(_mm_set1_epi16((4 + 4 + 4) * 256), sum444_a[0]);
    Sum343_444W(b2, sum343_b[1], sum444_b[0]);

    const uint8_t* src_ptr = src + x;
    uint8_t* dst_ptr = dst + x;
    int y = height;
    do {
      ab_ptr += 8;
      a2 = b2[0] = LoadAligned16(ab_ptr);

      row[0] = row[1];
      row[1] = row[2];

      row_sq[0][0] = row_sq[1][0], row_sq[0][1] = row_sq[1][1];
      row_sq[1][0] = row_sq[2][0], row_sq[1][1] = row_sq[2][1];
      column += src_stride;
      row[2] = LoadUnaligned16Msan(column, x + 16 - width);

      row_sq[2][0] = VmullLo8(row[2], row[2]);
      row_sq[2][1] = VmullHi8(row[2], row[2]);

      BoxFilterPreProcess8<3, 0>(row, row_sq, s, &a2, &b2[1], ab_ptr);

      const __m128i src_u8 = LoadLo8(src_ptr);
      const __m128i p = BoxFilterPass2(src_u8, a2, b2, sum343_a, sum444_a,
                                       sum343_b, sum444_b);
      SelfGuidedSingleMultiplier(src_u8, p, w0, w1, dst_ptr);
      sum343_a[0] = sum343_a[1];
      sum343_a[1] = sum343_a[2];
      sum444_a[0] = sum444_a[1];
      sum343_b[0][0] = sum343_b[1][0], sum343_b[0][1] = sum343_b[1][1];
      sum343_b[1][0] = sum343_b[2][0], sum343_b[1][1] = sum343_b[2][1];
      sum444_b[0][0] = sum444_b[1][0], sum444_b[0][1] = sum444_b[1][1];
      src_ptr += src_stride;
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

#else   // !LIBGAV1_ENABLE_SSE4_1
namespace libgav1 {
namespace dsp {

void LoopRestorationInit_SSE4_1() {}

}  // namespace dsp
}  // namespace libgav1
#endif  // LIBGAV1_ENABLE_SSE4_1
