// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/wm/window_state.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::wm::WindowState*);

namespace ash {
namespace internal {
DEFINE_WINDOW_PROPERTY_KEY(bool, kStayInSameRootWindowKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(wm::WindowState,
                                 kWindowStateKey, NULL);
}  // namespace internal
}  // namespace ash
