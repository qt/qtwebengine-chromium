#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * VP9
 */

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct vp9_variance_vtable;

#define DEC_MVCOSTS int *mvjcost, int *mvcost[2]
union int_mv;
struct yv12_buffer_config;

void vp9_idct_add_16x16_c(int16_t *input, uint8_t *dest, int stride, int eob);
#define vp9_idct_add_16x16 vp9_idct_add_16x16_c

void vp9_idct_add_8x8_c(int16_t *input, uint8_t *dest, int stride, int eob);
#define vp9_idct_add_8x8 vp9_idct_add_8x8_c

void vp9_idct_add_c(int16_t *input, uint8_t *dest, int stride, int eob);
#define vp9_idct_add vp9_idct_add_c

void vp9_idct_add_32x32_c(int16_t *q, uint8_t *dst, int stride, int eob);
#define vp9_idct_add_32x32 vp9_idct_add_32x32_c

void vp9_d207_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_4x4 vp9_d207_predictor_4x4_c

void vp9_d45_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_4x4 vp9_d45_predictor_4x4_c

void vp9_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_4x4 vp9_d63_predictor_4x4_c

void vp9_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_4x4 vp9_h_predictor_4x4_c

void vp9_d117_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_4x4 vp9_d117_predictor_4x4_c

void vp9_d135_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_4x4 vp9_d135_predictor_4x4_c

void vp9_d153_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_4x4 vp9_d153_predictor_4x4_c

void vp9_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_4x4 vp9_v_predictor_4x4_c

void vp9_tm_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_4x4 vp9_tm_predictor_4x4_c

void vp9_dc_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_4x4 vp9_dc_predictor_4x4_c

void vp9_dc_top_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_4x4 vp9_dc_top_predictor_4x4_c

void vp9_dc_left_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_4x4 vp9_dc_left_predictor_4x4_c

void vp9_dc_128_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_4x4 vp9_dc_128_predictor_4x4_c

void vp9_d207_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_8x8 vp9_d207_predictor_8x8_c

void vp9_d45_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_8x8 vp9_d45_predictor_8x8_c

void vp9_d63_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_8x8 vp9_d63_predictor_8x8_c

void vp9_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_8x8 vp9_h_predictor_8x8_c

void vp9_d117_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_8x8 vp9_d117_predictor_8x8_c

void vp9_d135_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_8x8 vp9_d135_predictor_8x8_c

void vp9_d153_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_8x8 vp9_d153_predictor_8x8_c

void vp9_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_8x8 vp9_v_predictor_8x8_c

void vp9_tm_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_8x8 vp9_tm_predictor_8x8_c

void vp9_dc_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_8x8 vp9_dc_predictor_8x8_c

void vp9_dc_top_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_8x8 vp9_dc_top_predictor_8x8_c

void vp9_dc_left_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_8x8 vp9_dc_left_predictor_8x8_c

void vp9_dc_128_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_8x8 vp9_dc_128_predictor_8x8_c

void vp9_d207_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_16x16 vp9_d207_predictor_16x16_c

void vp9_d45_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_16x16 vp9_d45_predictor_16x16_c

void vp9_d63_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_16x16 vp9_d63_predictor_16x16_c

void vp9_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_16x16 vp9_h_predictor_16x16_c

void vp9_d117_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_16x16 vp9_d117_predictor_16x16_c

void vp9_d135_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_16x16 vp9_d135_predictor_16x16_c

void vp9_d153_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_16x16 vp9_d153_predictor_16x16_c

void vp9_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_16x16 vp9_v_predictor_16x16_c

void vp9_tm_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_16x16 vp9_tm_predictor_16x16_c

void vp9_dc_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_16x16 vp9_dc_predictor_16x16_c

void vp9_dc_top_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_16x16 vp9_dc_top_predictor_16x16_c

void vp9_dc_left_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_16x16 vp9_dc_left_predictor_16x16_c

void vp9_dc_128_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_16x16 vp9_dc_128_predictor_16x16_c

void vp9_d207_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d207_predictor_32x32 vp9_d207_predictor_32x32_c

void vp9_d45_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d45_predictor_32x32 vp9_d45_predictor_32x32_c

void vp9_d63_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d63_predictor_32x32 vp9_d63_predictor_32x32_c

void vp9_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_h_predictor_32x32 vp9_h_predictor_32x32_c

void vp9_d117_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d117_predictor_32x32 vp9_d117_predictor_32x32_c

void vp9_d135_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d135_predictor_32x32 vp9_d135_predictor_32x32_c

void vp9_d153_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_d153_predictor_32x32 vp9_d153_predictor_32x32_c

