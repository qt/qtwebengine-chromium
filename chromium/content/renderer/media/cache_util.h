// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CACHE_UTIL_H_
#define CONTENT_RENDERER_MEDIA_CACHE_UTIL_H_

#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace blink {
class WebURLResponse;
}

namespace content {

// Reasons that a cached WebURLResponse will *not* prevent a future request to
// the server.  Reported via UMA, so don't change/reuse previously-existing
// values.
enum UncacheableReason {
  kNoData = 1 << 0,  // Not 200 or 206.
  kPre11PartialResponse = 1 << 1,  // 206 but HTTP version < 1.1.
  kNoStrongValidatorOnPartialResponse = 1 << 2,  // 206, no strong validator.
  kShortMaxAge = 1 << 3,  // Max age less than 1h (arbitrary value).
  kExpiresTooSoon = 1 << 4,  // Expires in less than 1h (arbitrary value).
  kHasMustRevalidate = 1 << 5,  // Response asks for revalidation.
  kNoCache = 1 << 6,  // Response included a no-cache header.
  kNoStore = 1 << 7,  // Response included a no-store header.
  kMaxReason  // Needs to be one more than max legitimate reason.
};

// Return the logical OR of the reasons "response" cannot be used for a future
// request (using the disk cache), or 0 if it might be useful.
uint32 CONTENT_EXPORT GetReasonsForUncacheability(
    const blink::WebURLResponse& response);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CACHE_UTIL_H_
