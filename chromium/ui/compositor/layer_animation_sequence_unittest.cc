// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_sequence.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/compositor/test/test_layer_animation_observer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

// Check that the sequence behaves sanely when it contains no elements.
TEST(LayerAnimationSequenceTest, NoElement) {
  LayerAnimationSequence sequence;
  base::TimeTicks start_time;
  start_time += base::TimeDelta::FromSeconds(1);
  sequence.set_start_time(start_time);
  EXPECT_TRUE(sequence.IsFinished(start_time));
  EXPECT_TRUE(sequence.properties().size() == 0);
  LayerAnimationElement::AnimatableProperties properties;
  EXPECT_FALSE(sequence.HasConflictingProperty(properties));
}

// Check that the sequences progresses the delegate as expected when it contains
// a single non-threaded element.
TEST(LayerAnimationSequenceTest, SingleElement) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start = 0.0f;
  float middle = 0.5f;
  float target = 1.0f;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    sequence.set_start_time(start_time);
    delegate.SetBrightnessFromAnimation(start);
    sequence.Start(&delegate);
    sequence.Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, delegate.GetBrightnessForAnimation());
    sequence.Progress(start_time + base::TimeDelta::FromMilliseconds(500),
                      &delegate);
    EXPECT_FLOAT_EQ(middle, delegate.GetBrightnessForAnimation());
    EXPECT_TRUE(sequence.IsFinished(start_time + delta));
    sequence.Progress(start_time + base::TimeDelta::FromMilliseconds(1000),
                      &delegate);
    EXPECT_FLOAT_EQ(target, delegate.GetBrightnessForAnimation());
  }

  EXPECT_TRUE(sequence.properties().size() == 1);
  EXPECT_TRUE(sequence.properties().find(LayerAnimationElement::BRIGHTNESS) !=
              sequence.properties().end());
}

// Check that the sequences progresses the delegate as expected when it contains
// a single threaded element.
TEST(LayerAnimationSequenceTest, SingleThreadedElement) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start = 0.0f;
  float middle = 0.5f;
  float target = 1.0f;
  base::TimeTicks start_time;
  base::TimeTicks effective_start;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateOpacityElement(target, delta));

  for (int i = 0; i < 2; ++i) {
    int starting_group_id = 1;
    sequence.set_animation_group_id(starting_group_id);
    start_time = effective_start + delta;
    sequence.set_start_time(start_time);
    delegate.SetOpacityFromAnimation(start);
    sequence.Start(&delegate);
    sequence.Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(start, sequence.last_progressed_fraction());
    effective_start = start_time + delta;
    sequence.OnThreadedAnimationStarted(cc::AnimationEvent(
        cc::AnimationEvent::Started,
        0,
        sequence.animation_group_id(),
        cc::Animation::Opacity,
        (effective_start - base::TimeTicks()).InSecondsF()));
    sequence.Progress(effective_start + delta/2, &delegate);
    EXPECT_FLOAT_EQ(middle, sequence.last_progressed_fraction());
    EXPECT_TRUE(sequence.IsFinished(effective_start + delta));
    sequence.Progress(effective_start + delta, &delegate);
    EXPECT_FLOAT_EQ(target, sequence.last_progressed_fraction());
    EXPECT_FLOAT_EQ(target, delegate.GetOpacityForAnimation());
  }

  EXPECT_TRUE(sequence.properties().size() == 1);
  EXPECT_TRUE(sequence.properties().find(LayerAnimationElement::OPACITY) !=
              sequence.properties().end());
}

