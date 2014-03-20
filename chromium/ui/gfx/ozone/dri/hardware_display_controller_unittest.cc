// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/ozone/dri/dri_skbitmap.h"
#include "ui/gfx/ozone/dri/dri_surface.h"
#include "ui/gfx/ozone/dri/dri_wrapper.h"
#include "ui/gfx/ozone/dri/hardware_display_controller.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

// Mock file descriptor ID.
const int kFd = 3;

// Mock connector ID.
const uint32_t kConnectorId = 1;

// Mock CRTC ID.
const uint32_t kCrtcId = 1;

const uint32_t kDPMSPropertyId = 1;

// The real DriWrapper makes actual DRM calls which we can't use in unit tests.
class MockDriWrapper : public gfx::DriWrapper {
 public:
  MockDriWrapper(int fd) : DriWrapper(""),
                                get_crtc_call_count_(0),
                                free_crtc_call_count_(0),
                                restore_crtc_call_count_(0),
                                add_framebuffer_call_count_(0),
                                remove_framebuffer_call_count_(0),
                                set_crtc_expectation_(true),
                                add_framebuffer_expectation_(true),
                                page_flip_expectation_(true) {
    fd_ = fd;
  }

  virtual ~MockDriWrapper() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE {
    get_crtc_call_count_++;
    return new drmModeCrtc;
  }

  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {
    free_crtc_call_count_++;
    delete crtc;
  }

  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE {
    return set_crtc_expectation_;
  }

  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    restore_crtc_call_count_++;
    return true;
  }

  virtual bool AddFramebuffer(const drmModeModeInfo& mode,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE {
    add_framebuffer_call_count_++;
    return add_framebuffer_expectation_;
  }

  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE {
    remove_framebuffer_call_count_++;
    return true;
  }

  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    return page_flip_expectation_;
  }

  virtual bool ConnectorSetProperty(uint32_t connector_id,
                                    uint32_t property_id,
                                    uint64_t value) OVERRIDE { return true; }

  int get_get_crtc_call_count() const {
    return get_crtc_call_count_;
  }

  int get_free_crtc_call_count() const {
    return free_crtc_call_count_;
  }

  int get_restore_crtc_call_count() const {
    return restore_crtc_call_count_;
  }

  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }

  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }

  void set_set_crtc_expectation(bool state) {
    set_crtc_expectation_ = state;
  }

  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }

  void set_page_flip_expectation(bool state) {
    page_flip_expectation_ = state;
  }

 private:
  int get_crtc_call_count_;
  int free_crtc_call_count_;
  int restore_crtc_call_count_;
  int add_framebuffer_call_count_;
  int remove_framebuffer_call_count_;

  bool set_crtc_expectation_;
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriWrapper);
};

class MockDriSkBitmap : public gfx::DriSkBitmap {
 public:
  MockDriSkBitmap(int fd) : DriSkBitmap(fd) {}
  virtual ~MockDriSkBitmap() {}

  virtual bool Initialize() OVERRIDE {
    return allocPixels();
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
    return new MockDriSkBitmap(kFd);
  }
  DISALLOW_COPY_AND_ASSIGN(MockDriSurface);
};

}  // namespace

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() {}
  virtual ~HardwareDisplayControllerTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<gfx::HardwareDisplayController> controller_;
  scoped_ptr<MockDriWrapper> drm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerTest);
};

void HardwareDisplayControllerTest::SetUp() {
  controller_.reset(new gfx::HardwareDisplayController());
  drm_.reset(new MockDriWrapper(kFd));
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_.reset();
}

TEST_F(HardwareDisplayControllerTest, CheckInitialState) {
  EXPECT_EQ(gfx::HardwareDisplayController::UNASSOCIATED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest,
       CheckStateAfterControllerIsInitialized) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);

  EXPECT_EQ(1, drm_->get_get_crtc_call_count());
  EXPECT_EQ(gfx::HardwareDisplayController::UNINITIALIZED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterSurfaceIsBound) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::DriSurface> surface(
      new MockDriSurface(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  EXPECT_EQ(2, drm_->get_add_framebuffer_call_count());
  EXPECT_EQ(gfx::HardwareDisplayController::SURFACE_INITIALIZED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfBindingFails) {
  drm_->set_add_framebuffer_expectation(false);

  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::DriSurface> surface(
      new MockDriSurface(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_FALSE(controller_->BindSurfaceToController(surface.Pass()));

  EXPECT_EQ(1, drm_->get_add_framebuffer_call_count());
  EXPECT_EQ(gfx::HardwareDisplayController::FAILED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::DriSurface> surface(
      new MockDriSurface(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  controller_->SchedulePageFlip();

  EXPECT_EQ(gfx::HardwareDisplayController::INITIALIZED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::DriSurface> surface(
      new MockDriSurface(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  controller_->SchedulePageFlip();

  EXPECT_EQ(gfx::HardwareDisplayController::FAILED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfPageFlipFails) {
  drm_->set_page_flip_expectation(false);

  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::DriSurface> surface(
      new MockDriSurface(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  controller_->SchedulePageFlip();

  EXPECT_EQ(gfx::HardwareDisplayController::FAILED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerTest, CheckProperDestruction) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::DriSurface> surface(
      new MockDriSurface(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  EXPECT_EQ(gfx::HardwareDisplayController::SURFACE_INITIALIZED,
            controller_->get_state());

  controller_.reset();

  EXPECT_EQ(2, drm_->get_remove_framebuffer_call_count());
  EXPECT_EQ(1, drm_->get_restore_crtc_call_count());
  EXPECT_EQ(1, drm_->get_free_crtc_call_count());
}
