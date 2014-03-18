// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "cc/debug/rendering_stats.h"

namespace cc {

MainThreadRenderingStats::MainThreadRenderingStats()
    : frame_count(0),
      painted_pixel_count(0),
      recorded_pixel_count(0) {}

scoped_refptr<base::debug::ConvertableToTraceFormat>
MainThreadRenderingStats::AsTraceableData() const {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  record_data->SetInteger("frame_count", frame_count);
  record_data->SetDouble("paint_time", paint_time.InSecondsF());
  record_data->SetInteger("painted_pixel_count", painted_pixel_count);
  record_data->SetDouble("record_time", record_time.InSecondsF());
  record_data->SetInteger("recorded_pixel_count", recorded_pixel_count);
  return TracedValue::FromValue(record_data.release());
}

void MainThreadRenderingStats::Add(const MainThreadRenderingStats& other) {
  frame_count += other.frame_count;
  paint_time += other.paint_time;
  painted_pixel_count += other.painted_pixel_count;
  record_time += other.record_time;
  recorded_pixel_count += other.recorded_pixel_count;
}

ImplThreadRenderingStats::ImplThreadRenderingStats()
    : frame_count(0),
      rasterized_pixel_count(0) {}

scoped_refptr<base::debug::ConvertableToTraceFormat>
ImplThreadRenderingStats::AsTraceableData() const {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  record_data->SetInteger("frame_count", frame_count);
  record_data->SetDouble("rasterize_time", rasterize_time.InSecondsF());
  record_data->SetInteger("rasterized_pixel_count", rasterized_pixel_count);
  return TracedValue::FromValue(record_data.release());
}

void ImplThreadRenderingStats::Add(const ImplThreadRenderingStats& other) {
  frame_count += other.frame_count;
  rasterize_time += other.rasterize_time;
  analysis_time += other.analysis_time;
  rasterized_pixel_count += other.rasterized_pixel_count;
}

void RenderingStats::EnumerateFields(Enumerator* enumerator) const {
  enumerator->AddInt64("frameCount",
                       main_stats.frame_count + impl_stats.frame_count);
  enumerator->AddDouble("paintTime",
                        main_stats.paint_time.InSecondsF());
  enumerator->AddInt64("paintedPixelCount",
                       main_stats.painted_pixel_count);
  enumerator->AddDouble("recordTime",
                        main_stats.record_time.InSecondsF());
  enumerator->AddInt64("recordedPixelCount",
                       main_stats.recorded_pixel_count);
  // Combine rasterization and analysis time as a precursor to combining
  // them in the same step internally.
  enumerator->AddDouble("rasterizeTime",
                        impl_stats.rasterize_time.InSecondsF() +
                            impl_stats.analysis_time.InSecondsF());
  enumerator->AddInt64("rasterizedPixelCount",
                       impl_stats.rasterized_pixel_count);
}

void RenderingStats::Add(const RenderingStats& other) {
  main_stats.Add(other.main_stats);
  impl_stats.Add(other.impl_stats);
}

}  // namespace cc
