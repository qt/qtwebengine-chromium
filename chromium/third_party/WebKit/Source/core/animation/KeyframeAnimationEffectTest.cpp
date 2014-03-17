/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/KeyframeAnimationEffect.h"

#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/css/CSSPrimitiveValue.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

AnimatableValue* unknownAnimatableValue(double n)
{
    return AnimatableUnknown::create(CSSPrimitiveValue::create(n, CSSPrimitiveValue::CSS_UNKNOWN).get()).leakRef();
}

AnimatableValue* pixelAnimatableValue(double n)
{
    return AnimatableLength::create(CSSPrimitiveValue::create(n, CSSPrimitiveValue::CSS_PX).get()).leakRef();
}

KeyframeAnimationEffect::KeyframeVector keyframesAtZeroAndOne(AnimatableValue* zeroValue, AnimatableValue* oneValue)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(2);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, zeroValue);
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(1.0);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, oneValue);
    return keyframes;
}

void expectDoubleValue(double expectedValue, PassRefPtr<AnimatableValue> value)
{
    ASSERT_TRUE(value->isLength() || value->isUnknown());

    double actualValue;
    if (value->isLength())
        actualValue = toCSSPrimitiveValue(toAnimatableLength(value.get())->toCSSValue().get())->getDoubleValue();
    else
        actualValue = toCSSPrimitiveValue(toAnimatableUnknown(value.get())->toCSSValue().get())->getDoubleValue();

    EXPECT_FLOAT_EQ(static_cast<float>(expectedValue), actualValue);
}


