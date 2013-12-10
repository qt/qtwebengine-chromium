/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./vpx_config.h"
#include "vp9/encoder/vp9_encodemb.h"
#include "vp9/common/vp9_reconinter.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_tokenize.h"
#include "vp9/common/vp9_reconintra.h"
#include "vpx_mem/vpx_mem.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9_rtcd.h"

DECLARE_ALIGNED(16, extern const uint8_t,
                vp9_pt_energy_class[MAX_ENTROPY_TOKENS]);

void vp9_subtract_block_c(int rows, int cols,
                          int16_t *diff_ptr, ptrdiff_t diff_stride,
                          const uint8_t *src_ptr, ptrdiff_t src_stride,
                          const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int r, c;

  for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++)
      diff_ptr[c] = src_ptr[c] - pred_ptr[c];

    diff_ptr += diff_stride;
    pred_ptr += pred_stride;
    src_ptr  += src_stride;
  }
}

static void inverse_transform_b_4x4_add(MACROBLOCKD *xd, int eob,
                                        int16_t *dqcoeff, uint8_t *dest,
                                        int stride) {
  if (eob <= 1)
    xd->inv_txm4x4_1_add(dqcoeff, dest, stride);
  else
    xd->inv_txm4x4_add(dqcoeff, dest, stride);
}

static void inverse_transform_b_8x8_add(int eob,
                                        int16_t *dqcoeff, uint8_t *dest,
                                        int stride) {
  if (eob <= 1)
    vp9_short_idct8x8_1_add(dqcoeff, dest, stride);
  else if (eob <= 10)
    vp9_short_idct10_8x8_add(dqcoeff, dest, stride);
  else
    vp9_short_idct8x8_add(dqcoeff, dest, stride);
}

static void inverse_transform_b_16x16_add(int eob,
                                          int16_t *dqcoeff, uint8_t *dest,
                                          int stride) {
  if (eob <= 1)
    vp9_short_idct16x16_1_add(dqcoeff, dest, stride);
  else if (eob <= 10)
    vp9_short_idct10_16x16_add(dqcoeff, dest, stride);
  else
    vp9_short_idct16x16_add(dqcoeff, dest, stride);
}

static void subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  struct macroblock_plane *const p = &x->plane[plane];
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int bw = plane_block_width(bsize, pd);
  const int bh = plane_block_height(bsize, pd);

  vp9_subtract_block(bh, bw, p->src_diff, bw,
                     p->src.buf, p->src.stride,
                     pd->dst.buf, pd->dst.stride);
}

void vp9_subtract_sby(MACROBLOCK *x, BLOCK_SIZE bsize) {
  subtract_plane(x, bsize, 0);
}

void vp9_subtract_sbuv(MACROBLOCK *x, BLOCK_SIZE bsize) {
  int i;

  for (i = 1; i < MAX_MB_PLANE; i++)
    subtract_plane(x, bsize, i);
}

void vp9_subtract_sb(MACROBLOCK *x, BLOCK_SIZE bsize) {
  vp9_subtract_sby(x, bsize);
  vp9_subtract_sbuv(x, bsize);
}


#define RDTRUNC(RM,DM,R,D) ( (128+(R)*(RM)) & 0xFF )
typedef struct vp9_token_state vp9_token_state;

struct vp9_token_state {
  int           rate;
  int           error;
  int           next;
  signed char   token;
  short         qc;
};

// TODO: experiments to find optimal multiple numbers
#define Y1_RD_MULT 4
#define UV_RD_MULT 2

static const int plane_rd_mult[4] = {
  Y1_RD_MULT,
  UV_RD_MULT,
};

#define UPDATE_RD_COST()\
{\
  rd_cost0 = RDCOST(rdmult, rddiv, rate0, error0);\
  rd_cost1 = RDCOST(rdmult, rddiv, rate1, error1);\
  if (rd_cost0 == rd_cost1) {\
    rd_cost0 = RDTRUNC(rdmult, rddiv, rate0, error0);\
    rd_cost1 = RDTRUNC(rdmult, rddiv, rate1, error1);\
  }\
}

