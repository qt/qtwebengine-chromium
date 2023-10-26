// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_MEDIA_RECORD_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_MEDIA_RECORD_ID_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class LayoutObject;
class ImageResourceContent;

using MediaRecordIdHash = size_t;

class MediaRecordId {
  STACK_ALLOCATED();

 public:
  static MediaRecordIdHash CORE_EXPORT GenerateHash(const LayoutObject* layout,
                                                    const ImageResourceContent* content);

  MediaRecordId(const LayoutObject* layout, const ImageResourceContent* content);

  MediaRecordIdHash GetHash() const { return hash_; }
  const LayoutObject* GetLayoutObject() const { return layout_object_; }
  const ImageResourceContent* GetImageResourceContent() const { return image_resource_content_; }

 private:
  const LayoutObject* const layout_object_;
  const ImageResourceContent* const image_resource_content_;
  const MediaRecordIdHash hash_;
};

}  // namespace blink
#endif
