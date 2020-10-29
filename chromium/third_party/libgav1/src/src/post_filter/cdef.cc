// Copyright 2020 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "src/post_filter.h"
#include "src/utils/blocking_counter.h"
#include "src/utils/compiler_attributes.h"
#include "src/utils/constants.h"

namespace libgav1 {
namespace {

constexpr int kStep64x64 = 16;  // =64/4.
constexpr int kCdefSkip = 8;

constexpr uint8_t kCdefUvDirection[2][2][8] = {
    {{0, 1, 2, 3, 4, 5, 6, 7}, {1, 2, 2, 2, 3, 4, 6, 0}},
    {{7, 0, 2, 4, 5, 6, 6, 6}, {0, 1, 2, 3, 4, 5, 6, 7}}};

template <typename Pixel>
void CopyRowForCdef(const Pixel* src, int block_width, int unit_width,
                    bool is_frame_left, bool is_frame_right,
                    uint16_t* const dst) {
  if (sizeof(src[0]) == sizeof(dst[0])) {
    if (is_frame_left) {
      Memset(dst - kCdefBorder, kCdefLargeValue, kCdefBorder);
    } else {
      memcpy(dst - kCdefBorder, src - kCdefBorder,
             kCdefBorder * sizeof(dst[0]));
    }
    memcpy(dst, src, block_width * sizeof(dst[0]));
    if (is_frame_right) {
      Memset(dst + block_width, kCdefLargeValue,
             unit_width + kCdefBorder - block_width);
    } else {
      memcpy(dst + block_width, src + block_width,
             (unit_width + kCdefBorder - block_width) * sizeof(dst[0]));
    }
    return;
  }
  for (int x = -kCdefBorder; x < 0; ++x) {
    dst[x] = is_frame_left ? static_cast<uint16_t>(kCdefLargeValue) : src[x];
  }
  for (int x = 0; x < block_width; ++x) {
    dst[x] = src[x];
  }
  for (int x = block_width; x < unit_width + kCdefBorder; ++x) {
    dst[x] = is_frame_right ? static_cast<uint16_t>(kCdefLargeValue) : src[x];
  }
}

// For |height| rows, copy |width| pixels of size |pixel_size| from |src| to
// |dst|.
void CopyPixels(const uint8_t* src, int src_stride, uint8_t* dst,
                int dst_stride, int width, int height, size_t pixel_size) {
  int y = height;
  do {
    memcpy(dst, src, width * pixel_size);
    src += src_stride;
    dst += dst_stride;
  } while (--y != 0);
}

}  // namespace

uint8_t* PostFilter::GetCdefBufferAndStride(const int start_x,
                                            const int start_y, const int plane,
                                            const int window_buffer_plane_size,
                                            int* cdef_stride) const {
  if (thread_pool_ != nullptr) {
    // write output to threaded_window_buffer.
    *cdef_stride = window_buffer_width_ * pixel_size_;
    const int column_window =
        start_x % (window_buffer_width_ >> subsampling_x_[plane]);
    const int row_window =
        start_y % (window_buffer_height_ >> subsampling_y_[plane]);
    return threaded_window_buffer_ + plane * window_buffer_plane_size +
           row_window * (*cdef_stride) + column_window * pixel_size_;
  }
  // write output to |cdef_buffer_|.
  *cdef_stride = frame_buffer_.stride(plane);
  return cdef_buffer_[plane] + start_y * (*cdef_stride) + start_x * pixel_size_;
}

template <typename Pixel>
void PostFilter::PrepareCdefBlock(int block_width4x4, int block_height4x4,
                                  int row4x4, int column4x4,
                                  uint16_t* cdef_source, ptrdiff_t cdef_stride,
                                  const bool y_plane) {
  assert(y_plane || planes_ == kMaxPlanes);
  const int max_planes = y_plane ? 1 : kMaxPlanes;
  const int8_t subsampling_x = y_plane ? 0 : subsampling_x_[kPlaneU];
  const int8_t subsampling_y = y_plane ? 0 : subsampling_y_[kPlaneU];
  const int start_x = MultiplyBy4(column4x4) >> subsampling_x;
  const int start_y = MultiplyBy4(row4x4) >> subsampling_y;
  const int plane_width = RightShiftWithRounding(width_, subsampling_x);
  const int plane_height = RightShiftWithRounding(height_, subsampling_y);
  const int block_width = MultiplyBy4(block_width4x4) >> subsampling_x;
  const int block_height = MultiplyBy4(block_height4x4) >> subsampling_y;
  // unit_width, unit_height are the same as block_width, block_height unless
  // it reaches the frame boundary, where block_width < 64 or
  // block_height < 64. unit_width, unit_height guarantee we build blocks on
  // a multiple of 8.
  const int unit_width = Align(block_width, 8 >> subsampling_x);
  const int unit_height = Align(block_height, 8 >> subsampling_y);
  const bool is_frame_left = column4x4 == 0;
  const bool is_frame_right = start_x + block_width >= plane_width;
  const bool is_frame_top = row4x4 == 0;
  const bool is_frame_bottom = start_y + block_height >= plane_height;
  const int y_offset = is_frame_top ? 0 : kCdefBorder;

  for (int plane = y_plane ? kPlaneY : kPlaneU; plane < max_planes; ++plane) {
    uint16_t* cdef_src = cdef_source + plane * kCdefUnitSizeWithBorders *
                                           kCdefUnitSizeWithBorders;
    const int src_stride = frame_buffer_.stride(plane) / sizeof(Pixel);
    const Pixel* src_buffer =
        reinterpret_cast<const Pixel*>(source_buffer_[plane]) +
        (start_y - y_offset) * src_stride + start_x;

    // All the copying code will use negative indices for populating the left
    // border. So the starting point is set to kCdefBorder.
    cdef_src += kCdefBorder;

    // Copy the top 2 rows.
    if (is_frame_top) {
      for (int y = 0; y < kCdefBorder; ++y) {
        Memset(cdef_src - kCdefBorder, kCdefLargeValue,
               unit_width + 2 * kCdefBorder);
        cdef_src += cdef_stride;
      }
    } else {
      for (int y = 0; y < kCdefBorder; ++y) {
        CopyRowForCdef(src_buffer, block_width, unit_width, is_frame_left,
                       is_frame_right, cdef_src);
        src_buffer += src_stride;
        cdef_src += cdef_stride;
      }
    }

    // Copy the body.
    int y = block_height;
    do {
      CopyRowForCdef(src_buffer, block_width, unit_width, is_frame_left,
                     is_frame_right, cdef_src);
      cdef_src += cdef_stride;
      src_buffer += src_stride;
    } while (--y != 0);

    // Copy the bottom 2 rows.
    if (is_frame_bottom) {
      do {
        Memset(cdef_src - kCdefBorder, kCdefLargeValue,
               unit_width + 2 * kCdefBorder);
        cdef_src += cdef_stride;
      } while (++y < kCdefBorder + unit_height - block_height);
    } else {
      do {
        CopyRowForCdef(src_buffer, block_width, unit_width, is_frame_left,
                       is_frame_right, cdef_src);
        src_buffer += src_stride;
        cdef_src += cdef_stride;
      } while (++y < kCdefBorder + unit_height - block_height);
    }
  }
}

template <typename Pixel>
void PostFilter::ApplyCdefForOneUnit(uint16_t* cdef_block, const int index,
                                     const int block_width4x4,
                                     const int block_height4x4,
                                     const int row4x4_start,
                                     const int column4x4_start) {
  // Cdef operates in 8x8 blocks (4x4 for chroma with subsampling).
  static constexpr int kStep = 8;
  static constexpr int kStep4x4 = 2;

  const int window_buffer_plane_size =
      window_buffer_width_ * window_buffer_height_ * sizeof(Pixel);
  int cdef_buffer_row_base_stride[kMaxPlanes];
  int cdef_buffer_stride[kMaxPlanes];
  uint8_t* cdef_buffer_row_base[kMaxPlanes];
  int src_buffer_row_base_stride[kMaxPlanes];
  const uint8_t* src_buffer_row_base[kMaxPlanes];
  int column_step[kMaxPlanes];
  assert(planes_ >= 1);
  for (int plane = kPlaneY; plane < planes_; ++plane) {
    const int start_y = MultiplyBy4(row4x4_start) >> subsampling_y_[plane];
    const int start_x = MultiplyBy4(column4x4_start) >> subsampling_x_[plane];
    cdef_buffer_row_base[plane] = GetCdefBufferAndStride(
        start_x, start_y, plane, window_buffer_plane_size,
        &cdef_buffer_stride[plane]);
    cdef_buffer_row_base_stride[plane] =
        cdef_buffer_stride[plane] * (kStep >> subsampling_y_[plane]);
    src_buffer_row_base[plane] = source_buffer_[plane] +
                                 start_y * frame_buffer_.stride(plane) +
                                 start_x * sizeof(Pixel);
    src_buffer_row_base_stride[plane] =
        frame_buffer_.stride(plane) * (kStep >> subsampling_y_[plane]);
    column_step[plane] = (kStep >> subsampling_x_[plane]) * sizeof(Pixel);
  }

  if (index == -1) {
    for (int plane = kPlaneY; plane < planes_; ++plane) {
      CopyPixels(src_buffer_row_base[plane], frame_buffer_.stride(plane),
                 cdef_buffer_row_base[plane], cdef_buffer_stride[plane],
                 MultiplyBy4(block_width4x4) >> subsampling_x_[plane],
                 MultiplyBy4(block_height4x4) >> subsampling_y_[plane],
                 sizeof(Pixel));
    }
    return;
  }

  PrepareCdefBlock<Pixel>(block_width4x4, block_height4x4, row4x4_start,
                          column4x4_start, cdef_block, kCdefUnitSizeWithBorders,
                          true);

  // Stored direction used during the u/v pass.  If bit 3 is set, then block is
  // a skip.
  int direction_y[8 * 8];
  int y_index = 0;

  const uint8_t y_primary_strength =
      frame_header_.cdef.y_primary_strength[index];
  const uint8_t y_secondary_strength =
      frame_header_.cdef.y_secondary_strength[index];
  // y_strength_index is 0 for both primary and secondary strengths being
  // non-zero, 1 for primary only, 2 for secondary only. This will be updated
  // with y_primary_strength after variance is applied.
  int y_strength_index = static_cast<int>(y_secondary_strength == 0);

  const bool compute_direction_and_variance =
      (y_primary_strength | frame_header_.cdef.uv_primary_strength[index]) != 0;
  BlockParameters* const* bp_row0_base =
      block_parameters_.Address(row4x4_start, column4x4_start);
  BlockParameters* const* bp_row1_base =
      bp_row0_base + block_parameters_.columns4x4();
  const int bp_stride = MultiplyBy2(block_parameters_.columns4x4());
  int row4x4 = row4x4_start;
  do {
    uint8_t* cdef_buffer_base = cdef_buffer_row_base[kPlaneY];
    const uint8_t* src_buffer_base = src_buffer_row_base[kPlaneY];
    BlockParameters* const* bp0 = bp_row0_base;
    BlockParameters* const* bp1 = bp_row1_base;
    int column4x4 = column4x4_start;
    do {
      const int block_width = kStep;
      const int block_height = kStep;
      const int cdef_stride = cdef_buffer_stride[kPlaneY];
      uint8_t* const cdef_buffer = cdef_buffer_base;
      const int src_stride = frame_buffer_.stride(kPlaneY);
      const uint8_t* const src_buffer = src_buffer_base;

      const bool skip = (*bp0)->skip && (*(bp0 + 1))->skip && (*bp1)->skip &&
                        (*(bp1 + 1))->skip;

      if (skip) {  // No cdef filtering.
        direction_y[y_index] = kCdefSkip;
        CopyPixels(src_buffer, src_stride, cdef_buffer, cdef_stride,
                   block_width, block_height, sizeof(Pixel));
      } else {
        // Zero out residual skip flag.
        direction_y[y_index] = 0;

        int variance = 0;
        if (compute_direction_and_variance) {
          dsp_.cdef_direction(src_buffer, src_stride, &direction_y[y_index],
                              &variance);
        }
        const int direction =
            (y_primary_strength == 0) ? 0 : direction_y[y_index];
        const int variance_strength =
            ((variance >> 6) != 0) ? std::min(FloorLog2(variance >> 6), 12) : 0;
        const uint8_t primary_strength =
            (variance != 0)
                ? (y_primary_strength * (4 + variance_strength) + 8) >> 4
                : 0;

        if ((primary_strength | y_secondary_strength) == 0) {
          CopyPixels(src_buffer, src_stride, cdef_buffer, cdef_stride,
                     block_width, block_height, sizeof(Pixel));
        } else {
          uint16_t* cdef_src =
              cdef_block + kCdefBorder * kCdefUnitSizeWithBorders + kCdefBorder;
          cdef_src +=
              (MultiplyBy4(row4x4 - row4x4_start)) * kCdefUnitSizeWithBorders +
              (MultiplyBy4(column4x4 - column4x4_start));
          const int strength_index =
              y_strength_index | (static_cast<int>(primary_strength == 0) << 1);
          dsp_.cdef_filters[1][strength_index](
              cdef_src, kCdefUnitSizeWithBorders, block_height,
              primary_strength, y_secondary_strength,
              frame_header_.cdef.damping, direction, cdef_buffer, cdef_stride);
        }
      }
      cdef_buffer_base += column_step[kPlaneY];
      src_buffer_base += column_step[kPlaneY];

      bp0 += kStep4x4;
      bp1 += kStep4x4;
      column4x4 += kStep4x4;
      y_index++;
    } while (column4x4 < column4x4_start + block_width4x4);

    cdef_buffer_row_base[kPlaneY] += cdef_buffer_row_base_stride[kPlaneY];
    src_buffer_row_base[kPlaneY] += src_buffer_row_base_stride[kPlaneY];
    bp_row0_base += bp_stride;
    bp_row1_base += bp_stride;
    row4x4 += kStep4x4;
  } while (row4x4 < row4x4_start + block_height4x4);

  if (planes_ == kMaxPlanesMonochrome) {
    return;
  }

  const uint8_t uv_primary_strength =
      frame_header_.cdef.uv_primary_strength[index];
  const uint8_t uv_secondary_strength =
      frame_header_.cdef.uv_secondary_strength[index];

  if ((uv_primary_strength | uv_secondary_strength) == 0) {
    for (int plane = kPlaneU; plane <= kPlaneV; ++plane) {
      CopyPixels(src_buffer_row_base[plane], frame_buffer_.stride(plane),
                 cdef_buffer_row_base[plane], cdef_buffer_stride[plane],
                 MultiplyBy4(block_width4x4) >> subsampling_x_[plane],
                 MultiplyBy4(block_height4x4) >> subsampling_y_[plane],
                 sizeof(Pixel));
    }
    return;
  }

  PrepareCdefBlock<Pixel>(block_width4x4, block_height4x4, row4x4_start,
                          column4x4_start, cdef_block, kCdefUnitSizeWithBorders,
                          false);

  // uv_strength_index is 0 for both primary and secondary strengths being
  // non-zero, 1 for primary only, 2 for secondary only.
  const int uv_strength_index =
      (static_cast<int>(uv_primary_strength == 0) << 1) |
      static_cast<int>(uv_secondary_strength == 0);
  for (int plane = kPlaneU; plane <= kPlaneV; ++plane) {
    const int8_t subsampling_x = subsampling_x_[plane];
    const int8_t subsampling_y = subsampling_y_[plane];
    const int block_width = kStep >> subsampling_x;
    const int block_height = kStep >> subsampling_y;
    int row4x4 = row4x4_start;

    y_index = 0;
    do {
      uint8_t* cdef_buffer_base = cdef_buffer_row_base[plane];
      const uint8_t* src_buffer_base = src_buffer_row_base[plane];
      int column4x4 = column4x4_start;
      do {
        const int cdef_stride = cdef_buffer_stride[plane];
        uint8_t* const cdef_buffer = cdef_buffer_base;
        const int src_stride = frame_buffer_.stride(plane);
        const uint8_t* const src_buffer = src_buffer_base;
        const bool skip = (direction_y[y_index] & kCdefSkip) != 0;
        int dual_cdef = 0;

        if (skip) {  // No cdef filtering.
          CopyPixels(src_buffer, src_stride, cdef_buffer, cdef_stride,
                     block_width, block_height, sizeof(Pixel));
        } else {
          // Make sure block pair is not out of bounds.
          if (column4x4 + (kStep4x4 * 2) <= column4x4_start + block_width4x4) {
            // Enable dual processing if subsampling_x is 1.
            dual_cdef = subsampling_x;
          }

          int direction = (uv_primary_strength == 0)
                              ? 0
                              : kCdefUvDirection[subsampling_x][subsampling_y]
                                                [direction_y[y_index]];

          if (dual_cdef != 0) {
            if (uv_primary_strength &&
                direction_y[y_index] != direction_y[y_index + 1]) {
              // Disable dual processing if the second block of the pair does
              // not have the same direction.
              dual_cdef = 0;
            }

            // Disable dual processing if the second block of the pair is a
            // skip.
            if (direction_y[y_index + 1] == kCdefSkip) {
              dual_cdef = 0;
            }
          }

          uint16_t* cdef_src = cdef_block + plane * kCdefUnitSizeWithBorders *
                                                kCdefUnitSizeWithBorders;
          cdef_src += kCdefBorder * kCdefUnitSizeWithBorders + kCdefBorder;
          cdef_src +=
              (MultiplyBy4(row4x4 - row4x4_start) >> subsampling_y) *
                  kCdefUnitSizeWithBorders +
              (MultiplyBy4(column4x4 - column4x4_start) >> subsampling_x);
          // Block width is 8 if either dual_cdef is true or subsampling_x == 0.
          const int width_index = dual_cdef | (subsampling_x ^ 1);
          dsp_.cdef_filters[width_index][uv_strength_index](
              cdef_src, kCdefUnitSizeWithBorders, block_height,
              uv_primary_strength, uv_secondary_strength,
              frame_header_.cdef.damping - 1, direction, cdef_buffer,
              cdef_stride);
        }
        // When dual_cdef is set, the above cdef_filter() will process 2 blocks,
        // so adjust the pointers and indexes for 2 blocks.
        cdef_buffer_base += column_step[plane] << dual_cdef;
        src_buffer_base += column_step[plane] << dual_cdef;
        column4x4 += kStep4x4 << dual_cdef;
        y_index += 1 << dual_cdef;
      } while (column4x4 < column4x4_start + block_width4x4);

      cdef_buffer_row_base[plane] += cdef_buffer_row_base_stride[plane];
      src_buffer_row_base[plane] += src_buffer_row_base_stride[plane];
      row4x4 += kStep4x4;
    } while (row4x4 < row4x4_start + block_height4x4);
  }
}

void PostFilter::ApplyCdefForOneSuperBlockRowHelper(int row4x4,
                                                    int block_height4x4) {
  for (int column4x4 = 0; column4x4 < frame_header_.columns4x4;
       column4x4 += kStep64x64) {
    const int index = cdef_index_[DivideBy16(row4x4)][DivideBy16(column4x4)];
    const int block_width4x4 =
        std::min(kStep64x64, frame_header_.columns4x4 - column4x4);

#if LIBGAV1_MAX_BITDEPTH >= 10
    if (bitdepth_ >= 10) {
      ApplyCdefForOneUnit<uint16_t>(cdef_block_, index, block_width4x4,
                                    block_height4x4, row4x4, column4x4);
      continue;
    }
#endif  // LIBGAV1_MAX_BITDEPTH >= 10
    ApplyCdefForOneUnit<uint8_t>(cdef_block_, index, block_width4x4,
                                 block_height4x4, row4x4, column4x4);
  }
}

void PostFilter::ApplyCdefForOneSuperBlockRow(int row4x4_start, int sb4x4,
                                              bool is_last_row) {
  assert(row4x4_start >= 0);
  assert(DoCdef());
  for (int y = 0; y < sb4x4; y += kStep64x64) {
    const int row4x4 = row4x4_start + y;
    if (row4x4 >= frame_header_.rows4x4) return;

    // Apply cdef for the last 8 rows of the previous superblock row.
    // One exception: If the superblock size is 128x128 and is_last_row is true,
    // then we simply apply cdef for the entire superblock row without any lag.
    // In that case, apply cdef for the previous superblock row only during the
    // first iteration (y == 0).
    if (row4x4 > 0 && (!is_last_row || y == 0)) {
      assert(row4x4 >= 16);
      ApplyCdefForOneSuperBlockRowHelper(row4x4 - 2, 2);
    }

    // Apply cdef for the current superblock row. If this is the last superblock
    // row we apply cdef for all the rows, otherwise we leave out the last 8
    // rows.
    const int block_height4x4 =
        std::min(kStep64x64, frame_header_.rows4x4 - row4x4);
    const int height4x4 = block_height4x4 - (is_last_row ? 0 : 2);
    if (height4x4 > 0) {
      ApplyCdefForOneSuperBlockRowHelper(row4x4, height4x4);
    }
  }
}

template <typename Pixel>
void PostFilter::ApplyCdefForOneRowInWindow(const int row4x4,
                                            const int column4x4_start) {
  uint16_t cdef_block[kCdefUnitSizeWithBorders * kCdefUnitSizeWithBorders * 3];

  for (int column4x4_64x64 = 0;
       column4x4_64x64 < std::min(DivideBy4(window_buffer_width_),
                                  frame_header_.columns4x4 - column4x4_start);
       column4x4_64x64 += kStep64x64) {
    const int column4x4 = column4x4_start + column4x4_64x64;
    const int index = cdef_index_[DivideBy16(row4x4)][DivideBy16(column4x4)];
    const int block_width4x4 =
        std::min(kStep64x64, frame_header_.columns4x4 - column4x4);
    const int block_height4x4 =
        std::min(kStep64x64, frame_header_.rows4x4 - row4x4);

    ApplyCdefForOneUnit<Pixel>(cdef_block, index, block_width4x4,
                               block_height4x4, row4x4, column4x4);
  }
}

// Each thread processes one row inside the window.
// Y, U, V planes are processed together inside one thread.
template <typename Pixel>
void PostFilter::ApplyCdefThreaded() {
  assert((window_buffer_height_ & 63) == 0);
  const int num_workers = thread_pool_->num_threads();
  const int window_buffer_plane_size =
      window_buffer_width_ * window_buffer_height_;
  const int window_buffer_height4x4 = DivideBy4(window_buffer_height_);
  for (int row4x4 = 0; row4x4 < frame_header_.rows4x4;
       row4x4 += window_buffer_height4x4) {
    const int actual_window_height4x4 =
        std::min(window_buffer_height4x4, frame_header_.rows4x4 - row4x4);
    const int vertical_units_per_window =
        DivideBy16(actual_window_height4x4 + 15);
    for (int column4x4 = 0; column4x4 < frame_header_.columns4x4;
         column4x4 += DivideBy4(window_buffer_width_)) {
      const int jobs_for_threadpool =
          vertical_units_per_window * num_workers / (num_workers + 1);
      BlockingCounter pending_jobs(jobs_for_threadpool);
      int job_count = 0;
      for (int row64x64 = 0; row64x64 < actual_window_height4x4;
           row64x64 += kStep64x64) {
        if (job_count < jobs_for_threadpool) {
          thread_pool_->Schedule(
              [this, row4x4, column4x4, row64x64, &pending_jobs]() {
                ApplyCdefForOneRowInWindow<Pixel>(row4x4 + row64x64, column4x4);
                pending_jobs.Decrement();
              });
        } else {
          ApplyCdefForOneRowInWindow<Pixel>(row4x4 + row64x64, column4x4);
        }
        ++job_count;
      }
      pending_jobs.Wait();

      // Copy |threaded_window_buffer_| to |cdef_buffer_|.
      for (int plane = kPlaneY; plane < planes_; ++plane) {
        const ptrdiff_t src_stride =
            frame_buffer_.stride(plane) / sizeof(Pixel);
        const int plane_row = MultiplyBy4(row4x4) >> subsampling_y_[plane];
        const int plane_column =
            MultiplyBy4(column4x4) >> subsampling_x_[plane];
        int copy_width = std::min(frame_header_.columns4x4 - column4x4,
                                  DivideBy4(window_buffer_width_));
        copy_width = MultiplyBy4(copy_width) >> subsampling_x_[plane];
        int copy_height =
            std::min(frame_header_.rows4x4 - row4x4, window_buffer_height4x4);
        copy_height = MultiplyBy4(copy_height) >> subsampling_y_[plane];
        CopyPlane<Pixel>(
            reinterpret_cast<const Pixel*>(threaded_window_buffer_) +
                plane * window_buffer_plane_size,
            window_buffer_width_, copy_width, copy_height,
            reinterpret_cast<Pixel*>(cdef_buffer_[plane]) +
                plane_row * src_stride + plane_column,
            src_stride);
      }
    }
  }
}

void PostFilter::ApplyCdef() {
#if LIBGAV1_MAX_BITDEPTH >= 10
  if (bitdepth_ >= 10) {
    ApplyCdefThreaded<uint16_t>();
    return;
  }
#endif
  ApplyCdefThreaded<uint8_t>();
}

}  // namespace libgav1
