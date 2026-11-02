// Copyright 2024 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/dib/cfx_scanlinecompositor.h"

#include <stdint.h>

#include <array>

#include "build/build_config.h"
#include "core/fxcrt/span.h"
#include "core/fxcrt/stl_util.h"
#include "core/fxge/dib/fx_dib.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

namespace {

constexpr FX_BGRA_STRUCT<uint8_t> kDestScan[] = {
    {.blue = 255, .green = 100, .red = 0, .alpha = 0},
    {.blue = 255, .green = 100, .red = 0, .alpha = 0},
    {.blue = 255, .green = 100, .red = 0, .alpha = 255},
    {.blue = 255, .green = 100, .red = 0, .alpha = 255},
    {.blue = 255, .green = 100, .red = 0, .alpha = 100},
    {.blue = 255, .green = 100, .red = 0, .alpha = 100},
    {.blue = 255, .green = 100, .red = 0, .alpha = 200},
    {.blue = 255, .green = 100, .red = 0, .alpha = 200},
};
constexpr FX_BGRA_STRUCT<uint8_t> kSrcScan1[] = {
    {.blue = 255, .green = 100, .red = 0, .alpha = 0},
    {.blue = 255, .green = 100, .red = 0, .alpha = 255},
    {.blue = 255, .green = 100, .red = 0, .alpha = 0},
    {.blue = 255, .green = 100, .red = 0, .alpha = 255},
    {.blue = 255, .green = 100, .red = 0, .alpha = 100},
    {.blue = 255, .green = 100, .red = 0, .alpha = 200},
    {.blue = 255, .green = 100, .red = 0, .alpha = 100},
    {.blue = 255, .green = 100, .red = 0, .alpha = 200},
};
constexpr FX_BGRA_STRUCT<uint8_t> kSrcScan2[] = {
    {.blue = 100, .green = 0, .red = 255, .alpha = 0},
    {.blue = 100, .green = 0, .red = 255, .alpha = 255},
    {.blue = 100, .green = 0, .red = 255, .alpha = 0},
    {.blue = 100, .green = 0, .red = 255, .alpha = 255},
    {.blue = 100, .green = 0, .red = 255, .alpha = 100},
    {.blue = 100, .green = 0, .red = 255, .alpha = 200},
    {.blue = 100, .green = 0, .red = 255, .alpha = 100},
    {.blue = 100, .green = 0, .red = 255, .alpha = 200},
};
constexpr FX_BGRA_STRUCT<uint8_t> kSrcScan3[] = {
    {.blue = 0, .green = 255, .red = 100, .alpha = 0},
    {.blue = 0, .green = 255, .red = 100, .alpha = 255},
    {.blue = 0, .green = 255, .red = 100, .alpha = 0},
    {.blue = 0, .green = 255, .red = 100, .alpha = 255},
    {.blue = 0, .green = 255, .red = 100, .alpha = 100},
    {.blue = 0, .green = 255, .red = 100, .alpha = 200},
    {.blue = 0, .green = 255, .red = 100, .alpha = 100},
    {.blue = 0, .green = 255, .red = 100, .alpha = 200},
};

void RunTest(CFX_ScanlineCompositor& compositor,
             pdfium::span<const FX_BGRA_STRUCT<uint8_t>> src_span,
             pdfium::span<const FX_BGRA_STRUCT<uint8_t>> expectations) {
  std::array<FX_BGRA_STRUCT<uint8_t>, 8> dest_scan;
  fxcrt::Copy(kDestScan, dest_scan);
  compositor.CompositeRgbBitmapLine(pdfium::as_writable_byte_span(dest_scan),
                                    pdfium::as_bytes(src_span),
                                    dest_scan.size(), {});
  EXPECT_THAT(dest_scan, ElementsAreArray(expectations));
}

#if defined(PDF_USE_SKIA)
void PreMultiplyScanLine(pdfium::span<FX_BGRA_STRUCT<uint8_t>> scanline) {
  for (auto& pixel : scanline) {
    pixel = PreMultiplyColor(pixel);
  }
}

void UnPreMultiplyScanLine(pdfium::span<FX_BGRA_STRUCT<uint8_t>> scanline) {
  for (auto& pixel : scanline) {
    pixel = UnPreMultiplyColor(pixel);
  }
}

void RunPreMultiplyTest(
    CFX_ScanlineCompositor& compositor,
    pdfium::span<const FX_BGRA_STRUCT<uint8_t>> src_span,
    pdfium::span<const FX_BGRA_STRUCT<uint8_t>> expectations) {
  std::array<FX_BGRA_STRUCT<uint8_t>, 8> dest_scan;
  fxcrt::Copy(kDestScan, dest_scan);
  PreMultiplyScanLine(dest_scan);
  std::array<FX_BGRA_STRUCT<uint8_t>, 8> src_scan;
  fxcrt::Copy(src_span, src_scan);
  PreMultiplyScanLine(src_scan);
  compositor.CompositeRgbBitmapLine(pdfium::as_writable_byte_span(dest_scan),
                                    pdfium::as_byte_span(src_scan),
                                    dest_scan.size(), {});
  UnPreMultiplyScanLine(dest_scan);
  EXPECT_THAT(dest_scan, ElementsAreArray(expectations));
}
#endif  // defined(PDF_USE_SKIA)

}  // namespace

