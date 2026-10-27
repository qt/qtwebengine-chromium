// Copyright 2026 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "avif/avif.h"
#include "gtest/gtest.h"
#include "testutil.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

std::string GetFilename(const char* file_name) {
  return std::string(data_path) + file_name;
}

class SkCrabbyAvifCodec {
 public:
  SkCrabbyAvifCodec(const std::string& file_name) : file_name_(file_name) {}

  struct Rect {
    int x;
    int y;
    int width;
    int height;
  };

  struct Options {
    avifRGBFormat rgb_format;
    int rgb_depth;
    int width;
    int height;
    bool gainmap_only;
  };

  avifResult Decode(const Options& options);

 private:
  std::string file_name_;
};

// Call crabbyavif functions roughly in the same way as the skia wrapper does:
// https://skia.googlesource.com/skia/+/e4d0350f477005ae71691642ec0db96cce7e3266/src/codec/SkCrabbyAvifCodec.cpp
avifResult SkCrabbyAvifCodec::Decode(const Options& options) {
  std::vector<uint8_t> data =
      testutil::read_file(GetFilename(file_name_.c_str()).c_str());

  DecoderPtr decoder(avifDecoderCreate());
  if (!decoder) return AVIF_RESULT_OUT_OF_MEMORY;

  decoder->ignoreXMP = AVIF_TRUE;
  decoder->ignoreExif = AVIF_TRUE;
  decoder->strictFlags = AVIF_STRICT_DISABLED;
  decoder->allowSampleTransform = AVIF_FALSE;

  if (options.gainmap_only) {
    decoder->imageContentToDecode = AVIF_IMAGE_CONTENT_GAIN_MAP;
  }

  avifResult res =
      avifDecoderSetIOMemory(decoder.get(), data.data(), data.size());
  if (res != AVIF_RESULT_OK) return res;

  res = avifDecoderParse(decoder.get());
  if (res != AVIF_RESULT_OK) return res;

  if (options.gainmap_only && (!decoder->image || !decoder->image->gainMap)) {
    return AVIF_RESULT_NO_CONTENT;
  }

  avifImage* image =
      options.gainmap_only ? decoder->image->gainMap->image : decoder->image;
  if (!image) return AVIF_RESULT_NO_CONTENT;

  switch (options.rgb_format) {
    case AVIF_RGB_FORMAT_BGRA:
    case AVIF_RGB_FORMAT_RGB565:
      decoder->androidMediaCodecOutputColorFormat =
          ANDROID_MEDIA_CODEC_OUTPUT_COLOR_FORMAT_YUV420_FLEXIBLE;
      break;
    case AVIF_RGB_FORMAT_RGBA1010102:
      decoder->androidMediaCodecOutputColorFormat =
          ANDROID_MEDIA_CODEC_OUTPUT_COLOR_FORMAT_P010;
      break;
    case AVIF_RGB_FORMAT_RGBA:
      if (options.rgb_depth == 8) {
        decoder->androidMediaCodecOutputColorFormat =
            ANDROID_MEDIA_CODEC_OUTPUT_COLOR_FORMAT_YUV420_FLEXIBLE;
      } else {
        decoder->androidMediaCodecOutputColorFormat =
            ANDROID_MEDIA_CODEC_OUTPUT_COLOR_FORMAT_P010;
      }
      break;
    default:
      // Not reached.
      return AVIF_RESULT_NO_CONTENT;
  }

  res = avifDecoderNthImage(decoder.get(), 0);
  if (res != AVIF_RESULT_OK) return res;

  image =
      options.gainmap_only ? decoder->image->gainMap->image : decoder->image;
  if (!image) return AVIF_RESULT_NO_CONTENT;

  ImagePtr cropped_image(nullptr);
  if (image->transformFlags & AVIF_TRANSFORM_CLAP) {
    avifCropRect rect;
    if (avifCropRectConvertCleanApertureBox(&rect, &image->clap, image->width,
                                            image->height, image->yuvFormat,
                                            nullptr)) {
      cropped_image.reset(avifImageCreateEmpty());
      res = avifImageSetViewRect(cropped_image.get(), image, &rect);
      if (res != AVIF_RESULT_OK) return res;
      image = cropped_image.get();
    }
  }

  const uint32_t dst_width =
      (options.width == -1) ? image->width : options.width;
  const uint32_t dst_height =
      (options.height == -1) ? image->height : options.height;
  ImagePtr scaled_image(nullptr);
  if (dst_width != image->width || dst_height != image->height) {
    scaled_image.reset(avifImageCreateEmpty());
    res = avifImageCopy(scaled_image.get(), image, AVIF_PLANES_ALL);
    if (res != AVIF_RESULT_OK) return res;

    image = scaled_image.get();
    res = avifImageScale(image, dst_width, dst_height, &decoder->diag);
    if (res != AVIF_RESULT_OK) return res;
  }

  avifRGBImage rgbImage;
  avifRGBImageSetDefaults(&rgbImage, image);

  rgbImage.format = options.rgb_format;
  rgbImage.depth = options.rgb_depth;
  if (options.rgb_format == AVIF_RGB_FORMAT_RGBA && options.rgb_depth == 16) {
    rgbImage.isFloat = true;
  }

  if (avifRGBImageAllocatePixels(&rgbImage) != AVIF_RESULT_OK) {
    return AVIF_RESULT_OUT_OF_MEMORY;
  }
  res = avifImageYUVToRGB(image, &rgbImage);
  avifRGBImageFreePixels(&rgbImage);
  return res;
}

