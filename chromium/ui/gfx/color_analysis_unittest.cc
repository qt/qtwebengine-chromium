// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_analysis.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

using color_utils::FindClosestColor;

namespace {

const unsigned char k1x1White[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
  0xde, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
  0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
  0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00,
  0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xdb, 0x02, 0x11, 0x15,
  0x16, 0x1b, 0xaa, 0x58, 0x38, 0x76, 0x00, 0x00,
  0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x43, 0x6f,
  0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x43, 0x72,
  0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69,
  0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x57,
  0x81, 0x0e, 0x17, 0x00, 0x00, 0x00, 0x0c, 0x49,
  0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
  0xff, 0x3f, 0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc,
  0xcc, 0x59, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

const unsigned char k1x3BlueWhite[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x08, 0x02, 0x00, 0x00, 0x00, 0xdd, 0xbf, 0xf2,
  0xd5, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
  0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
  0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00,
  0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xdb, 0x02, 0x12, 0x01,
  0x0a, 0x2c, 0xfd, 0x08, 0x64, 0x66, 0x00, 0x00,
  0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x43, 0x6f,
  0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x43, 0x72,
  0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69,
  0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x57,
  0x81, 0x0e, 0x17, 0x00, 0x00, 0x00, 0x14, 0x49,
  0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
  0xff, 0x3f, 0x13, 0x03, 0x03, 0x03, 0x03, 0x03,
  0xc3, 0x7f, 0x00, 0x1e, 0xfd, 0x03, 0xff, 0xde,
  0x72, 0x58, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

const unsigned char k1x3BlueRed[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x08, 0x02, 0x00, 0x00, 0x00, 0xdd, 0xbf, 0xf2,
  0xd5, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
  0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
  0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00,
  0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xdb, 0x02, 0x12, 0x01,
  0x07, 0x09, 0x03, 0xa2, 0xce, 0x6c, 0x00, 0x00,
  0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x43, 0x6f,
  0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x43, 0x72,
  0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69,
  0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x57,
  0x81, 0x0e, 0x17, 0x00, 0x00, 0x00, 0x14, 0x49,
  0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xcf,
  0xc0, 0xc0, 0xc4, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
  0xf0, 0x1f, 0x00, 0x0c, 0x10, 0x02, 0x01, 0x2c,
  0x8f, 0x8b, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

class MockKMeanImageSampler : public color_utils::KMeanImageSampler {
 public:
  MockKMeanImageSampler() : current_result_index_(0) {
  }

  explicit MockKMeanImageSampler(const std::vector<int>& samples)
      : prebaked_sample_results_(samples),
        current_result_index_(0) {
  }

  virtual ~MockKMeanImageSampler() {
  }

  void AddSample(int sample) {
    prebaked_sample_results_.push_back(sample);
  }

  void Reset() {
    prebaked_sample_results_.clear();
    ResetCounter();
  }

  void ResetCounter() {
    current_result_index_ = 0;
  }

  virtual int GetSample(int width, int height) OVERRIDE {
    if (current_result_index_ >= prebaked_sample_results_.size()) {
      current_result_index_ = 0;
    }

    if (prebaked_sample_results_.empty()) {
      return 0;
    }

    return prebaked_sample_results_[current_result_index_++];
  }

 protected:
  std::vector<int> prebaked_sample_results_;
  size_t current_result_index_;
};

// Return true if a color channel is approximately equal to an expected value.
bool ChannelApproximatelyEqual(int expected, uint8_t channel) {
  return (abs(expected - static_cast<int>(channel)) <= 1);
}

// Compute minimal and maximal graylevel (or alphalevel) of the input |bitmap|.
// |bitmap| has to be allocated and configured to kA8_Config.
void Calculate8bitBitmapMinMax(const SkBitmap& bitmap,
                               uint8_t* min_gl,
                               uint8_t* max_gl) {
  SkAutoLockPixels bitmap_lock(bitmap);
  DCHECK(bitmap.getPixels());
  DCHECK(bitmap.config() == SkBitmap::kA8_Config);
  DCHECK(min_gl);
  DCHECK(max_gl);
  *min_gl = std::numeric_limits<uint8_t>::max();
  *max_gl = std::numeric_limits<uint8_t>::min();
  for (int y = 0; y < bitmap.height(); ++y) {
    uint8_t* current_color = bitmap.getAddr8(0, y);
    for (int x = 0; x < bitmap.width(); ++x, ++current_color) {
      *min_gl = std::min(*min_gl, *current_color);
      *max_gl = std::max(*max_gl, *current_color);
    }
  }
}

} // namespace

class ColorAnalysisTest : public testing::Test {
};

TEST_F(ColorAnalysisTest, CalculatePNGKMeanAllWhite) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);

  scoped_refptr<base::RefCountedBytes> png(
      new base::RefCountedBytes(
          std::vector<unsigned char>(
              k1x1White,
              k1x1White + sizeof(k1x1White) / sizeof(unsigned char))));

  SkColor color =
      color_utils::CalculateKMeanColorOfPNG(png, 100, 600, &test_sampler);

  EXPECT_EQ(color, SK_ColorWHITE);
}

TEST_F(ColorAnalysisTest, CalculatePNGKMeanIgnoreWhite) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);
  test_sampler.AddSample(1);
  test_sampler.AddSample(2);

  scoped_refptr<base::RefCountedBytes> png(
     new base::RefCountedBytes(
         std::vector<unsigned char>(
             k1x3BlueWhite,
             k1x3BlueWhite + sizeof(k1x3BlueWhite) / sizeof(unsigned char))));

  SkColor color =
      color_utils::CalculateKMeanColorOfPNG(png, 100, 600, &test_sampler);

  EXPECT_EQ(color, SkColorSetARGB(0xFF, 0x00, 0x00, 0xFF));
}

