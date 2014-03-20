// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skbitmap_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

namespace {

// Returns true if each channel of the given two colors are "close." This is
// used for comparing colors where rounding errors may cause off-by-one.
inline bool ColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) <= 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) <= 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) <= 2 &&
         abs(static_cast<int>(SkColorGetA(a) - SkColorGetA(b))) <= 2;
}

inline bool MultipliedColorsClose(uint32_t a, uint32_t b) {
  return ColorsClose(SkUnPreMultiply::PMColorToColor(a),
                     SkUnPreMultiply::PMColorToColor(b));
}

bool BitmapsClose(const SkBitmap& a, const SkBitmap& b) {
  SkAutoLockPixels a_lock(a);
  SkAutoLockPixels b_lock(b);

  for (int y = 0; y < a.height(); y++) {
    for (int x = 0; x < a.width(); x++) {
      SkColor a_pixel = *a.getAddr32(x, y);
      SkColor b_pixel = *b.getAddr32(x, y);
      if (!ColorsClose(a_pixel, b_pixel))
        return false;
    }
  }
  return true;
}

void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp->allocPixels();

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    src_data[i * 4 + 0] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 1] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 2] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 3] = static_cast<unsigned char>(i % 255);
  }
}

// The reference (i.e., old) implementation of |CreateHSLShiftedBitmap()|.
SkBitmap ReferenceCreateHSLShiftedBitmap(
    const SkBitmap& bitmap,
    color_utils::HSL hsl_shift) {
  SkBitmap shifted;
  shifted.setConfig(SkBitmap::kARGB_8888_Config, bitmap.width(),
                    bitmap.height());
  shifted.allocPixels();
  shifted.eraseARGB(0, 0, 0, 0);

  SkAutoLockPixels lock_bitmap(bitmap);
  SkAutoLockPixels lock_shifted(shifted);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < bitmap.height(); ++y) {
    SkPMColor* pixels = bitmap.getAddr32(0, y);
    SkPMColor* tinted_pixels = shifted.getAddr32(0, y);

    for (int x = 0; x < bitmap.width(); ++x) {
      tinted_pixels[x] = SkPreMultiplyColor(color_utils::HSLShift(
          SkUnPreMultiply::PMColorToColor(pixels[x]), hsl_shift));
    }
  }

  return shifted;
}

}  // namespace

// Invert bitmap and verify the each pixel is inverted and the alpha value is
// not changed.
TEST(SkBitmapOperationsTest, CreateInvertedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      *src.getAddr32(x, y) =
          SkColorSetARGB((255 - i) % 255, i % 255, i * 4 % 255, 0);
    }
  }

  SkBitmap inverted = SkBitmapOperations::CreateInvertedBitmap(src);
  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels inverted_lock(inverted);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      EXPECT_EQ(static_cast<unsigned int>((255 - i) % 255),
                SkColorGetA(*inverted.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(255 - (i % 255)),
                SkColorGetR(*inverted.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(255 - (i * 4 % 255)),
                SkColorGetG(*inverted.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(255),
                SkColorGetB(*inverted.getAddr32(x, y)));
    }
  }
}

// Blend two bitmaps together at 50% alpha and verify that the result
// is the middle-blend of the two.
TEST(SkBitmapOperationsTest, CreateBlendedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src_a;
  src_a.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src_a.allocPixels();

  SkBitmap src_b;
  src_b.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src_b.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src_a.getAddr32(x, y) = SkColorSetARGB(255, 0, i * 2 % 255, i % 255);
      *src_b.getAddr32(x, y) =
          SkColorSetARGB((255 - i) % 255, i % 255, i * 4 % 255, 0);
      i++;
    }
  }

  // Shift to red.
  SkBitmap blended = SkBitmapOperations::CreateBlendedBitmap(
    src_a, src_b, 0.5);
  SkAutoLockPixels srca_lock(src_a);
  SkAutoLockPixels srcb_lock(src_b);
  SkAutoLockPixels blended_lock(blended);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      EXPECT_EQ(static_cast<unsigned int>((255 + ((255 - i) % 255)) / 2),
                SkColorGetA(*blended.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(i % 255 / 2),
                SkColorGetR(*blended.getAddr32(x, y)));
      EXPECT_EQ((static_cast<unsigned int>((i * 2) % 255 + (i * 4) % 255) / 2),
                SkColorGetG(*blended.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(i % 255 / 2),
                SkColorGetB(*blended.getAddr32(x, y)));
    }
  }
}

