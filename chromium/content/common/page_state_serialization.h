// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_
#define CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_

#include <vector>

#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "ui/gfx/point.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT ExplodedHttpBodyElement {
  WebKit::WebHTTPBody::Element::Type type;
  std::string data;
  base::NullableString16 file_path;
  GURL filesystem_url;
  int64 file_start;
  int64 file_length;
  double file_modification_time;
  GURL deprecated_blob_url;

  ExplodedHttpBodyElement();
  ~ExplodedHttpBodyElement();
};

struct CONTENT_EXPORT ExplodedHttpBody {
  base::NullableString16 http_content_type;
  std::vector<ExplodedHttpBodyElement> elements;
  int64 identifier;
  bool contains_passwords;
  bool is_null;

  ExplodedHttpBody();
  ~ExplodedHttpBody();
};

struct CONTENT_EXPORT ExplodedFrameState {
  base::NullableString16 url_string;
  base::NullableString16 original_url_string;
  base::NullableString16 referrer;
  base::NullableString16 target;
  base::NullableString16 parent;
  base::NullableString16 title;
  base::NullableString16 alternate_title;
  base::NullableString16 state_object;
  std::vector<base::NullableString16> document_state;
  gfx::Point scroll_offset;
  int64 item_sequence_number;
  int64 document_sequence_number;
  int visit_count;
  double visited_time;
  double page_scale_factor;
  bool is_target_item;
  ExplodedHttpBody http_body;
  std::vector<ExplodedFrameState> children;

  ExplodedFrameState();
  ~ExplodedFrameState();
};

struct CONTENT_EXPORT ExplodedPageState {
  std::vector<base::NullableString16> referenced_files;
  ExplodedFrameState top;

  ExplodedPageState();
  ~ExplodedPageState();
};

CONTENT_EXPORT bool DecodePageState(const std::string& encoded,
                                    ExplodedPageState* exploded);
CONTENT_EXPORT bool EncodePageState(const ExplodedPageState& exploded,
                                    std::string* encoded);

#if defined(OS_ANDROID)
CONTENT_EXPORT bool DecodePageStateWithDeviceScaleFactorForTesting(
    const std::string& encoded,
    float device_scale_factor,
    ExplodedPageState* exploded);
#endif

}  // namespace content

#endif  // CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_
