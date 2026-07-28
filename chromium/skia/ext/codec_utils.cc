// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/codec_utils.h"

#include "base/base64.h"
#include "skia/buildflags.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/codec/SkPngRustDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "third_party/skia/include/encode/SkPngRustEncoder.h"

namespace skia {

namespace {

sk_sp<SkData> EncodePngAsSkData(
    GrDirectContext* context,
    const SkImage* src,
    SkPngRustEncoder::CompressionLevel compression_level) {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
  const SkPngRustEncoder::Options options = {.fCompressionLevel =
                                                 compression_level};
  return SkPngRustEncoder::Encode(context, src, options);
#else
  const int zlib_level = SkPngRustEncoder::ConvertToZLibLevel(compression_level);
  const SkPngEncoder::Options options = {.fZLibLevel = zlib_level};
  return SkPngEncoder::Encode(context, src, options);
#endif
}

}  // namespace

sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src) {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
  const SkPngRustEncoder::Options kDefaultOptions = {};
  return SkPngRustEncoder::Encode(src, kDefaultOptions);
#else
  const SkPngEncoder::Options kDefaultOptions = {};
  return SkPngEncoder::Encode(src, kDefaultOptions);
#endif
}

sk_sp<SkData> EncodePngAsSkData(GrDirectContext* context, const SkImage* src) {
  return EncodePngAsSkData(context, src,
                           SkPngRustEncoder::CompressionLevel::kMedium);
}

sk_sp<SkData> FastEncodePngAsSkData(GrDirectContext* context,
                                    const SkImage* src) {
  return EncodePngAsSkData(context, src,
                           SkPngRustEncoder::CompressionLevel::kLow);
}

std::string EncodePngAsDataUri(const SkPixmap& src) {
  std::string result;
  if (sk_sp<SkData> data = EncodePngAsSkData(src); data) {
    result += "data:image/png;base64,";
    result += base::Base64Encode(skia::as_byte_span(*data));
  }
  return result;
}

void EnsurePNGDecoderRegistered() {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
  SkCodecs::Register(SkPngRustDecoder::Decoder());
#else
  SkCodecs::Register(SkPngDecoder::Decoder());
#endif
}

}  // namespace skia