// Test our masking functions.
TEST(SkBitmapOperationsTest, CreateMaskedBitmap) {
  int src_w = 16, src_h = 16;

  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Generate alpha mask
  SkBitmap alpha;
  alpha.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  alpha.allocPixels();
  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *alpha.getAddr32(x, y) = SkColorSetARGB((i + 128) % 255,
                                              (i + 128) % 255,
                                              (i + 64) % 255,
                                              (i + 0) % 255);
      i++;
    }
  }

  SkBitmap masked = SkBitmapOperations::CreateMaskedBitmap(src, alpha);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels alpha_lock(alpha);
  SkAutoLockPixels masked_lock(masked);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      // Test that the alpha is equal.
      SkColor src_pixel = SkUnPreMultiply::PMColorToColor(*src.getAddr32(x, y));
      SkColor alpha_pixel =
          SkUnPreMultiply::PMColorToColor(*alpha.getAddr32(x, y));
      SkColor masked_pixel = *masked.getAddr32(x, y);

      int alpha_value = SkAlphaMul(SkColorGetA(src_pixel),
                                   SkAlpha255To256(SkColorGetA(alpha_pixel)));
      int alpha_value_256 = SkAlpha255To256(alpha_value);
      SkColor expected_pixel = SkColorSetARGB(
          alpha_value,
          SkAlphaMul(SkColorGetR(src_pixel), alpha_value_256),
          SkAlphaMul(SkColorGetG(src_pixel), alpha_value_256),
          SkAlphaMul(SkColorGetB(src_pixel), alpha_value_256));

      EXPECT_EQ(expected_pixel, masked_pixel);
    }
  }
}

// Make sure that when shifting a bitmap without any shift parameters,
// the end result is close enough to the original (rounding errors
// notwithstanding).
TEST(SkBitmapOperationsTest, CreateHSLShiftedBitmapToSame) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkPreMultiplyColor(SkColorSetARGB((i + 128) % 255,
          (i + 128) % 255, (i + 64) % 255, (i + 0) % 255));
      i++;
    }
  }

  color_utils::HSL hsl = { -1, -1, -1 };
  SkBitmap shifted = ReferenceCreateHSLShiftedBitmap(src, hsl);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels shifted_lock(shifted);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      SkColor src_pixel = *src.getAddr32(x, y);
      SkColor shifted_pixel = *shifted.getAddr32(x, y);
      EXPECT_TRUE(MultipliedColorsClose(src_pixel, shifted_pixel)) <<
          "source: (a,r,g,b) = (" << SkColorGetA(src_pixel) << "," <<
                                     SkColorGetR(src_pixel) << "," <<
                                     SkColorGetG(src_pixel) << "," <<
                                     SkColorGetB(src_pixel) << "); " <<
          "shifted: (a,r,g,b) = (" << SkColorGetA(shifted_pixel) << "," <<
                                     SkColorGetR(shifted_pixel) << "," <<
                                     SkColorGetG(shifted_pixel) << "," <<
                                     SkColorGetB(shifted_pixel) << ")";
    }
  }
}

// Shift a blue bitmap to red.
TEST(SkBitmapOperationsTest, CreateHSLShiftedBitmapHueOnly) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkColorSetARGB(255, 0, 0, i % 255);
      i++;
    }
  }

  // Shift to red.
  color_utils::HSL hsl = { 0, -1, -1 };

  SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(src, hsl);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels shifted_lock(shifted);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_TRUE(ColorsClose(shifted.getColor(x, y),
                              SkColorSetARGB(255, i % 255, 0, 0)));
      i++;
    }
  }
}

