// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/security_metadata.h"

#include "url/gurl.h"

namespace media::hls {

// static
SecurityMetadata SecurityMetadata::CreateForTesting(std::string url,
                                                    bool would_taint_origin,
                                                    bool did_redirect) {
  return SecurityMetadata{
      .would_taint_origin = would_taint_origin,
      .did_redirect = did_redirect,
      .has_range_request = false,
      .response_origins = {url::Origin::Create(GURL(url))},
  };
}

}  // namespace media::hls
