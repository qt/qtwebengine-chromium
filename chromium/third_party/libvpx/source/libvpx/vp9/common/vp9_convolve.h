/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VP9_COMMON_CONVOLVE_H_
#define VP9_COMMON_CONVOLVE_H_

#include "./vpx_config.h"
#include "vpx/vpx_integer.h"

#define FILTER_BITS 7

typedef void (*convolve_fn_t)(const uint8_t *src, ptrdiff_t src_stride,
                              uint8_t *dst, ptrdiff_t dst_stride,
                              const int16_t *filter_x, int x_step_q4,
                              const int16_t *filter_y, int y_step_q4,
                              int w, int h);

struct subpix_fn_table {
  const int16_t (*filter_x)[8];
  const int16_t (*filter_y)[8];
};

#endif  // VP9_COMMON_CONVOLVE_H_
