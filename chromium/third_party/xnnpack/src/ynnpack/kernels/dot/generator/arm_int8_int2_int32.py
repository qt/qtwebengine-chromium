# Copyright 2026 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

"""Specializations for int8 x int2 arm dot kernel generators."""

# pylint: disable=missing-class-docstring
# pylint: disable=invalid-name

from ynnpack.kernels.dot.generator.arm import arm_neon
from ynnpack.kernels.dot.generator.dot_base import generate_dot_kernels


class arm_int8_int2_int32(arm_neon):

  def __init__(self, arch, tile_shape):
    super().__init__(arch, "int8_int2_int32", "int32_t", tile_shape)
    self.a_type = "int8_t"
    self.b_type = "int2x4"
    self.flags += ["dot_flag::consistent_arithmetic"]

  def b_tile_size_bytes(self):
    return f"{self.tile_shape[1] * self.tile_shape[2] // 4}"

  def begin_func(self, func_name):
    result = super().begin_func(func_name)
    result += """
  const int8x16_t shift_vec = {6, 4, 2, 0, 6, 4, 2, 0, 6, 4, 2, 0, 6, 4, 2, 0};
"""
    return result


class arm_neondot_int8_int2_int32(arm_int8_int2_int32):

  def __init__(self):
    super().__init__("neondot", (1, 4, 8))

  def header(self):
    return super().header() + """
namespace {

#if YNN_ARCH_ARM64
YNN_INTRINSIC int8x16_t broadcast_even_x4(int8x8_t x) {
  const uint8x16_t mask_even = {0, 0, 0, 0, 2, 2, 2, 2, 4, 4, 4, 4, 6, 6, 6, 6};
  int8x16_t x2 = vcombine_s8(x, x);
  return vqtbl1q_s8(x2, mask_even);
}

YNN_INTRINSIC int8x16_t broadcast_odd_x4(int8x8_t x) {
  const uint8x16_t mask_odd  = {1, 1, 1, 1, 3, 3, 3, 3, 5, 5, 5, 5, 7, 7, 7, 7};
  int8x16_t x2 = vcombine_s8(x, x);
  return vqtbl1q_s8(x2, mask_odd);
}
#elif YNN_ARCH_ARM32
YNN_INTRINSIC int8x16_t broadcast_even_x4(int8x8_t x) {
  int8x8x2_t unzipped = vuzp_s8(x, x);
  int8x8x2_t zipped1 = vzip_s8(unzipped.val[0], unzipped.val[0]);
  int8x8x2_t zipped2 = vzip_s8(zipped1.val[0], zipped1.val[0]);
  return vcombine_s8(zipped2.val[0], zipped2.val[1]);
}

YNN_INTRINSIC int8x16_t broadcast_odd_x4(int8x8_t x) {
  int8x8x2_t unzipped = vuzp_s8(x, x);
  int8x8x2_t zipped1 = vzip_s8(unzipped.val[1], unzipped.val[1]);
  int8x8x2_t zipped2 = vzip_s8(zipped1.val[0], zipped1.val[0]);
  return vcombine_s8(zipped2.val[0], zipped2.val[1]);
}
#endif

}  // namespace
"""

  def load_a_tile(self, i, k):
    return f"int8x8_t a_{i}_{k} = vld1_s8({self.a_ptr(i, k)});\n"

  def load_b_tile(self, k, j):
    # The b values come in groups of 8 at a time. We:
    # 1. Load 4 of these groups.
    # 2. Replicate each byte 4x.
    # 3. Move each 2-bit weight to the lower 2 bits by shifting.
    return f"""
int8x8_t b_{k}_{j}_x1 = vld1_s8({self.b_ptr(k, j, "int8_t")});
int8x16_t b_{k}_{j}_even = broadcast_even_x4(b_{k}_{j}_x1);
int8x16_t b_{k}_{j}_odd  = broadcast_odd_x4(b_{k}_{j}_x1);
int8x16_t b_{k+0}_{j} = vshrq_n_s8(vshlq_s8(b_{k}_{j}_even, shift_vec), 6);
int8x16_t b_{k+4}_{j} = vshrq_n_s8(vshlq_s8(b_{k}_{j}_odd, shift_vec), 6);
"""

  def product(self, i, j, k):
    return f"""
c_{i}_{j} = vdotq_lane_s32(c_{i}_{j}, b_{k+0}_{j}, a_{i}_{k}, 0);
c_{i}_{j} = vdotq_lane_s32(c_{i}_{j}, b_{k+4}_{j}, a_{i}_{k}, 1);
"""


