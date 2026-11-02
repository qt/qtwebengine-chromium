// Copyright 2025 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include "ynnpack/xnnpack/dynamic_quantization.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "ynnpack/base/log.h"
#include "ynnpack/include/ynnpack.h"
#include "ynnpack/kernels/dot/dot.h"
#include "ynnpack/subgraph/subgraph.h"

namespace ynn {

ynn_status compute_qd8_params(ynn_subgraph_t subgraph, size_t num_nonbatch_axes,
                              uint32_t input_id, uint32_t output_id,
                              uint32_t scale_id, uint32_t zero_point_id) {
  ynn_value& output = subgraph->value(output_id);

  const ynn_value& scale = subgraph->value(scale_id);
  const ynn_value& zero_point = subgraph->value(zero_point_id);
  assert(scale.rank() == zero_point.rank());
  (void)scale;
  (void)zero_point;

  // XNNPACK defines dynamic quantization as a number of "nonbatch_dims", which
  // is the rank of the quantization data, and is always the last dimensions.
  // We need to compute the reduction over these dimensions.
  int32_t nonbatch_axes[YNN_MAX_TENSOR_RANK];
  for (int32_t i = 0; i < num_nonbatch_axes; ++i) {
    nonbatch_axes[i] = -i - 1;
  }

  uint32_t min_max_id = YNN_INVALID_VALUE_ID;
  ynn_status status = ynn_define_reduce(
      subgraph, ynn_reduce_min_max, num_nonbatch_axes, nonbatch_axes, input_id,
      YNN_INVALID_VALUE_ID, &min_max_id,
      /*flags=*/YNN_NODE_FLAG_KEEP_DIMS);
  if (status != ynn_status_success) {
    return status;
  }

  return ynn_define_dynamic_quantization(subgraph, min_max_id, output.type,
                                         &zero_point_id, &scale_id,
                                         /*flags=*/0);
}

}  // namespace ynn
