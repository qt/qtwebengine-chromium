// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

// static
scoped_ptr<RenderingStatsInstrumentation>
    RenderingStatsInstrumentation::Create() {
  return make_scoped_ptr(new RenderingStatsInstrumentation());
}

RenderingStatsInstrumentation::RenderingStatsInstrumentation()
    : record_rendering_stats_(false) {
}

RenderingStatsInstrumentation::~RenderingStatsInstrumentation() {}

RenderingStats RenderingStatsInstrumentation::GetRenderingStats() {
  base::AutoLock scoped_lock(lock_);
  RenderingStats rendering_stats;
  rendering_stats.main_stats = main_stats_accu_;
  rendering_stats.main_stats.Add(main_stats_);
  rendering_stats.impl_stats = impl_stats_accu_;
  rendering_stats.impl_stats.Add(impl_stats_);
  return rendering_stats;
}

void RenderingStatsInstrumentation::AccumulateAndClearMainThreadStats() {
  main_stats_accu_.Add(main_stats_);
  main_stats_ = MainThreadRenderingStats();
}

void RenderingStatsInstrumentation::AccumulateAndClearImplThreadStats() {
  impl_stats_accu_.Add(impl_stats_);
  impl_stats_ = ImplThreadRenderingStats();
}

base::TimeTicks RenderingStatsInstrumentation::StartRecording() const {
  if (record_rendering_stats_) {
    if (base::TimeTicks::IsThreadNowSupported())
      return base::TimeTicks::ThreadNow();
    return base::TimeTicks::HighResNow();
  }
  return base::TimeTicks();
}

base::TimeDelta RenderingStatsInstrumentation::EndRecording(
    base::TimeTicks start_time) const {
  if (!start_time.is_null()) {
    if (base::TimeTicks::IsThreadNowSupported())
      return base::TimeTicks::ThreadNow() - start_time;
    return base::TimeTicks::HighResNow() - start_time;
  }
  return base::TimeDelta();
}

void RenderingStatsInstrumentation::IncrementFrameCount(int64 count,
                                                        bool main_thread) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  if (main_thread)
    main_stats_.frame_count += count;
  else
    impl_stats_.frame_count += count;
}

void RenderingStatsInstrumentation::AddPaint(base::TimeDelta duration,
                                             int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.paint_time += duration;
  main_stats_.painted_pixel_count += pixels;
}

void RenderingStatsInstrumentation::AddRecord(base::TimeDelta duration,
                                              int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.record_time += duration;
  main_stats_.recorded_pixel_count += pixels;
}

void RenderingStatsInstrumentation::AddRaster(base::TimeDelta duration,
                                              int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.rasterize_time += duration;
  impl_stats_.rasterized_pixel_count += pixels;
}

void RenderingStatsInstrumentation::AddAnalysis(base::TimeDelta duration,
                                                int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.analysis_time += duration;
}

}  // namespace cc
