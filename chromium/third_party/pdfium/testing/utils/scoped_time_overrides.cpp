// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/utils/scoped_time_overrides.h"

ScopedTimeFunction::ScopedTimeFunction(TimeFunction func)
    : old_func_(FXSYS_SetTimeFunction(func)) {}

ScopedTimeFunction::~ScopedTimeFunction() {
  FXSYS_SetTimeFunction(old_func_);
}

ScopedLocaltimeFunction::ScopedLocaltimeFunction(LocaltimeFunction func)
    : old_func_(FXSYS_SetLocaltimeFunction(func)) {}

ScopedLocaltimeFunction::~ScopedLocaltimeFunction() {
  FXSYS_SetLocaltimeFunction(old_func_);
}