class arm64_neoni8mm_int8_int2_int32(arm_int8_int2_int32):

  def __init__(self):
    super().__init__("neoni8mm", (2, 4, 8))
    self.flags += ["dot_flag::transpose_a"]

  def begin_func(self, func_name):
    result = super().begin_func(func_name)
    result += """
  const uint8x16_t mask_lo = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
  const uint8x16_t mask_hi = {4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7};
"""
    return result

  def load_a_tile(self, i, k):
    return f"const int8x16_t a_{i}_{k} = vld1q_s8({self.a_ptr(i, k)});\n"

  def load_b_tile(self, k, j):
    # The b values come in groups of 8 at a time. We:
    # 1. Load 4 of these groups.
    # 2. Replicate each byte 4x using tbl.
    # 3. Move each 2-bit weight to the lower 2 bits by shifting.
    return f"""
int8x8_t b_{k}_{j}_x1 = vld1_s8({self.b_ptr(k, j, "int8_t")});
int8x16_t b_{k}_{j}_x2 = vcombine_s8(b_{k}_{j}_x1, vcreate_s8(0));
int8x16_t b_{k}_{j+0} = vqtbl1q_s8(b_{k}_{j}_x2, mask_lo);
int8x16_t b_{k}_{j+2} = vqtbl1q_s8(b_{k}_{j}_x2, mask_hi);
b_{k}_{j+0} = vshrq_n_s8(vshlq_s8(b_{k}_{j+0}, shift_vec), 6);
b_{k}_{j+2} = vshrq_n_s8(vshlq_s8(b_{k}_{j+2}, shift_vec), 6);
"""

  def init_c_tile(self, i, j):
    return f"""
int32x4_t c_{i}_{j+0} = vdupq_n_s32(0);
int32x4_t c_{i}_{j+2} = vdupq_n_s32(0);
"""

  def product(self, i, j, k):
    return f"""
c_{i}_{j+0} = vmmlaq_s32(c_{i}_{j+0}, a_{i}_{k}, b_{k}_{j+0});
c_{i}_{j+2} = vmmlaq_s32(c_{i}_{j+2}, a_{i}_{k}, b_{k}_{j+2});
"""

  def add_c_tile(self, i, j):
    return f"""
c_{i+0}_{j} = vaddq_s32(c_{i+0}_{j}, vld1q_s32({self.c_in_ptr(i+0, j)}));
c_{i+1}_{j} = vaddq_s32(c_{i+1}_{j}, vld1q_s32({self.c_in_ptr(i+1, j)}));
"""

  def store_c_tile(self, i, j):
    return f"""
vst1q_s32({self.c_out_ptr(i+1, j)}, c_{i+1}_{j});
vst1q_s32({self.c_out_ptr(i+0, j)}, c_{i+0}_{j});
"""

  def finalize_c_tile(self, i, j):
    c0 = f"c_{i}_{j+0}"
    c2 = f"c_{i}_{j+2}"
    return f"""
int32x4_t c_{i+1}_{j} = vcombine_s32(vget_high_s32({c0}), vget_high_s32({c2}));
c_{i+0}_{j} = vcombine_s32(vget_low_s32({c0}), vget_low_s32({c2}));
"""


generate_dot_kernels(
    arm_neondot_int8_int2_int32(),
    [
        (1, 32, 8),
        (1, 16, 8),
        (2, 16, 8),
        (1, 8, 8),
        (2, 8, 8),
        (3, 8, 8),
        (4, 8, 8),
        (8, 8, 8),
        (8, 4, 8),
    ],
)

generate_dot_kernels(
    arm64_neoni8mm_int8_int2_int32(),
    [
        (2, 32, 8),
        (4, 16, 8),
        (4, 8, 8),
        (6, 8, 8),
        (8, 8, 8),
        (10, 8, 8),
        (16, 4, 8),
    ],
)