TEST(AnimationKeyframeAnimationEffectTest, BasicOperation)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(unknownAnimatableValue(3.0), unknownAnimatableValue(5.0));
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    OwnPtr<AnimationEffect::CompositableValueList> values = effect->sample(0, 0.6);
    ASSERT_EQ(1UL, values->size());
    EXPECT_EQ(CSSPropertyLeft, values->at(0).first);
    expectDoubleValue(5.0, values->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, CompositeReplaceNonInterpolable)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(unknownAnimatableValue(3.0), unknownAnimatableValue(5.0));
    keyframes[0]->setComposite(AnimationEffect::CompositeReplace);
    keyframes[1]->setComposite(AnimationEffect::CompositeReplace);
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(5.0, effect->sample(0, 0.6)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, CompositeReplace)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(pixelAnimatableValue(3.0), pixelAnimatableValue(5.0));
    keyframes[0]->setComposite(AnimationEffect::CompositeReplace);
    keyframes[1]->setComposite(AnimationEffect::CompositeReplace);
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(3.0 * 0.4 + 5.0 * 0.6, effect->sample(0, 0.6)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, CompositeAdd)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(pixelAnimatableValue(3.0), pixelAnimatableValue(5.0));
    keyframes[0]->setComposite(AnimationEffect::CompositeAdd);
    keyframes[1]->setComposite(AnimationEffect::CompositeAdd);
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, effect->sample(0, 0.6)->at(0).second->compositeOnto(pixelAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, ExtrapolateReplaceNonInterpolable)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(unknownAnimatableValue(3.0), unknownAnimatableValue(5.0));
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    keyframes[0]->setComposite(AnimationEffect::CompositeReplace);
    keyframes[1]->setComposite(AnimationEffect::CompositeReplace);
    expectDoubleValue(5.0, effect->sample(0, 1.6)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, ExtrapolateReplace)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(pixelAnimatableValue(3.0), pixelAnimatableValue(5.0));
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    keyframes[0]->setComposite(AnimationEffect::CompositeReplace);
    keyframes[1]->setComposite(AnimationEffect::CompositeReplace);
    expectDoubleValue(3.0 * -0.6 + 5.0 * 1.6, effect->sample(0, 1.6)->at(0).second->compositeOnto(pixelAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, ExtrapolateAdd)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(pixelAnimatableValue(3.0), pixelAnimatableValue(5.0));
    keyframes[0]->setComposite(AnimationEffect::CompositeAdd);
    keyframes[1]->setComposite(AnimationEffect::CompositeAdd);
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue((7.0 + 3.0) * -0.6 + (7.0 + 5.0) * 1.6, effect->sample(0, 1.6)->at(0).second->compositeOnto(pixelAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, ZeroKeyframes)
{
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(KeyframeAnimationEffect::KeyframeVector());
    EXPECT_TRUE(effect->sample(0, 0.5)->isEmpty());
}

TEST(AnimationKeyframeAnimationEffectTest, SingleKeyframeAtOffsetZero)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(1);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(3.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(3.0, effect->sample(0, 0.6)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, SingleKeyframeAtOffsetOne)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(1);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(1.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, pixelAnimatableValue(5.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(7.0 * 0.4 + 5.0 * 0.6, effect->sample(0, 0.6)->at(0).second->compositeOnto(pixelAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, MoreThanTwoKeyframes)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(3);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(3.0));
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(0.5);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(4.0));
    keyframes[2] = Keyframe::create();
    keyframes[2]->setOffset(1.0);
    keyframes[2]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(5.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(4.0, effect->sample(0, 0.3)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    expectDoubleValue(5.0, effect->sample(0, 0.8)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, EndKeyframeOffsetsUnspecified)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(3);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(3.0));
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(0.5);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(4.0));
    keyframes[2] = Keyframe::create();
    keyframes[2]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(5.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(3.0, effect->sample(0, 0.1)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    expectDoubleValue(4.0, effect->sample(0, 0.6)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    expectDoubleValue(5.0, effect->sample(0, 0.9)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, SampleOnKeyframe)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(3);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(3.0));
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(0.5);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(4.0));
    keyframes[2] = Keyframe::create();
    keyframes[2]->setOffset(1.0);
    keyframes[2]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(5.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(3.0, effect->sample(0, 0.0)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    expectDoubleValue(4.0, effect->sample(0, 0.5)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    expectDoubleValue(5.0, effect->sample(0, 1.0)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

// Note that this tests an implementation detail, not behaviour defined by the spec.
TEST(AnimationKeyframeAnimationEffectTest, SampleReturnsSameAnimatableValueInstance)
{
    AnimatableValue* threePixelsValue = unknownAnimatableValue(3.0);
    AnimatableValue* fourPixelsValue = unknownAnimatableValue(4.0);
    AnimatableValue* fivePixelsValue = unknownAnimatableValue(5.0);

    KeyframeAnimationEffect::KeyframeVector keyframes(3);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, threePixelsValue);
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(0.5);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, fourPixelsValue);
    keyframes[2] = Keyframe::create();
    keyframes[2]->setOffset(1.0);
    keyframes[2]->setPropertyValue(CSSPropertyLeft, fivePixelsValue);

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    EXPECT_EQ(threePixelsValue, effect->sample(0, 0.0)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_EQ(threePixelsValue, effect->sample(0, 0.1)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_EQ(fourPixelsValue, effect->sample(0, 0.4)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_EQ(fourPixelsValue, effect->sample(0, 0.5)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_EQ(fourPixelsValue, effect->sample(0, 0.6)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_EQ(fivePixelsValue, effect->sample(0, 0.9)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_EQ(fivePixelsValue, effect->sample(0, 1.0)->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, MultipleKeyframesWithSameOffset)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(7);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.1);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(1.0));
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(0.1);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(2.0));
    keyframes[2] = Keyframe::create();
    keyframes[2]->setOffset(0.5);
    keyframes[2]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(3.0));
    keyframes[3] = Keyframe::create();
    keyframes[3]->setOffset(0.5);
    keyframes[3]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(4.0));
    keyframes[4] = Keyframe::create();
    keyframes[4]->setOffset(0.5);
    keyframes[4]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(5.0));
    keyframes[5] = Keyframe::create();
    keyframes[5]->setOffset(0.9);
    keyframes[5]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(6.0));
    keyframes[6] = Keyframe::create();
    keyframes[6]->setOffset(0.9);
    keyframes[6]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(7.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(2.0, effect->sample(0, 0.0)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
    expectDoubleValue(2.0, effect->sample(0, 0.2)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
    expectDoubleValue(3.0, effect->sample(0, 0.4)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
    expectDoubleValue(5.0, effect->sample(0, 0.5)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
    expectDoubleValue(5.0, effect->sample(0, 0.6)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
    expectDoubleValue(6.0, effect->sample(0, 0.8)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
    expectDoubleValue(6.0, effect->sample(0, 1.0)->at(0).second->compositeOnto(unknownAnimatableValue(8.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, PerKeyframeComposite)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(2);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, pixelAnimatableValue(3.0));
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(1.0);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, pixelAnimatableValue(5.0));
    keyframes[1]->setComposite(AnimationEffect::CompositeAdd);

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(3.0 * 0.4 + (7.0 + 5.0) * 0.6, effect->sample(0, 0.6)->at(0).second->compositeOnto(pixelAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, MultipleProperties)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(2);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(3.0));
    keyframes[0]->setPropertyValue(CSSPropertyRight, unknownAnimatableValue(4.0));
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(1.0);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, unknownAnimatableValue(5.0));
    keyframes[1]->setPropertyValue(CSSPropertyRight, unknownAnimatableValue(6.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    OwnPtr<AnimationEffect::CompositableValueList> values = effect->sample(0, 0.6);
    ASSERT_EQ(2UL, values->size());
    EXPECT_TRUE(values->at(0).first == CSSPropertyLeft);
    expectDoubleValue(5.0, values->at(0).second->compositeOnto(unknownAnimatableValue(7.0)));
    EXPECT_TRUE(values->at(1).first == CSSPropertyRight);
    expectDoubleValue(6.0, values->at(1).second->compositeOnto(unknownAnimatableValue(7.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, RecompositeCompositableValue)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(pixelAnimatableValue(3.0), pixelAnimatableValue(5.0));
    keyframes[0]->setComposite(AnimationEffect::CompositeAdd);
    keyframes[1]->setComposite(AnimationEffect::CompositeAdd);
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    OwnPtr<AnimationEffect::CompositableValueList> values = effect->sample(0, 0.6);
    expectDoubleValue((7.0 + 3.0) * 0.4 + (7.0 + 5.0) * 0.6, values->at(0).second->compositeOnto(pixelAnimatableValue(7.0)));
    expectDoubleValue((9.0 + 3.0) * 0.4 + (9.0 + 5.0) * 0.6, values->at(0).second->compositeOnto(pixelAnimatableValue(9.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, MultipleIterations)
{
    KeyframeAnimationEffect::KeyframeVector keyframes = keyframesAtZeroAndOne(pixelAnimatableValue(1.0), pixelAnimatableValue(3.0));
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    expectDoubleValue(2.0, effect->sample(0, 0.5)->at(0).second->compositeOnto(unknownAnimatableValue(0.0)));
    expectDoubleValue(2.0, effect->sample(1, 0.5)->at(0).second->compositeOnto(unknownAnimatableValue(0.0)));
    expectDoubleValue(2.0, effect->sample(2, 0.5)->at(0).second->compositeOnto(unknownAnimatableValue(0.0)));
}

TEST(AnimationKeyframeAnimationEffectTest, DependsOnUnderlyingValue)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(3);
    keyframes[0] = Keyframe::create();
    keyframes[0]->setOffset(0.0);
    keyframes[0]->setPropertyValue(CSSPropertyLeft, pixelAnimatableValue(1.0));
    keyframes[0]->setComposite(AnimationEffect::CompositeAdd);
    keyframes[1] = Keyframe::create();
    keyframes[1]->setOffset(0.5);
    keyframes[1]->setPropertyValue(CSSPropertyLeft, pixelAnimatableValue(1.0));
    keyframes[2] = Keyframe::create();
    keyframes[2]->setOffset(1.0);
    keyframes[2]->setPropertyValue(CSSPropertyLeft, pixelAnimatableValue(1.0));

    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);
    EXPECT_TRUE(effect->sample(0, 0)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_TRUE(effect->sample(0, 0.1)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_TRUE(effect->sample(0, 0.25)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_TRUE(effect->sample(0, 0.4)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_FALSE(effect->sample(0, 0.5)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_FALSE(effect->sample(0, 0.6)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_FALSE(effect->sample(0, 0.75)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_FALSE(effect->sample(0, 0.8)->at(0).second->dependsOnUnderlyingValue());
    EXPECT_FALSE(effect->sample(0, 1)->at(0).second->dependsOnUnderlyingValue());
}

TEST(AnimationKeyframeAnimationEffectTest, ToKeyframeAnimationEffect)
{
    KeyframeAnimationEffect::KeyframeVector keyframes(0);
    RefPtr<KeyframeAnimationEffect> effect = KeyframeAnimationEffect::create(keyframes);

    AnimationEffect* baseEffect = effect.get();
    EXPECT_TRUE(toKeyframeAnimationEffect(baseEffect));
}

} // namespace
