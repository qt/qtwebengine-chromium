// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/simd/convert_yuv_to_rgb.h"
#include "media/base/simd/yuv_to_rgb_table.h"

namespace media {

#define packuswb(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
#define paddsw(x, y) (((x) + (y)) < -32768 ? -32768 : \
    (((x) + (y)) > 32767 ? 32767 : ((x) + (y))))

// On Android, pixel layout is RGBA (see skia/include/core/SkColorPriv.h);
// however, other Chrome platforms use BGRA (see skia/config/SkUserConfig.h).
// Ideally, android should not use the functions here due to performance issue
// (http://crbug.com/249980).
#if defined(OS_ANDROID)
#define SK_R32_SHIFT    0
#define SK_G32_SHIFT    8
#define SK_B32_SHIFT    16
#define SK_A32_SHIFT    24
#define R_INDEX         0
#define G_INDEX         1
#define B_INDEX         2
#define A_INDEX         3
#else
#define SK_B32_SHIFT    0
#define SK_G32_SHIFT    8
#define SK_R32_SHIFT    16
#define SK_A32_SHIFT    24
#define B_INDEX         0
#define G_INDEX         1
#define R_INDEX         2
#define A_INDEX         3
#endif

static inline void ConvertYUVToRGB32_C(uint8 y,
                                       uint8 u,
                                       uint8 v,
                                       uint8* rgb_buf) {
  int b = kCoefficientsRgbY[256+u][B_INDEX];
  int g = kCoefficientsRgbY[256+u][G_INDEX];
  int r = kCoefficientsRgbY[256+u][R_INDEX];
  int a = kCoefficientsRgbY[256+u][A_INDEX];

  b = paddsw(b, kCoefficientsRgbY[512+v][B_INDEX]);
  g = paddsw(g, kCoefficientsRgbY[512+v][G_INDEX]);
  r = paddsw(r, kCoefficientsRgbY[512+v][R_INDEX]);
  a = paddsw(a, kCoefficientsRgbY[512+v][A_INDEX]);

  b = paddsw(b, kCoefficientsRgbY[y][B_INDEX]);
  g = paddsw(g, kCoefficientsRgbY[y][G_INDEX]);
  r = paddsw(r, kCoefficientsRgbY[y][R_INDEX]);
  a = paddsw(a, kCoefficientsRgbY[y][A_INDEX]);

  b >>= 6;
  g >>= 6;
  r >>= 6;
  a >>= 6;

  *reinterpret_cast<uint32*>(rgb_buf) = (packuswb(b) << SK_B32_SHIFT) |
                                        (packuswb(g) << SK_G32_SHIFT) |
                                        (packuswb(r) << SK_R32_SHIFT) |
                                        (packuswb(a) << SK_A32_SHIFT);
}

static inline void ConvertYUVAToARGB_C(uint8 y,
                                       uint8 u,
                                       uint8 v,
                                       uint8 a,
                                       uint8* rgb_buf) {
  int b = kCoefficientsRgbY[256+u][0];
  int g = kCoefficientsRgbY[256+u][1];
  int r = kCoefficientsRgbY[256+u][2];

  b = paddsw(b, kCoefficientsRgbY[512+v][0]);
  g = paddsw(g, kCoefficientsRgbY[512+v][1]);
  r = paddsw(r, kCoefficientsRgbY[512+v][2]);

  b = paddsw(b, kCoefficientsRgbY[y][0]);
  g = paddsw(g, kCoefficientsRgbY[y][1]);
  r = paddsw(r, kCoefficientsRgbY[y][2]);

  b >>= 6;
  g >>= 6;
  r >>= 6;

  b = packuswb(b) * a >> 8;
  g = packuswb(g) * a >> 8;
  r = packuswb(r) * a >> 8;

  *reinterpret_cast<uint32*>(rgb_buf) = (b << SK_B32_SHIFT) |
                                        (g << SK_G32_SHIFT) |
                                        (r << SK_R32_SHIFT) |
                                        (a << SK_A32_SHIFT);
}

void ConvertYUVToRGB32Row_C(const uint8* y_buf,
                            const uint8* u_buf,
                            const uint8* v_buf,
                            uint8* rgb_buf,
                            ptrdiff_t width) {
  for (int x = 0; x < width; x += 2) {
    uint8 u = u_buf[x >> 1];
    uint8 v = v_buf[x >> 1];
    uint8 y0 = y_buf[x];
    ConvertYUVToRGB32_C(y0, u, v, rgb_buf);
    if ((x + 1) < width) {
      uint8 y1 = y_buf[x + 1];
      ConvertYUVToRGB32_C(y1, u, v, rgb_buf + 4);
    }
    rgb_buf += 8;  // Advance 2 pixels.
  }
}

