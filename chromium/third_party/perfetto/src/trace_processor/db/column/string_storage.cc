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

#include "src/trace_processor/db/column/string_storage.h"

#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/ext/base/string_utils.h"
#include "protos/perfetto/trace_processor/serialization.pbzero.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/containers/null_term_string_view.h"
#include "src/trace_processor/containers/row_map.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/db/column/column.h"
#include "src/trace_processor/db/column/types.h"

#include "src/trace_processor/db/column/utils.h"
#include "src/trace_processor/tp_metatrace.h"
#include "src/trace_processor/util/glob.h"
#include "src/trace_processor/util/regex.h"

namespace perfetto {
namespace trace_processor {
namespace column {

namespace {

struct Greater {
  bool operator()(StringPool::Id lhs, NullTermStringView rhs) const {
    return lhs != StringPool::Id::Null() && pool_->Get(lhs) > rhs;
  }
  const StringPool* pool_;
};

struct GreaterEqual {
  bool operator()(StringPool::Id lhs, NullTermStringView rhs) const {
    return lhs != StringPool::Id::Null() && pool_->Get(lhs) >= rhs;
  }
  const StringPool* pool_;
};

struct Less {
  bool operator()(StringPool::Id lhs, NullTermStringView rhs) const {
    return lhs != StringPool::Id::Null() && pool_->Get(lhs) < rhs;
  }
  const StringPool* pool_;
};

struct LessEqual {
  bool operator()(StringPool::Id lhs, NullTermStringView rhs) const {
    return lhs != StringPool::Id::Null() && pool_->Get(lhs) <= rhs;
  }
  const StringPool* pool_;
};

struct NotEqual {
  bool operator()(StringPool::Id lhs, StringPool::Id rhs) {
    return lhs != StringPool::Id::Null() && lhs != rhs;
  }
};

struct Glob {
  bool operator()(StringPool::Id lhs, util::GlobMatcher& matcher) const {
    return lhs != StringPool::Id::Null() && matcher.Matches(pool_->Get(lhs));
  }
  const StringPool* pool_;
};

struct GlobFullStringPool {
  GlobFullStringPool(StringPool* pool, util::GlobMatcher& matcher)
      : pool_(pool), matches_(pool->MaxSmallStringId().raw_id()) {
    PERFETTO_DCHECK(!pool->HasLargeString());
    for (auto it = pool->CreateIterator(); it; ++it) {
      auto id = it.StringId();
      matches_[id.raw_id()] = matcher.Matches(pool->Get(id));
    }
  }
  bool operator()(StringPool::Id lhs, StringPool::Id) {
    return lhs != StringPool::Id::Null() && matches_[lhs.raw_id()];
  }
  StringPool* pool_;
  std::vector<uint8_t> matches_;
};

struct Regex {
  bool operator()(StringPool::Id lhs, regex::Regex& pattern) const {
    return lhs != StringPool::Id::Null() &&
           pattern.Search(pool_->Get(lhs).c_str());
  }
  const StringPool* pool_;
};

struct RegexFullStringPool {
  RegexFullStringPool(StringPool* pool, const regex::Regex& regex)
      : pool_(pool), matches_(pool->MaxSmallStringId().raw_id()) {
    PERFETTO_DCHECK(!pool->HasLargeString());
    for (auto it = pool->CreateIterator(); it; ++it) {
      auto id = it.StringId();
      matches_[id.raw_id()] =
          id != StringPool::Id::Null() && regex.Search(pool_->Get(id).c_str());
    }
  }
  bool operator()(StringPool::Id lhs, StringPool::Id) {
    return matches_[lhs.raw_id()];
  }
  StringPool* pool_;
  std::vector<uint8_t> matches_;
};

struct IsNull {
  bool operator()(StringPool::Id lhs, StringPool::Id) const {
    return lhs == StringPool::Id::Null();
  }
};

struct IsNotNull {
  bool operator()(StringPool::Id lhs, StringPool::Id) const {
    return lhs != StringPool::Id::Null();
  }
};

uint32_t LowerBoundIntrinsic(StringPool* pool,
                             const StringPool::Id* data,
                             NullTermStringView val,
                             Range search_range) {
  Less comp{pool};
  auto lower =
      std::lower_bound(data + search_range.start, data + search_range.end, val,
                       [comp](StringPool::Id id, NullTermStringView val) {
                         return comp(id, val);
                       });
  return static_cast<uint32_t>(std::distance(data, lower));
}

uint32_t UpperBoundIntrinsic(StringPool* pool,
                             const StringPool::Id* data,
                             NullTermStringView val,
                             Range search_range) {
  Greater comp{pool};
  auto upper =
      std::upper_bound(data + search_range.start, data + search_range.end, val,
                       [comp](NullTermStringView val, StringPool::Id id) {
                         return comp(id, val);
                       });
  return static_cast<uint32_t>(std::distance(data, upper));
}

}  // namespace

SearchValidationResult StringStorage::ValidateSearchConstraints(
    SqlValue val,
    FilterOp op) const {
  // Type checks.
  switch (val.type) {
    case SqlValue::kNull:
    case SqlValue::kString:
      break;
    case SqlValue::kLong:
    case SqlValue::kDouble:
      // Any string is always more than any numeric.
      if (op == FilterOp::kGt || op == FilterOp::kGe) {
        return SearchValidationResult::kAllData;
      }
      return SearchValidationResult::kNoData;
    case SqlValue::kBytes:
      return SearchValidationResult::kNoData;
  }

  return SearchValidationResult::kOk;
}

RangeOrBitVector StringStorage::Search(FilterOp op,
                                       SqlValue sql_val,
                                       Range search_range) const {
  PERFETTO_TP_TRACE(metatrace::Category::DB, "StringStorage::Search",
                    [&search_range, op](metatrace::Record* r) {
                      r->AddArg("Start", std::to_string(search_range.start));
                      r->AddArg("End", std::to_string(search_range.end));
                      r->AddArg("Op",
                                std::to_string(static_cast<uint32_t>(op)));
                    });

  if (is_sorted_) {
    switch (op) {
      case FilterOp::kEq:
      case FilterOp::kGe:
      case FilterOp::kGt:
      case FilterOp::kLe:
      case FilterOp::kLt:
        return RangeOrBitVector(
            BinarySearchIntrinsic(op, sql_val, search_range));
      case FilterOp::kNe: {
        // Not equal is a special operation on binary search, as it doesn't
        // define a range, and rather just `not` range returned with `equal`
        // operation.
        Range r = BinarySearchIntrinsic(FilterOp::kEq, sql_val, search_range);
        BitVector bv(r.start, true);
        bv.Resize(r.end);
        bv.Resize(search_range.end, true);
        return RangeOrBitVector(std::move(bv));
      }
      case FilterOp::kGlob:
      case FilterOp::kRegex:
      case FilterOp::kIsNull:
      case FilterOp::kIsNotNull:
        // Those operations can't be binary searched so we fall back on not
        // sorted algorithm.
        break;
    }
  }
  return RangeOrBitVector(LinearSearch(op, sql_val, search_range));
}

RangeOrBitVector StringStorage::IndexSearch(FilterOp op,
                                            SqlValue sql_val,
                                            uint32_t* indices,
                                            uint32_t indices_size,
                                            bool indices_sorted) const {
  PERFETTO_TP_TRACE(metatrace::Category::DB, "StringStorage::IndexSearch",
                    [indices_size, op](metatrace::Record* r) {
                      r->AddArg("Count", std::to_string(indices_size));
                      r->AddArg("Op",
                                std::to_string(static_cast<uint32_t>(op)));
                    });

  if (indices_sorted) {
    return RangeOrBitVector(
        BinarySearchExtrinsic(op, sql_val, indices, indices_size));
  }
  return RangeOrBitVector(
      IndexSearchInternal(op, sql_val, indices, indices_size));
}

BitVector StringStorage::LinearSearch(FilterOp op,
                                      SqlValue sql_val,
                                      Range range) const {
  if (sql_val.is_null() &&
      (op != FilterOp::kIsNotNull && op != FilterOp::kIsNull)) {
    return BitVector(range.end, false);
  }

  if (sql_val.type != SqlValue::kString &&
      (op == FilterOp::kGlob || op == FilterOp::kRegex)) {
    return BitVector(range.end, false);
  }

  StringPool::Id val =
      (op == FilterOp::kIsNull || op == FilterOp::kIsNotNull)
          ? StringPool::Id::Null()
          : string_pool_->InternString(base::StringView(sql_val.AsString()));
  const StringPool::Id* start = values_->data() + range.start;

  BitVector::Builder builder(range.end, range.start);
  switch (op) {
    case FilterOp::kEq:
      utils::LinearSearchWithComparator(
          val, start, std::equal_to<StringPool::Id>(), builder);
      break;
    case FilterOp::kNe:
      utils::LinearSearchWithComparator(val, start, NotEqual(), builder);
      break;
    case FilterOp::kLe:
      utils::LinearSearchWithComparator(string_pool_->Get(val), start,
                                        LessEqual{string_pool_}, builder);
      break;
    case FilterOp::kLt:
      utils::LinearSearchWithComparator(string_pool_->Get(val), start,
                                        Less{string_pool_}, builder);
      break;
    case FilterOp::kGt:
      utils::LinearSearchWithComparator(string_pool_->Get(val), start,
                                        Greater{string_pool_}, builder);
      break;
    case FilterOp::kGe:
      utils::LinearSearchWithComparator(string_pool_->Get(val), start,
                                        GreaterEqual{string_pool_}, builder);
      break;
    case FilterOp::kGlob: {
      util::GlobMatcher matcher =
          util::GlobMatcher::FromPattern(sql_val.AsString());

      // If glob pattern doesn't involve any special characters, the function
      // called should be equality.
      if (matcher.IsEquality()) {
        utils::LinearSearchWithComparator(
            val, start, std::equal_to<StringPool::Id>(), builder);
        break;
      }

      // For very big string pools (or small ranges) or pools with large strings
      // run a standard glob function.
      if (range.size() < string_pool_->size() ||
          string_pool_->HasLargeString()) {
        utils::LinearSearchWithComparator(std::move(matcher), start,
                                          Glob{string_pool_}, builder);
        break;
      }

      utils::LinearSearchWithComparator(
          StringPool::Id::Null(), start,
          GlobFullStringPool{string_pool_, matcher}, builder);
      break;
    }
    case FilterOp::kRegex: {
      // Caller should ensure that the regex is valid.
      base::StatusOr<regex::Regex> regex =
          regex::Regex::Create(sql_val.AsString());
      PERFETTO_CHECK(regex.status().ok());

      // For very big string pools (or small ranges) or pools with large
      // strings run a standard regex function.
      if (range.size() < string_pool_->size() ||
          string_pool_->HasLargeString()) {
        utils::LinearSearchWithComparator(std::move(regex.value()), start,
                                          Regex{string_pool_}, builder);
        break;
      }
      utils::LinearSearchWithComparator(
          StringPool::Id::Null(), start,
          RegexFullStringPool{string_pool_, regex.value()}, builder);
      break;
    }
    case FilterOp::kIsNull:
      utils::LinearSearchWithComparator(val, start, IsNull(), builder);
      break;
    case FilterOp::kIsNotNull:
      utils::LinearSearchWithComparator(val, start, IsNotNull(), builder);
  }

  return std::move(builder).Build();
}

RangeOrBitVector StringStorage::IndexSearchInternal(
    FilterOp op,
    SqlValue sql_val,
    uint32_t* indices,
    uint32_t indices_size) const {
  if (sql_val.is_null() &&
      (op != FilterOp::kIsNotNull && op != FilterOp::kIsNull)) {
    return RangeOrBitVector(Range());
  }
  StringPool::Id val =
      (op == FilterOp::kIsNull || op == FilterOp::kIsNotNull)
          ? StringPool::Id::Null()
          : string_pool_->InternString(base::StringView(sql_val.AsString()));
  const StringPool::Id* start = values_->data();
  PERFETTO_TP_TRACE(
      metatrace::Category::DB, "StringStorage::IndexSearch",
      [indices_size, op, &sql_val](metatrace::Record* r) {
        r->AddArg("Count", std::to_string(indices_size));
        r->AddArg("Op", std::to_string(static_cast<uint32_t>(op)));
        r->AddArg("String", sql_val.type == SqlValue::Type::kString
                                ? sql_val.AsString()
                                : "NULL");
      });

  BitVector::Builder builder(indices_size);

  switch (op) {
    case FilterOp::kEq:
      utils::IndexSearchWithComparator(
          val, start, indices, std::equal_to<StringPool::Id>(), builder);
      break;
    case FilterOp::kNe:
      utils::IndexSearchWithComparator(val, start, indices, NotEqual(),
                                       builder);
      break;
    case FilterOp::kLe:
      utils::IndexSearchWithComparator(string_pool_->Get(val), start, indices,
                                       LessEqual{string_pool_}, builder);
      break;
    case FilterOp::kLt:
      utils::IndexSearchWithComparator(string_pool_->Get(val), start, indices,
                                       Less{string_pool_}, builder);
      break;
    case FilterOp::kGt:
      utils::IndexSearchWithComparator(string_pool_->Get(val), start, indices,
                                       Greater{string_pool_}, builder);
      break;
    case FilterOp::kGe:
      utils::IndexSearchWithComparator(string_pool_->Get(val), start, indices,
                                       GreaterEqual{string_pool_}, builder);
      break;
    case FilterOp::kGlob: {
      util::GlobMatcher matcher =
          util::GlobMatcher::FromPattern(sql_val.AsString());
      if (matcher.IsEquality()) {
        utils::IndexSearchWithComparator(
            val, start, indices, std::equal_to<StringPool::Id>(), builder);
        break;
      }
      utils::IndexSearchWithComparator(std::move(matcher), start, indices,
                                       Glob{string_pool_}, builder);
      break;
    }
    case FilterOp::kRegex: {
      base::StatusOr<regex::Regex> regex =
          regex::Regex::Create(sql_val.AsString());
      utils::IndexSearchWithComparator(std::move(regex.value()), start, indices,
                                       Regex{string_pool_}, builder);
      break;
    }
    case FilterOp::kIsNull:
      utils::IndexSearchWithComparator(val, start, indices, IsNull(), builder);
      break;
    case FilterOp::kIsNotNull:
      utils::IndexSearchWithComparator(val, start, indices, IsNotNull(),
                                       builder);
      break;
  }

  return RangeOrBitVector(std::move(builder).Build());
}

Range StringStorage::BinarySearchIntrinsic(FilterOp op,
                                           SqlValue sql_val,
                                           Range search_range) const {
  if (sql_val.type != SqlValue::kString &&
      (op == FilterOp::kGlob || op == FilterOp::kRegex)) {
    return Range();
  }

  StringPool::Id val =
      (op == FilterOp::kIsNull || op == FilterOp::kIsNotNull)
          ? StringPool::Id::Null()
          : string_pool_->InternString(base::StringView(sql_val.AsString()));
  NullTermStringView val_str = string_pool_->Get(val);

  switch (op) {
    case FilterOp::kEq:
      return Range(LowerBoundIntrinsic(string_pool_, values_->data(), val_str,
                                       search_range),
                   UpperBoundIntrinsic(string_pool_, values_->data(), val_str,
                                       search_range));
    case FilterOp::kLe:
      return Range(search_range.start,
                   UpperBoundIntrinsic(string_pool_, values_->data(), val_str,
                                       search_range));
    case FilterOp::kLt:
      return Range(search_range.start,
                   LowerBoundIntrinsic(string_pool_, values_->data(), val_str,
                                       search_range));
    case FilterOp::kGe:
      return Range(LowerBoundIntrinsic(string_pool_, values_->data(), val_str,
                                       search_range),
                   search_range.end);
    case FilterOp::kGt:
      return Range(UpperBoundIntrinsic(string_pool_, values_->data(), val_str,
                                       search_range),
                   search_range.end);

    case FilterOp::kNe:
    case FilterOp::kIsNull:
    case FilterOp::kIsNotNull:
    case FilterOp::kGlob:
    case FilterOp::kRegex:
      PERFETTO_FATAL("Shouldn't be called");
  }
  PERFETTO_FATAL("For gcc");
}

Range StringStorage::BinarySearchExtrinsic(FilterOp,
                                           SqlValue,
                                           uint32_t*,
                                           uint32_t) const {
  PERFETTO_FATAL("Not implemented");
}
void StringStorage::StableSort(uint32_t* indices, uint32_t indices_size) const {
  std::stable_sort(indices, indices + indices_size,
                   [this](uint32_t a_idx, uint32_t b_idx) {
                     return string_pool_->Get(values_->data()[a_idx]) <
                            string_pool_->Get(values_->data()[b_idx]);
                   });
}

void StringStorage::Sort(uint32_t* indices, uint32_t indices_size) const {
  std::sort(indices, indices + indices_size,
            [this](uint32_t a_idx, uint32_t b_idx) {
              return string_pool_->Get(values_->data()[a_idx]) <
                     string_pool_->Get(values_->data()[b_idx]);
            });
}

void StringStorage::Serialize(StorageProto* msg) const {
  auto* string_storage_msg = msg->set_string_storage();
  string_storage_msg->set_is_sorted(is_sorted_);

  string_storage_msg->set_values(
      reinterpret_cast<const uint8_t*>(values_->data()),
      sizeof(StringPool::Id) * size());
}

}  // namespace column
}  // namespace trace_processor
}  // namespace perfetto
