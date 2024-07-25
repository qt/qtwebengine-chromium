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

#include "src/trace_redaction/redact_process_free.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/ftrace/ftrace_event.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.gen.h"
#include "protos/perfetto/trace/ftrace/sched.gen.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"

namespace perfetto::trace_redaction {

class RedactProcessFreeTest : public testing::Test {};

TEST_F(RedactProcessFreeTest, ClearsComm) {
  protos::gen::FtraceEvent source_event;
  source_event.set_timestamp(123456789);
  source_event.set_pid(10);

  auto* process_free = source_event.mutable_sched_process_free();
  process_free->set_comm("comm-a");
  process_free->set_pid(11);

  RedactProcessFree redact;
  Context context;

  protos::pbzero::FtraceEvent::Decoder event_decoder(
      source_event.SerializeAsString());
  protozero::HeapBuffered<protos::pbzero::FtraceEvent> event_message;

  auto result =
      redact.Redact(context, event_decoder, event_decoder.sched_switch(),
                    event_message.get());
  ASSERT_OK(result) << result.c_message();

  protos::gen::FtraceEvent redacted_event;
  redacted_event.ParseFromString(event_message.SerializeAsString());

  // No process free event should have been added to the ftrace event.
  ASSERT_FALSE(redacted_event.has_sched_process_free());
}

// Even if there is no pid in the process free event, the process free event
// should still exist but no comm value should be present.
TEST_F(RedactProcessFreeTest, NoPidClearsEvent) {
  protos::gen::FtraceEvent source_event;
  source_event.set_timestamp(123456789);
  source_event.set_pid(10);

  // Don't add a pid. This should stop the process free event from being added
  // to the event message.
  auto* process_free = source_event.mutable_sched_process_free();
  process_free->set_comm("comm-a");

  RedactProcessFree redact;
  Context context;

  protos::pbzero::FtraceEvent::Decoder event_decoder(
      source_event.SerializeAsString());
  protozero::HeapBuffered<protos::pbzero::FtraceEvent> event_message;

  // Even if the process free event has been dropped, there should be no
  // resulting error.
  auto result =
      redact.Redact(context, event_decoder, event_decoder.sched_switch(),
                    event_message.get());
  ASSERT_OK(result) << result.c_message();

  protos::gen::FtraceEvent redacted_event;
  redacted_event.ParseFromString(event_message.SerializeAsString());

  ASSERT_FALSE(redacted_event.has_sched_process_free());
}

}  // namespace perfetto::trace_redaction
