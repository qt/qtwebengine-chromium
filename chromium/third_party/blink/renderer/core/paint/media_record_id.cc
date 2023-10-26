// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/media_record_id.h"

#include "base/hash/hash.h"

namespace blink {

MediaRecordId::MediaRecordId(const LayoutObject* layout,
                             const ImageResourceContent* content)
    : layout_object_(layout),
      image_resource_content_(content),
      hash_(GenerateHash(layout, image_resource_content_)) {}

// This hash is used as a key where previously MediaRecordId was used directly.
// That helps us avoid storing references to the GCed LayoutObject and
// MediaTiming, as that can be unsafe when using regular WTF containers. It also
// helps us avoid needlessly allocating MediaRecordId on the heap.
MediaRecordIdHash MediaRecordId::GenerateHash(const LayoutObject* layout,
                                              const ImageResourceContent* content) {
  return base::HashInts(reinterpret_cast<MediaRecordIdHash>(layout),
                        reinterpret_cast<MediaRecordIdHash>(content));
}

}  // namespace blink