// This function is a place holder for now but may ultimately need
// to scan previous tokens to work out the correct context.
static int trellis_get_coeff_context(const int16_t *scan,
                                     const int16_t *nb,
                                     int idx, int token,
                                     uint8_t *token_cache) {
  int bak = token_cache[scan[idx]], pt;
  token_cache[scan[idx]] = vp9_pt_energy_class[token];
  pt = get_coef_context(nb, token_cache, idx + 1);
  token_cache[scan[idx]] = bak;
  return pt;
}

static void optimize_b(MACROBLOCK *mb,
                       int plane, int block, BLOCK_SIZE plane_bsize,
                       ENTROPY_CONTEXT *a, ENTROPY_CONTEXT *l,
                       TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &mb->e_mbd;
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int ref = is_inter_block(&xd->this_mi->mbmi);
  vp9_token_state tokens[1025][2];
  unsigned best_index[1025][2];
  const int16_t *coeff_ptr = BLOCK_OFFSET(mb->plane[plane].coeff, block);
  int16_t *qcoeff_ptr;
  int16_t *dqcoeff_ptr;
  int eob = pd->eobs[block], final_eob, sz = 0;
  const int i0 = 0;
  int rc, x, next, i;
  int64_t rdmult, rddiv, rd_cost0, rd_cost1;
  int rate0, rate1, error0, error1, t0, t1;
  int best, band, pt;
  PLANE_TYPE type = pd->plane_type;
  int err_mult = plane_rd_mult[type];
  int default_eob;
  const int16_t *scan, *nb;
  const int mul = 1 + (tx_size == TX_32X32);
  uint8_t token_cache[1024];
  const int ib = txfrm_block_to_raster_block(plane_bsize, tx_size, block);
  const int16_t *dequant_ptr = pd->dequant;
  const uint8_t * band_translate;

  assert((!type && !plane) || (type && plane));
  dqcoeff_ptr = BLOCK_OFFSET(pd->dqcoeff, block);
  qcoeff_ptr = BLOCK_OFFSET(pd->qcoeff, block);
  switch (tx_size) {
    default:
    case TX_4X4:
      default_eob = 16;
      scan = get_scan_4x4(get_tx_type_4x4(type, xd, ib));
      band_translate = vp9_coefband_trans_4x4;
      break;
    case TX_8X8:
      scan = get_scan_8x8(get_tx_type_8x8(type, xd));
      default_eob = 64;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
    case TX_16X16:
      scan = get_scan_16x16(get_tx_type_16x16(type, xd));
      default_eob = 256;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
    case TX_32X32:
      scan = vp9_default_scan_32x32;
      default_eob = 1024;
      band_translate = vp9_coefband_trans_8x8plus;
      break;
  }
  assert(eob <= default_eob);

  /* Now set up a Viterbi trellis to evaluate alternative roundings. */
  rdmult = mb->rdmult * err_mult;
  if (mb->e_mbd.mi_8x8[0]->mbmi.ref_frame[0] == INTRA_FRAME)
    rdmult = (rdmult * 9) >> 4;
  rddiv = mb->rddiv;
  /* Initialize the sentinel node of the trellis. */
  tokens[eob][0].rate = 0;
  tokens[eob][0].error = 0;
  tokens[eob][0].next = default_eob;
  tokens[eob][0].token = DCT_EOB_TOKEN;
  tokens[eob][0].qc = 0;
  *(tokens[eob] + 1) = *(tokens[eob] + 0);
  next = eob;
  for (i = 0; i < eob; i++)
    token_cache[scan[i]] = vp9_pt_energy_class[vp9_dct_value_tokens_ptr[
        qcoeff_ptr[scan[i]]].token];
  nb = vp9_get_coef_neighbors_handle(scan);

  for (i = eob; i-- > i0;) {
    int base_bits, d2, dx;

    rc = scan[i];
    x = qcoeff_ptr[rc];
    /* Only add a trellis state for non-zero coefficients. */
    if (x) {
      int shortcut = 0;
      error0 = tokens[next][0].error;
      error1 = tokens[next][1].error;
      /* Evaluate the first possibility for this state. */
      rate0 = tokens[next][0].rate;
      rate1 = tokens[next][1].rate;
      t0 = (vp9_dct_value_tokens_ptr + x)->token;
      /* Consider both possible successor states. */
      if (next < default_eob) {
        band = get_coef_band(band_translate, i + 1);
        pt = trellis_get_coeff_context(scan, nb, i, t0, token_cache);
        rate0 +=
          mb->token_costs[tx_size][type][ref][band][0][pt]
                         [tokens[next][0].token];
        rate1 +=
          mb->token_costs[tx_size][type][ref][band][0][pt]
                         [tokens[next][1].token];
      }
      UPDATE_RD_COST();
      /* And pick the best. */
      best = rd_cost1 < rd_cost0;
      base_bits = *(vp9_dct_value_cost_ptr + x);
      dx = mul * (dqcoeff_ptr[rc] - coeff_ptr[rc]);
      d2 = dx * dx;
      tokens[i][0].rate = base_bits + (best ? rate1 : rate0);
      tokens[i][0].error = d2 + (best ? error1 : error0);
      tokens[i][0].next = next;
      tokens[i][0].token = t0;
      tokens[i][0].qc = x;
      best_index[i][0] = best;

      /* Evaluate the second possibility for this state. */
      rate0 = tokens[next][0].rate;
      rate1 = tokens[next][1].rate;

      if ((abs(x)*dequant_ptr[rc != 0] > abs(coeff_ptr[rc]) * mul) &&
          (abs(x)*dequant_ptr[rc != 0] < abs(coeff_ptr[rc]) * mul +
                                         dequant_ptr[rc != 0]))
        shortcut = 1;
      else
        shortcut = 0;

      if (shortcut) {
        sz = -(x < 0);
        x -= 2 * sz + 1;
      }

      /* Consider both possible successor states. */
      if (!x) {
        /* If we reduced this coefficient to zero, check to see if
         *  we need to move the EOB back here.
         */
        t0 = tokens[next][0].token == DCT_EOB_TOKEN ?
             DCT_EOB_TOKEN : ZERO_TOKEN;
        t1 = tokens[next][1].token == DCT_EOB_TOKEN ?
             DCT_EOB_TOKEN : ZERO_TOKEN;
      } else {
        t0 = t1 = (vp9_dct_value_tokens_ptr + x)->token;
      }
      if (next < default_eob) {
        band = get_coef_band(band_translate, i + 1);
        if (t0 != DCT_EOB_TOKEN) {
          pt = trellis_get_coeff_context(scan, nb, i, t0, token_cache);
          rate0 += mb->token_costs[tx_size][type][ref][band][!x][pt]
                                  [tokens[next][0].token];
        }
        if (t1 != DCT_EOB_TOKEN) {
          pt = trellis_get_coeff_context(scan, nb, i, t1, token_cache);
          rate1 += mb->token_costs[tx_size][type][ref][band][!x][pt]
                                  [tokens[next][1].token];
        }
      }

      UPDATE_RD_COST();
      /* And pick the best. */
      best = rd_cost1 < rd_cost0;
      base_bits = *(vp9_dct_value_cost_ptr + x);

      if (shortcut) {
        dx -= (dequant_ptr[rc != 0] + sz) ^ sz;
        d2 = dx * dx;
      }
      tokens[i][1].rate = base_bits + (best ? rate1 : rate0);
      tokens[i][1].error = d2 + (best ? error1 : error0);
      tokens[i][1].next = next;
      tokens[i][1].token = best ? t1 : t0;
      tokens[i][1].qc = x;
      best_index[i][1] = best;
      /* Finally, make this the new head of the trellis. */
      next = i;
    }
    /* There's no choice to make for a zero coefficient, so we don't
     *  add a new trellis node, but we do need to update the costs.
     */
    else {
      band = get_coef_band(band_translate, i + 1);
      t0 = tokens[next][0].token;
      t1 = tokens[next][1].token;
      /* Update the cost of each path if we're past the EOB token. */
      if (t0 != DCT_EOB_TOKEN) {
        tokens[next][0].rate +=
            mb->token_costs[tx_size][type][ref][band][1][0][t0];
        tokens[next][0].token = ZERO_TOKEN;
      }
      if (t1 != DCT_EOB_TOKEN) {
        tokens[next][1].rate +=
            mb->token_costs[tx_size][type][ref][band][1][0][t1];
        tokens[next][1].token = ZERO_TOKEN;
      }
      best_index[i][0] = best_index[i][1] = 0;
      /* Don't update next, because we didn't add a new node. */
    }
  }

  /* Now pick the best path through the whole trellis. */
  band = get_coef_band(band_translate, i + 1);
  pt = combine_entropy_contexts(*a, *l);
  rate0 = tokens[next][0].rate;
  rate1 = tokens[next][1].rate;
  error0 = tokens[next][0].error;
  error1 = tokens[next][1].error;
  t0 = tokens[next][0].token;
  t1 = tokens[next][1].token;
  rate0 += mb->token_costs[tx_size][type][ref][band][0][pt][t0];
  rate1 += mb->token_costs[tx_size][type][ref][band][0][pt][t1];
  UPDATE_RD_COST();
  best = rd_cost1 < rd_cost0;
  final_eob = i0 - 1;
  vpx_memset(qcoeff_ptr, 0, sizeof(*qcoeff_ptr) * (16 << (tx_size * 2)));
  vpx_memset(dqcoeff_ptr, 0, sizeof(*dqcoeff_ptr) * (16 << (tx_size * 2)));
  for (i = next; i < eob; i = next) {
    x = tokens[i][best].qc;
    if (x) {
      final_eob = i;
    }
    rc = scan[i];
    qcoeff_ptr[rc] = x;
    dqcoeff_ptr[rc] = (x * dequant_ptr[rc != 0]) / mul;

    next = tokens[i][best].next;
    best = best_index[i][best];
  }
  final_eob++;

  xd->plane[plane].eobs[block] = final_eob;
  *a = *l = (final_eob > 0);
}

