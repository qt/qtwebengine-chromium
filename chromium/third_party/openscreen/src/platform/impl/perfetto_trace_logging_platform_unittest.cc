// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/perfetto_trace_logging_platform.h"

#include <unistd.h>

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/api/trace_event.h"

namespace openscreen {

class PerfettoTraceLoggingPlatformTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Construct the expected filename based on the current PID.
    // implementation uses: "openscreen_{pid}.pftrace"
    expected_filename_ = "openscreen_" + std::to_string(getpid()) + ".pftrace";

    // Ensure we start with a clean state.
    remove(expected_filename_.c_str());
  }

  void TearDown() override {
    // Clean up the trace file after test.
    remove(expected_filename_.c_str());
  }

  std::string expected_filename_;
};

TEST_F(PerfettoTraceLoggingPlatformTest, LifecycleAndLogging) {
  // 1. Scope the platform instance to force destruction and file write.
  {
    PerfettoTraceLoggingPlatform platform;

    // 2. Verify Logging is enabled
    EXPECT_TRUE(platform.IsTraceLoggingEnabled(TraceCategory::kAny));
    EXPECT_TRUE(platform.IsTraceLoggingEnabled(TraceCategory::kMdns));

    // 3. Exercise Logging Methods
    // We can't easily assert the binary output content without a proto parser,
    // but we can ensure these calls don't crash and nominally succeed.
    TraceEvent event;
    event.category = TraceCategory::kAny;
    event.name = "TestEvent";
    event.start_time = Clock::now();

    platform.LogTrace(event, Clock::now());
    platform.LogAsyncStart(event);
    platform.LogAsyncEnd(event);
    platform.LogFlow(event, FlowType::kFlowBegin);
  }

  // 4. Verify Output
  // After destruction, the trace file should exist on disk.
  EXPECT_EQ(access(expected_filename_.c_str(), F_OK), 0)
      << "Trace file should be created at: " << expected_filename_;
}

}  // namespace openscreen
