/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_tile_common.h"

#include "vp9/common/vp9_onyxc_int.h"

#define MIN_TILE_WIDTH_B64 4
#define MAX_TILE_WIDTH_B64 64

static int to_sbs(n_mis) {
  return mi_cols_aligned_to_sb(n_mis) >> MI_BLOCK_SIZE_LOG2;
}

static void get_tile_offsets(int *min_tile_off, int *max_tile_off,
                             int tile_idx, int log2_n_tiles, int n_mis) {
  const int n_sbs = to_sbs(n_mis);
  const int sb_off1 =  (tile_idx      * n_sbs) >> log2_n_tiles;
  const int sb_off2 = ((tile_idx + 1) * n_sbs) >> log2_n_tiles;

  *min_tile_off = MIN(sb_off1 << 3, n_mis);
  *max_tile_off = MIN(sb_off2 << 3, n_mis);
}

void vp9_tile_init(TileInfo *tile, const VP9_COMMON *cm,
                   int row_idx, int col_idx) {
  get_tile_offsets(&tile->mi_row_start, &tile->mi_row_end,
                   row_idx, cm->log2_tile_rows, cm->mi_rows);
  get_tile_offsets(&tile->mi_col_start, &tile->mi_col_end,
                   col_idx, cm->log2_tile_cols, cm->mi_cols);
}

void vp9_get_tile_n_bits(int mi_cols,
                         int *min_log2_tile_cols, int *max_log2_tile_cols) {
  const int sb_cols = to_sbs(mi_cols);
  int min_log2_n_tiles, max_log2_n_tiles;

  for (max_log2_n_tiles = 0;
       (sb_cols >> max_log2_n_tiles) >= MIN_TILE_WIDTH_B64;
       max_log2_n_tiles++) {}
  max_log2_n_tiles--;
  if (max_log2_n_tiles <  0)
    max_log2_n_tiles = 0;

  for (min_log2_n_tiles = 0;
       (MAX_TILE_WIDTH_B64 << min_log2_n_tiles) < sb_cols;
       min_log2_n_tiles++) {}

  assert(min_log2_n_tiles <= max_log2_n_tiles);

  *min_log2_tile_cols = min_log2_n_tiles;
  *max_log2_tile_cols = max_log2_n_tiles;
}
