/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_DSP_COMMON_H_
#define LIBGAV1_SRC_DSP_COMMON_H_

#include <cstddef>  // ptrdiff_t
#include <cstdint>

#include "src/dsp/constants.h"
#include "src/utils/constants.h"
#include "src/utils/memory.h"

namespace libgav1 {

// Self guided projection filter.
struct SgrProjInfo {
  int index;
  int multiplier[2];
};

struct WienerInfo {
  static const int kVertical = 0;
  static const int kHorizontal = 1;

  alignas(kMaxAlignment) int16_t filter[2][kSubPixelTaps];
};

struct RestorationUnitInfo : public MaxAlignedAllocable {
  LoopRestorationType type;
  SgrProjInfo sgr_proj_info;
  WienerInfo wiener_info;
};

union RestorationBuffer {
  // For self-guided filter.
  alignas(kMaxAlignment) uint16_t sgf_buffer[12 * (kRestorationUnitHeight + 2)];
  // For wiener filter.
  // The array |intermediate| in Section 7.17.4, the intermediate results
  // between the horizontal and vertical filters.
  alignas(kMaxAlignment) uint16_t
      wiener_buffer[(kRestorationUnitHeight + kSubPixelTaps - 1) *
                    kRestorationUnitWidth];
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_DSP_COMMON_H_