// Validate HSL shift.
TEST(SkBitmapOperationsTest, ValidateHSLShift) {
  // Note: 255/51 = 5 (exactly) => 6 including 0!
  const int inc = 51;
  const int dim = 255 / inc + 1;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, dim*dim, dim*dim);
  src.allocPixels();

  for (int a = 0, y = 0; a <= 255; a += inc) {
    for (int r = 0; r <= 255; r += inc, y++) {
      for (int g = 0, x = 0; g <= 255; g += inc) {
        for (int b = 0; b <= 255; b+= inc, x++) {
          *src.getAddr32(x, y) =
              SkPreMultiplyColor(SkColorSetARGB(a, r, g, b));
        }
      }
    }
  }

  // Shhhh. The spec says I should set things to -1 for "no change", but
  // actually -0.1 will do. Don't tell anyone I did this.
  for (double h = -0.1; h <= 1.0001; h += 0.1) {
    for (double s = -0.1; s <= 1.0001; s += 0.1) {
      for (double l = -0.1; l <= 1.0001; l += 0.1) {
        color_utils::HSL hsl = { h, s, l };
        SkBitmap ref_shifted = ReferenceCreateHSLShiftedBitmap(src, hsl);
        SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(src, hsl);
        EXPECT_TRUE(BitmapsClose(ref_shifted, shifted))
            << "h = " << h << ", s = " << s << ", l = " << l;
      }
    }
  }
}

// Test our cropping.
TEST(SkBitmapOperationsTest, CreateCroppedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(src, 4, 4,
                                                              8, 8);
  ASSERT_EQ(8, cropped.width());
  ASSERT_EQ(8, cropped.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels cropped_lock(cropped);
  for (int y = 4; y < 12; y++) {
    for (int x = 4; x < 12; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32(x - 4, y - 4));
    }
  }
}

// Test whether our cropping correctly wraps across image boundaries.
TEST(SkBitmapOperationsTest, CreateCroppedBitmapWrapping) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
      src, src_w / 2, src_h / 2, src_w, src_h);
  ASSERT_EQ(src_w, cropped.width());
  ASSERT_EQ(src_h, cropped.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels cropped_lock(cropped);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32((x + src_w / 2) % src_w,
                                   (y + src_h / 2) % src_h));
    }
  }
}

TEST(SkBitmapOperationsTest, DownsampleByTwo) {
  // Use an odd-sized bitmap to make sure the edge cases where there isn't a
  // 2x2 block of pixels is handled correctly.
  // Here's the ARGB example
  //
  //    50% transparent green             opaque 50% blue           white
  //        80008000                         FF000080              FFFFFFFF
  //
  //    50% transparent red               opaque 50% gray           black
  //        80800000                         80808080              FF000000
  //
  //         black                            white                50% gray
  //        FF000000                         FFFFFFFF              FF808080
  //
  // The result of this computation should be:
  //        A0404040  FF808080
  //        FF808080  FF808080
  SkBitmap input;
  input.setConfig(SkBitmap::kARGB_8888_Config, 3, 3);
  input.allocPixels();

  // The color order may be different, but we don't care (the channels are
  // trated the same).
  *input.getAddr32(0, 0) = 0x80008000;
  *input.getAddr32(1, 0) = 0xFF000080;
  *input.getAddr32(2, 0) = 0xFFFFFFFF;
  *input.getAddr32(0, 1) = 0x80800000;
  *input.getAddr32(1, 1) = 0x80808080;
  *input.getAddr32(2, 1) = 0xFF000000;
  *input.getAddr32(0, 2) = 0xFF000000;
  *input.getAddr32(1, 2) = 0xFFFFFFFF;
  *input.getAddr32(2, 2) = 0xFF808080;

  SkBitmap result = SkBitmapOperations::DownsampleByTwo(input);
  EXPECT_EQ(2, result.width());
  EXPECT_EQ(2, result.height());

  // Some of the values are off-by-one due to rounding.
  SkAutoLockPixels lock(result);
  EXPECT_EQ(0x9f404040, *result.getAddr32(0, 0));
  EXPECT_EQ(0xFF7f7f7f, *result.getAddr32(1, 0));
  EXPECT_EQ(0xFF7f7f7f, *result.getAddr32(0, 1));
  EXPECT_EQ(0xFF808080, *result.getAddr32(1, 1));
}

