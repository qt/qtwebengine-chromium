// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/bindings/python/cast_runtime.h"

#include "gtest/gtest.h"
#include "platform/impl/task_runner.h"

namespace openscreen::cast {

TEST(CastRuntimeTest, ConstructorAndDestructor) {
  {
    CastRuntime runtime;
    EXPECT_NE(runtime.task_runner(), nullptr);
  }
}

}  // namespace openscreen::cast
