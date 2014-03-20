// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_thinning.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScrollbarAnimationControllerThinningTest : public testing::Test {
 public:
  ScrollbarAnimationControllerThinningTest() : host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    scroll_layer_ = LayerImpl::Create(host_impl_.active_tree(), 1);
    const int kId = 2;
    const int kThumbThickness = 10;
    const bool kIsLeftSideVerticalScrollbar = false;
    scrollbar_layer_ = SolidColorScrollbarLayerImpl::Create(
        host_impl_.active_tree(), kId, HORIZONTAL, kThumbThickness,
        kIsLeftSideVerticalScrollbar);

    scroll_layer_->SetMaxScrollOffset(gfx::Vector2d(50, 50));
    scroll_layer_->SetBounds(gfx::Size(50, 50));
    scroll_layer_->SetHorizontalScrollbarLayer(scrollbar_layer_.get());

    scrollbar_controller_ = ScrollbarAnimationControllerThinning::CreateForTest(
        scroll_layer_.get(),
        base::TimeDelta::FromSeconds(2),
        base::TimeDelta::FromSeconds(3));
  }

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<ScrollbarAnimationControllerThinning> scrollbar_controller_;
  scoped_ptr<LayerImpl> scroll_layer_;
  scoped_ptr<SolidColorScrollbarLayerImpl> scrollbar_layer_;
};

// Check initialization of scrollbar.
TEST_F(ScrollbarAnimationControllerThinningTest, Idle) {
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Scroll content. Confirm the scrollbar gets dark and then becomes light
// after stopping.
TEST_F(ScrollbarAnimationControllerThinningTest, AwakenByProgrammaticScroll) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  // Scrollbar doesn't change size if triggered by scroll.
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_EQ(1, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Subsequent scroll restarts animation.
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_EQ(1, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
}

// Initiate a scroll when the pointer is already near the scrollbar. It should
// remain thick.
TEST_F(ScrollbarAnimationControllerThinningTest, ScrollWithMouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidMouseMoveNear(time, 1);
  time += base::TimeDelta::FromSeconds(3);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  // Scrollbar should still be thick.
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(5);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

// Move the pointer near the scrollbar. Confirm it gets thick and narrow when
// moved away.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseNear) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveNear(time, 1);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened but not darken.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());

  // Subsequent moves should not change anything.
  scrollbar_controller_->DidMouseMoveNear(time, 1);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());

  // Now move away from bar.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveNear(time, 26);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Animate to narrow.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
}

// Move the pointer over the scrollbar. Make sure it gets thick and dark
// and that it gets thin and light when moved away.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseOver) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveNear(time, 0);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened and darkened.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());

  // Subsequent moves should not change anything.
  scrollbar_controller_->DidMouseMoveNear(time, 0);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());

  // Now move away from bar.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveNear(time, 26);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Animate to narrow.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
}

// First move the pointer near the scrollbar, then over it, then back near
// then far away. Confirm that first the bar gets thick, then dark, then light,
// then narrow.
TEST_F(ScrollbarAnimationControllerThinningTest, MouseNearThenOver) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveNear(time, 1);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());

  // Should animate to thickened but not darken.
  time += base::TimeDelta::FromSeconds(3);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());

  // Now move over.
  scrollbar_controller_->DidMouseMoveNear(time, 0);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(0, scrollbar_controller_->DelayBeforeStart(time).InSeconds());

  // Should animate to darkened.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());

  // This is tricky. The DidMouseMoveOffScrollbar() is sent before the
  // subsequent DidMouseMoveNear(), if the mouse moves in that direction.
  // This results in the thumb thinning. We want to make sure that when the
  // thumb starts expanding it doesn't first narrow to the idle thinness.
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidMouseMoveOffScrollbar(time);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  scrollbar_controller_->DidMouseMoveNear(time, 1);
  // A new animation is kicked off.
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  // We will initiate the narrowing again, but it won't get decremented until
  // the new animation catches up to it.
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  // Now the thickness should be increasing, but it shouldn't happen until the
  // animation catches up.
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  // The thickness now gets big again.
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  // The thickness now gets big again.
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
}

}  // namespace
}  // namespace cc
