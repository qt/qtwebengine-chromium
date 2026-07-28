// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

#include "base/feature_list.h"
#include "skia/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/codec/SkPngRustDecoder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

PngImageDecoder::~PngImageDecoder() = default;

String PngImageDecoder::FilenameExtension() const {
  return "png";
}

const AtomicString& PngImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, png_mime_type, ("image/png"));
  return png_mime_type;
}

std::unique_ptr<SkCodec> PngImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
  std::unique_ptr<SkCodec> codec =
      SkPngRustDecoder::Decode(std::move(stream), result);
#else
  std::unique_ptr<SkCodec> codec =
      SkPngDecoder::Decode(std::move(stream), result);
#endif
  return codec;
}

}  // namespace blink