TEST_F(ColorAnalysisTest, CalculatePNGKMeanPickMostCommon) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);
  test_sampler.AddSample(1);
  test_sampler.AddSample(2);

  scoped_refptr<base::RefCountedBytes> png(
     new base::RefCountedBytes(
         std::vector<unsigned char>(
             k1x3BlueRed,
             k1x3BlueRed + sizeof(k1x3BlueRed) / sizeof(unsigned char))));

  SkColor color =
      color_utils::CalculateKMeanColorOfPNG(png, 100, 600, &test_sampler);

  EXPECT_EQ(color, SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00));
}

TEST_F(ColorAnalysisTest, GridSampler) {
  color_utils::GridSampler sampler;
  const int kWidth = 16;
  const int kHeight = 16;
  // Sample starts at 1,1.
  EXPECT_EQ(1 + 1 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(1 + 4 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(1 + 7 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(1 + 10 * kWidth, sampler.GetSample(kWidth, kHeight));
  // Step over by 3.
  EXPECT_EQ(4 + 1 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(4 + 4 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(4 + 7 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(4 + 10 * kWidth, sampler.GetSample(kWidth, kHeight));
}

TEST_F(ColorAnalysisTest, FindClosestColor) {
  // Empty image returns input color.
  SkColor color = FindClosestColor(NULL, 0, 0, SK_ColorRED);
  EXPECT_EQ(SK_ColorRED, color);

  // Single color image returns that color.
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorWHITE);
  color = FindClosestColor(static_cast<uint8_t*>(bitmap.getPixels()),
                           bitmap.width(),
                           bitmap.height(),
                           SK_ColorRED);
  EXPECT_EQ(SK_ColorWHITE, color);

  // Write a black pixel into the image. A dark grey input pixel should match
  // the black one in the image.
  uint32_t* pixel = bitmap.getAddr32(0, 0);
  *pixel = SK_ColorBLACK;
  color = FindClosestColor(static_cast<uint8_t*>(bitmap.getPixels()),
                           bitmap.width(),
                           bitmap.height(),
                           SK_ColorDKGRAY);
  EXPECT_EQ(SK_ColorBLACK, color);
}

TEST_F(ColorAnalysisTest, CalculateKMeanColorOfBitmap) {
  // Create a 16x16 bitmap to represent a favicon.
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  bitmap.allocPixels();
  bitmap.eraseARGB(255, 100, 150, 200);

  SkColor color = color_utils::CalculateKMeanColorOfBitmap(bitmap);
  EXPECT_EQ(255u, SkColorGetA(color));
  // Color values are not exactly equal due to reversal of premultiplied alpha.
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));

  // Test a bitmap with an alpha channel.
  bitmap.eraseARGB(128, 100, 150, 200);
  color = color_utils::CalculateKMeanColorOfBitmap(bitmap);

  // Alpha channel should be ignored for dominant color calculation.
  EXPECT_EQ(255u, SkColorGetA(color));
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));
}

TEST_F(ColorAnalysisTest, ComputeColorCovarianceTrivial) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 200);

  EXPECT_EQ(gfx::Matrix3F::Zeros(),
            color_utils::ComputeColorCovariance(bitmap));
  bitmap.allocPixels();
  bitmap.eraseRGB(50, 150, 200);
  gfx::Matrix3F covariance = color_utils::ComputeColorCovariance(bitmap);
  // The answer should be all zeros.
  EXPECT_TRUE(covariance == gfx::Matrix3F::Zeros());
}