void ConvertYUVAToARGBRow_C(const uint8* y_buf,
                            const uint8* u_buf,
                            const uint8* v_buf,
                            const uint8* a_buf,
                            uint8* rgba_buf,
                            ptrdiff_t width) {
  for (int x = 0; x < width; x += 2) {
    uint8 u = u_buf[x >> 1];
    uint8 v = v_buf[x >> 1];
    uint8 y0 = y_buf[x];
    uint8 a0 = a_buf[x];
    ConvertYUVAToARGB_C(y0, u, v, a0, rgba_buf);
    if ((x + 1) < width) {
      uint8 y1 = y_buf[x + 1];
      uint8 a1 = a_buf[x + 1];
      ConvertYUVAToARGB_C(y1, u, v, a1, rgba_buf + 4);
    }
    rgba_buf += 8;  // Advance 2 pixels.
  }
}

// 16.16 fixed point is used.  A shift by 16 isolates the integer.
// A shift by 17 is used to further subsample the chrominence channels.
// & 0xffff isolates the fixed point fraction.  >> 2 to get the upper 2 bits,
// for 1/65536 pixel accurate interpolation.
void ScaleYUVToRGB32Row_C(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          ptrdiff_t width,
                          ptrdiff_t source_dx) {
  int x = 0;
  for (int i = 0; i < width; i += 2) {
    int y = y_buf[x >> 16];
    int u = u_buf[(x >> 17)];
    int v = v_buf[(x >> 17)];
    ConvertYUVToRGB32_C(y, u, v, rgb_buf);
    x += source_dx;
    if ((i + 1) < width) {
      y = y_buf[x >> 16];
      ConvertYUVToRGB32_C(y, u, v, rgb_buf+4);
      x += source_dx;
    }
    rgb_buf += 8;
  }
}

void LinearScaleYUVToRGB32Row_C(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                ptrdiff_t width,
                                ptrdiff_t source_dx) {
  // Avoid point-sampling for down-scaling by > 2:1.
  int source_x = 0;
  if (source_dx >= 0x20000)
    source_x += 0x8000;
  LinearScaleYUVToRGB32RowWithRange_C(y_buf, u_buf, v_buf, rgb_buf, width,
                                      source_x, source_dx);
}

void LinearScaleYUVToRGB32RowWithRange_C(const uint8* y_buf,
                                         const uint8* u_buf,
                                         const uint8* v_buf,
                                         uint8* rgb_buf,
                                         int dest_width,
                                         int x,
                                         int source_dx) {
  for (int i = 0; i < dest_width; i += 2) {
    int y0 = y_buf[x >> 16];
    int y1 = y_buf[(x >> 16) + 1];
    int u0 = u_buf[(x >> 17)];
    int u1 = u_buf[(x >> 17) + 1];
    int v0 = v_buf[(x >> 17)];
    int v1 = v_buf[(x >> 17) + 1];
    int y_frac = (x & 65535);
    int uv_frac = ((x >> 1) & 65535);
    int y = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;
    int u = (uv_frac * u1 + (uv_frac ^ 65535) * u0) >> 16;
    int v = (uv_frac * v1 + (uv_frac ^ 65535) * v0) >> 16;
    ConvertYUVToRGB32_C(y, u, v, rgb_buf);
    x += source_dx;
    if ((i + 1) < dest_width) {
      y0 = y_buf[x >> 16];
      y1 = y_buf[(x >> 16) + 1];
      y_frac = (x & 65535);
      y = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;
      ConvertYUVToRGB32_C(y, u, v, rgb_buf+4);
      x += source_dx;
    }
    rgb_buf += 8;
  }
}

void ConvertYUVToRGB32_C(const uint8* yplane,
                         const uint8* uplane,
                         const uint8* vplane,
                         uint8* rgbframe,
                         int width,
                         int height,
                         int ystride,
                         int uvstride,
                         int rgbstride,
                         YUVType yuv_type) {
  unsigned int y_shift = yuv_type;
  for (int y = 0; y < height; ++y) {
    uint8* rgb_row = rgbframe + y * rgbstride;
    const uint8* y_ptr = yplane + y * ystride;
    const uint8* u_ptr = uplane + (y >> y_shift) * uvstride;
    const uint8* v_ptr = vplane + (y >> y_shift) * uvstride;

    ConvertYUVToRGB32Row_C(y_ptr,
                           u_ptr,
                           v_ptr,
                           rgb_row,
                           width);
  }
}

void ConvertYUVAToARGB_C(const uint8* yplane,
                         const uint8* uplane,
                         const uint8* vplane,
                         const uint8* aplane,
                         uint8* rgbaframe,
                         int width,
                         int height,
                         int ystride,
                         int uvstride,
                         int astride,
                         int rgbastride,
                         YUVType yuv_type) {
  unsigned int y_shift = yuv_type;
  for (int y = 0; y < height; y++) {
    uint8* rgba_row = rgbaframe + y * rgbastride;
    const uint8* y_ptr = yplane + y * ystride;
    const uint8* u_ptr = uplane + (y >> y_shift) * uvstride;
    const uint8* v_ptr = vplane + (y >> y_shift) * uvstride;
    const uint8* a_ptr = aplane + y * astride;

    ConvertYUVAToARGBRow_C(y_ptr,
                           u_ptr,
                           v_ptr,
                           a_ptr,
                           rgba_row,
                           width);
  }
}

}  // namespace media
