/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "src/trace_processor/db/storage/id_storage.h"
#include "perfetto/base/logging.h"
#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/containers/row_map.h"
#include "src/trace_processor/db/storage/types.h"

namespace perfetto {
namespace trace_processor {
namespace storage {

using Range = RowMap::Range;

BitVector IdStorage::LinearSearch(FilterOp op,
                                  SqlValue sql_val,
                                  RowMap::Range range) const {
  PERFETTO_DCHECK(op == FilterOp::kNe);
  if (sql_val.AsLong() > std::numeric_limits<uint32_t>::max() ||
      sql_val.AsLong() < std::numeric_limits<uint32_t>::min())
    return BitVector(size_, false);

  uint32_t val = static_cast<uint32_t>(sql_val.AsLong());
  BitVector ret(range.start, false);
  ret.Resize(range.end, true);
  ret.Resize(size_, false);

  ret.Clear(val);
  return ret;
}

template <typename Comparator>
BitVector LinearSearchWithComparator(uint32_t val,
                                     uint32_t* indices,
                                     uint32_t indices_size,
                                     Comparator comparator) {
  // Slow path: we compare <64 elements and append to get us to a word
  // boundary.
  const uint32_t* ptr = indices;
  BitVector::Builder builder(indices_size);
  uint32_t front_elements = builder.BitsUntilWordBoundaryOrFull();
  for (uint32_t i = 0; i < front_elements; ++i) {
    builder.Append(comparator(ptr[i], val));
  }
  ptr += front_elements;

  // Fast path: we compare as many groups of 64 elements as we can.
  // This should be very easy for the compiler to auto-vectorize.
  uint32_t fast_path_elements = builder.BitsInCompleteWordsUntilFull();
  for (uint32_t i = 0; i < fast_path_elements; i += BitVector::kBitsInWord) {
    uint64_t word = 0;
    // This part should be optimised by SIMD and is expected to be fast.
    for (uint32_t k = 0; k < BitVector::kBitsInWord; ++k) {
      bool comp_result = comparator(ptr[i + k], val);
      word |= static_cast<uint64_t>(comp_result) << k;
    }
    builder.AppendWord(word);
  }
  ptr += fast_path_elements;

  // Slow path: we compare <64 elements and append to fill the Builder.
  uint32_t back_elements = builder.BitsUntilFull();
  for (uint32_t i = 0; i < back_elements; ++i) {
    builder.Append(comparator(ptr[i], val));
  }
  return std::move(builder).Build();
}

BitVector IdStorage::IndexSearch(FilterOp op,
                                 SqlValue sql_val,
                                 uint32_t* indices,
                                 uint32_t indices_size) const {
  if (op == FilterOp::kIsNotNull)
    return BitVector(indices_size, true);

  if (op == FilterOp::kIsNull || op == FilterOp::kGlob || sql_val.is_null() ||
      sql_val.AsLong() > std::numeric_limits<uint32_t>::max() ||
      sql_val.AsLong() < std::numeric_limits<uint32_t>::min())
    return BitVector(indices_size, false);

  uint32_t val = static_cast<uint32_t>(sql_val.AsLong());

  switch (op) {
    case FilterOp::kEq:
      return LinearSearchWithComparator(val, indices, indices_size,
                                        std::equal_to<uint32_t>());
    case FilterOp::kNe:
      return LinearSearchWithComparator(val, indices, indices_size,
                                        std::not_equal_to<uint32_t>());
    case FilterOp::kLe:
      return LinearSearchWithComparator(val, indices, indices_size,
                                        std::less_equal<uint32_t>());
    case FilterOp::kLt:
      return LinearSearchWithComparator(val, indices, indices_size,
                                        std::less<uint32_t>());
    case FilterOp::kGt:
      return LinearSearchWithComparator(val, indices, indices_size,
                                        std::greater<uint32_t>());
    case FilterOp::kGe:
      return LinearSearchWithComparator(val, indices, indices_size,
                                        std::greater_equal<uint32_t>());
    case FilterOp::kGlob:
    case FilterOp::kIsNotNull:
    case FilterOp::kIsNull:
      PERFETTO_FATAL("Illegal argument");
  }
  PERFETTO_FATAL("FilterOp not matched");
}

RowMap::Range IdStorage::BinarySearchIntrinsic(FilterOp op,
                                               SqlValue sql_val,
                                               Range range) const {
  PERFETTO_DCHECK(range.end <= size_);
  if (op == FilterOp::kEq) {
    int64_t long_val = sql_val.AsLong();
    auto end = static_cast<uint32_t>(
        std::min(long_val, static_cast<int64_t>(range.end - 1)));
    auto start = static_cast<uint32_t>(
        std::max(long_val, static_cast<int64_t>(range.start)));
    return Range(end, end + (start <= end));
  }

  if (op == FilterOp::kIsNotNull)
    return range;

  if (op == FilterOp::kIsNull || op == FilterOp::kGlob || sql_val.is_null() ||
      sql_val.AsLong() > std::numeric_limits<uint32_t>::max() ||
      sql_val.AsLong() < std::numeric_limits<uint32_t>::min())
    return RowMap::Range();

  uint32_t val = static_cast<uint32_t>(sql_val.AsLong());

  switch (op) {
    case FilterOp::kLe:
      return RowMap::Range(range.start, std::min(val + 1, range.end));
    case FilterOp::kLt:
      return RowMap::Range(range.start, std::min(val, range.end));
    case FilterOp::kGe:
      return RowMap::Range(std::max(val, range.start), range.end);
    case FilterOp::kGt:
      return RowMap::Range(std::max(val + 1, range.start), range.end);
    case FilterOp::kEq:
    case FilterOp::kNe:
    case FilterOp::kIsNull:
    case FilterOp::kIsNotNull:
    case FilterOp::kGlob:
      return RowMap::Range();
  }
  return RowMap::Range();
}

void IdStorage::StableSort(uint32_t* indices, uint32_t indices_size) const {
  Sort(indices, indices_size);
}

void IdStorage::Sort(uint32_t* indices, uint32_t indices_size) const {
  std::sort(indices, indices + indices_size);
}

}  // namespace storage
}  // namespace trace_processor
}  // namespace perfetto