TEST_F(ColorAnalysisTest, ComputeColorCovarianceWithCanvas) {
  gfx::Canvas canvas(gfx::Size(250, 200), 1.0f, true);
  // The image consists of vertical stripes, with color bands set to 100
  // in overlapping stripes 150 pixels wide.
  canvas.FillRect(gfx::Rect(0, 0, 50, 200), SkColorSetRGB(100, 0, 0));
  canvas.FillRect(gfx::Rect(50, 0, 50, 200), SkColorSetRGB(100, 100, 0));
  canvas.FillRect(gfx::Rect(100, 0, 50, 200), SkColorSetRGB(100, 100, 100));
  canvas.FillRect(gfx::Rect(150, 0, 50, 200), SkColorSetRGB(0, 100, 100));
  canvas.FillRect(gfx::Rect(200, 0, 50, 200), SkColorSetRGB(0, 0, 100));

  SkBitmap bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  gfx::Matrix3F covariance = color_utils::ComputeColorCovariance(bitmap);

  gfx::Matrix3F expected_covariance = gfx::Matrix3F::Zeros();
  expected_covariance.set(2400, 400, -1600,
                          400, 2400, 400,
                          -1600, 400, 2400);
  EXPECT_EQ(expected_covariance, covariance);
}

TEST_F(ColorAnalysisTest, ApplyColorReductionSingleColor) {
  // The test runs color reduction on a single-colot image, where results are
  // bound to be uninteresting. This is an important edge case, though.
  SkBitmap source, result;
  source.setConfig(SkBitmap::kARGB_8888_Config, 300, 200);
  result.setConfig(SkBitmap::kA8_Config, 300, 200);

  source.allocPixels();
  result.allocPixels();
  source.eraseRGB(50, 150, 200);

  gfx::Vector3dF transform(1.0f, .5f, 0.1f);
  // This transform, if not scaled, should result in GL=145.
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, false, &result));

  uint8_t min_gl = 0;
  uint8_t max_gl = 0;
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);
  EXPECT_EQ(145, min_gl);
  EXPECT_EQ(145, max_gl);

  // Now scan requesting rescale. Expect all 0.
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &result));
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);
  EXPECT_EQ(0, min_gl);
  EXPECT_EQ(0, max_gl);

  // Test cliping to upper limit.
  transform.set_z(1.1f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, false, &result));
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);
  EXPECT_EQ(0xFF, min_gl);
  EXPECT_EQ(0xFF, max_gl);

  // Test cliping to upper limit.
  transform.Scale(-1.0f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, false, &result));
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);
  EXPECT_EQ(0x0, min_gl);
  EXPECT_EQ(0x0, max_gl);
}

TEST_F(ColorAnalysisTest, ApplyColorReductionBlackAndWhite) {
  // Check with images with multiple colors. This is really different only when
  // the result is scaled.
  gfx::Canvas canvas(gfx::Size(300, 200), 1.0f, true);

  // The image consists of vertical non-overlapping stripes 150 pixels wide.
  canvas.FillRect(gfx::Rect(0, 0, 150, 200), SkColorSetRGB(0, 0, 0));
  canvas.FillRect(gfx::Rect(150, 0, 150, 200), SkColorSetRGB(255, 255, 255));
  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  SkBitmap result;
  result.setConfig(SkBitmap::kA8_Config, 300, 200);
  result.allocPixels();

  gfx::Vector3dF transform(1.0f, 0.5f, 0.1f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &result));
  uint8_t min_gl = 0;
  uint8_t max_gl = 0;
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);

  EXPECT_EQ(0, min_gl);
  EXPECT_EQ(255, max_gl);
  EXPECT_EQ(min_gl, SkColorGetA(result.getColor(0, 0)));
  EXPECT_EQ(max_gl, SkColorGetA(result.getColor(299, 199)));

  // Reverse test.
  transform.Scale(-1.0f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &result));
  min_gl = 0;
  max_gl = 0;
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);

  EXPECT_EQ(0, min_gl);
  EXPECT_EQ(255, max_gl);
  EXPECT_EQ(max_gl, SkColorGetA(result.getColor(0, 0)));
  EXPECT_EQ(min_gl, SkColorGetA(result.getColor(299, 199)));
}

