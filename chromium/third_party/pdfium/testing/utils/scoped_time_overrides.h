// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_UTILS_SCOPED_TIME_OVERRIDES_H_
#define TESTING_UTILS_SCOPED_TIME_OVERRIDES_H_

#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_memory.h"

class ScopedTimeFunction {
 public:
  FX_STACK_ALLOCATED();

  explicit ScopedTimeFunction(TimeFunction func);
  ScopedTimeFunction(const ScopedTimeFunction&) = delete;
  ScopedTimeFunction& operator=(const ScopedTimeFunction&) = delete;
  ~ScopedTimeFunction();

 private:
  const TimeFunction old_func_;
};

class ScopedLocaltimeFunction {
 public:
  FX_STACK_ALLOCATED();

  explicit ScopedLocaltimeFunction(LocaltimeFunction func);
  ScopedLocaltimeFunction(const ScopedLocaltimeFunction&) = delete;
  ScopedLocaltimeFunction& operator=(const ScopedLocaltimeFunction&) = delete;
  ~ScopedLocaltimeFunction();

 private:
  const LocaltimeFunction old_func_;
};

#endif  // TESTING_UTILS_SCOPED_TIME_OVERRIDES_H_
