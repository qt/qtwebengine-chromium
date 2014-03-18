// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/ozone/dri/dri_skbitmap.h"
#include "ui/gfx/ozone/dri/dri_surface.h"
#include "ui/gfx/ozone/dri/dri_surface_factory.h"
#include "ui/gfx/ozone/dri/dri_wrapper.h"
#include "ui/gfx/ozone/dri/hardware_display_controller.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"

namespace {

const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

// Mock file descriptor ID.
const int kFd = 3;

// Mock connector ID.
const uint32_t kConnectorId = 1;

// Mock CRTC ID.
const uint32_t kCrtcId = 1;

const uint32_t kDPMSPropertyId = 1;

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;

// The real DriWrapper makes actual DRM calls which we can't use in unit tests.
class MockDriWrapper : public gfx::DriWrapper {
 public:
  MockDriWrapper(int fd) : DriWrapper(""),
                                add_framebuffer_expectation_(true),
                                page_flip_expectation_(true) {
    fd_ = fd;
  }

  virtual ~MockDriWrapper() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE {
    return new drmModeCrtc;
  }

  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {
    delete crtc;
  }

  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE {
    return true;
  }

  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    return true;
  }

  virtual bool AddFramebuffer(const drmModeModeInfo& mode,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE {
    return add_framebuffer_expectation_;
  }

  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE { return true; }

  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    static_cast<gfx::HardwareDisplayController*>(data)->get_surface()
        ->SwapBuffers();
    return page_flip_expectation_;
  }

  virtual bool ConnectorSetProperty(uint32_t connector_id,
                                    uint32_t property_id,
                                    uint64_t value) OVERRIDE { return true; }

  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }

  void set_page_flip_expectation(bool state) {
    page_flip_expectation_ = state;
  }

 private:
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriWrapper);
};

class MockDriSkBitmap : public gfx::DriSkBitmap {
 public:
  MockDriSkBitmap() : DriSkBitmap(kFd) {}
  virtual ~MockDriSkBitmap() {}

  virtual bool Initialize() OVERRIDE {
    allocPixels();
    eraseColor(SK_ColorBLACK);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDriSkBitmap);
};

class MockDriSurface : public gfx::DriSurface {
 public:
  MockDriSurface(gfx::HardwareDisplayController* controller)
      : DriSurface(controller) {}
  virtual ~MockDriSurface() {}

 private:
  virtual gfx::DriSkBitmap* CreateBuffer() OVERRIDE {
    return new MockDriSkBitmap();
  }

  DISALLOW_COPY_AND_ASSIGN(MockDriSurface);
};

// SSFO would normally allocate DRM resources. We can't rely on having a DRM
// backend to allocate and display our buffers. Thus, we replace these
// resources with stubs. For DRM calls, we simply use stubs that do nothing and
// for buffers we use the default SkBitmap allocator.
class MockDriSurfaceFactory
    : public gfx::DriSurfaceFactory {
 public:
  MockDriSurfaceFactory()
      : DriSurfaceFactory(),
        mock_drm_(NULL),
        drm_wrapper_expectation_(true),
        initialize_controller_expectation_(true) {}
  virtual ~MockDriSurfaceFactory() {};

  void set_drm_wrapper_expectation(bool state) {
    drm_wrapper_expectation_ = state;
  }

  void set_initialize_controller_expectation(bool state) {
    initialize_controller_expectation_ = state;
  }

  MockDriWrapper* get_drm() const {
    return mock_drm_;
  }

 private:
  virtual gfx::DriSurface* CreateSurface(
      gfx::HardwareDisplayController* controller) OVERRIDE {
    return new MockDriSurface(controller);
  }

  virtual gfx::DriWrapper* CreateWrapper() OVERRIDE {
    if (drm_wrapper_expectation_)
      mock_drm_ = new MockDriWrapper(kFd);
    else
      mock_drm_ = new MockDriWrapper(-1);

    return mock_drm_;
  }

  // Normally we'd use DRM to figure out the controller configuration. But we
  // can't use DRM in unit tests, so we just create a fake configuration.
  virtual bool InitializeControllerForPrimaryDisplay(
      gfx::DriWrapper* drm,
      gfx::HardwareDisplayController* controller) OVERRIDE {
    if (initialize_controller_expectation_) {
      controller->SetControllerInfo(drm,
                                    kConnectorId,
                                    kCrtcId,
                                    kDPMSPropertyId,
                                    kDefaultMode);
      return true;
    } else {
      return false;
    }
  }

  virtual void WaitForPageFlipEvent(int fd) OVERRIDE {}

  MockDriWrapper* mock_drm_;
  bool drm_wrapper_expectation_;
  bool initialize_controller_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriSurfaceFactory);
};

}  // namespace

class DriSurfaceFactoryTest : public testing::Test {
 public:
  DriSurfaceFactoryTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<MockDriSurfaceFactory> factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactoryTest);
};

void DriSurfaceFactoryTest::SetUp() {
  message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
  factory_.reset(new MockDriSurfaceFactory());
}

void DriSurfaceFactoryTest::TearDown() {
  factory_.reset();
  message_loop_.reset();
}

TEST_F(DriSurfaceFactoryTest, FailInitialization) {
  factory_->set_drm_wrapper_expectation(false);

  EXPECT_EQ(gfx::SurfaceFactoryOzone::FAILED, factory_->InitializeHardware());
}

TEST_F(DriSurfaceFactoryTest, SuccessfulInitialization) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());
}

TEST_F(DriSurfaceFactoryTest, FailSurfaceInitialization) {
  factory_->set_initialize_controller_expectation(false);

  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_EQ(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));
}

TEST_F(DriSurfaceFactoryTest, FailBindingSurfaceToController) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  factory_->get_drm()->set_add_framebuffer_expectation(false);

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_EQ(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));
}

TEST_F(DriSurfaceFactoryTest, SuccessfulWidgetRealization) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_NE(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));
}

TEST_F(DriSurfaceFactoryTest, FailSchedulePageFlip) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  factory_->get_drm()->set_page_flip_expectation(false);

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_NE(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));

  EXPECT_FALSE(factory_->SchedulePageFlip(w));
}

TEST_F(DriSurfaceFactoryTest, SuccessfulSchedulePageFlip) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_NE(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));

  EXPECT_TRUE(factory_->SchedulePageFlip(w));
}
