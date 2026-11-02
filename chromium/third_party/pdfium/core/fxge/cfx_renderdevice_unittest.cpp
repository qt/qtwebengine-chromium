// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/cfx_fillrenderoptions.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_path.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/dib/fx_dib.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "core/fxge/win32/cfx_psfonttracker.h"
#endif

TEST(CFXRenderDeviceTest, GetClipBoxDefault) {
  std::unique_ptr<CFX_RenderDevice> device =
      CFX_RenderDevice::CreateForNewBitmap(16, 16, FXDIB_Format::kBgra);
  ASSERT_TRUE(device);

  EXPECT_EQ(FX_RECT(0, 0, 16, 16), device->GetClipBox());
}

TEST(CFXRenderDeviceTest, GetClipBoxPathFill) {
  // Matrix that transposes and translates by 1 unit on each axis.
  const CFX_Matrix object_to_device(0, 1, 1, 0, 1, -1);

  // Fill type cannot be none.
  const CFX_FillRenderOptions fill_options(
      CFX_FillRenderOptions::FillType::kEvenOdd);

  std::unique_ptr<CFX_RenderDevice> device =
      CFX_RenderDevice::CreateForNewBitmap(16, 16, FXDIB_Format::kBgra);
  ASSERT_TRUE(device);

  CFX_Path path;
  path.AppendRect(2, 4, 14, 12);
  EXPECT_TRUE(device->SetClip_PathFill(path, &object_to_device, fill_options));

  EXPECT_EQ(FX_RECT(5, 1, 13, 13), device->GetClipBox());
}

TEST(CFXRenderDeviceTest, GetClipBoxPathStroke) {
  // Matrix that transposes and translates by 1 unit on each axis.
  const CFX_Matrix object_to_device(0, 1, 1, 0, 1, -1);

  // Default line width is 1.
  const CFX_GraphStateData graphics_state;

  std::unique_ptr<CFX_RenderDevice> device =
      CFX_RenderDevice::CreateForNewBitmap(16, 16, FXDIB_Format::kBgra);
  ASSERT_TRUE(device);

  CFX_Path path;
  path.AppendRect(2, 4, 14, 12);
  EXPECT_TRUE(
      device->SetClip_PathStroke(path, &object_to_device, &graphics_state));

  EXPECT_EQ(FX_RECT(4, 0, 14, 14), device->GetClipBox());
}

TEST(CFXRenderDeviceTest, GetClipBoxRect) {
  std::unique_ptr<CFX_RenderDevice> device =
      CFX_RenderDevice::CreateForNewBitmap(16, 16, FXDIB_Format::kBgra);
  ASSERT_TRUE(device);

  EXPECT_TRUE(device->SetClip_Rect({2, 4, 14, 12}));

  EXPECT_EQ(FX_RECT(2, 4, 14, 12), device->GetClipBox());
}

TEST(CFXRenderDeviceTest, GetClipBoxEmpty) {
  std::unique_ptr<CFX_RenderDevice> device =
      CFX_RenderDevice::CreateForNewBitmap(16, 16, FXDIB_Format::kBgra);
  ASSERT_TRUE(device);

  EXPECT_TRUE(device->SetClip_Rect({2, 8, 14, 8}));

  EXPECT_TRUE(device->GetClipBox().IsEmpty());
}

#if BUILDFLAG(IS_WIN)
namespace {

constexpr CFX_Matrix kIdentityMatrix;

}  // namespace

class CFXWindowsRenderDeviceTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    // Get a device context with Windows GDI.
    dc_handle_ = CreateCompatibleDC(nullptr);
    ASSERT_TRUE(dc_handle_);
    device_ =
        CFX_RenderDevice::CreateForWindowsDC(dc_handle_, &psfont_tracker_);
    ASSERT_TRUE(device_);
    device_->SaveState();
  }

  void TearDown() override {
    device_->RestoreState(false);
    device_.reset();
    DeleteDC(dc_handle_);
    testing::Test::TearDown();
  }

 protected:
  HDC dc_handle_;
  CFX_PSFontTracker psfont_tracker_;
  std::unique_ptr<CFX_RenderDevice> device_;
};

TEST_F(CFXWindowsRenderDeviceTest, SimpleClipTriangle) {
  CFX_Path path_data;
  CFX_PointF p1(0.0f, 0.0f);
  CFX_PointF p2(0.0f, 100.0f);
  CFX_PointF p3(100.0f, 100.0f);

  path_data.AppendLine(p1, p2);
  path_data.AppendLine(p2, p3);
  path_data.AppendLine(p3, p1);
  path_data.ClosePath();
  EXPECT_TRUE(device_->SetClip_PathFill(
      path_data, &kIdentityMatrix, CFX_FillRenderOptions::WindingOptions()));
}

TEST_F(CFXWindowsRenderDeviceTest, SimpleClipRect) {
  CFX_Path path_data;

  path_data.AppendRect(0.0f, 100.0f, 200.0f, 0.0f);
  path_data.ClosePath();
  EXPECT_TRUE(device_->SetClip_PathFill(
      path_data, &kIdentityMatrix, CFX_FillRenderOptions::WindingOptions()));
}

TEST_F(CFXWindowsRenderDeviceTest, GargantuanClipRect) {
  CFX_Path path_data;

  path_data.AppendRect(-257698020.0f, -257697252.0f, 257698044.0f,
                       257698812.0f);
  path_data.ClosePath();
  // These coordinates for a clip path are valid, just very large. Using these
  // for a clip path should allow IntersectClipRect() to return success;
  // however they do not because the GDI API IntersectClipRect() errors out and
  // affect subsequent imaging.  crbug.com/1019026
  EXPECT_FALSE(device_->SetClip_PathFill(
      path_data, &kIdentityMatrix, CFX_FillRenderOptions::WindingOptions()));
}

TEST_F(CFXWindowsRenderDeviceTest, GargantuanClipRectWithBaseClip) {
  CFX_Path path_data;
  const FX_RECT kBaseClip(0, 0, 5100, 6600);

  device_->SetBaseClip(kBaseClip);
  path_data.AppendRect(-257698020.0f, -257697252.0f, 257698044.0f,
                       257698812.0f);
  path_data.ClosePath();
  // Use of a reasonable base clip ensures that we avoid getting an error back
  // from GDI API IntersectClipRect().
  EXPECT_TRUE(device_->SetClip_PathFill(
      path_data, &kIdentityMatrix, CFX_FillRenderOptions::WindingOptions()));
}
#endif
