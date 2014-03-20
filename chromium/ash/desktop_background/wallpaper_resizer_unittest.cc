// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/wallpaper_resizer.h"

#include "ash/desktop_background/wallpaper_resizer_observer.h"
#include "base/message_loop/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia_rep.h"

using aura::RootWindow;
using aura::Window;

namespace {

const int kTestImageWidth = 5;
const int kTestImageHeight = 2;
const int kTargetWidth = 1;
const int kTargetHeight = 1;
const uint32_t kExpectedCenter = 0x02020202u;
const uint32_t kExpectedCenterCropped = 0x03030303u;
const uint32_t kExpectedStretch = 0x04040404u;
const uint32_t kExpectedTile = 0;

gfx::ImageSkia CreateTestImage(const gfx::Size& size) {
  SkBitmap src;
  int w = size.width();
  int h = size.height();
  src.setConfig(SkBitmap::kARGB_8888_Config, w, h);
  src.allocPixels();

  // Fill bitmap with data.
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t component = static_cast<uint8_t>(y * w + x);
      const SkColor pixel = SkColorSetARGB(component, component,
                                           component, component);
      *(src.getAddr32(x, y)) = pixel;
    }
  }

  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(src);
  return image;
}

bool IsColor(const gfx::ImageSkia& image, const uint32_t expect) {
  EXPECT_EQ(image.width(), kTargetWidth);
  EXPECT_EQ(image.height(), kTargetHeight);
  const SkBitmap* image_bitmap = image.bitmap();
  SkAutoLockPixels image_lock(*image_bitmap);
  return *image_bitmap->getAddr32(0, 0) == expect;
}

}  // namespace

namespace ash {
namespace internal {

class WallpaperResizerTest : public testing::Test,
                             public WallpaperResizerObserver {
 public:
  WallpaperResizerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }
  virtual ~WallpaperResizerTest() {}

  gfx::ImageSkia Resize(const gfx::ImageSkia& image,
                        const gfx::Size& target_size,
                        WallpaperLayout layout) {
    scoped_ptr<WallpaperResizer> resizer;
    resizer.reset(new WallpaperResizer(image, target_size, layout));
    resizer->AddObserver(this);
    resizer->StartResize();
    WaitForResize();
    resizer->RemoveObserver(this);
    return resizer->image();
  }

  void WaitForResize() {
    message_loop_.Run();
  }

  virtual void OnWallpaperResized() OVERRIDE {
    message_loop_.Quit();
  }

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperResizerTest);
};

TEST_F(WallpaperResizerTest, BasicResize) {
  // Keeps in sync with WallpaperLayout enum.
  WallpaperLayout layouts[4] = {
      WALLPAPER_LAYOUT_CENTER,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      WALLPAPER_LAYOUT_STRETCH,
      WALLPAPER_LAYOUT_TILE,
  };
  const int length = arraysize(layouts);

  for (int i = 0; i < length; i++) {
    WallpaperLayout layout = layouts[i];
    gfx::ImageSkia small_image(gfx::ImageSkiaRep(gfx::Size(10, 20), 1.0f));

    gfx::ImageSkia resized_small = Resize(small_image, gfx::Size(800, 600),
                                          layout);
    EXPECT_EQ(10, resized_small.width());
    EXPECT_EQ(20, resized_small.height());

    gfx::ImageSkia large_image(gfx::ImageSkiaRep(gfx::Size(1000, 1000), 1.0f));
    gfx::ImageSkia resized_large = Resize(large_image, gfx::Size(800, 600),
                                          layout);
    EXPECT_EQ(800, resized_large.width());
    EXPECT_EQ(600, resized_large.height());
  }
}

// Test for crbug.com/244629. "CENTER_CROPPED generates the same image as
// STRETCH layout"
TEST_F(WallpaperResizerTest, AllLayoutDifferent) {
  gfx::ImageSkia image = CreateTestImage(
      gfx::Size(kTestImageWidth, kTestImageHeight));

  gfx::Size target_size = gfx::Size(kTargetWidth, kTargetHeight);
  gfx::ImageSkia center = Resize(image, target_size, WALLPAPER_LAYOUT_CENTER);

  gfx::ImageSkia center_cropped = Resize(image, target_size,
                                         WALLPAPER_LAYOUT_CENTER_CROPPED);

  gfx::ImageSkia stretch = Resize(image, target_size, WALLPAPER_LAYOUT_STRETCH);

  gfx::ImageSkia tile = Resize(image, target_size, WALLPAPER_LAYOUT_TILE);

  EXPECT_TRUE(IsColor(center, kExpectedCenter));
  EXPECT_TRUE(IsColor(center_cropped, kExpectedCenterCropped));
  EXPECT_TRUE(IsColor(stretch, kExpectedStretch));
  EXPECT_TRUE(IsColor(tile, kExpectedTile));
}

TEST_F(WallpaperResizerTest, ImageId) {
  gfx::ImageSkia image = CreateTestImage(
      gfx::Size(kTestImageWidth, kTestImageHeight));

  // Create a WallpaperResizer and check that it reports an original image ID
  // both pre- and post-resize that matches the ID returned by GetImageId().
  WallpaperResizer resizer(image, gfx::Size(10, 20), WALLPAPER_LAYOUT_STRETCH);
  EXPECT_EQ(WallpaperResizer::GetImageId(image), resizer.original_image_id());
  resizer.AddObserver(this);
  resizer.StartResize();
  WaitForResize();
  resizer.RemoveObserver(this);
  EXPECT_EQ(WallpaperResizer::GetImageId(image), resizer.original_image_id());
}

}  // namespace internal
}  // namespace ash
