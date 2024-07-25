/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_TYPES_TRACE_PROCESSOR_CONTEXT_H_
#define SRC_TRACE_PROCESSOR_TYPES_TRACE_PROCESSOR_CONTEXT_H_

#include <memory>
#include <vector>

#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/tables/metadata_tables_py.h"
#include "src/trace_processor/types/destructible.h"

namespace perfetto {
namespace trace_processor {

enum TraceType {
  kUnknownTraceType,
  kProtoTraceType,
  kJsonTraceType,
  kFuchsiaTraceType,
  kSystraceTraceType,
  kGzipTraceType,
  kCtraceTraceType,
  kNinjaLogTraceType,
  kAndroidBugreportTraceType,
  kPerfDataTraceType,
};

class ArgsTracker;
class ArgsTranslationTable;
class AsyncTrackSetTracker;
class AndroidProbesTracker;
class ChunkedTraceReader;
class ClockTracker;
class ClockConverter;
class DeobfuscationMappingTable;
class EtwModule;
class EventTracker;
class ForwardingTraceParser;
class FtraceModule;
class GlobalArgsTracker;
class StackProfileTracker;
class HeapGraphTracker;
class PerfSampleTracker;
class MachineTracker;
class MappingTracker;
class MetadataTracker;
class MultiMachineTraceManager;
class PacketAnalyzer;
class ProtoImporterModule;
class TrackEventModule;
class ProcessTracker;
class SchedEventTracker;
class SliceTracker;
class SliceTranslationTable;
class FlowTracker;
class TraceParser;
class TraceSorter;
class TraceStorage;
class TrackTracker;
class DescriptorPool;

using MachineId = tables::MachineTable::Id;

class TraceProcessorContext {
 public:
  struct InitArgs {
    Config config;
    std::shared_ptr<TraceStorage> storage;
    uint32_t raw_machine_id = 0;
  };
  explicit TraceProcessorContext(const InitArgs&);
  // The default constructor is used in testing.
  TraceProcessorContext();
  ~TraceProcessorContext();

  TraceProcessorContext(TraceProcessorContext&&) = default;
  TraceProcessorContext& operator=(TraceProcessorContext&&) = default;

  Config config;

  // |storage| is shared among multiple contexts in multi-machine tracing.
  std::shared_ptr<TraceStorage> storage;

  std::unique_ptr<ChunkedTraceReader> chunk_reader;

  // The sorter is used to sort trace data by timestamp and is shared among
  // multiple machines.
  std::shared_ptr<TraceSorter> sorter;

  // Keep the global tracker before the args tracker as we access the global
  // tracker in the destructor of the args tracker. Also keep it before other
  // trackers, as they may own ArgsTrackers themselves.
  std::unique_ptr<GlobalArgsTracker> global_args_tracker;
  std::unique_ptr<ArgsTracker> args_tracker;
  std::unique_ptr<ArgsTranslationTable> args_translation_table;

  std::unique_ptr<TrackTracker> track_tracker;
  std::unique_ptr<AsyncTrackSetTracker> async_track_set_tracker;
  std::unique_ptr<SliceTracker> slice_tracker;
  std::unique_ptr<SliceTranslationTable> slice_translation_table;
  std::unique_ptr<FlowTracker> flow_tracker;
  std::unique_ptr<ProcessTracker> process_tracker;
  std::unique_ptr<EventTracker> event_tracker;
  std::unique_ptr<SchedEventTracker> sched_event_tracker;
  std::unique_ptr<ClockTracker> clock_tracker;
  std::unique_ptr<ClockConverter> clock_converter;
  std::unique_ptr<MappingTracker> mapping_tracker;
  std::unique_ptr<MachineTracker> machine_tracker;
  std::unique_ptr<PerfSampleTracker> perf_sample_tracker;
  std::unique_ptr<StackProfileTracker> stack_profile_tracker;
  std::unique_ptr<MetadataTracker> metadata_tracker;

