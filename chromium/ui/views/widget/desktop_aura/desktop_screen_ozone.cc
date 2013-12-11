// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen.h"

#include "ui/gfx/ozone/surface_factory_ozone.h"

namespace views {

gfx::Screen* CreateDesktopScreen() {
  return gfx::SurfaceFactoryOzone::GetInstance()->CreateDesktopScreen();
}

}  // namespace views
