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

#ifndef SRC_TRACE_REDACTION_REDACT_PROCESS_FREE_H_
#define SRC_TRACE_REDACTION_REDACT_PROCESS_FREE_H_

#include "src/trace_redaction/redact_ftrace_event.h"
#include "src/trace_redaction/trace_redaction_framework.h"

namespace perfetto::trace_redaction {

// Goes through ftrace events and conditonally removes the comm values from
// process free events.
class RedactProcessFree : public FtraceEventRedaction {
 public:
  RedactProcessFree();

  base::Status Redact(
      const Context& context,
      const protos::pbzero::FtraceEvent::Decoder& event,
      protozero::ConstBytes bytes,
      protos::pbzero::FtraceEvent* event_message) const override;
};

}  // namespace perfetto::trace_redaction

#endif  // SRC_TRACE_REDACTION_REDACT_PROCESS_FREE_H_
