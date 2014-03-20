// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/android/edge_effect.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/vector2d_f.h"

class SkBitmap;

namespace cc {
class Layer;
}

namespace content {

/* |OverscrollGlow| mirrors its Android counterpart, OverscrollGlow.java.
 * Conscious tradeoffs were made to align this as closely as possible with the
 * original Android java version.
 */
class OverscrollGlow {
 public:
  // Create a new effect. If |enabled| is false, the effect will remain
  // deactivated until explicitly enabled.
  // Note: No resources will be allocated until the effect is both
  //       enabled and an overscroll event has occurred.
  static scoped_ptr<OverscrollGlow> Create(bool enabled);

  ~OverscrollGlow();

  // Enable the effect. If the effect was previously disabled, it will remain
  // dormant until subsequent calls to |OnOverscrolled()|.
  void Enable();

  // Deactivate and detach the effect. Subsequent calls to |OnOverscrolled()| or
  // |Animate()| will have no effect.
  void Disable();

  // Effect layers will be attached to |overscrolling_layer| if necessary.
  // |overscroll| is the accumulated overscroll for the current gesture.
  // |velocity| is the instantaneous velocity for the overscroll.
  // Returns true if the effect still needs animation ticks.
  bool OnOverscrolled(cc::Layer* overscrolling_layer,
                      base::TimeTicks current_time,
                      gfx::Vector2dF overscroll,
                      gfx::Vector2dF velocity);

  // Returns true if the effect still needs animation ticks.
  // Note: The effect will detach itself when no further animation is required.
  bool Animate(base::TimeTicks current_time);

  // Horizontal overscroll will be ignored when false.
  void set_horizontal_overscroll_enabled(bool enabled) {
    horizontal_overscroll_enabled_ = enabled;
  }
  // Vertical overscroll will be ignored when false.
  void set_vertical_overscroll_enabled(bool enabled) {
    vertical_overscroll_enabled_ = enabled;
  }
  // The size of the layer for which edges will be animated.
  void set_size(gfx::SizeF size) {
    size_ = size;
  }

 private:
  enum Axis { AXIS_X, AXIS_Y };

  OverscrollGlow(bool enabled);

  // Returns whether the effect is initialized.
  bool InitializeIfNecessary();
  bool NeedsAnimate() const;
  void UpdateLayerAttachment(cc::Layer* parent);
  void Detach();
  void Pull(base::TimeTicks current_time,
            gfx::Vector2dF added_overscroll);
  void Absorb(base::TimeTicks current_time,
              gfx::Vector2dF velocity,
              gfx::Vector2dF overscroll,
              gfx::Vector2dF old_overscroll);
  void Release(base::TimeTicks current_time);
  void ReleaseAxis(Axis axis, base::TimeTicks current_time);

  EdgeEffect* GetOppositeEdge(int edge_index);

  scoped_ptr<EdgeEffect> edge_effects_[EdgeEffect::EDGE_COUNT];

  bool enabled_;
  bool initialized_;
  gfx::SizeF size_;
  gfx::Vector2dF old_overscroll_;
  gfx::Vector2dF old_velocity_;
  bool horizontal_overscroll_enabled_;
  bool vertical_overscroll_enabled_;

  scoped_refptr<cc::Layer> root_layer_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollGlow);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SCROLL_GLOW_H_
