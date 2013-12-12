// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/stats_histogram.h"

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "net/disk_cache/stats.h"

namespace disk_cache {

using base::BucketRanges;
using base::Histogram;
using base::HistogramSamples;
using base::SampleVector;
using base::StatisticsRecorder;

StatsHistogram::StatsHistogram(const std::string& name,
                               Sample minimum,
                               Sample maximum,
                               const BucketRanges* ranges,
                               const Stats* stats)
    : Histogram(name, minimum, maximum, ranges),
      stats_(stats) {}

StatsHistogram::~StatsHistogram() {}

// static
void StatsHistogram::InitializeBucketRanges(const Stats* stats,
                                            BucketRanges* ranges) {
  for (size_t i = 0; i < ranges->size(); ++i) {
    ranges->set_range(i, stats->GetBucketRange(i));
  }
  ranges->ResetChecksum();
}

StatsHistogram* StatsHistogram::FactoryGet(const std::string& name,
                                           const Stats* stats) {
  Sample minimum = 1;
  Sample maximum = disk_cache::Stats::kDataSizesLength - 1;
  size_t bucket_count = disk_cache::Stats::kDataSizesLength;
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    DCHECK(stats);

    // To avoid racy destruction at shutdown, the following will be leaked.
    BucketRanges* ranges = new BucketRanges(bucket_count + 1);
    InitializeBucketRanges(stats, ranges);
    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);

    // To avoid racy destruction at shutdown, the following will be leaked.
    StatsHistogram* stats_histogram =
        new StatsHistogram(name, minimum, maximum, registered_ranges, stats);
    stats_histogram->SetFlags(kUmaTargetedHistogramFlag);
    histogram = StatisticsRecorder::RegisterOrDeleteDuplicate(stats_histogram);
  }

  DCHECK(base::HISTOGRAM == histogram->GetHistogramType());
  DCHECK(histogram->HasConstructionArguments(minimum, maximum, bucket_count));

  // We're preparing for an otherwise unsafe upcast by ensuring we have the
  // proper class type.
  StatsHistogram* return_histogram = static_cast<StatsHistogram*>(histogram);
  return return_histogram;
}

void StatsHistogram::Disable() {
  stats_ = NULL;
}

scoped_ptr<HistogramSamples> StatsHistogram::SnapshotSamples() const {
  scoped_ptr<SampleVector> samples(new SampleVector(bucket_ranges()));
  if (stats_)
     stats_->Snapshot(samples.get());

  // Only report UMA data once.
  StatsHistogram* mutable_me = const_cast<StatsHistogram*>(this);
  mutable_me->ClearFlags(kUmaTargetedHistogramFlag);

  return samples.PassAs<HistogramSamples>();
}

int StatsHistogram::FindCorruption(const HistogramSamples& samples) const {
  // This class won't monitor inconsistencies.
  return HistogramBase::NO_INCONSISTENCIES;
}

}  // namespace disk_cache
