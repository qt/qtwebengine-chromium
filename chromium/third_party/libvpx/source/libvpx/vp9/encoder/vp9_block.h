/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_BLOCK_H_
#define VP9_ENCODER_VP9_BLOCK_H_

#include "vp9/common/vp9_onyx.h"
#include "vp9/common/vp9_entropymv.h"
#include "vp9/common/vp9_entropy.h"
#include "vpx_ports/mem.h"
#include "vp9/common/vp9_onyxc_int.h"

// motion search site
typedef struct {
  MV mv;
  int offset;
} search_site;

// Structure to hold snapshot of coding context during the mode picking process
typedef struct {
  MODE_INFO mic;
  uint8_t *zcoeff_blk;
  int num_4x4_blk;
  int skip;
  int_mv best_ref_mv;
  int_mv second_best_ref_mv;
  int_mv ref_mvs[MAX_REF_FRAMES][MAX_MV_REF_CANDIDATES];
  int rate;
  int distortion;
  int64_t intra_error;
  int best_mode_index;
  int rddiv;
  int rdmult;
  int hybrid_pred_diff;
  int comp_pred_diff;
  int single_pred_diff;
  int64_t tx_rd_diff[TX_MODES];
  int64_t best_filter_diff[SWITCHABLE_FILTER_CONTEXTS];

  // motion vector cache for adaptive motion search control in partition
  // search loop
  int_mv pred_mv[MAX_REF_FRAMES];

  // Bit flag for each mode whether it has high error in comparison to others.
  unsigned int modes_with_high_error;

  // Bit flag for each ref frame whether it has high error compared to others.
  unsigned int frames_with_high_error;
} PICK_MODE_CONTEXT;

struct macroblock_plane {
  DECLARE_ALIGNED(16, int16_t, src_diff[64 * 64]);
  DECLARE_ALIGNED(16, int16_t, coeff[64 * 64]);
  struct buf_2d src;

  // Quantizer setings
  int16_t *quant;
  int16_t *quant_shift;
  int16_t *zbin;
  int16_t *round;

  // Zbin Over Quant value
  int16_t zbin_extra;
};

/* The [2] dimension is for whether we skip the EOB node (i.e. if previous
 * coefficient in this block was zero) or not. */
typedef unsigned int vp9_coeff_cost[BLOCK_TYPES][REF_TYPES][COEF_BANDS][2]
                                   [PREV_COEF_CONTEXTS][MAX_ENTROPY_TOKENS];

typedef struct macroblock MACROBLOCK;
struct macroblock {
  struct macroblock_plane plane[MAX_MB_PLANE];

  MACROBLOCKD e_mbd;
  int skip_block;

  search_site *ss;
  int ss_count;
  int searches_per_step;

  int errorperbit;
  int sadperbit16;
  int sadperbit4;
  int rddiv;
  int rdmult;
  unsigned int mb_energy;
  unsigned int *mb_activity_ptr;
  int *mb_norm_activity_ptr;
  signed int act_zbin_adj;

  int mv_best_ref_index[MAX_REF_FRAMES];
  unsigned int max_mv_context[MAX_REF_FRAMES];
  unsigned int source_variance;

  int nmvjointcost[MV_JOINTS];
  int nmvcosts[2][MV_VALS];
  int *nmvcost[2];
  int nmvcosts_hp[2][MV_VALS];
  int *nmvcost_hp[2];
  int **mvcost;

  int nmvjointsadcost[MV_JOINTS];
  int nmvsadcosts[2][MV_VALS];
  int *nmvsadcost[2];
  int nmvsadcosts_hp[2][MV_VALS];
  int *nmvsadcost_hp[2];
  int **mvsadcost;

