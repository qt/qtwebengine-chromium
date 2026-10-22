// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_PERFETTO_TRACE_LOGGING_PLATFORM_H_
#define PLATFORM_IMPL_PERFETTO_TRACE_LOGGING_PLATFORM_H_

#include <memory>

#include "platform/api/trace_logging_platform.h"

namespace perfetto {
class TracingSession;
}

namespace openscreen {

class PerfettoTraceLoggingPlatform : public TraceLoggingPlatform {
 public:
  PerfettoTraceLoggingPlatform();

  // NOTE: We will only emit a trace file if `this` is properly destructed.
  // Meaning that no file is emitted if the application crashes.
  ~PerfettoTraceLoggingPlatform() override;

  // TraceLoggingPlatform implementation.
  bool IsTraceLoggingEnabled(TraceCategory category) override;
  void LogTrace(TraceEvent event, Clock::time_point end_time) override;
  void LogAsyncStart(TraceEvent event) override;
  void LogAsyncEnd(TraceEvent event) override;
  void LogFlow(TraceEvent event, FlowType type) override;

 private:
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
};

}  // namespace openscreen

#endif  // PLATFORM_IMPL_PERFETTO_TRACE_LOGGING_PLATFORM_H_