void vp9_optimize_b(int plane, int block, BLOCK_SIZE plane_bsize,
                    TX_SIZE tx_size, MACROBLOCK *mb, struct optimize_ctx *ctx) {
  int x, y;
  txfrm_block_to_raster_xy(plane_bsize, tx_size, block, &x, &y);
  optimize_b(mb, plane, block, plane_bsize,
             &ctx->ta[plane][x], &ctx->tl[plane][y], tx_size);
}

static void optimize_init_b(int plane, BLOCK_SIZE bsize,
                            struct encode_b_args *args) {
  const MACROBLOCKD *xd = &args->x->e_mbd;
  const struct macroblockd_plane* const pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
  const int num_4x4_w = num_4x4_blocks_wide_lookup[plane_bsize];
  const int num_4x4_h = num_4x4_blocks_high_lookup[plane_bsize];
  const MB_MODE_INFO *mbmi = &xd->this_mi->mbmi;
  const TX_SIZE tx_size = plane ? get_uv_tx_size(mbmi) : mbmi->tx_size;
  int i;

  switch (tx_size) {
    case TX_4X4:
      vpx_memcpy(args->ctx->ta[plane], pd->above_context,
                 sizeof(ENTROPY_CONTEXT) * num_4x4_w);
      vpx_memcpy(args->ctx->tl[plane], pd->left_context,
                 sizeof(ENTROPY_CONTEXT) * num_4x4_h);
      break;
    case TX_8X8:
      for (i = 0; i < num_4x4_w; i += 2)
        args->ctx->ta[plane][i] = !!*(uint16_t *)&pd->above_context[i];
      for (i = 0; i < num_4x4_h; i += 2)
        args->ctx->tl[plane][i] = !!*(uint16_t *)&pd->left_context[i];
      break;
    case TX_16X16:
      for (i = 0; i < num_4x4_w; i += 4)
        args->ctx->ta[plane][i] = !!*(uint32_t *)&pd->above_context[i];
      for (i = 0; i < num_4x4_h; i += 4)
        args->ctx->tl[plane][i] = !!*(uint32_t *)&pd->left_context[i];
      break;
    case TX_32X32:
      for (i = 0; i < num_4x4_w; i += 8)
        args->ctx->ta[plane][i] = !!*(uint64_t *)&pd->above_context[i];
      for (i = 0; i < num_4x4_h; i += 8)
        args->ctx->tl[plane][i] = !!*(uint64_t *)&pd->left_context[i];
      break;
    default:
      assert(0);
  }
}

