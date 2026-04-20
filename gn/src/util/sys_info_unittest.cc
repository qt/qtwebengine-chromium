// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/sys_info.h"

#include "util/build_config.h"
#include "util/test/test.h"

TEST(SysInfoTest, NumberOfProcessors) {
  int num_processors = NumberOfProcessors();
  EXPECT_GT(num_processors, 0);
}

TEST(SysInfoTest, NumberOfPerformanceProcessors) {
  int num_perf_processors = NumberOfPerformanceProcessors();
  // On all platforms this should be at least 1 (if implemented) or same as
  // NumberOfProcessors
  EXPECT_GT(num_perf_processors, 0);

#if defined(OS_MACOSX) && defined(ARCH_CPU_ARM64)
  // Apple Silicon has both performance and efficiency cores, so the number of
  // performance cores should be less than the total number of processors.
  EXPECT_LE(num_perf_processors, NumberOfProcessors());
#else
  // On other platforms, it returns NumberOfProcessors().
  EXPECT_EQ(num_perf_processors, NumberOfProcessors());
#endif
}