  // These fields are stored as pointers to Destructible objects rather than
  // their actual type (a subclass of Destructible), as the concrete subclass
  // type is only available in storage_full target. To access these fields use
  // the GetOrCreate() method on their subclass type, e.g.
  // SyscallTracker::GetOrCreate(context)
  // clang-format off
  std::unique_ptr<Destructible> android_probes_tracker;    // AndroidProbesTracker
  std::unique_ptr<Destructible> binder_tracker;            // BinderTracker
  std::unique_ptr<Destructible> heap_graph_tracker;        // HeapGraphTracker
  std::unique_ptr<Destructible> syscall_tracker;           // SyscallTracker
  std::unique_ptr<Destructible> system_info_tracker;       // SystemInfoTracker
  std::unique_ptr<Destructible> v4l2_tracker;              // V4l2Tracker
  std::unique_ptr<Destructible> virtio_video_tracker;      // VirtioVideoTracker
  std::unique_ptr<Destructible> systrace_parser;           // SystraceParser
  std::unique_ptr<Destructible> thread_state_tracker;      // ThreadStateTracker
  std::unique_ptr<Destructible> i2c_tracker;               // I2CTracker
  std::unique_ptr<Destructible> perf_data_tracker;         // PerfDataTracker
  std::unique_ptr<Destructible> content_analyzer;          // ProtoContentAnalyzer
  std::unique_ptr<Destructible> shell_transitions_tracker; // ShellTransitionsTracker
  std::unique_ptr<Destructible> protolog_messages_tracker; // ProtoLogMessagesTracker
  std::unique_ptr<Destructible> ftrace_sched_tracker;      // FtraceSchedEventTracker
  std::unique_ptr<Destructible> v8_tracker;                // V8Tracker
  std::unique_ptr<Destructible> jit_tracker;               // JitTracker
  // clang-format on

  // These fields are trace readers which will be called by |forwarding_parser|
  // once the format of the trace is discovered. They are placed here as they
  // are only available in the lib target.
  std::unique_ptr<ChunkedTraceReader> json_trace_tokenizer;
  std::unique_ptr<ChunkedTraceReader> fuchsia_trace_tokenizer;
  std::unique_ptr<ChunkedTraceReader> ninja_log_parser;
  std::unique_ptr<ChunkedTraceReader> android_bugreport_parser;
  std::unique_ptr<ChunkedTraceReader> systrace_trace_parser;
  std::unique_ptr<ChunkedTraceReader> gzip_trace_parser;
  std::unique_ptr<ChunkedTraceReader> perf_data_trace_tokenizer;

  // These fields are trace parsers which will be called by |forwarding_parser|
  // once the format of the trace is discovered. They are placed here as they
  // are only available in the lib target.
  std::unique_ptr<TraceParser> json_trace_parser;
  std::unique_ptr<TraceParser> fuchsia_trace_parser;
  std::unique_ptr<TraceParser> perf_data_parser;

  // This field contains the list of proto descriptors that can be used by
  // reflection-based parsers.
  std::unique_ptr<DescriptorPool> descriptor_pool_;

  // The module at the index N is registered to handle field id N in
  // TracePacket.
  std::vector<std::vector<ProtoImporterModule*>> modules_by_field;
  std::vector<std::unique_ptr<ProtoImporterModule>> modules;
  // Pointers to modules from the modules vector that need to be called for
  // all fields.
  std::vector<ProtoImporterModule*> modules_for_all_fields;
  FtraceModule* ftrace_module = nullptr;
  EtwModule* etw_module = nullptr;
  TrackEventModule* track_module = nullptr;

  // Marks whether the uuid was read from the trace.
  // If the uuid was NOT read, the uuid will be made from the hash of the first
  // 4KB of the trace.
  bool uuid_found_in_trace = false;

  TraceType trace_type = kUnknownTraceType;

  std::optional<MachineId> machine_id() const;

  // Manages the contexts for reading trace data emitted from remote machines.
  std::unique_ptr<MultiMachineTraceManager> multi_machine_trace_manager;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TYPES_TRACE_PROCESSOR_CONTEXT_H_
