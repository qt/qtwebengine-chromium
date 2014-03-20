/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_CONFIG_VAQ_H_
#define VP9_ENCODER_VP9_CONFIG_VAQ_H_

#include "vp9/encoder/vp9_onyx_int.h"

unsigned int vp9_vaq_segment_id(int energy);
double vp9_vaq_rdmult_ratio(int energy);
double vp9_vaq_inv_q_ratio(int energy);

void vp9_vaq_init();
void vp9_vaq_frame_setup(VP9_COMP *cpi);

int vp9_block_energy(VP9_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bs);

#endif  // VP9_ENCODER_VP9_CONFIG_VAQ_H_