void vp9_xform_quant(int plane, int block, BLOCK_SIZE plane_bsize,
                     TX_SIZE tx_size, void *arg) {
  struct encode_b_args* const args = arg;
  MACROBLOCK* const x = args->x;
  MACROBLOCKD* const xd = &x->e_mbd;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int16_t *coeff = BLOCK_OFFSET(p->coeff, block);
  int16_t *qcoeff = BLOCK_OFFSET(pd->qcoeff, block);
  int16_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const int16_t *scan, *iscan;
  uint16_t *eob = &pd->eobs[block];
  const int bwl = b_width_log2(plane_bsize), bw = 1 << bwl;
  const int twl = bwl - tx_size, twmask = (1 << twl) - 1;
  int xoff, yoff;
  int16_t *src_diff;

  switch (tx_size) {
    case TX_32X32:
      scan = vp9_default_scan_32x32;
      iscan = vp9_default_iscan_32x32;
      block >>= 6;
      xoff = 32 * (block & twmask);
      yoff = 32 * (block >> twl);
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      if (x->use_lp32x32fdct)
        vp9_short_fdct32x32_rd(src_diff, coeff, bw * 8);
      else
        vp9_short_fdct32x32(src_diff, coeff, bw * 8);
      vp9_quantize_b_32x32(coeff, 1024, x->skip_block, p->zbin, p->round,
                           p->quant, p->quant_shift, qcoeff, dqcoeff,
                           pd->dequant, p->zbin_extra, eob, scan, iscan);
      break;
    case TX_16X16:
      scan = vp9_default_scan_16x16;
      iscan = vp9_default_iscan_16x16;
      block >>= 4;
      xoff = 16 * (block & twmask);
      yoff = 16 * (block >> twl);
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      x->fwd_txm16x16(src_diff, coeff, bw * 8);
      vp9_quantize_b(coeff, 256, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob, scan, iscan);
      break;
    case TX_8X8:
      scan = vp9_default_scan_8x8;
      iscan = vp9_default_iscan_8x8;
      block >>= 2;
      xoff = 8 * (block & twmask);
      yoff = 8 * (block >> twl);
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      x->fwd_txm8x8(src_diff, coeff, bw * 8);
      vp9_quantize_b(coeff, 64, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob, scan, iscan);
      break;
    case TX_4X4:
      scan = vp9_default_scan_4x4;
      iscan = vp9_default_iscan_4x4;
      xoff = 4 * (block & twmask);
      yoff = 4 * (block >> twl);
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      x->fwd_txm4x4(src_diff, coeff, bw * 8);
      vp9_quantize_b(coeff, 16, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob, scan, iscan);
      break;
    default:
      assert(0);
  }
}

