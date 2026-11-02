# Copyright 2026 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

"""Specializations for uint8 x int2 x86 dot kernel generators."""

# pylint: disable=missing-class-docstring
# pylint: disable=invalid-name

from ynnpack.kernels.dot.generator.dot_base import generate_dot_kernels
from ynnpack.kernels.dot.generator.x86 import x86
from ynnpack.kernels.dot.generator.x86 import x86_avx
from ynnpack.kernels.dot.generator.x86 import x86_avx512


class x86_uint8_int2_int32(x86):

  def __init__(self, arch, vector_bits, tile_shape):
    super().__init__(
        arch, "uint8_int2_int32", "int32_t", vector_bits, tile_shape
    )
    self.a_type = "uint8_t"
    self.b_type = "int2x4"
    self.flags += ["dot_flag::consistent_arithmetic"]

  def b_tile_size_bytes(self):
    return f"{self.tile_shape[1] * self.tile_shape[2] // 4}"

  def header(self):
    return super().header() + """
#include <immintrin.h>

namespace {

YNN_INTRINSIC int64_t unaligned_load_u8x8(const uint8_t* ptr) {
  int64_t value;
  memcpy(&value, ptr, sizeof(int64_t));
  return value;
}

}  // namespace
"""

  def load_a_tile(self, i, k):
    a = f"unaligned_load_u8x8({self.a_ptr(i, k)})"
    if self.bits == 512:
      return f"__m512i a_{i}_{k} = _mm512_set1_epi64({a});\n"
    else:
      return f"__m{self.bits}i a_{i}_{k} = {self._mm()}_set1_epi64x({a});\n"

  def init_c_tile(self, i, j):
    mm = self._mm()
    n = self.tile_shape[1] // 2
    return f"""
__m{self.bits}i c_{i}_{j+0} = {mm}_setzero_si{self.bits}();
__m{self.bits}i c_{i}_{j+n} = {mm}_setzero_si{self.bits}();
"""

  def product(self, i, j, k):
    mm = self._mm()
    n = self.tile_shape[1] // 2
    a_ik = f"a_{i}_{k}"
    b_kj0 = f"b_{k}_{j+0}"
    b_kjn = f"b_{k}_{j+n}"
    c_ij0 = f"c_{i}_{j+0}"
    c_ijn = f"c_{i}_{j+n}"
    if k == 0:
      # The first tile initializes the local accumulator of int16 values.
      result = f"""
__m{self.bits}i {c_ij0}_block = {mm}_maddubs_epi16({a_ik}, {b_kj0});
__m{self.bits}i {c_ijn}_block = {mm}_maddubs_epi16({a_ik}, {b_kjn});
"""
    else:
      # Subsequent tiles add to the local accumulator. We can add 64 values of k
      # without overflow (32768 / (2*256) = 64). The accumulators are split in 2
      assert k <= 128
      result = f"""
{c_ij0}_block = {mm}_add_epi16({c_ij0}_block, {mm}_maddubs_epi16({a_ik}, {b_kj0}));
{c_ijn}_block = {mm}_add_epi16({c_ijn}_block, {mm}_maddubs_epi16({a_ik}, {b_kjn}));
"""
    if k + self.tile_shape[2] == self.block_shape[2]:
      # On the last tile, widen to int32, and add to the global accumulator.
      result += f"""
{c_ij0} = {mm}_sub_epi32({c_ij0}, {mm}_madd_epi16({c_ij0}_block, {mm}_set1_epi16(-1)));
{c_ijn} = {mm}_sub_epi32({c_ijn}, {mm}_madd_epi16({c_ijn}_block, {mm}_set1_epi16(-1)));
"""
    return result

  def finalize_c_tile(self, i, j):
    mm = self._mm()
    n = self.tile_shape[1] // 2
    return f"c_{i}_{j} = {mm}_hadd_epi32(c_{i}_{j}, c_{i}_{j+n});\n"


class x86_avx2_uint8_int2_int32(x86_uint8_int2_int32, x86_avx):

  def __init__(self, arch="avx2", vector_bits=256):
    super().__init__(arch, vector_bits, tile_shape=(1, 8, 8))

  def begin_func(self, func_name):
    result = super().begin_func(func_name)
    result += """
const __m256i mask = _mm256_set1_epi8(0x03);
const __m256i sign_lut = _mm256_setr_epi8(
    0, 1, -2, -1, 0, 1, -2, -1, 0, 1, -2, -1, 0, 1, -2, -1,
    0, 1, -2, -1, 0, 1, -2, -1, 0, 1, -2, -1, 0, 1, -2, -1);
const __m256i shift_amt = _mm256_setr_epi32(0, 2, 4, 6, 0, 2, 4, 6);
const __m256i shuf_mask = _mm256_setr_epi8(
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15,
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
"""
    return result

  def load_b_tile(self, k, j):
    bits = self.bits
    mm = self._mm()
    n = self.tile_shape[1] // 2
    return f"""
__m128i b_raw_{k}_{j} = _mm_load_si128(reinterpret_cast<const __m128i*>({self.b_ptr(k, j)}));
__m{bits}i b_raw_256_{k}_{j} = {mm}_castsi128_si{bits}(b_raw_{k}_{j});
__m{bits}i b_{k}_{j+0} = {mm}_permutevar8x32_epi32(b_raw_256_{k}_{j}, _mm256_setr_epi32(0, 0, 0, 0, 2, 2, 2, 2));
b_{k}_{j+0} = {mm}_srlv_epi32(b_{k}_{j+0}, shift_amt);
b_{k}_{j+0} = {mm}_shuffle_epi8(b_{k}_{j+0}, shuf_mask);
b_{k}_{j+0} = {mm}_and_si{bits}(b_{k}_{j+0}, mask);
b_{k}_{j+0} = {mm}_shuffle_epi8(sign_lut, b_{k}_{j+0});

__m{bits}i b_{k}_{j+n} = {mm}_permutevar8x32_epi32(b_raw_256_{k}_{j}, _mm256_setr_epi32(1, 1, 1, 1, 3, 3, 3, 3));
b_{k}_{j+n} = {mm}_srlv_epi32(b_{k}_{j+n}, shift_amt);
b_{k}_{j+n} = {mm}_shuffle_epi8(b_{k}_{j+n}, shuf_mask);
b_{k}_{j+n} = {mm}_and_si{bits}(b_{k}_{j+n}, mask);
b_{k}_{j+n} = {mm}_shuffle_epi8(sign_lut, b_{k}_{j+n});
"""


