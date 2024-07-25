/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <cstdint>
#include <string>

#include "perfetto/base/status.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/collect_timeline_events.h"
#include "src/trace_redaction/find_package_uid.h"
#include "src/trace_redaction/optimize_timeline.h"
#include "src/trace_redaction/redact_sched_switch.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "src/trace_redaction/trace_redaction_integration_fixture.h"
#include "src/trace_redaction/trace_redactor.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "protos/perfetto/trace/ftrace/sched.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto::trace_redaction {

class RedactSchedSwitchIntegrationTest
    : public testing::Test,
      protected TraceRedactionIntegrationFixure {
 protected:
  void SetUp() override {
    trace_redactor()->emplace_collect<FindPackageUid>();
    trace_redactor()->emplace_collect<CollectTimelineEvents>();
    trace_redactor()->emplace_build<OptimizeTimeline>();

    auto* ftrace_event_redactions =
        trace_redactor()->emplace_transform<RedactFtraceEvent>();
    ftrace_event_redactions->emplace_back<RedactSchedSwitch>();

    context()->package_name = "com.Unity.com.unity.multiplayer.samples.coop";
  }
};

// >>> SELECT uid
// >>>   FROM package_list
// >>>   WHERE package_name='com.Unity.com.unity.multiplayer.samples.coop'
//
//     +-------+
//     |  uid  |
//     +-------+
//     | 10252 |
//     +-------+
//
// >>> SELECT uid, upid, name
// >>>   FROM process
// >>>   WHERE uid=10252
//
//     +-------+------+----------------------------------------------+
//     |  uid  | upid | name                                         |
//     +-------+------+----------------------------------------------+
//     | 10252 | 843  | com.Unity.com.unity.multiplayer.samples.coop |
//     +-------+------+----------------------------------------------+
//
// >>> SELECT tid, name
// >>>   FROM thread
// >>>   WHERE upid=843 AND name IS NOT NULL
//
//     +------+-----------------+
//     | tid  | name            |
//     +------+-----------------+
//     | 7120 | Binder:7105_2   |
//     | 7127 | UnityMain       |
//     | 7142 | Job.worker 0    |
//     | 7143 | Job.worker 1    |
//     | 7144 | Job.worker 2    |
//     | 7145 | Job.worker 3    |
//     | 7146 | Job.worker 4    |
//     | 7147 | Job.worker 5    |
//     | 7148 | Job.worker 6    |
//     | 7150 | Background Job. |
//     | 7151 | Background Job. |
//     | 7167 | UnityGfxDeviceW |
//     | 7172 | AudioTrack      |
//     | 7174 | FMOD stream thr |
//     | 7180 | Binder:7105_3   |
//     | 7184 | UnityChoreograp |
//     | 7945 | Filter0         |
//     | 7946 | Filter1         |
//     | 7947 | Thread-7        |
//     | 7948 | FMOD mixer thre |
//     | 7950 | UnityGfxDeviceW |
//     | 7969 | UnityGfxDeviceW |
//     +------+-----------------+

TEST_F(RedactSchedSwitchIntegrationTest, ClearsNonTargetSwitchComms) {
  auto result = Redact();
  ASSERT_OK(result) << result.c_message();

  auto original = LoadOriginal();
  ASSERT_OK(original) << original.status().c_message();

  auto redacted = LoadRedacted();
  ASSERT_OK(redacted) << redacted.status().c_message();

  base::FlatHashMap<int32_t, std::string> expected_names;
  expected_names.Insert(7120, "Binder:7105_2");
  expected_names.Insert(7127, "UnityMain");
  expected_names.Insert(7142, "Job.worker 0");
  expected_names.Insert(7143, "Job.worker 1");
  expected_names.Insert(7144, "Job.worker 2");
  expected_names.Insert(7145, "Job.worker 3");
  expected_names.Insert(7146, "Job.worker 4");
  expected_names.Insert(7147, "Job.worker 5");
  expected_names.Insert(7148, "Job.worker 6");
  expected_names.Insert(7150, "Background Job.");
  expected_names.Insert(7151, "Background Job.");
  expected_names.Insert(7167, "UnityGfxDeviceW");
  expected_names.Insert(7172, "AudioTrack");
  expected_names.Insert(7174, "FMOD stream thr");
  expected_names.Insert(7180, "Binder:7105_3");
  expected_names.Insert(7184, "UnityChoreograp");
  expected_names.Insert(7945, "Filter0");
  expected_names.Insert(7946, "Filter1");
  expected_names.Insert(7947, "Thread-7");
  expected_names.Insert(7948, "FMOD mixer thre");
  expected_names.Insert(7950, "UnityGfxDeviceW");
  expected_names.Insert(7969, "UnityGfxDeviceW");

  auto redacted_trace_data = LoadRedacted();
  ASSERT_OK(redacted_trace_data) << redacted.status().c_message();

  protos::pbzero::Trace::Decoder decoder(redacted_trace_data.value());

  for (auto packet = decoder.packet(); packet; ++packet) {
    protos::pbzero::TracePacket::Decoder packet_decoder(*packet);

    if (!packet_decoder.has_ftrace_events()) {
      continue;
    }

    protos::pbzero::FtraceEventBundle::Decoder ftrace_events_decoder(
        packet_decoder.ftrace_events());

    for (auto event = ftrace_events_decoder.event(); event; ++event) {
      protos::pbzero::FtraceEvent::Decoder event_decoder(*event);

      if (!event_decoder.has_sched_switch()) {
        continue;
      }

      protos::pbzero::SchedSwitchFtraceEvent::Decoder sched_decoder(
          event_decoder.sched_switch());

      ASSERT_TRUE(sched_decoder.has_next_pid());
      ASSERT_TRUE(sched_decoder.has_prev_pid());

      auto next_pid = sched_decoder.next_pid();
      auto prev_pid = sched_decoder.prev_pid();

      // If the pid is expected, make sure it has the right now. If it is not
      // expected, it should be missing.
      const auto* next_comm = expected_names.Find(next_pid);
      const auto* prev_comm = expected_names.Find(prev_pid);

      if (next_comm) {
        EXPECT_TRUE(sched_decoder.has_next_comm());
        EXPECT_EQ(sched_decoder.next_comm().ToStdString(), *next_comm);
      } else {
        EXPECT_FALSE(sched_decoder.has_next_comm());
      }

      if (prev_comm) {
        EXPECT_TRUE(sched_decoder.has_prev_comm());
        EXPECT_EQ(sched_decoder.prev_comm().ToStdString(), *prev_comm);
      } else {
        EXPECT_FALSE(sched_decoder.has_prev_comm());
      }
    }
  }
}

}  // namespace perfetto::trace_redaction
