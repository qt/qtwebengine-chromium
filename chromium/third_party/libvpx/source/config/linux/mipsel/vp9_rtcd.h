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

struct loop_filter_info;
struct macroblockd;
struct loop_filter_info;

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

void vp9_copy_mem16x16_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem16x16 vp9_copy_mem16x16_c

void vp9_copy_mem8x8_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem8x8 vp9_copy_mem8x8_c

void vp9_copy_mem8x4_c(const uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch);
#define vp9_copy_mem8x4 vp9_copy_mem8x4_c

void vp9_build_intra_predictors_c(uint8_t *src, int src_stride, uint8_t *pred, int y_stride, int mode, int bw, int bh, int up_available, int left_available, int right_available);
#define vp9_build_intra_predictors vp9_build_intra_predictors_c

void vp9_build_intra_predictors_sby_s_c(struct macroblockd *x, enum BLOCK_SIZE_TYPE bsize);
#define vp9_build_intra_predictors_sby_s vp9_build_intra_predictors_sby_s_c

void vp9_build_intra_predictors_sbuv_s_c(struct macroblockd *x, enum BLOCK_SIZE_TYPE bsize);
#define vp9_build_intra_predictors_sbuv_s vp9_build_intra_predictors_sbuv_s_c

void vp9_intra4x4_predict_c(struct macroblockd *xd, int block, enum BLOCK_SIZE_TYPE bsize, int b_mode, uint8_t *predictor, int pre_stride);
#define vp9_intra4x4_predict vp9_intra4x4_predict_c

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

void vp9_mb_lpf_horizontal_edge_w_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define vp9_mb_lpf_horizontal_edge_w vp9_mb_lpf_horizontal_edge_w_c

void vp9_mbloop_filter_horizontal_edge_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_mbloop_filter_horizontal_edge vp9_mbloop_filter_horizontal_edge_c

void vp9_loop_filter_horizontal_edge_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count);
#define vp9_loop_filter_horizontal_edge vp9_loop_filter_horizontal_edge_c

void vp9_mbpost_proc_down_c(uint8_t *dst, int pitch, int rows, int cols, int flimit);
#define vp9_mbpost_proc_down vp9_mbpost_proc_down_c

void vp9_mbpost_proc_across_ip_c(uint8_t *src, int pitch, int rows, int cols, int flimit);
#define vp9_mbpost_proc_across_ip vp9_mbpost_proc_across_ip_c

void vp9_post_proc_down_and_across_c(const uint8_t *src_ptr, uint8_t *dst_ptr, int src_pixels_per_line, int dst_pixels_per_line, int rows, int cols, int flimit);
#define vp9_post_proc_down_and_across vp9_post_proc_down_and_across_c

void vp9_plane_add_noise_c(uint8_t *Start, char *noise, char blackclamp[16], char whiteclamp[16], char bothclamp[16], unsigned int Width, unsigned int Height, int Pitch);
#define vp9_plane_add_noise vp9_plane_add_noise_c

void vp9_blend_mb_inner_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_inner vp9_blend_mb_inner_c

void vp9_blend_mb_outer_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_mb_outer vp9_blend_mb_outer_c

void vp9_blend_b_c(uint8_t *y, uint8_t *u, uint8_t *v, int y1, int u1, int v1, int alpha, int stride);
#define vp9_blend_b vp9_blend_b_c

void vp9_convolve8_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8 vp9_convolve8_c

void vp9_convolve8_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_horiz vp9_convolve8_horiz_c

void vp9_convolve8_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_vert vp9_convolve8_vert_c

void vp9_convolve8_avg_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg vp9_convolve8_avg_c

void vp9_convolve8_avg_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_horiz vp9_convolve8_avg_horiz_c

void vp9_convolve8_avg_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define vp9_convolve8_avg_vert vp9_convolve8_avg_vert_c

void vp9_short_idct4x4_1_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct4x4_1_add vp9_short_idct4x4_1_add_c

void vp9_short_idct4x4_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct4x4_add vp9_short_idct4x4_add_c

void vp9_short_idct8x8_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct8x8_add vp9_short_idct8x8_add_c

void vp9_short_idct10_8x8_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct10_8x8_add vp9_short_idct10_8x8_add_c

void vp9_short_idct1_8x8_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_8x8 vp9_short_idct1_8x8_c

void vp9_short_idct16x16_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct16x16_add vp9_short_idct16x16_add_c

void vp9_short_idct10_16x16_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct10_16x16_add vp9_short_idct10_16x16_add_c

void vp9_short_idct1_16x16_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_16x16 vp9_short_idct1_16x16_c

void vp9_short_idct32x32_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct32x32_add vp9_short_idct32x32_add_c

void vp9_short_idct1_32x32_c(int16_t *input, int16_t *output);
#define vp9_short_idct1_32x32 vp9_short_idct1_32x32_c

void vp9_short_idct10_32x32_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_idct10_32x32_add vp9_short_idct10_32x32_add_c

void vp9_short_iht4x4_add_c(int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp9_short_iht4x4_add vp9_short_iht4x4_add_c

void vp9_short_iht8x8_add_c(int16_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define vp9_short_iht8x8_add vp9_short_iht8x8_add_c

void vp9_short_iht16x16_add_c(int16_t *input, uint8_t *output, int pitch, int tx_type);
#define vp9_short_iht16x16_add vp9_short_iht16x16_add_c

void vp9_idct4_1d_c(int16_t *input, int16_t *output);
#define vp9_idct4_1d vp9_idct4_1d_c

void vp9_dc_only_idct_add_c(int input_dc, uint8_t *pred_ptr, uint8_t *dst_ptr, int pitch, int stride);
#define vp9_dc_only_idct_add vp9_dc_only_idct_add_c

void vp9_short_iwalsh4x4_1_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_iwalsh4x4_1_add vp9_short_iwalsh4x4_1_add_c

void vp9_short_iwalsh4x4_add_c(int16_t *input, uint8_t *dest, int dest_stride);
#define vp9_short_iwalsh4x4_add vp9_short_iwalsh4x4_add_c

unsigned int vp9_sad32x3_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride, int max_sad);
#define vp9_sad32x3 vp9_sad32x3_c

unsigned int vp9_sad3x32_c(const uint8_t *src_ptr, int  src_stride, const uint8_t *ref_ptr, int ref_stride, int max_sad);
#define vp9_sad3x32 vp9_sad3x32_c

void vp9_rtcd(void);
#include "vpx_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{

}
#endif
#endif
