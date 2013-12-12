/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_MV_H_
#define VP9_COMMON_VP9_MV_H_

#include "vpx/vpx_integer.h"

#include "vp9/common/vp9_common.h"

typedef struct {
  int16_t row;
  int16_t col;
} MV;

typedef union int_mv {
  uint32_t as_int;
  MV as_mv;
} int_mv; /* facilitates faster equality tests and copies */

typedef struct {
  int32_t row;
  int32_t col;
} MV32;

static void clamp_mv(MV *mv, int min_col, int max_col,
                             int min_row, int max_row) {
  mv->col = clamp(mv->col, min_col, max_col);
  mv->row = clamp(mv->row, min_row, max_row);
}

#endif  // VP9_COMMON_VP9_MV_H_
