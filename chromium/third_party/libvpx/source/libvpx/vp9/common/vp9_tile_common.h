/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_TILE_COMMON_H_
#define VP9_COMMON_VP9_TILE_COMMON_H_

struct VP9Common;

typedef struct TileInfo {
  int mi_row_start, mi_row_end;
  int mi_col_start, mi_col_end;
} TileInfo;

// initializes 'tile->mi_(row|col)_(start|end)' for (row_idx, col_idx) based on
// 'cm->log2_tile_(rows|cols)' & 'cm->mi_(rows|cols)'
void vp9_tile_init(TileInfo *tile, const struct VP9Common *cm,
                   int row_idx, int col_idx);

void vp9_get_tile_n_bits(int mi_cols,
                         int *min_log2_tile_cols, int *max_log2_tile_cols);

#endif  // VP9_COMMON_VP9_TILE_COMMON_H_