namespace fxge {

inline bool operator==(const FX_BGRA_STRUCT<uint8_t>& lhs,
                       const FX_BGRA_STRUCT<uint8_t>& rhs) {
  return lhs.blue == rhs.blue && lhs.green == rhs.green && lhs.red == rhs.red &&
         lhs.alpha == rhs.alpha;
}

}  // namespace fxge

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraNormal) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kNormal,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 161},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 158, .green = 38, .red = 158, .alpha = 161},
      {.blue = 115, .green = 10, .red = 229, .alpha = 222},
      {.blue = 185, .green = 55, .red = 114, .alpha = 222},
      {.blue = 127, .green = 18, .red = 209, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 97, .green = 196, .red = 61, .alpha = 161},
      {.blue = 26, .green = 239, .red = 89, .alpha = 222},
      {.blue = 141, .green = 169, .red = 44, .alpha = 222},
      {.blue = 46, .green = 227, .red = 81, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraScreen) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kScreen,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 161, .red = 0, .alpha = 255},
      {.blue = 255, .green = 114, .red = 0, .alpha = 161},
      {.blue = 255, .green = 120, .red = 0, .alpha = 222},
      {.blue = 255, .green = 121, .red = 0, .alpha = 222},
      {.blue = 255, .green = 138, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 255, .alpha = 255},
      {.blue = 196, .green = 62, .red = 158, .alpha = 161},
      {.blue = 169, .green = 45, .red = 229, .alpha = 222},
      {.blue = 239, .green = 90, .red = 114, .alpha = 222},
      {.blue = 227, .green = 81, .red = 209, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 255, .red = 100, .alpha = 255},
      {.blue = 158, .green = 196, .red = 61, .alpha = 161},
      {.blue = 115, .green = 239, .red = 89, .alpha = 222},
      {.blue = 230, .green = 169, .red = 44, .alpha = 222},
      {.blue = 209, .green = 227, .red = 81, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraDarken) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kDarken,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 161},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 100, .green = 0, .red = 0, .alpha = 255},
      {.blue = 158, .green = 38, .red = 96, .alpha = 161},
      {.blue = 115, .green = 10, .red = 139, .alpha = 222},
      {.blue = 185, .green = 55, .red = 24, .alpha = 222},
      {.blue = 127, .green = 18, .red = 45, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 100, .red = 0, .alpha = 255},
      {.blue = 97, .green = 158, .red = 37, .alpha = 161},
      {.blue = 26, .green = 184, .red = 53, .alpha = 222},
      {.blue = 141, .green = 114, .red = 9, .alpha = 222},
      {.blue = 46, .green = 127, .red = 17, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraSoftLight) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kSoftLight,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 87, .red = 0, .alpha = 255},
      {.blue = 255, .green = 96, .red = 0, .alpha = 161},
      {.blue = 255, .green = 94, .red = 0, .alpha = 222},
      {.blue = 255, .green = 95, .red = 0, .alpha = 222},
      {.blue = 255, .green = 90, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 40, .red = 0, .alpha = 255},
      {.blue = 196, .green = 47, .red = 96, .alpha = 161},
      {.blue = 169, .green = 23, .red = 139, .alpha = 222},
      {.blue = 239, .green = 69, .red = 24, .alpha = 222},
      {.blue = 227, .green = 43, .red = 45, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 160, .red = 0, .alpha = 255},
      {.blue = 158, .green = 172, .red = 37, .alpha = 161},
      {.blue = 115, .green = 205, .red = 53, .alpha = 222},
      {.blue = 230, .green = 135, .red = 9, .alpha = 222},
      {.blue = 209, .green = 165, .red = 17, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraHue) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kHue,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 161},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 158, .green = 38, .red = 158, .alpha = 161},
      {.blue = 115, .green = 10, .red = 229, .alpha = 222},
      {.blue = 185, .green = 55, .red = 114, .alpha = 222},
      {.blue = 127, .green = 18, .red = 209, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 123, .red = 49, .alpha = 255},
      {.blue = 97, .green = 163, .red = 49, .alpha = 161},
      {.blue = 26, .green = 192, .red = 71, .alpha = 222},
      {.blue = 141, .green = 122, .red = 26, .alpha = 222},
      {.blue = 46, .green = 141, .red = 49, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraSaturation) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kSaturation,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 161},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 196, .green = 62, .red = 96, .alpha = 161},
      {.blue = 169, .green = 45, .red = 139, .alpha = 222},
      {.blue = 239, .green = 90, .red = 24, .alpha = 222},
      {.blue = 227, .green = 81, .red = 45, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 158, .green = 158, .red = 37, .alpha = 161},
      {.blue = 115, .green = 184, .red = 53, .alpha = 222},
      {.blue = 230, .green = 114, .red = 9, .alpha = 222},
      {.blue = 209, .green = 127, .red = 17, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraColor) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kColor,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 161},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 158, .green = 38, .red = 158, .alpha = 161},
      {.blue = 115, .green = 10, .red = 229, .alpha = 222},
      {.blue = 185, .green = 55, .red = 114, .alpha = 222},
      {.blue = 127, .green = 18, .red = 209, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 123, .red = 49, .alpha = 255},
      {.blue = 97, .green = 163, .red = 49, .alpha = 161},
      {.blue = 26, .green = 192, .red = 71, .alpha = 222},
      {.blue = 141, .green = 122, .red = 26, .alpha = 222},
      {.blue = 46, .green = 141, .red = 49, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraLuminosity) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgra,
                              /*src_format=*/FXDIB_Format::kBgra,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kLuminosity,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 255, .green = 100, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 161},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 222},
      {.blue = 255, .green = 100, .red = 0, .alpha = 244},
  };
  RunTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 100, .green = 0, .red = 255, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 196, .green = 62, .red = 96, .alpha = 161},
      {.blue = 169, .green = 45, .red = 139, .alpha = 222},
      {.blue = 239, .green = 90, .red = 24, .alpha = 222},
      {.blue = 227, .green = 81, .red = 45, .alpha = 244},
  };
  RunTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 255, .red = 100, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 185, .red = 142, .alpha = 255},
      {.blue = 158, .green = 178, .red = 71, .alpha = 161},
      {.blue = 115, .green = 214, .red = 104, .alpha = 222},
      {.blue = 230, .green = 144, .red = 59, .alpha = 222},
      {.blue = 209, .green = 181, .red = 108, .alpha = 244},
  };
  RunTest(compositor, kSrcScan3, kExpectations3);
}

