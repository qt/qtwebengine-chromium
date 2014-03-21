// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_OZONE_PLATFORM_TEST_H_
#define UI_OZONE_PLATFORM_TEST_OZONE_PLATFORM_TEST_H_

#include "base/files/file_path.h"
#include "ui/events/ozone/evdev/event_factory.h"
#include "ui/gfx/ozone/impl/file_surface_factory.h"
#include "ui/ozone/ozone_platform.h"

namespace ui {

// OzonePlatform for testing
//
// This platform dumps images to a file for testing purposes.
class OzonePlatformTest : public OzonePlatform {
 public:
  OzonePlatformTest(const base::FilePath& dump_file);
  virtual ~OzonePlatformTest();

  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE;
  virtual ui::EventFactoryOzone* GetEventFactoryOzone() OVERRIDE;

 private:
  gfx::FileSurfaceFactory surface_factory_ozone_;
  ui::EventFactoryEvdev event_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformTest);
};

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformTest();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_OZONE_PLATFORM_TEST_H_