  int mbmode_cost[MB_MODE_COUNT];
  unsigned inter_mode_cost[INTER_MODE_CONTEXTS][INTER_MODES];
  int intra_uv_mode_cost[2][MB_MODE_COUNT];
  int y_mode_costs[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  int switchable_interp_costs[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];

  // These define limits to motion vector components to prevent them
  // from extending outside the UMV borders
  int mv_col_min;
  int mv_col_max;
  int mv_row_min;
  int mv_row_max;

  uint8_t zcoeff_blk[TX_SIZES][256];
  int skip;

  int encode_breakout;

  unsigned char *active_ptr;

  // note that token_costs is the cost when eob node is skipped
  vp9_coeff_cost token_costs[TX_SIZES];
  uint8_t token_cache[1024];

  int optimize;

  // indicate if it is in the rd search loop or encoding process
  int use_lp32x32fdct;
  int skip_encode;

  // Used to store sub partition's choices.
  int fast_ms;
  int_mv pred_mv[MAX_REF_FRAMES];
  int subblock_ref;

  // TODO(jingning): Need to refactor the structure arrays that buffers the
  // coding mode decisions of each partition type.
  PICK_MODE_CONTEXT ab4x4_context[4][4][4];
  PICK_MODE_CONTEXT sb8x4_context[4][4][4];
  PICK_MODE_CONTEXT sb4x8_context[4][4][4];
  PICK_MODE_CONTEXT sb8x8_context[4][4][4];
  PICK_MODE_CONTEXT sb8x16_context[4][4][2];
  PICK_MODE_CONTEXT sb16x8_context[4][4][2];
  PICK_MODE_CONTEXT mb_context[4][4];
  PICK_MODE_CONTEXT sb32x16_context[4][2];
  PICK_MODE_CONTEXT sb16x32_context[4][2];
  // when 4 MBs share coding parameters:
  PICK_MODE_CONTEXT sb32_context[4];
  PICK_MODE_CONTEXT sb32x64_context[2];
  PICK_MODE_CONTEXT sb64x32_context[2];
  PICK_MODE_CONTEXT sb64_context;
  int partition_cost[PARTITION_CONTEXTS][PARTITION_TYPES];

  BLOCK_SIZE b_partitioning[4][4][4];
  BLOCK_SIZE mb_partitioning[4][4];
  BLOCK_SIZE sb_partitioning[4];
  BLOCK_SIZE sb64_partitioning;

  void (*fwd_txm4x4)(const int16_t *input, int16_t *output, int stride);
};

// TODO(jingning): the variables used here are little complicated. need further
// refactoring on organizing the temporary buffers, when recursive
// partition down to 4x4 block size is enabled.
static PICK_MODE_CONTEXT *get_block_context(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;

  switch (bsize) {
    case BLOCK_64X64:
      return &x->sb64_context;
    case BLOCK_64X32:
      return &x->sb64x32_context[xd->sb_index];
    case BLOCK_32X64:
      return &x->sb32x64_context[xd->sb_index];
    case BLOCK_32X32:
      return &x->sb32_context[xd->sb_index];
    case BLOCK_32X16:
      return &x->sb32x16_context[xd->sb_index][xd->mb_index];
    case BLOCK_16X32:
      return &x->sb16x32_context[xd->sb_index][xd->mb_index];
    case BLOCK_16X16:
      return &x->mb_context[xd->sb_index][xd->mb_index];
    case BLOCK_16X8:
      return &x->sb16x8_context[xd->sb_index][xd->mb_index][xd->b_index];
    case BLOCK_8X16:
      return &x->sb8x16_context[xd->sb_index][xd->mb_index][xd->b_index];
    case BLOCK_8X8:
      return &x->sb8x8_context[xd->sb_index][xd->mb_index][xd->b_index];
    case BLOCK_8X4:
      return &x->sb8x4_context[xd->sb_index][xd->mb_index][xd->b_index];
    case BLOCK_4X8:
      return &x->sb4x8_context[xd->sb_index][xd->mb_index][xd->b_index];
    case BLOCK_4X4:
      return &x->ab4x4_context[xd->sb_index][xd->mb_index][xd->b_index];
    default:
      assert(0);
      return NULL;
  }
}

struct rdcost_block_args {
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[16];
  ENTROPY_CONTEXT t_left[16];
  TX_SIZE tx_size;
  int bw;
  int bh;
  int rate;
  int64_t dist;
  int64_t sse;
  int this_rate;
  int64_t this_dist;
  int64_t this_sse;
  int64_t this_rd;
  int64_t best_rd;
  int skip;
  const int16_t *scan, *nb;
};

#endif  // VP9_ENCODER_VP9_BLOCK_H_