struct ImageConfig {
  int width;
  int height;
  int depth;
};

struct FileParams {
  const char* file_name;
  ImageConfig image;
  bool has_gainmap;
  ImageConfig gainmap;
};

constexpr FileParams kFiles[] = {
    // AVIF files.
    {"white_2x2.avif", {2, 2, 8}, false},
    {"sofa_grid1x5_420.avif", {1024, 770, 8}, false},
    {"alpha.avif", {80, 80, 8}, false},
    {"grid_icc_individual_cells.avif", {403, 302, 8}, false},

#ifdef __ANDROID__
    // HEIC files.
    {"heic/blue_alpha.heic", {320, 240, 8}, false},
    {"heic/blue_gh_issue_692.heic", {320, 240, 8}, false},
    {"heic/blue_grid_alpha.heic", {320, 240, 8}, false},
    {"heic/blue.heic", {320, 240, 8}, false},
    {"heic/yuv420_image_with_yuv400_gainmap.heic",
     {4032, 3024, 8},
     // Android does not support monochrome gainmaps.
     false},
#endif

    // AVIF files with a supported gainmap.
    {"color_grid_gainmap_different_grid.avif",
     {512, 600, 10},
     true,
     {128, 160, 8}},
    {"seine_sdr_gainmap_srgb.avif", {400, 300, 8}, true, {400, 300, 8}},
};

void PrintTo(const FileParams& param, std::ostream* os) {
  *os << "FileParams{" << param.file_name << ", image: {" << param.image.width
      << "x" << param.image.height << " depth: " << param.image.depth << "}"
      << ", has_gainmap: " << (param.has_gainmap ? "true" : "false")
      << ", gainmap: {" << param.gainmap.width << "x" << param.gainmap.height
      << " depth: " << param.gainmap.depth << "}"
      << "}";
}

struct RGBFormatParams {
  avifRGBFormat format;
  int depth;
};

void PrintTo(const RGBFormatParams& param, std::ostream* os) {
  *os << "RGBFormatParams{" << param.format << ", depth: " << param.depth
      << "}";
}

constexpr float kScaleFactors[] = {0.1, 0.25, 0.5, 0.75, 1.0};
constexpr RGBFormatParams kRgbFormats[] = {{AVIF_RGB_FORMAT_RGBA, 8},
                                           {AVIF_RGB_FORMAT_BGRA, 8},
                                           {AVIF_RGB_FORMAT_RGB565, 8},
                                           {AVIF_RGB_FORMAT_RGBA1010102, 10},
                                           {AVIF_RGB_FORMAT_RGBA, 16}};

class SkCrabbyAvifCodecSimulationDecodeTest
    : public ::testing::TestWithParam<
          std::tuple<FileParams, float, RGBFormatParams>> {};

TEST_P(SkCrabbyAvifCodecSimulationDecodeTest, Decode) {
  const auto& file_param = std::get<0>(GetParam());
  const float scale_factor = std::get<1>(GetParam());
  const auto& rgb_param = std::get<2>(GetParam());

  if (file_param.image.depth == 8 &&
      rgb_param.format == AVIF_RGB_FORMAT_RGBA1010102) {
    GTEST_SKIP() << "Unsupported depth/rgb format combination, skip test.";
  }

  const int width = static_cast<int>(file_param.image.width * scale_factor);
  const int height = static_cast<int>(file_param.image.height * scale_factor);
  if (width < 2 || height < 2) {
    GTEST_SKIP() << "Width/height too small, skip test.";
  }

  const SkCrabbyAvifCodec::Options options{
      .rgb_format = rgb_param.format,
      .rgb_depth = rgb_param.depth,
      .width = width,
      .height = height,
      .gainmap_only = false,
  };
  SkCrabbyAvifCodec codec(file_param.file_name);
  ASSERT_EQ(codec.Decode(options), AVIF_RESULT_OK);

  const int gainmap_width =
      file_param.has_gainmap
          ? static_cast<int>(file_param.gainmap.width * scale_factor)
          : width;
  const int gainmap_height =
      file_param.has_gainmap
          ? static_cast<int>(file_param.gainmap.height * scale_factor)
          : height;

  if (gainmap_width < 2 || gainmap_height < 2) {
    return;
  }

  const SkCrabbyAvifCodec::Options gainmap_options{
      .rgb_format = rgb_param.format,
      .rgb_depth = rgb_param.depth,
      .width = gainmap_width,
      .height = gainmap_height,
      .gainmap_only = true,
  };
  SkCrabbyAvifCodec gainmap_codec(file_param.file_name);

  bool expect_success;
  if (file_param.gainmap.depth == 8 &&
      rgb_param.format == AVIF_RGB_FORMAT_RGBA1010102) {
    // Unsupported depth/rgb format combination.
    expect_success = false;
  } else {
    expect_success = file_param.has_gainmap;
  }
  const auto result = gainmap_codec.Decode(gainmap_options);
  if (expect_success) {
    EXPECT_EQ(result, AVIF_RESULT_OK);
  } else {
    EXPECT_NE(result, AVIF_RESULT_OK);
  }
}

INSTANTIATE_TEST_SUITE_P(GeneralDecodeTests,
                         SkCrabbyAvifCodecSimulationDecodeTest,
                         ::testing::Combine(::testing::ValuesIn(kFiles),
                                            ::testing::ValuesIn(kScaleFactors),
                                            ::testing::ValuesIn(kRgbFormats)));

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "There must be exactly one argument containing the path to "
                 "the test data folder"
              << std::endl;
    return 1;
  }
  avif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