#if defined(PDF_USE_SKIA)
TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulNormal) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kNormal,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
      {.blue = 255, .green = 99, .red = 0, .alpha = 161},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
#else
      {.blue = 255, .green = 98, .red = 0, .alpha = 160},
      {.blue = 255, .green = 99, .red = 0, .alpha = 221},
      {.blue = 255, .green = 99, .red = 0, .alpha = 221},
#endif
      {.blue = 255, .green = 99, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
      {.blue = 158, .green = 38, .red = 158, .alpha = 161},
      {.blue = 114, .green = 9, .red = 229, .alpha = 222},
      {.blue = 184, .green = 53, .red = 114, .alpha = 222},
#else
      {.blue = 157, .green = 36, .red = 159, .alpha = 160},
      {.blue = 114, .green = 9, .red = 230, .alpha = 221},
      {.blue = 184, .green = 54, .red = 115, .alpha = 221},
#endif
      {.blue = 126, .green = 17, .red = 209, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
      {.blue = 96, .green = 196, .red = 61, .alpha = 161},
      {.blue = 25, .green = 238, .red = 89, .alpha = 222},
      {.blue = 140, .green = 168, .red = 44, .alpha = 222},
#else
      {.blue = 95, .green = 196, .red = 62, .alpha = 160},
      {.blue = 24, .green = 240, .red = 90, .alpha = 221},
      {.blue = 139, .green = 169, .red = 45, .alpha = 221},
#endif
      {.blue = 45, .green = 227, .red = 81, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulScreen) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kScreen,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 161, .red = 0, .alpha = 255},
      {.blue = 255, .green = 114, .red = 0, .alpha = 161},
      {.blue = 255, .green = 120, .red = 0, .alpha = 222},
      {.blue = 255, .green = 120, .red = 0, .alpha = 222},
      {.blue = 255, .green = 138, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 255, .alpha = 255},
      {.blue = 196, .green = 61, .red = 158, .alpha = 161},
      {.blue = 168, .green = 44, .red = 229, .alpha = 222},
      {.blue = 238, .green = 89, .red = 114, .alpha = 222},
      {.blue = 227, .green = 81, .red = 209, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 255, .red = 100, .alpha = 255},
      {.blue = 158, .green = 196, .red = 61, .alpha = 161},
      {.blue = 114, .green = 238, .red = 89, .alpha = 222},
      {.blue = 229, .green = 168, .red = 44, .alpha = 222},
      {.blue = 209, .green = 227, .red = 81, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulDarken) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kDarken,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
      {.blue = 255, .green = 99, .red = 0, .alpha = 161},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
#else
      {.blue = 253, .green = 98, .red = 0, .alpha = 161},
      {.blue = 253, .green = 98, .red = 0, .alpha = 222},
      {.blue = 253, .green = 98, .red = 0, .alpha = 222},
#endif
      {.blue = 255, .green = 99, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 100, .green = 0, .red = 0, .alpha = 255},
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
      {.blue = 158, .green = 38, .red = 96, .alpha = 161},
      {.blue = 114, .green = 9, .red = 140, .alpha = 222},
      {.blue = 184, .green = 53, .red = 25, .alpha = 222},
#else
      {.blue = 156, .green = 36, .red = 95, .alpha = 161},
      {.blue = 113, .green = 9, .red = 138, .alpha = 222},
      {.blue = 183, .green = 53, .red = 24, .alpha = 222},
#endif
      {.blue = 126, .green = 17, .red = 45, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 100, .red = 0, .alpha = 255},
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
      {.blue = 96, .green = 158, .red = 38, .alpha = 161},
      {.blue = 25, .green = 184, .red = 53, .alpha = 222},
      {.blue = 140, .green = 114, .red = 9, .alpha = 222},
#else
      {.blue = 95, .green = 156, .red = 36, .alpha = 161},
      {.blue = 24, .green = 183, .red = 53, .alpha = 222},
      {.blue = 138, .green = 113, .red = 9, .alpha = 222},
#endif
      {.blue = 45, .green = 126, .red = 17, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulSoftLight) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kSoftLight,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 87, .red = 0, .alpha = 255},
      {.blue = 255, .green = 96, .red = 0, .alpha = 161},
      {.blue = 255, .green = 94, .red = 0, .alpha = 222},
      {.blue = 255, .green = 94, .red = 0, .alpha = 222},
      {.blue = 255, .green = 91, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 39, .red = 0, .alpha = 255},
      {.blue = 196, .green = 47, .red = 96, .alpha = 161},
      {.blue = 168, .green = 22, .red = 140, .alpha = 222},
      {.blue = 238, .green = 67, .red = 25, .alpha = 222},
      {.blue = 227, .green = 43, .red = 45, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 160, .red = 0, .alpha = 255},
      {.blue = 158, .green = 172, .red = 38, .alpha = 161},
      {.blue = 114, .green = 205, .red = 53, .alpha = 222},
      {.blue = 229, .green = 135, .red = 9, .alpha = 222},
      {.blue = 209, .green = 165, .red = 17, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulHue) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kHue,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 99, .red = 0, .alpha = 161},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 99, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 99, .green = 0, .red = 254, .alpha = 255},
      {.blue = 158, .green = 38, .red = 158, .alpha = 161},
      {.blue = 113, .green = 9, .red = 228, .alpha = 222},
      {.blue = 183, .green = 53, .red = 113, .alpha = 222},
      {.blue = 126, .green = 17, .red = 208, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 123, .red = 48, .alpha = 255},
      {.blue = 96, .green = 163, .red = 49, .alpha = 161},
      {.blue = 25, .green = 192, .red = 71, .alpha = 222},
      {.blue = 140, .green = 122, .red = 26, .alpha = 222},
      {.blue = 45, .green = 141, .red = 48, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulSaturation) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kSaturation,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 99, .red = 0, .alpha = 161},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 99, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 196, .green = 61, .red = 96, .alpha = 161},
      {.blue = 168, .green = 44, .red = 140, .alpha = 222},
      {.blue = 238, .green = 89, .red = 25, .alpha = 222},
      {.blue = 227, .green = 81, .red = 45, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 158, .green = 158, .red = 38, .alpha = 161},
      {.blue = 114, .green = 184, .red = 53, .alpha = 222},
      {.blue = 229, .green = 114, .red = 9, .alpha = 222},
      {.blue = 209, .green = 126, .red = 17, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulColor) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kColor,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 99, .red = 0, .alpha = 161},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 99, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 99, .green = 0, .red = 254, .alpha = 255},
      {.blue = 158, .green = 38, .red = 158, .alpha = 161},
      {.blue = 113, .green = 9, .red = 228, .alpha = 222},
      {.blue = 183, .green = 53, .red = 113, .alpha = 222},
      {.blue = 126, .green = 17, .red = 208, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 0, .green = 123, .red = 48, .alpha = 255},
      {.blue = 96, .green = 163, .red = 49, .alpha = 161},
      {.blue = 25, .green = 192, .red = 71, .alpha = 222},
      {.blue = 140, .green = 122, .red = 26, .alpha = 222},
      {.blue = 45, .green = 141, .red = 48, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}

