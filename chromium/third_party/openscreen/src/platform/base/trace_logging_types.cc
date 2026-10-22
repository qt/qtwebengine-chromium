// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/trace_logging_types.h"

#include <cstdlib>
#include <limits>

namespace openscreen {

std::string TraceIdHierarchy::ToString() const {
  std::stringstream ss;
  ss << "[" << std::hex << (HasRoot() ? root : 0) << ":"
     << (HasParent() ? parent : 0) << ":" << (HasCurrent() ? current : 0)
     << std::dec << "]";

  return ss.str();
}

std::ostream& operator<<(std::ostream& out, const TraceIdHierarchy& ids) {
  return out << ids.ToString();
}

bool operator==(const TraceIdHierarchy& lhs, const TraceIdHierarchy& rhs) {
  return lhs.current == rhs.current && lhs.parent == rhs.parent &&
         lhs.root == rhs.root;
}

bool operator!=(const TraceIdHierarchy& lhs, const TraceIdHierarchy& rhs) {
  return !(lhs == rhs);
}

const char* ToString(TraceCategory category) {
  switch (category) {
    case TraceCategory::kAny:
      return "any";
    case TraceCategory::kMdns:
      return "mdns";
    case TraceCategory::kQuic:
      return "quic";
    case TraceCategory::kSsl:
      return "ssl";
    case TraceCategory::kPresentation:
      return "presentation";
    case TraceCategory::kStandaloneReceiver:
      return "standalone_receiver";
    case TraceCategory::kDiscovery:
      return "discovery";
    case TraceCategory::kStandaloneSender:
      return "standalone_sender";
    case TraceCategory::kReceiver:
      return "receiver";
    case TraceCategory::kSender:
      return "sender";
  }

  // OSP_NOTREACHED is not available in platform/base.
  std::abort();
}

}  // namespace openscreen
