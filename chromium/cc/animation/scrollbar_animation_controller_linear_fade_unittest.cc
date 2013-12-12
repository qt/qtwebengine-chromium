// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScrollbarAnimationControllerLinearFadeTest : public testing::Test {
 public:
  ScrollbarAnimationControllerLinearFadeTest() : host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    scroll_layer_ = LayerImpl::Create(host_impl_.active_tree(), 1);
    scrollbar_layer_ = PaintedScrollbarLayerImpl::Create(
        host_impl_.active_tree(), 2, HORIZONTAL);

    scroll_layer_->SetMaxScrollOffset(gfx::Vector2d(50, 50));
    scroll_layer_->SetBounds(gfx::Size(50, 50));
    scroll_layer_->SetHorizontalScrollbarLayer(scrollbar_layer_.get());

    scrollbar_controller_ = ScrollbarAnimationControllerLinearFade::Create(
        scroll_layer_.get(),
        base::TimeDelta::FromSeconds(2),
        base::TimeDelta::FromSeconds(3));
  }

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<ScrollbarAnimationControllerLinearFade> scrollbar_controller_;
  scoped_ptr<LayerImpl> scroll_layer_;
  scoped_ptr<PaintedScrollbarLayerImpl> scrollbar_layer_;
};

TEST_F(ScrollbarAnimationControllerLinearFadeTest, HiddenInBegin) {
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest,
       HiddenAfterNonScrollingGesture) {
  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FALSE(scrollbar_controller_->Animate(base::TimeTicks()));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(100);
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
  scrollbar_controller_->DidScrollGestureEnd(time);

  time += base::TimeDelta::FromSeconds(100);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, AwakenByScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollGestureBegin();
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());

  EXPECT_FALSE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  scrollbar_controller_->DidScrollGestureEnd(time);

  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_FALSE(scrollbar_controller_->DidScrollUpdate(time));
  scrollbar_controller_->DidScrollGestureEnd(time);

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, AwakenByProgrammaticScroll) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest,
       AnimationPreservedByNonScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(3);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  scrollbar_controller_->DidScrollGestureEnd(time);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest,
       AnimationOverriddenByScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(3);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_FALSE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollGestureEnd(time);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->opacity());
}

}  // namespace
}  // namespace cc
