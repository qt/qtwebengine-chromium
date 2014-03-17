// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_

#include "ash/ash_export.h"
#include "ui/gfx/transform.h"
#include "ui/views/corewm/window_animations.h"

namespace aura {
class Window;
}
namespace ui {
class Layer;
}

// This is only for animations specific to Ash. For window animations shared
// with desktop Chrome, see ui/views/corewm/window_animations.h.
namespace ash {

// An extension of the window animations provided by CoreWm. These should be
// Ash-specific only.
enum WindowVisibilityAnimationType {
  // Window scale/rotates down to its launcher icon.
  WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE =
      views::corewm::WINDOW_VISIBILITY_ANIMATION_MAX,
  // Fade in/out using brightness and grayscale web filters.
  WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE
};

// Direction for ash-specific window animations used in workspaces and
// lock/unlock animations.
enum LayerScaleAnimationDirection {
  LAYER_SCALE_ANIMATION_ABOVE,
  LAYER_SCALE_ANIMATION_BELOW,
};

// Amount of time for the cross fade animation.
extern const int kCrossFadeDurationMS;

// Animate a cross-fade of |window| from its current bounds to |new_bounds|.
ASH_EXPORT void CrossFadeToBounds(aura::Window* window,
                                  const gfx::Rect& new_bounds);

// Returns the duration of the cross-fade animation based on the |old_bounds|
// and |new_bounds| of the |window|.
ASH_EXPORT base::TimeDelta GetCrossFadeDuration(aura::Window* window,
                                                const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds);

ASH_EXPORT bool AnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                      bool visible);

// Creates vector of animation sequences that lasts for |duration| and changes
// brightness and grayscale to |target_value|. Caller takes ownership of
// returned LayerAnimationSequence objects.
ASH_EXPORT std::vector<ui::LayerAnimationSequence*>
CreateBrightnessGrayscaleAnimationSequence(float target_value,
                                           base::TimeDelta duration);

// Applies scale related to the specified AshWindowScaleType.
ASH_EXPORT void SetTransformForScaleAnimation(
    ui::Layer* layer,
    LayerScaleAnimationDirection type);

// Returns the approximate bounds to which |window| will be animated when it
// is minimized. The bounds are approximate because the minimize animation
// involves rotation.
ASH_EXPORT gfx::Rect GetMinimizeAnimationTargetBoundsInScreen(
    aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
