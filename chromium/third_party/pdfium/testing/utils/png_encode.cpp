// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/utils/png_encode.h"

#include "core/fxcrt/check.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/notreached.h"
#include "public/fpdfview.h"
#include "testing/image_diff/image_diff_png.h"

#ifdef PDF_ENABLE_SKIA
#include "third_party/skia/include/core/SkAlphaType.h"  // nogncheck
#include "third_party/skia/include/core/SkColorType.h"  // nogncheck
#include "third_party/skia/include/core/SkImageInfo.h"  // nogncheck
#include "third_party/skia/include/core/SkPixmap.h"     // nogncheck
#endif

namespace {

#ifdef PDF_ENABLE_SKIA
std::vector<uint8_t> ConvertToStraightAlpha(pdfium::span<const uint8_t> input,
                                            int width,
                                            int height,
                                            int row_byte_width) {
  SkImageInfo premul_alpha_image_info = SkImageInfo::Make(
      width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  SkPixmap src_pixmap(premul_alpha_image_info, input.data(), row_byte_width);

  std::vector<uint8_t> result(input.size());
  SkImageInfo straight_alpha_image_info = SkImageInfo::Make(
      width, height, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);
  SkPixmap dst_pixmap(straight_alpha_image_info, result.data(), row_byte_width);

  CHECK(src_pixmap.readPixels(dst_pixmap));
  return result;
}
#endif

}  // namespace

std::vector<uint8_t> EncodePng(pdfium::span<const uint8_t> input,
                               int width,
                               int height,
                               int stride,
                               int format) {
  std::vector<uint8_t> png;
  switch (format) {
    case FPDFBitmap_Unknown:
      break;
    case FPDFBitmap_Gray:
      png = image_diff_png::EncodeGrayPNG(input, width, height, stride);
      break;
    case FPDFBitmap_BGR:
      png = image_diff_png::EncodeBGRPNG(input, width, height, stride);
      break;
    case FPDFBitmap_BGRx:
      png = image_diff_png::EncodeBGRAPNG(input, width, height, stride,
                                          /*discard_transparency=*/true);
      break;
    case FPDFBitmap_BGRA:
      png = image_diff_png::EncodeBGRAPNG(input, width, height, stride,
                                          /*discard_transparency=*/false);
      break;
#ifdef PDF_ENABLE_SKIA
    case FPDFBitmap_BGRA_Premul: {
      std::vector<uint8_t> input_with_straight_alpha =
          ConvertToStraightAlpha(input, width, height, stride);
      png = image_diff_png::EncodeBGRAPNG(input_with_straight_alpha, width,
                                          height, stride,
                                          /*discard_transparency=*/false);
      break;
    }
#endif
    default:
      NOTREACHED();
  }
  return png;
}

std::vector<uint8_t> EncodePng(FPDF_BITMAP bitmap) {
  const int stride = FPDFBitmap_GetStride(bitmap);
  const int width = FPDFBitmap_GetWidth(bitmap);
  const int height = FPDFBitmap_GetHeight(bitmap);
  CHECK(stride >= 0);
  CHECK(width >= 0);
  CHECK(height >= 0);
  FX_SAFE_FILESIZE size = stride;
  size *= height;
  auto input =
      pdfium::span(static_cast<const uint8_t*>(FPDFBitmap_GetBuffer(bitmap)),
                   pdfium::ValueOrDieForType<size_t>(size));

  return EncodePng(input, width, height, stride, FPDFBitmap_GetFormat(bitmap));
}
