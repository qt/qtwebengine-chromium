// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_ROOT_WINDOW_TRANSFORMERS_H_
#define ASH_DISPLAY_ROOT_WINDOW_TRANSFORMERS_H_

#include "ash/ash_export.h"

namespace aura {
class RootWindowTransformer;
class Window;
}

namespace gfx {
class Display;
class Transform;
}

namespace ash {
namespace internal {
class DisplayInfo;

ASH_EXPORT aura::RootWindowTransformer* CreateRootWindowTransformerForDisplay(
    aura::Window* root,
    const gfx::Display& display);

// Creates a RootWindowTransformers for mirror root window.
// |source_display_info| specifies the display being mirrored,
// and |mirror_display_info| specifies the display used to
// mirror the content.
ASH_EXPORT aura::RootWindowTransformer*
CreateRootWindowTransformerForMirroredDisplay(
    const DisplayInfo& source_display_info,
    const DisplayInfo& mirror_display_info);

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_ROOT_WINDOW_TRANSFORMERS_H_