static void encode_block(int plane, int block, BLOCK_SIZE plane_bsize,
                         TX_SIZE tx_size, void *arg) {
  struct encode_b_args *const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int raster_block = txfrm_block_to_raster_block(plane_bsize, tx_size,
                                                       block);

  int16_t *const dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  uint8_t *const dst = raster_block_offset_uint8(plane_bsize, raster_block,
                                                 pd->dst.buf, pd->dst.stride);
  vp9_xform_quant(plane, block, plane_bsize, tx_size, arg);

  if (x->optimize)
    vp9_optimize_b(plane, block, plane_bsize, tx_size, x, args->ctx);

  if (x->skip_encode || pd->eobs[block] == 0)
    return;

  switch (tx_size) {
    case TX_32X32:
      vp9_short_idct32x32_add(dqcoeff, dst, pd->dst.stride);
      break;
    case TX_16X16:
      inverse_transform_b_16x16_add(pd->eobs[block], dqcoeff, dst,
                                    pd->dst.stride);
      break;
    case TX_8X8:
      inverse_transform_b_8x8_add(pd->eobs[block], dqcoeff, dst,
                                  pd->dst.stride);
      break;
    case TX_4X4:
      // this is like vp9_short_idct4x4 but has a special case around eob<=1
      // which is significant (not just an optimization) for the lossless
      // case.
      inverse_transform_b_4x4_add(xd, pd->eobs[block], dqcoeff,
                                  dst, pd->dst.stride);
      break;
    default:
      assert(!"Invalid transform size");
  }
}