class x86_avx512_uint8_int2_int32(x86_uint8_int2_int32, x86_avx512):

  def __init__(self, arch="avx512", vector_bits=512):
    super().__init__(arch, vector_bits, tile_shape=(1, 16, 8))

  def header(self):
    return super().header() + """
namespace {

YNN_INTRINSIC __m512i _mm512_hadd_epi32(__m512i a, __m512i b) {
  __m512i t0 = _mm512_unpacklo_epi64(a, b);
  __m512i t1 = _mm512_unpackhi_epi64(a, b);
  __m512i even = _mm512_add_epi32(t0, _mm512_srli_epi64(t0, 32));
  __m512i odd = _mm512_add_epi32(t1, _mm512_slli_epi64(t1, 32));
  return _mm512_mask_blend_epi32(0xAAAA, even, odd);
}

}  // namespace
"""

  def begin_func(self, func_name):
    return super().begin_func(func_name) + """
const __m512i mask = _mm512_set1_epi8(0x03);
const __m512i sign_lut = _mm512_set_epi8(
    -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0,
    -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0,
    -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0,
    -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0, -1, -2, 1, 0);
const __m512i shift_amt = _mm512_setr_epi32(
    0, 2, 4, 6, 0, 2, 4, 6, 0, 2, 4, 6, 0, 2, 4, 6);
const __m512i shuf_mask = _mm512_set_epi8(
    15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0,
    15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0,
    15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0,
    15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
const __m512i perm_mask_0 = _mm512_setr_epi32(
    0, 0, 0, 0, 2, 2, 2, 2, 4, 4, 4, 4, 6, 6, 6, 6);
const __m512i perm_mask_1 = _mm512_setr_epi32(
    1, 1, 1, 1, 3, 3, 3, 3, 5, 5, 5, 5, 7, 7, 7, 7);
"""

  def load_b_tile(self, k, j):
    bits = self.bits
    mm = self._mm()
    n = self.tile_shape[1] // 2
    return f"""
__m256i b_raw_{k}_{j} = _mm256_load_si256(reinterpret_cast<const __m256i*>({self.b_ptr(k, j)}));
__m{bits}i b_raw_512_{k}_{j} = {mm}_castsi256_si{bits}(b_raw_{k}_{j});
__m{bits}i b_{k}_{j+0} = {mm}_permutexvar_epi32(perm_mask_0, b_raw_512_{k}_{j});
b_{k}_{j+0} = {mm}_srlv_epi32(b_{k}_{j+0}, shift_amt);
b_{k}_{j+0} = {mm}_shuffle_epi8(b_{k}_{j+0}, shuf_mask);
b_{k}_{j+0} = {mm}_and_si{bits}(b_{k}_{j+0}, mask);
b_{k}_{j+0} = {mm}_shuffle_epi8(sign_lut, b_{k}_{j+0});

__m{bits}i b_{k}_{j+n} = {mm}_permutexvar_epi32(perm_mask_1, b_raw_512_{k}_{j});
b_{k}_{j+n} = {mm}_srlv_epi32(b_{k}_{j+n}, shift_amt);
b_{k}_{j+n} = {mm}_shuffle_epi8(b_{k}_{j+n}, shuf_mask);
b_{k}_{j+n} = {mm}_and_si{bits}(b_{k}_{j+n}, mask);
b_{k}_{j+n} = {mm}_shuffle_epi8(sign_lut, b_{k}_{j+n});
"""


class x86_avx512vnni_uint8_int2_int32(x86_avx512_uint8_int2_int32):

  def __init__(self, arch="avx512vnni"):
    super().__init__(arch)

  def product(self, i, j, k):
    mm = self._mm()
    n = self.tile_shape[1] // 2
    a_ik = f"a_{i}_{k}"
    return f"""
c_{i}_{j+0} = {mm}_dpbusd_epi32(c_{i}_{j+0}, {a_ik}, b_{k}_{j+0});
c_{i}_{j+n} = {mm}_dpbusd_epi32(c_{i}_{j+n}, {a_ik}, b_{k}_{j+n});
"""


generate_dot_kernels(
    x86_avx2_uint8_int2_int32(),
    [
        (1, 16, 64),
        (2, 16, 64),
        (3, 16, 64),
        (1, 8, 64),
        (3, 8, 64),
        (4, 8, 64),
        (8, 8, 64),
    ],
)

generate_dot_kernels(
    x86_avx512_uint8_int2_int32(),
    [
        (1, 32, 64),
        (2, 32, 64),
        (3, 32, 64),
        (4, 32, 64),
        (5, 32, 64),
        (1, 16, 64),
        (5, 16, 64),
        (8, 16, 64),
    ],
)

generate_dot_kernels(
    x86_avx512vnni_uint8_int2_int32(),
    [
        (1, 32, 8),
        (2, 32, 8),
        (3, 32, 8),
        (4, 32, 8),
        (5, 32, 8),
        (1, 16, 8),
        (5, 16, 8),
        (8, 16, 8),
    ],
)