TEST(ScanlineCompositorTest, CompositeRgbBitmapLineBgraPremulLuminosity) {
  CFX_ScanlineCompositor compositor;
  ASSERT_TRUE(compositor.Init(/*dest_format=*/FXDIB_Format::kBgraPremul,
                              /*src_format=*/FXDIB_Format::kBgraPremul,
                              /*src_palette=*/{},
                              /*mask_color=*/0,
                              /*blend_type=*/BlendMode::kLuminosity,
                              /*bRgbByteOrder=*/false));

  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations1[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 99, .red = 0, .alpha = 161},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 98, .red = 0, .alpha = 222},
      {.blue = 255, .green = 99, .red = 0, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan1, kExpectations1);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations2[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 100, .green = 0, .red = 255, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 100, .red = 1, .alpha = 255},
      {.blue = 196, .green = 61, .red = 96, .alpha = 161},
      {.blue = 168, .green = 44, .red = 140, .alpha = 222},
      {.blue = 238, .green = 89, .red = 25, .alpha = 222},
      {.blue = 227, .green = 81, .red = 46, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan2, kExpectations2);
  static constexpr FX_BGRA_STRUCT<uint8_t> kExpectations3[] = {
      {.blue = 0, .green = 0, .red = 0, .alpha = 0},
      {.blue = 0, .green = 255, .red = 100, .alpha = 255},
      {.blue = 255, .green = 100, .red = 0, .alpha = 255},
      {.blue = 255, .green = 186, .red = 142, .alpha = 255},
      {.blue = 158, .green = 178, .red = 72, .alpha = 161},
      {.blue = 114, .green = 214, .red = 104, .alpha = 222},
      {.blue = 229, .green = 144, .red = 59, .alpha = 222},
      {.blue = 209, .green = 182, .red = 109, .alpha = 243},
  };
  RunPreMultiplyTest(compositor, kSrcScan3, kExpectations3);
}
#endif  // defined(PDF_USE_SKIA)