void vp9_encode_sby(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  struct encode_b_args arg = {x, &ctx};

  vp9_subtract_sby(x, bsize);
  if (x->optimize)
    optimize_init_b(0, bsize, &arg);

  foreach_transformed_block_in_plane(xd, bsize, 0, encode_block, &arg);
}

void vp9_encode_sb(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  struct encode_b_args arg = {x, &ctx};

  vp9_subtract_sb(x, bsize);

  if (x->optimize) {
    int i;
    for (i = 0; i < MAX_MB_PLANE; ++i)
      optimize_init_b(i, bsize, &arg);
  }

  foreach_transformed_block(xd, bsize, encode_block, &arg);
}

void vp9_encode_block_intra(int plane, int block, BLOCK_SIZE plane_bsize,
                            TX_SIZE tx_size, void *arg) {
  struct encode_b_args* const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->this_mi->mbmi;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int16_t *coeff = BLOCK_OFFSET(p->coeff, block);
  int16_t *qcoeff = BLOCK_OFFSET(pd->qcoeff, block);
  int16_t *dqcoeff = BLOCK_OFFSET(pd->dqcoeff, block);
  const int16_t *scan, *iscan;
  TX_TYPE tx_type;
  MB_PREDICTION_MODE mode;
  const int bwl = b_width_log2(plane_bsize), bw = 1 << bwl;
  const int twl = bwl - tx_size, twmask = (1 << twl) - 1;
  int xoff, yoff;
  uint8_t *src, *dst;
  int16_t *src_diff;
  uint16_t *eob = &pd->eobs[block];

  if (xd->mb_to_right_edge < 0 || xd->mb_to_bottom_edge < 0)
    extend_for_intra(xd, plane_bsize, plane, block, tx_size);

  // if (x->optimize)
  // vp9_optimize_b(plane, block, plane_bsize, tx_size, x, args->ctx);

  switch (tx_size) {
    case TX_32X32:
      scan = vp9_default_scan_32x32;
      iscan = vp9_default_iscan_32x32;
      mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;
      block >>= 6;
      xoff = 32 * (block & twmask);
      yoff = 32 * (block >> twl);
      dst = pd->dst.buf + yoff * pd->dst.stride + xoff;
      src = p->src.buf + yoff * p->src.stride + xoff;
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      vp9_predict_intra_block(xd, block, bwl, TX_32X32, mode,
                              dst, pd->dst.stride, dst, pd->dst.stride);
      vp9_subtract_block(32, 32, src_diff, bw * 4,
                         src, p->src.stride, dst, pd->dst.stride);
      if (x->use_lp32x32fdct)
        vp9_short_fdct32x32_rd(src_diff, coeff, bw * 8);
      else
        vp9_short_fdct32x32(src_diff, coeff, bw * 8);
      vp9_quantize_b_32x32(coeff, 1024, x->skip_block, p->zbin, p->round,
                           p->quant, p->quant_shift, qcoeff, dqcoeff,
                           pd->dequant, p->zbin_extra, eob, scan, iscan);
      if (!x->skip_encode && *eob)
        vp9_short_idct32x32_add(dqcoeff, dst, pd->dst.stride);
      break;
    case TX_16X16:
      tx_type = get_tx_type_16x16(pd->plane_type, xd);
      scan = get_scan_16x16(tx_type);
      iscan = get_iscan_16x16(tx_type);
      mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;
      block >>= 4;
      xoff = 16 * (block & twmask);
      yoff = 16 * (block >> twl);
      dst = pd->dst.buf + yoff * pd->dst.stride + xoff;
      src = p->src.buf + yoff * p->src.stride + xoff;
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      vp9_predict_intra_block(xd, block, bwl, TX_16X16, mode,
                              dst, pd->dst.stride, dst, pd->dst.stride);
      vp9_subtract_block(16, 16, src_diff, bw * 4,
                         src, p->src.stride, dst, pd->dst.stride);
      if (tx_type != DCT_DCT)
        vp9_short_fht16x16(src_diff, coeff, bw * 4, tx_type);
      else
        x->fwd_txm16x16(src_diff, coeff, bw * 8);
      vp9_quantize_b(coeff, 256, x->skip_block, p->zbin, p->round,
                     p->quant, p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob, scan, iscan);
      if (!x->skip_encode && *eob) {
        if (tx_type == DCT_DCT)
          inverse_transform_b_16x16_add(*eob, dqcoeff, dst, pd->dst.stride);
        else
          vp9_short_iht16x16_add(dqcoeff, dst, pd->dst.stride, tx_type);
      }
      break;
    case TX_8X8:
      tx_type = get_tx_type_8x8(pd->plane_type, xd);
      scan = get_scan_8x8(tx_type);
      iscan = get_iscan_8x8(tx_type);
      mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;
      block >>= 2;
      xoff = 8 * (block & twmask);
      yoff = 8 * (block >> twl);
      dst = pd->dst.buf + yoff * pd->dst.stride + xoff;
      src = p->src.buf + yoff * p->src.stride + xoff;
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      vp9_predict_intra_block(xd, block, bwl, TX_8X8, mode,
                              dst, pd->dst.stride, dst, pd->dst.stride);
      vp9_subtract_block(8, 8, src_diff, bw * 4,
                         src, p->src.stride, dst, pd->dst.stride);
      if (tx_type != DCT_DCT)
        vp9_short_fht8x8(src_diff, coeff, bw * 4, tx_type);
      else
        x->fwd_txm8x8(src_diff, coeff, bw * 8);
      vp9_quantize_b(coeff, 64, x->skip_block, p->zbin, p->round, p->quant,
                     p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob, scan, iscan);
      if (!x->skip_encode && *eob) {
        if (tx_type == DCT_DCT)
          inverse_transform_b_8x8_add(*eob, dqcoeff, dst, pd->dst.stride);
        else
          vp9_short_iht8x8_add(dqcoeff, dst, pd->dst.stride, tx_type);
      }
      break;
    case TX_4X4:
      tx_type = get_tx_type_4x4(pd->plane_type, xd, block);
      scan = get_scan_4x4(tx_type);
      iscan = get_iscan_4x4(tx_type);
      if (mbmi->sb_type < BLOCK_8X8 && plane == 0)
        mode = xd->this_mi->bmi[block].as_mode;
      else
        mode = plane == 0 ? mbmi->mode : mbmi->uv_mode;

      xoff = 4 * (block & twmask);
      yoff = 4 * (block >> twl);
      dst = pd->dst.buf + yoff * pd->dst.stride + xoff;
      src = p->src.buf + yoff * p->src.stride + xoff;
      src_diff = p->src_diff + 4 * bw * yoff + xoff;
      vp9_predict_intra_block(xd, block, bwl, TX_4X4, mode,
                              dst, pd->dst.stride, dst, pd->dst.stride);
      vp9_subtract_block(4, 4, src_diff, bw * 4,
                         src, p->src.stride, dst, pd->dst.stride);
      if (tx_type != DCT_DCT)
        vp9_short_fht4x4(src_diff, coeff, bw * 4, tx_type);
      else
        x->fwd_txm4x4(src_diff, coeff, bw * 8);
      vp9_quantize_b(coeff, 16, x->skip_block, p->zbin, p->round, p->quant,
                     p->quant_shift, qcoeff, dqcoeff,
                     pd->dequant, p->zbin_extra, eob, scan, iscan);
      if (!x->skip_encode && *eob) {
        if (tx_type == DCT_DCT)
          // this is like vp9_short_idct4x4 but has a special case around eob<=1
          // which is significant (not just an optimization) for the lossless
          // case.
          inverse_transform_b_4x4_add(xd, *eob, dqcoeff, dst, pd->dst.stride);
        else
          vp9_short_iht4x4_add(dqcoeff, dst, pd->dst.stride, tx_type);
      }
      break;
    default:
      assert(0);
  }
}

void vp9_encode_intra_block_y(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD* const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  struct encode_b_args arg = {x, &ctx};

  foreach_transformed_block_in_plane(xd, bsize, 0, vp9_encode_block_intra,
                                     &arg);
}
void vp9_encode_intra_block_uv(MACROBLOCK *x, BLOCK_SIZE bsize) {
  MACROBLOCKD* const xd = &x->e_mbd;
  struct optimize_ctx ctx;
  struct encode_b_args arg = {x, &ctx};
  foreach_transformed_block_uv(xd, bsize, vp9_encode_block_intra, &arg);
}

