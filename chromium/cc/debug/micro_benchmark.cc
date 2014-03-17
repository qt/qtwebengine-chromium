// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/micro_benchmark.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "cc/debug/micro_benchmark_impl.h"

namespace cc {

MicroBenchmark::MicroBenchmark(const DoneCallback& callback)
    : callback_(callback),
      is_done_(false),
      processed_for_benchmark_impl_(false) {}

MicroBenchmark::~MicroBenchmark() {}

bool MicroBenchmark::IsDone() const {
  return is_done_;
}

void MicroBenchmark::DidUpdateLayers(LayerTreeHost* host) {}

void MicroBenchmark::NotifyDone(scoped_ptr<base::Value> result) {
  callback_.Run(result.Pass());
  is_done_ = true;
}

void MicroBenchmark::RunOnLayer(Layer* layer) {}

void MicroBenchmark::RunOnLayer(PictureLayer* layer) {}

bool MicroBenchmark::ProcessedForBenchmarkImpl() const {
  return processed_for_benchmark_impl_;
}

scoped_ptr<MicroBenchmarkImpl> MicroBenchmark::GetBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop) {
  DCHECK(!processed_for_benchmark_impl_);
  processed_for_benchmark_impl_ = true;
  return CreateBenchmarkImpl(origin_loop);
}

scoped_ptr<MicroBenchmarkImpl> MicroBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop) {
  return make_scoped_ptr<MicroBenchmarkImpl>(NULL);
}

}  // namespace cc
