// Copyright 2025 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/image_diff/image_diff_png.h"

#include <stdint.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace image_diff_png {

namespace {

// Test helper to decode `input` bytes into RGBA output, expecting a 1x1 image.
//
// When successful, a vector of exactly 4 bytes is returned.
// An empty vector indicates that an error has occurred.
std::vector<uint8_t> Decode1x1PNGToRGBA(pdfium::span<const uint8_t> input) {
  constexpr bool kReverseByteOrder = false;  // This asks for RGBA output.

  int output_width = -1;
  int output_height = -1;
  std::vector<uint8_t> rgba_output =
      DecodePNG(input, kReverseByteOrder, &output_width, &output_height);
  EXPECT_EQ(output_height, 1);
  EXPECT_EQ(output_width, 1);
  return rgba_output;
}

}  // namespace

TEST(ImageDiffPng, EncodeBGRAPNGAndDiscardTransparency) {
  const std::vector<uint8_t> bgrx_input = {1, 2, 3, /*ignored_alpha=*/4};
  std::vector<uint8_t> png = EncodeBGRAPNG(bgrx_input,
                                           /*width=*/1,
                                           /*height=*/1,
                                           /*row_byte_width=*/4,
                                           /*discard_transparency=*/true);
  ASSERT_FALSE(png.empty());

  std::vector<uint8_t> rgba_output = Decode1x1PNGToRGBA(png);
  ASSERT_EQ(rgba_output.size(), 4u);

  // Verifying that the RGB / BGR pixels are preserved.
  EXPECT_EQ(rgba_output[0], bgrx_input[2]);
  EXPECT_EQ(rgba_output[1], bgrx_input[1]);
  EXPECT_EQ(rgba_output[2], bgrx_input[0]);

  // Verifying that the decoded pixels are fully opaque
  // (i.e. that the `4` in input got discarded / ignored).
  EXPECT_EQ(rgba_output[3], 255);
}

TEST(ImageDiffPng, EncodeBGRAPNGAndPreserveTransparency) {
  const std::vector<uint8_t> bgra_input = {1, 2, 3, 4};
  std::vector<uint8_t> png = EncodeBGRAPNG(bgra_input,
                                           /*width=*/1,
                                           /*height=*/1,
                                           /*row_byte_width=*/4,
                                           /*discard_transparency=*/false);
  ASSERT_FALSE(png.empty());

  std::vector<uint8_t> rgba_output = Decode1x1PNGToRGBA(png);
  ASSERT_EQ(rgba_output.size(), 4u);

  // Verifying that all the RGBA / BGRA pixels are preserved.
  EXPECT_EQ(rgba_output[0], bgra_input[2]);
  EXPECT_EQ(rgba_output[1], bgra_input[1]);
  EXPECT_EQ(rgba_output[2], bgra_input[0]);
  EXPECT_EQ(rgba_output[3], bgra_input[3]);
}

TEST(ImageDiffPng, EncodeBGRPNG) {
  std::vector<uint8_t> bgr_input = {1, 2, 3};
  std::vector<uint8_t> png = EncodeBGRPNG(bgr_input,
                                          /*width=*/1,
                                          /*height=*/1,
                                          /*row_byte_width=*/3);
  ASSERT_FALSE(png.empty());

  std::vector<uint8_t> rgba_output = Decode1x1PNGToRGBA(png);
  ASSERT_EQ(rgba_output.size(), 4u);

  // Verifying that all the RGB / BGR pixels are preserved.
  EXPECT_EQ(rgba_output[0], bgr_input[2]);
  EXPECT_EQ(rgba_output[1], bgr_input[1]);
  EXPECT_EQ(rgba_output[2], bgr_input[0]);

  // Verifying that alpha is opaque.
  EXPECT_EQ(rgba_output[3], 255);
}

TEST(ImageDiffPng, EncodeRGBAPNG) {
  std::vector<uint8_t> rgba_input = {1, 2, 3, 4};
  std::vector<uint8_t> png = EncodeRGBAPNG(rgba_input,
                                           /*width=*/1,
                                           /*height=*/1,
                                           /*row_byte_width=*/4);
  ASSERT_FALSE(png.empty());

  std::vector<uint8_t> rgba_output = Decode1x1PNGToRGBA(png);
  ASSERT_EQ(rgba_output.size(), 4u);

  // Verifying that all the RGBA values are preserved.
  EXPECT_EQ(rgba_output[0], rgba_input[0]);
  EXPECT_EQ(rgba_output[1], rgba_input[1]);
  EXPECT_EQ(rgba_output[2], rgba_input[2]);
  EXPECT_EQ(rgba_output[3], rgba_input[3]);
}

TEST(ImageDiffPng, EncodeGrayPNG) {
  std::vector<uint8_t> grayscale_input = {123};
  std::vector<uint8_t> png = EncodeGrayPNG(grayscale_input,
                                           /*width=*/1,
                                           /*height=*/1,
                                           /*row_byte_width=*/1);
  ASSERT_FALSE(png.empty());

  std::vector<uint8_t> rgba_output = Decode1x1PNGToRGBA(png);
  ASSERT_EQ(rgba_output.size(), 4u);

  // Verifying that all the RGBA values correspond to the input.
  EXPECT_EQ(rgba_output[0], grayscale_input[0]);  // R=gray
  EXPECT_EQ(rgba_output[1], grayscale_input[0]);  // G=gray
  EXPECT_EQ(rgba_output[2], grayscale_input[0]);  // B=gray
  EXPECT_EQ(rgba_output[3], 255);                 // A=opaque
}

}  // namespace image_diff_png