void vp9_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_v_predictor_32x32 vp9_v_predictor_32x32_c

void vp9_tm_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_tm_predictor_32x32 vp9_tm_predictor_32x32_c

void vp9_dc_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_predictor_32x32 vp9_dc_predictor_32x32_c

void vp9_dc_top_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_top_predictor_32x32 vp9_dc_top_predictor_32x32_c

void vp9_dc_left_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_left_predictor_32x32 vp9_dc_left_predictor_32x32_c

void vp9_dc_128_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define vp9_dc_128_predictor_32x32 vp9_dc_128_predictor_32x32_c

void vp9_add_constant_residual_8x8_c(const int16_t diff, uint8_t *dest, int stride);
#define vp9_add_constant_residual_8x8 vp9_add_constant_residual_8x8_c

void vp9_add_constant_residual_16x16_c(const int16_t diff, uint8_t *dest, int stride);
#define vp9_add_constant_residual_16x16 vp9_add_constant_residual_16x16_c

void vp9_add_constant_residual_32x32_c(const int16_t diff, uint8_t *dest, int stride);
#define vp9_add_constant_residual_32x32 vp9_add_constant_residual_32x32_c

void vp9_mb_lpf_vertical_edge_w_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define vp9_mb_lpf_vertical_edge_w vp9_mb_lpf_vertical_edge_w_c

void vp9_mbloop_filter_vertical_edge_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_mbloop_filter_vertical_edge vp9_mbloop_filter_vertical_edge_c

void vp9_loop_filter_vertical_edge_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_loop_filter_vertical_edge vp9_loop_filter_vertical_edge_c

void vp9_mb_lpf_horizontal_edge_w_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_mb_lpf_horizontal_edge_w vp9_mb_lpf_horizontal_edge_w_c

void vp9_mbloop_filter_horizontal_edge_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_mbloop_filter_horizontal_edge vp9_mbloop_filter_horizontal_edge_c

void vp9_loop_filter_horizontal_edge_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_loop_filter_horizontal_edge vp9_loop_filter_horizontal_edge_c

void vp9_blend_mb_inner_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_inner vp9_blend_mb_inner_c

void vp9_blend_mb_outer_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_outer vp9_blend_mb_outer_c

void vp9_blend_b_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_b vp9_blend_b_c

void vp9_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve_copy vp9_convolve_copy_c

void vp9_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve_avg vp9_convolve_avg_c

void vp9_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8 vp9_convolve8_c

void vp9_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_horiz vp9_convolve8_horiz_c

void vp9_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_vert vp9_convolve8_vert_c

void vp9_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg vp9_convolve8_avg_c

void vp9_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_horiz vp9_convolve8_avg_horiz_c

void vp9_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_vert vp9_convolve8_avg_vert_c

void vp9_short_idct4x4_1_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct4x4_1_add vp9_short_idct4x4_1_add_c

void vp9_short_idct4x4_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct4x4_add vp9_short_idct4x4_add_c

void vp9_short_idct8x8_1_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct8x8_1_add vp9_short_idct8x8_1_add_c

void vp9_short_idct8x8_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct8x8_add vp9_short_idct8x8_add_c

void vp9_short_idct10_8x8_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct10_8x8_add vp9_short_idct10_8x8_add_c

void vp9_short_idct16x16_1_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct16x16_1_add vp9_short_idct16x16_1_add_c

void vp9_short_idct16x16_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct16x16_add vp9_short_idct16x16_add_c

void vp9_short_idct10_16x16_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct10_16x16_add vp9_short_idct10_16x16_add_c

void vp9_short_idct32x32_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct32x32_add vp9_short_idct32x32_add_c

void vp9_short_idct1_32x32_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_32x32 vp9_short_idct1_32x32_c

void vp9_short_iht4x4_add_c(int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp9_short_iht4x4_add vp9_short_iht4x4_add_c

void vp9_short_iht8x8_add_c(int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp9_short_iht8x8_add vp9_short_iht8x8_add_c

void vp9_short_iht16x16_add_c(int16_t *input, uint8_t *output, int pitch, int tx_type);
#define vp9_short_iht16x16_add vp9_short_iht16x16_add_c

void vp9_idct4_1d_c(int16_t *input, int16_t *output);
#define vp9_idct4_1d vp9_idct4_1d_c

void vp9_short_iwalsh4x4_1_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_iwalsh4x4_1_add vp9_short_iwalsh4x4_1_add_c

void vp9_short_iwalsh4x4_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_iwalsh4x4_add vp9_short_iwalsh4x4_add_c

void vp9_rtcd(void);
#include "vpx_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{

}
#endif
#endif