TEST_F(ColorAnalysisTest, ApplyColorReductionMultiColor) {
  // Check with images with multiple colors. This is really different only when
  // the result is scaled.
  gfx::Canvas canvas(gfx::Size(300, 200), 1.0f, true);

  // The image consists of vertical non-overlapping stripes 100 pixels wide.
  canvas.FillRect(gfx::Rect(0, 0, 100, 200), SkColorSetRGB(100, 0, 0));
  canvas.FillRect(gfx::Rect(100, 0, 100, 200), SkColorSetRGB(0, 255, 0));
  canvas.FillRect(gfx::Rect(200, 0, 100, 200), SkColorSetRGB(0, 0, 128));
  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  SkBitmap result;
  result.setConfig(SkBitmap::kA8_Config, 300, 200);
  result.allocPixels();

  gfx::Vector3dF transform(1.0f, 0.5f, 0.1f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, false, &result));
  uint8_t min_gl = 0;
  uint8_t max_gl = 0;
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);
  EXPECT_EQ(12, min_gl);
  EXPECT_EQ(127, max_gl);
  EXPECT_EQ(min_gl, SkColorGetA(result.getColor(299, 199)));
  EXPECT_EQ(max_gl, SkColorGetA(result.getColor(150, 0)));
  EXPECT_EQ(100U, SkColorGetA(result.getColor(0, 0)));

  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &result));
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);
  EXPECT_EQ(0, min_gl);
  EXPECT_EQ(255, max_gl);
  EXPECT_EQ(min_gl, SkColorGetA(result.getColor(299, 199)));
  EXPECT_EQ(max_gl, SkColorGetA(result.getColor(150, 0)));
  EXPECT_EQ(193U, SkColorGetA(result.getColor(0, 0)));
}

TEST_F(ColorAnalysisTest, ComputePrincipalComponentImageNotComputable) {
  SkBitmap source, result;
  source.setConfig(SkBitmap::kARGB_8888_Config, 300, 200);
  result.setConfig(SkBitmap::kA8_Config, 300, 200);

  source.allocPixels();
  result.allocPixels();
  source.eraseRGB(50, 150, 200);

  // This computation should fail since all colors always vary together.
  EXPECT_FALSE(color_utils::ComputePrincipalComponentImage(source, &result));
}

TEST_F(ColorAnalysisTest, ComputePrincipalComponentImage) {
  gfx::Canvas canvas(gfx::Size(300, 200), 1.0f, true);

  // The image consists of vertical non-overlapping stripes 100 pixels wide.
  canvas.FillRect(gfx::Rect(0, 0, 100, 200), SkColorSetRGB(10, 10, 10));
  canvas.FillRect(gfx::Rect(100, 0, 100, 200), SkColorSetRGB(100, 100, 100));
  canvas.FillRect(gfx::Rect(200, 0, 100, 200), SkColorSetRGB(255, 255, 255));
  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  SkBitmap result;
  result.setConfig(SkBitmap::kA8_Config, 300, 200);
  result.allocPixels();

  // This computation should fail since all colors always vary together.
  EXPECT_TRUE(color_utils::ComputePrincipalComponentImage(source, &result));

  uint8_t min_gl = 0;
  uint8_t max_gl = 0;
  Calculate8bitBitmapMinMax(result, &min_gl, &max_gl);

  EXPECT_EQ(0, min_gl);
  EXPECT_EQ(255, max_gl);
  EXPECT_EQ(min_gl, SkColorGetA(result.getColor(0, 0)));
  EXPECT_EQ(max_gl, SkColorGetA(result.getColor(299, 199)));
  EXPECT_EQ(93U, SkColorGetA(result.getColor(150, 0)));
}