// Check that the sequences progresses the delegate as expected when it contains
// multiple elements. Note, see the layer animator tests for cyclic sequences.
TEST(LayerAnimationSequenceTest, MultipleElement) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start_opacity = 0.0f;
  float target_opacity = 1.0f;
  base::TimeTicks start_time;
  base::TimeTicks opacity_effective_start;
  base::TimeTicks transform_effective_start;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateOpacityElement(target_opacity, delta));

  // Pause bounds for a second.
  LayerAnimationElement::AnimatableProperties properties;
  properties.insert(LayerAnimationElement::BOUNDS);

  sequence.AddElement(
      LayerAnimationElement::CreatePauseElement(properties, delta));

  gfx::Transform start_transform, target_transform, middle_transform;
  start_transform.Rotate(-30.0);
  target_transform.Rotate(30.0);

  sequence.AddElement(
      LayerAnimationElement::CreateTransformElement(target_transform, delta));

  for (int i = 0; i < 2; ++i) {
    int starting_group_id = 1;
    sequence.set_animation_group_id(starting_group_id);
    start_time = opacity_effective_start + 4 * delta;
    sequence.set_start_time(start_time);
    delegate.SetOpacityFromAnimation(start_opacity);
    delegate.SetTransformFromAnimation(start_transform);

    sequence.Start(&delegate);
    sequence.Progress(start_time, &delegate);
    EXPECT_FLOAT_EQ(0.0, sequence.last_progressed_fraction());
    opacity_effective_start = start_time + delta;
    EXPECT_EQ(starting_group_id, sequence.animation_group_id());
    sequence.OnThreadedAnimationStarted(cc::AnimationEvent(
        cc::AnimationEvent::Started,
        0,
        sequence.animation_group_id(),
        cc::Animation::Opacity,
        (opacity_effective_start - base::TimeTicks()).InSecondsF()));
    sequence.Progress(opacity_effective_start + delta/2, &delegate);
    EXPECT_FLOAT_EQ(0.5, sequence.last_progressed_fraction());
    sequence.Progress(opacity_effective_start + delta, &delegate);
    EXPECT_FLOAT_EQ(target_opacity, delegate.GetOpacityForAnimation());

    // Now at the start of the pause.
    EXPECT_FLOAT_EQ(0.0, sequence.last_progressed_fraction());
    TestLayerAnimationDelegate copy = delegate;

    // In the middle of the pause -- nothing should have changed.
    sequence.Progress(opacity_effective_start + delta + delta/2,
                      &delegate);
    CheckApproximatelyEqual(delegate.GetBoundsForAnimation(),
                            copy.GetBoundsForAnimation());
    CheckApproximatelyEqual(delegate.GetTransformForAnimation(),
                            copy.GetTransformForAnimation());
    EXPECT_FLOAT_EQ(delegate.GetOpacityForAnimation(),
                    copy.GetOpacityForAnimation());

    sequence.Progress(opacity_effective_start + 2 * delta, &delegate);
    CheckApproximatelyEqual(start_transform,
                            delegate.GetTransformForAnimation());
    EXPECT_FLOAT_EQ(0.0, sequence.last_progressed_fraction());
    transform_effective_start = opacity_effective_start + 3 * delta;
    EXPECT_NE(starting_group_id, sequence.animation_group_id());
    sequence.OnThreadedAnimationStarted(cc::AnimationEvent(
        cc::AnimationEvent::Started,
        0,
        sequence.animation_group_id(),
        cc::Animation::Transform,
        (transform_effective_start - base::TimeTicks()).InSecondsF()));
    sequence.Progress(transform_effective_start + delta/2, &delegate);
    EXPECT_FLOAT_EQ(0.5, sequence.last_progressed_fraction());
    EXPECT_TRUE(sequence.IsFinished(transform_effective_start + delta));
    sequence.Progress(transform_effective_start + delta, &delegate);
    CheckApproximatelyEqual(target_transform,
                            delegate.GetTransformForAnimation());
  }

  EXPECT_TRUE(sequence.properties().size() == 3);
  EXPECT_TRUE(sequence.properties().find(LayerAnimationElement::OPACITY) !=
              sequence.properties().end());
  EXPECT_TRUE(sequence.properties().find(LayerAnimationElement::TRANSFORM) !=
              sequence.properties().end());
  EXPECT_TRUE(sequence.properties().find(LayerAnimationElement::BOUNDS) !=
              sequence.properties().end());
}

// Check that a sequence can still be aborted if it has cycled many times.
TEST(LayerAnimationSequenceTest, AbortingCyclicSequence) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start_brightness = 0.0f;
  float target_brightness = 1.0f;
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(target_brightness, delta));

  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(start_brightness, delta));

  sequence.set_is_cyclic(true);

  delegate.SetBrightnessFromAnimation(start_brightness);

  start_time += delta;
  sequence.set_start_time(start_time);
  sequence.Start(&delegate);
  sequence.Progress(start_time + base::TimeDelta::FromMilliseconds(101000),
                    &delegate);
  EXPECT_FLOAT_EQ(target_brightness, delegate.GetBrightnessForAnimation());
  sequence.Abort(&delegate);

  // Should be able to reuse the sequence after aborting.
  delegate.SetBrightnessFromAnimation(start_brightness);
  start_time += base::TimeDelta::FromMilliseconds(101000);
  sequence.set_start_time(start_time);
  sequence.Progress(start_time + base::TimeDelta::FromMilliseconds(100000),
                    &delegate);
  EXPECT_FLOAT_EQ(start_brightness, delegate.GetBrightnessForAnimation());
}

// Check that a sequence can be 'fast-forwarded' to the end and the target set.
// Also check that this has no effect if the sequence is cyclic.
TEST(LayerAnimationSequenceTest, SetTarget) {
  LayerAnimationSequence sequence;
  TestLayerAnimationDelegate delegate;
  float start_opacity = 0.0f;
  float target_opacity = 1.0f;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  sequence.AddElement(
      LayerAnimationElement::CreateOpacityElement(target_opacity, delta));

  LayerAnimationElement::TargetValue target_value(&delegate);
  target_value.opacity = start_opacity;
  sequence.GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(target_opacity, target_value.opacity);

  sequence.set_is_cyclic(true);
  target_value.opacity = start_opacity;
  sequence.GetTargetValue(&target_value);
  EXPECT_FLOAT_EQ(start_opacity, target_value.opacity);
}

TEST(LayerAnimationSequenceTest, AddObserver) {
  base::TimeTicks start_time;
  base::TimeDelta delta = base::TimeDelta::FromSeconds(1);
  LayerAnimationSequence sequence;
  sequence.AddElement(
      LayerAnimationElement::CreateBrightnessElement(1.0f, delta));
  for (int i = 0; i < 2; ++i) {
    start_time += delta;
    sequence.set_start_time(start_time);
    TestLayerAnimationObserver observer;
    TestLayerAnimationDelegate delegate;
    sequence.AddObserver(&observer);
    EXPECT_TRUE(!observer.last_ended_sequence());
    sequence.Progress(start_time + delta, &delegate);
    EXPECT_EQ(observer.last_ended_sequence(), &sequence);
    sequence.RemoveObserver(&observer);
  }
}

} // namespace

} // namespace ui
