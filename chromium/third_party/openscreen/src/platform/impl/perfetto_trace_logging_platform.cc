// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/perfetto_trace_logging_platform.h"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/track_event.h"
#include "platform/impl/logging.h"
#include "util/chrono_helpers.h"

PERFETTO_DEFINE_CATEGORIES(
    ::perfetto::Category("any").SetDescription("General category"),
    ::perfetto::Category("mdns").SetDescription("mDNS protocol"),
    ::perfetto::Category("quic").SetDescription("QUIC protocol"),
    ::perfetto::Category("ssl").SetDescription("SSL/TLS"),
    ::perfetto::Category("presentation").SetDescription("Presentation API"),
    ::perfetto::Category("standalone_receiver")
        .SetDescription("Standalone Receiver"),
    ::perfetto::Category("discovery").SetDescription("Discovery"),
    ::perfetto::Category("standalone_sender")
        .SetDescription("Standalone Sender"),
    ::perfetto::Category("receiver").SetDescription("Cast Receiver"),
    ::perfetto::Category("sender").SetDescription("Cast Sender"));

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace openscreen {

PerfettoTraceLoggingPlatform::PerfettoTraceLoggingPlatform() {
  perfetto::TracingInitArgs args;
  // The in-process backend allows recording into a file or memory buffer
  // from within the same process.
  args.backends = perfetto::kInProcessBackend;

  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();

  perfetto::TraceConfig cfg;
  cfg.add_buffers()->set_size_kb(10 * 1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  tracing_session_ = perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  tracing_session_->Setup(cfg);
  tracing_session_->StartBlocking();

  StartTracing(this);
}

PerfettoTraceLoggingPlatform::~PerfettoTraceLoggingPlatform() {
  StopTracing();

  tracing_session_->StopBlocking();
  const std::vector<char> trace_data(tracing_session_->ReadTraceBlocking());
  const std::string filename = std::format("openscreen_{}.pftrace", getpid());

  std::ofstream output_file(filename, std::ios::out | std::ios::binary);
  output_file.write(trace_data.data(), trace_data.size());
  output_file.close();
  OSP_LOG_INFO << "Perfetto trace log written to: " << filename;
}

bool PerfettoTraceLoggingPlatform::IsTraceLoggingEnabled(
    TraceCategory category) {
  // Perfetto checks if the category is enabled before tracing, so this method
  // would only be helpful for avoiding instantiating heavy objects. However,
  // based on how we are using Perfetto currently it is likely that the effort
  // to check if the category is enabled cumulatively using
  // perfetto::DynamicString is more effort than occasionally constructing a
  // large trace object. So just return true here.
  return true;
}

void PerfettoTraceLoggingPlatform::LogTrace(TraceEvent event,
                                            Clock::time_point end_time) {
  const auto start_ns =
      to_nanoseconds(event.start_time.time_since_epoch()).count();
  const auto end_ns = to_nanoseconds(end_time.time_since_epoch()).count();

  const char* category_name = ToString(event.category);

  perfetto::TrackEvent::Trace([&](perfetto::TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(start_ns);
    auto* track_event = packet->set_track_event();
    track_event->set_type(
        perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    track_event->add_categories(category_name);
    track_event->set_name(event.name);

    for (const auto& arg : event.arguments) {
      auto* debug = track_event->add_debug_annotations();
      debug->set_name(arg.first);
      debug->set_string_value(arg.second);
    }
  });

  perfetto::TrackEvent::Trace([&](perfetto::TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(end_ns);
    auto* track_event = packet->set_track_event();
    track_event->set_type(perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END);
    track_event->add_categories(category_name);
  });
}

void PerfettoTraceLoggingPlatform::LogAsyncStart(TraceEvent event) {
  const char* category_name = ToString(event.category);
  const auto start_ns =
      to_nanoseconds(event.start_time.time_since_epoch()).count();

  const uint64_t flow_id = event.ids.current;

  perfetto::TrackEvent::Trace([&](perfetto::TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(start_ns);
    auto* track_event = packet->set_track_event();
    track_event->set_type(
        perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    track_event->set_track_uuid(flow_id);
    track_event->add_categories(category_name);
    track_event->set_name(event.name);

    for (const auto& arg : event.arguments) {
      auto* debug = track_event->add_debug_annotations();
      debug->set_name(arg.first);
      debug->set_string_value(arg.second);
    }
  });
}

void PerfettoTraceLoggingPlatform::LogAsyncEnd(TraceEvent event) {
  const char* category_name = ToString(event.category);
  const auto end_ns =
      to_nanoseconds(event.start_time.time_since_epoch()).count();

  const uint64_t flow_id = event.ids.current;

  perfetto::TrackEvent::Trace([&](perfetto::TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(end_ns);
    auto* track_event = packet->set_track_event();
    track_event->set_type(perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END);
    track_event->set_track_uuid(flow_id);
    track_event->add_categories(category_name);
  });
}

void PerfettoTraceLoggingPlatform::LogFlow(TraceEvent event, FlowType type) {
  const char* category_name = ToString(event.category);
  const auto timestamp_ns =
      to_nanoseconds(event.start_time.time_since_epoch()).count();

  // Use root ID for flow correlation if available, otherwise current.
  const uint64_t flow_id =
      (event.ids.root != kUnsetTraceId && event.ids.root != kEmptyTraceId)
          ? event.ids.root
          : event.ids.current;

  perfetto::TrackEvent::Trace([&](perfetto::TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(timestamp_ns);
    auto* track_event = packet->set_track_event();
    track_event->set_type(perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT);
    track_event->add_categories(category_name);
    track_event->set_name(event.name);

    if (type == FlowType::kFlowEnd) {
      track_event->add_terminating_flow_ids(flow_id);
    } else {
      track_event->add_flow_ids(flow_id);
    }

    for (const auto& arg : event.arguments) {
      auto* debug = track_event->add_debug_annotations();
      debug->set_name(arg.first);
      debug->set_string_value(arg.second);
    }
  });
}

}  // namespace openscreen