// Test edge cases for DownsampleByTwo.
TEST(SkBitmapOperationsTest, DownsampleByTwoSmall) {
  SkPMColor reference = 0xFF4080FF;

  // Test a 1x1 bitmap.
  SkBitmap one_by_one;
  one_by_one.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  one_by_one.allocPixels();
  *one_by_one.getAddr32(0, 0) = reference;
  SkBitmap result = SkBitmapOperations::DownsampleByTwo(one_by_one);
  SkAutoLockPixels lock1(result);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(1, result.height());
  EXPECT_EQ(reference, *result.getAddr32(0, 0));

  // Test an n by 1 bitmap.
  SkBitmap one_by_n;
  one_by_n.setConfig(SkBitmap::kARGB_8888_Config, 300, 1);
  one_by_n.allocPixels();
  result = SkBitmapOperations::DownsampleByTwo(one_by_n);
  SkAutoLockPixels lock2(result);
  EXPECT_EQ(300, result.width());
  EXPECT_EQ(1, result.height());

  // Test a 1 by n bitmap.
  SkBitmap n_by_one;
  n_by_one.setConfig(SkBitmap::kARGB_8888_Config, 1, 300);
  n_by_one.allocPixels();
  result = SkBitmapOperations::DownsampleByTwo(n_by_one);
  SkAutoLockPixels lock3(result);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(300, result.height());

  // Test an empty bitmap
  SkBitmap empty;
  result = SkBitmapOperations::DownsampleByTwo(empty);
  EXPECT_TRUE(result.isNull());
  EXPECT_EQ(0, result.width());
  EXPECT_EQ(0, result.height());
}

// Here we assume DownsampleByTwo works correctly (it's tested above) and
// just make sure that the wrapper function does the right thing.
TEST(SkBitmapOperationsTest, DownsampleByTwoUntilSize) {
  // First make sure a "too small" bitmap doesn't get modified at all.
  SkBitmap too_small;
  too_small.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  too_small.allocPixels();
  SkBitmap result = SkBitmapOperations::DownsampleByTwoUntilSize(
      too_small, 16, 16);
  EXPECT_EQ(10, result.width());
  EXPECT_EQ(10, result.height());

  // Now make sure giving it a 0x0 target returns something reasonable.
  result = SkBitmapOperations::DownsampleByTwoUntilSize(too_small, 0, 0);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(1, result.height());

  // Test multiple steps of downsampling.
  SkBitmap large;
  large.setConfig(SkBitmap::kARGB_8888_Config, 100, 43);
  large.allocPixels();
  result = SkBitmapOperations::DownsampleByTwoUntilSize(large, 6, 6);

  // The result should be divided in half 100x43 -> 50x22 -> 25x11
  EXPECT_EQ(25, result.width());
  EXPECT_EQ(11, result.height());
}

TEST(SkBitmapOperationsTest, UnPreMultiply) {
  SkBitmap input;
  input.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  input.allocPixels();

  // Set PMColors into the bitmap
  *input.getAddr32(0, 0) = SkPackARGB32NoCheck(0x80, 0x00, 0x00, 0x00);
  *input.getAddr32(1, 0) = SkPackARGB32NoCheck(0x80, 0x80, 0x80, 0x80);
  *input.getAddr32(0, 1) = SkPackARGB32NoCheck(0xFF, 0x00, 0xCC, 0x88);
  *input.getAddr32(1, 1) = SkPackARGB32NoCheck(0x00, 0x00, 0xCC, 0x88);

  SkBitmap result = SkBitmapOperations::UnPreMultiply(input);
  EXPECT_EQ(2, result.width());
  EXPECT_EQ(2, result.height());

  SkAutoLockPixels lock(result);
  EXPECT_EQ(0x80000000, *result.getAddr32(0, 0));
  EXPECT_EQ(0x80FFFFFF, *result.getAddr32(1, 0));
  EXPECT_EQ(0xFF00CC88, *result.getAddr32(0, 1));
  EXPECT_EQ(0x00000000u, *result.getAddr32(1, 1));  // "Division by zero".
}

