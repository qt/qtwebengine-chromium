// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/store_source_result.h"

#include <utility>

#include "base/functional/overloaded.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {
using StatusSSR = ::attribution_reporting::mojom::StoreSourceResult;
}  // namespace

StoreSourceResult::StoreSourceResult(StorableSource source,
                                     bool is_noised,
                                     Result result)
    : source_(std::move(source)),
      is_noised_(is_noised),
      result_(std::move(result)) {
  if (const auto* success = absl::get_if<Success>(&result_)) {
    CHECK(!success->min_fake_report_time.has_value() || is_noised_);
  }
}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

StatusSSR StoreSourceResult::status() const {
  return absl::visit(
      base::Overloaded{
          [&](Success) {
            return is_noised_ ? StatusSSR::kSuccessNoised : StatusSSR::kSuccess;
          },
          [](InternalError) { return StatusSSR::kInternalError; },
          [](InsufficientSourceCapacity) {
            return StatusSSR::kInsufficientSourceCapacity;
          },
          [](InsufficientUniqueDestinationCapacity) {
            return StatusSSR::kInsufficientUniqueDestinationCapacity;
          },
          [](ExcessiveReportingOrigins) {
            return StatusSSR::kExcessiveReportingOrigins;
          },
          [](ProhibitedByBrowserPolicy) {
            return StatusSSR::kProhibitedByBrowserPolicy;
          },
          [](DestinationReportingLimitReached) {
            return StatusSSR::kDestinationReportingLimitReached;
          },
          [](DestinationGlobalLimitReached) {
            return StatusSSR::kDestinationGlobalLimitReached;
          },
          [](DestinationBothLimitsReached) {
            return StatusSSR::kDestinationBothLimitsReached;
          },
          [](ReportingOriginsPerSiteLimitReached) {
            return StatusSSR::kReportingOriginsPerSiteLimitReached;
          },
          [](ExceedsMaxChannelCapacity) {
            return StatusSSR::kExceedsMaxChannelCapacity;
          },
          [](ExceedsMaxTriggerStateCardinality) {
            return StatusSSR::kExceedsMaxTriggerStateCardinality;
          },
      },
      result_);
}

}  // namespace content
