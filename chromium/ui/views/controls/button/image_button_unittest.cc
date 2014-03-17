// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace {

gfx::ImageSkia CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.allocPixels();
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

namespace views {

typedef ViewsTestBase ImageButtonTest;

TEST_F(ImageButtonTest, Basics) {
  ImageButton button(NULL);

  // Our image to paint starts empty.
  EXPECT_TRUE(button.GetImageToPaint().isNull());

  // Without a theme, buttons are 16x14 by default.
  EXPECT_EQ("16x14", button.GetPreferredSize().ToString());

  // We can set a preferred size when we have no image.
  button.SetPreferredSize(gfx::Size(5, 15));
  EXPECT_EQ("5x15", button.GetPreferredSize().ToString());

  // Set a normal image.
  gfx::ImageSkia normal_image = CreateTestImage(10, 20);
  button.SetImage(CustomButton::STATE_NORMAL, &normal_image);

  // Image uses normal image for painting.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // Preferred size is the normal button size.
  EXPECT_EQ("10x20", button.GetPreferredSize().ToString());

  // Set a pushed image.
  gfx::ImageSkia pushed_image = CreateTestImage(11, 21);
  button.SetImage(CustomButton::STATE_PRESSED, &pushed_image);

  // By convention, preferred size doesn't change, even though pushed image
  // is bigger.
  EXPECT_EQ("10x20", button.GetPreferredSize().ToString());

  // We're still painting the normal image.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // Set an overlay image.
  gfx::ImageSkia overlay_image = CreateTestImage(12, 22);
  button.SetOverlayImage(&overlay_image);
  EXPECT_EQ(12, button.overlay_image_.width());
  EXPECT_EQ(22, button.overlay_image_.height());

  // By convention, preferred size doesn't change, even though pushed image
  // is bigger.
  EXPECT_EQ("10x20", button.GetPreferredSize().ToString());

  // We're still painting the normal image.
  EXPECT_FALSE(button.GetImageToPaint().isNull());
  EXPECT_EQ(10, button.GetImageToPaint().width());
  EXPECT_EQ(20, button.GetImageToPaint().height());

  // Reset the overlay image.
  button.SetOverlayImage(NULL);
  EXPECT_TRUE(button.overlay_image_.isNull());
}

TEST_F(ImageButtonTest, SetAndGetImage) {
  ImageButton button(NULL);

  // Images start as null.
  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  // Setting images works as expected.
  gfx::ImageSkia image1 = CreateTestImage(10, 11);
  gfx::ImageSkia image2 = CreateTestImage(20, 21);
  button.SetImage(Button::STATE_NORMAL, &image1);
  button.SetImage(Button::STATE_HOVERED, &image2);
  EXPECT_TRUE(
      button.GetImage(Button::STATE_NORMAL).BackedBySameObjectAs(image1));
  EXPECT_TRUE(
      button.GetImage(Button::STATE_HOVERED).BackedBySameObjectAs(image2));
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  // ImageButton supports NULL image pointers.
  button.SetImage(Button::STATE_NORMAL, NULL);
  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
}

TEST_F(ImageButtonTest, ImagePositionWithBorder) {
  ImageButton button(NULL);
  gfx::ImageSkia image = CreateTestImage(20, 30);
  button.SetImage(CustomButton::STATE_NORMAL, &image);

  // The image should be painted at the top-left corner.
  EXPECT_EQ(gfx::Point().ToString(),
            button.ComputeImagePaintPosition(image).ToString());

  button.set_border(views::Border::CreateEmptyBorder(10, 5, 0, 0));
  EXPECT_EQ(gfx::Point(5, 10).ToString(),
            button.ComputeImagePaintPosition(image).ToString());

  button.set_border(NULL);
  button.SetBounds(0, 0, 50, 50);
  EXPECT_EQ(gfx::Point().ToString(),
            button.ComputeImagePaintPosition(image).ToString());

  button.SetImageAlignment(ImageButton::ALIGN_CENTER,
                           ImageButton::ALIGN_MIDDLE);
  EXPECT_EQ(gfx::Point(15, 10).ToString(),
            button.ComputeImagePaintPosition(image).ToString());
  button.set_border(views::Border::CreateEmptyBorder(10, 10, 0, 0));
  EXPECT_EQ(gfx::Point(20, 15).ToString(),
            button.ComputeImagePaintPosition(image).ToString());
}

TEST_F(ImageButtonTest, LeftAlignedMirrored) {
  ImageButton button(NULL);
  gfx::ImageSkia image = CreateTestImage(20, 30);
  button.SetImage(CustomButton::STATE_NORMAL, &image);
  button.SetBounds(0, 0, 50, 30);
  button.SetImageAlignment(ImageButton::ALIGN_LEFT,
                           ImageButton::ALIGN_BOTTOM);
  button.SetDrawImageMirrored(true);

  // Because the coordinates are flipped, we should expect this to draw as if
  // it were ALIGN_RIGHT.
  EXPECT_EQ(gfx::Point(30, 0).ToString(),
            button.ComputeImagePaintPosition(image).ToString());
}

TEST_F(ImageButtonTest, RightAlignedMirrored) {
  ImageButton button(NULL);
  gfx::ImageSkia image = CreateTestImage(20, 30);
  button.SetImage(CustomButton::STATE_NORMAL, &image);
  button.SetBounds(0, 0, 50, 30);
  button.SetImageAlignment(ImageButton::ALIGN_RIGHT,
                           ImageButton::ALIGN_BOTTOM);
  button.SetDrawImageMirrored(true);

  // Because the coordinates are flipped, we should expect this to draw as if
  // it were ALIGN_LEFT.
  EXPECT_EQ(gfx::Point(0, 0).ToString(),
            button.ComputeImagePaintPosition(image).ToString());
}

}  // namespace views