TEST(SkBitmapOperationsTest, CreateTransposedBitmap) {
  SkBitmap input;
  input.setConfig(SkBitmap::kARGB_8888_Config, 2, 3);
  input.allocPixels();

  for (int x = 0; x < input.width(); ++x) {
    for (int y = 0; y < input.height(); ++y) {
      *input.getAddr32(x, y) = x * input.width() + y;
    }
  }

  SkBitmap result = SkBitmapOperations::CreateTransposedBitmap(input);
  EXPECT_EQ(3, result.width());
  EXPECT_EQ(2, result.height());

  SkAutoLockPixels lock(result);
  for (int x = 0; x < input.width(); ++x) {
    for (int y = 0; y < input.height(); ++y) {
      EXPECT_EQ(*input.getAddr32(x, y), *result.getAddr32(y, x));
    }
  }
}

// Check that Rotate provides the desired results
TEST(SkBitmapOperationsTest, RotateImage) {
  const int src_w = 6, src_h = 4;
  SkBitmap src;
  // Create a simple 4 color bitmap:
  // RRRBBB
  // RRRBBB
  // GGGYYY
  // GGGYYY
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  SkCanvas canvas(src);
  src.eraseARGB(0, 0, 0, 0);
  SkRegion region;

  region.setRect(0, 0, src_w / 2, src_h / 2);
  canvas.setClipRegion(region);
  // This region is a semi-transparent red to test non-opaque pixels.
  canvas.drawColor(0x1FFF0000, SkXfermode::kSrc_Mode);
  region.setRect(src_w / 2, 0, src_w, src_h / 2);
  canvas.setClipRegion(region);
  canvas.drawColor(SK_ColorBLUE, SkXfermode::kSrc_Mode);
  region.setRect(0, src_h / 2, src_w / 2, src_h);
  canvas.setClipRegion(region);
  canvas.drawColor(SK_ColorGREEN, SkXfermode::kSrc_Mode);
  region.setRect(src_w / 2, src_h / 2, src_w, src_h);
  canvas.setClipRegion(region);
  canvas.drawColor(SK_ColorYELLOW, SkXfermode::kSrc_Mode);
  canvas.flush();

  SkBitmap rotate90, rotate180, rotate270;
  rotate90 = SkBitmapOperations::Rotate(src,
                                        SkBitmapOperations::ROTATION_90_CW);
  rotate180 = SkBitmapOperations::Rotate(src,
                                         SkBitmapOperations::ROTATION_180_CW);
  rotate270 = SkBitmapOperations::Rotate(src,
                                         SkBitmapOperations::ROTATION_270_CW);

  ASSERT_EQ(rotate90.width(), src.height());
  ASSERT_EQ(rotate90.height(), src.width());
  ASSERT_EQ(rotate180.width(), src.width());
  ASSERT_EQ(rotate180.height(), src.height());
  ASSERT_EQ(rotate270.width(), src.height());
  ASSERT_EQ(rotate270.height(), src.width());

  SkAutoLockPixels lock_src(src);
  SkAutoLockPixels lock_90(rotate90);
  SkAutoLockPixels lock_180(rotate180);
  SkAutoLockPixels lock_270(rotate270);

  for (int x=0; x < src_w; ++x) {
    for (int y=0; y < src_h; ++y) {
      ASSERT_EQ(*src.getAddr32(x,y), *rotate90.getAddr32(src_h - (y+1),x));
      ASSERT_EQ(*src.getAddr32(x,y), *rotate270.getAddr32(y, src_w - (x+1)));
      ASSERT_EQ(*src.getAddr32(x,y),
                *rotate180.getAddr32(src_w - (x+1), src_h - (y+1)));
    }
  }
}
