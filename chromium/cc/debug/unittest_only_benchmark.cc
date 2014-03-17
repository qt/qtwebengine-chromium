// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/unittest_only_benchmark.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "cc/debug/unittest_only_benchmark_impl.h"

namespace cc {

UnittestOnlyBenchmark::UnittestOnlyBenchmark(scoped_ptr<base::Value> value,
                                             const DoneCallback& callback)
    : MicroBenchmark(callback),
      create_impl_benchmark_(false),
      weak_ptr_factory_(this) {
  if (!value)
    return;

  base::DictionaryValue* settings = NULL;
  value->GetAsDictionary(&settings);
  if (!settings)
    return;

  if (settings->HasKey("run_benchmark_impl"))
    settings->GetBoolean("run_benchmark_impl", &create_impl_benchmark_);
}

UnittestOnlyBenchmark::~UnittestOnlyBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void UnittestOnlyBenchmark::DidUpdateLayers(LayerTreeHost* host) {
  NotifyDone(scoped_ptr<base::Value>());
}

void UnittestOnlyBenchmark::RecordImplResults(scoped_ptr<base::Value> results) {
  NotifyDone(results.Pass());
}

scoped_ptr<MicroBenchmarkImpl> UnittestOnlyBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop) {
  if (!create_impl_benchmark_)
    return make_scoped_ptr<MicroBenchmarkImpl>(NULL);

  return scoped_ptr<MicroBenchmarkImpl>(new UnittestOnlyBenchmarkImpl(
      origin_loop,
      NULL,
      base::Bind(&UnittestOnlyBenchmark::RecordImplResults,
                 weak_ptr_factory_.GetWeakPtr())));
}

}  // namespace cc
