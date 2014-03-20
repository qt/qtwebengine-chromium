/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/animation/TimedItem.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TestTimedItemEventDelegate : public TimedItem::EventDelegate {
public:
    void onEventCondition(const TimedItem* timedItem, bool isFirstSample, TimedItem::Phase previousPhase, double previousIteration) OVERRIDE
    {
        m_eventTriggered = true;
        m_phaseChanged = previousPhase != timedItem->phase();
        m_iterationChanged = previousIteration != timedItem->currentIteration();

    }
    void reset()
    {
        m_eventTriggered = false;
        m_phaseChanged = false;
        m_iterationChanged = false;
    }
    bool eventTriggered() { return m_eventTriggered; }
    bool phaseChanged() { return m_phaseChanged; }
    bool iterationChanged() { return m_iterationChanged; }

private:
    bool m_eventTriggered;
    bool m_phaseChanged;
    bool m_iterationChanged;
};

class TestTimedItem : public TimedItem {
public:
    static PassRefPtr<TestTimedItem> create(const Timing& specified)
    {
        return adoptRef(new TestTimedItem(specified, new TestTimedItemEventDelegate()));
    }

    void updateInheritedTime(double time)
    {
        m_eventDelegate->reset();
        TimedItem::updateInheritedTime(time);
    }

    bool updateChildrenAndEffects() const OVERRIDE { return false; }
    void willDetach() { }
    TestTimedItemEventDelegate* eventDelegate() { return m_eventDelegate; }
    double calculateTimeToEffectChange(double localTime, double timeToNextIteration) const OVERRIDE
    {
        m_localTime = localTime;
        m_timeToNextIteration = timeToNextIteration;
        return -1;
    }

    double takeLocalTime()
    {
        const double result = m_localTime;
        m_localTime = nullValue();
        return result;
    }

    double takeTimeToNextIteration()
    {
        const double result = m_timeToNextIteration;
        m_timeToNextIteration = nullValue();
        return result;
    }

private:
    TestTimedItem(const Timing& specified, TestTimedItemEventDelegate* eventDelegate)
        : TimedItem(specified, adoptPtr(eventDelegate))
        , m_eventDelegate(eventDelegate)
    {
    }

    TestTimedItemEventDelegate* m_eventDelegate;
    mutable double m_localTime;
    mutable double m_timeToNextIteration;
};

TEST(AnimationTimedItemTest, Sanity)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 2;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    EXPECT_EQ(0, timedItem->startTime());

    timedItem->updateInheritedTime(0);

    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->startTime());
    EXPECT_EQ(2, timedItem->activeDuration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->startTime());
    EXPECT_EQ(2, timedItem->activeDuration());
    EXPECT_EQ(0.5, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);

    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->startTime());
    EXPECT_EQ(2, timedItem->activeDuration());
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(3);

    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->startTime());
    EXPECT_EQ(2, timedItem->activeDuration());
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, FillForwards)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(2);
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, FillBackwards)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBackwards;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));
}

TEST(AnimationTimedItemTest, FillBoth)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, StartDelay)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.startDelay = 0.5;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0.5);
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.5);
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroIteration)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.iterationCount = 0;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_TRUE(isNull(timedItem->currentIteration()));
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, InfiniteIteration)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(timedItem->currentIteration()));
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, Iteration)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 2;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0.5, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(2);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(5);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, IterationStart)
{
    Timing timing;
    timing.iterationStart = 1.2;
    timing.iterationCount = 2.2;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_NEAR(0.2, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_NEAR(0.2, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(10);
    EXPECT_EQ(3, timedItem->currentIteration());
    EXPECT_NEAR(0.4, timedItem->timeFraction(), 0.000000000000001);
}

TEST(AnimationTimedItemTest, IterationAlternate)
{
    Timing timing;
    timing.iterationCount = 10;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0.75);
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0.75, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.75);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(0.25, timedItem->timeFraction());

    timedItem->updateInheritedTime(2.75);
    EXPECT_EQ(2, timedItem->currentIteration());
    EXPECT_EQ(0.75, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, IterationAlternateReverse)
{
    Timing timing;
    timing.iterationCount = 10;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0.75);
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0.25, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.75);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(0.75, timedItem->timeFraction());

    timedItem->updateInheritedTime(2.75);
    EXPECT_EQ(2, timedItem->currentIteration());
    EXPECT_EQ(0.25, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationSanity)
{
    Timing timing;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    EXPECT_EQ(0, timedItem->startTime());

    timedItem->updateInheritedTime(0);

    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->startTime());
    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->startTime());
    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationFillForwards)
{
    Timing timing;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationFillBackwards)
{
    Timing timing;
    timing.fillMode = Timing::FillModeBackwards;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(0);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(1);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));
}

TEST(AnimationTimedItemTest, ZeroDurationFillBoth)
{
    Timing timing;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationStartDelay)
{
    Timing timing;
    timing.startDelay = 0.5;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0.5);
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1.5);
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationIterationStartAndCount)
{
    Timing timing;
    timing.iterationStart = 0.1;
    timing.iterationCount = 0.2;
    timing.fillMode = Timing::FillModeBoth;
    timing.startDelay = 0.3;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(0.1, timedItem->timeFraction());

    timedItem->updateInheritedTime(0.3);
    EXPECT_DOUBLE_EQ(0.3, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_DOUBLE_EQ(0.3, timedItem->timeFraction());
}

// FIXME: Needs specification work.
TEST(AnimationTimedItemTest, ZeroDurationInfiniteIteration)
{
    Timing timing;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_TRUE(isNull(timedItem->currentIteration()));
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->currentIteration());
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationIteration)
{
    Timing timing;
    timing.iterationCount = 2;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(timedItem->currentIteration()));
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationIterationStart)
{
    Timing timing;
    timing.iterationStart = 1.2;
    timing.iterationCount = 2.2;
    timing.fillMode = Timing::FillModeBoth;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_NEAR(0.2, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(3, timedItem->currentIteration());
    EXPECT_NEAR(0.4, timedItem->timeFraction(), 0.000000000000001);

    timedItem->updateInheritedTime(10);
    EXPECT_EQ(3, timedItem->currentIteration());
    EXPECT_NEAR(0.4, timedItem->timeFraction(), 0.000000000000001);
}

TEST(AnimationTimedItemTest, ZeroDurationIterationAlternate)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(timedItem->currentIteration()));
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, ZeroDurationIterationAlternateReverse)
{
    Timing timing;
    timing.iterationCount = 2;
    timing.direction = Timing::PlaybackDirectionAlternateReverse;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(-1);
    EXPECT_TRUE(isNull(timedItem->currentIteration()));
    EXPECT_TRUE(isNull(timedItem->timeFraction()));

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(1, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);
    EXPECT_EQ(1, timedItem->currentIteration());
    EXPECT_EQ(1, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, InfiniteDurationSanity)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.iterationCount = 1;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    EXPECT_EQ(0, timedItem->startTime());

    timedItem->updateInheritedTime(0);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

// FIXME: Needs specification work.
TEST(AnimationTimedItemTest, InfiniteDurationZeroIterations)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.iterationCount = 0;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    EXPECT_EQ(0, timedItem->startTime());

    timedItem->updateInheritedTime(0);

    EXPECT_EQ(0, timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, InfiniteDurationInfiniteIterations)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.iterationCount = std::numeric_limits<double>::infinity();
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    EXPECT_EQ(0, timedItem->startTime());

    timedItem->updateInheritedTime(0);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(1);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, InfiniteDurationZeroPlaybackRate)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = std::numeric_limits<double>::infinity();
    timing.playbackRate = 0;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    EXPECT_EQ(0, timedItem->startTime());

    timedItem->updateInheritedTime(0);

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_TRUE(timedItem->isInPlay());
    EXPECT_TRUE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());

    timedItem->updateInheritedTime(std::numeric_limits<double>::infinity());

    EXPECT_EQ(std::numeric_limits<double>::infinity(), timedItem->activeDuration());
    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_FALSE(timedItem->isInPlay());
    EXPECT_FALSE(timedItem->isCurrent());
    EXPECT_TRUE(timedItem->isInEffect());
    EXPECT_EQ(0, timedItem->currentIteration());
    EXPECT_EQ(0, timedItem->timeFraction());
}

TEST(AnimationTimedItemTest, Events)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.iterationCount = 2;
    timing.startDelay = 1;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    // First sample
    timedItem->updateInheritedTime(0.0);
    EXPECT_TRUE(timedItem->eventDelegate()->eventTriggered());

    // Before start
    timedItem->updateInheritedTime(0.5);
    EXPECT_FALSE(timedItem->eventDelegate()->eventTriggered());

    // First iteration
    timedItem->updateInheritedTime(1.5);
    EXPECT_TRUE(timedItem->eventDelegate()->eventTriggered());
    EXPECT_TRUE(timedItem->eventDelegate()->phaseChanged());
    EXPECT_TRUE(timedItem->eventDelegate()->iterationChanged());

    timedItem->updateInheritedTime(1.6);
    EXPECT_FALSE(timedItem->eventDelegate()->eventTriggered());

    // Second iteration
    timedItem->updateInheritedTime(2.5);
    EXPECT_TRUE(timedItem->eventDelegate()->eventTriggered());
    EXPECT_FALSE(timedItem->eventDelegate()->phaseChanged());
    EXPECT_TRUE(timedItem->eventDelegate()->iterationChanged());

    timedItem->updateInheritedTime(2.6);
    EXPECT_FALSE(timedItem->eventDelegate()->eventTriggered());

    // After end
    timedItem->updateInheritedTime(3.5);
    EXPECT_TRUE(timedItem->eventDelegate()->eventTriggered());
    EXPECT_TRUE(timedItem->eventDelegate()->phaseChanged());
    EXPECT_FALSE(timedItem->eventDelegate()->iterationChanged());

    timedItem->updateInheritedTime(3.6);
    EXPECT_FALSE(timedItem->eventDelegate()->eventTriggered());
}

TEST(AnimationTimedItemTest, TimeToEffectChange)
{
    Timing timing;
    timing.hasIterationDuration = true;
    timing.iterationDuration = 1;
    timing.iterationStart = 0.2;
    timing.iterationCount = 2.5;
    timing.startDelay = 1;
    timing.direction = Timing::PlaybackDirectionAlternate;
    RefPtr<TestTimedItem> timedItem = TestTimedItem::create(timing);

    timedItem->updateInheritedTime(0);
    EXPECT_EQ(0, timedItem->takeLocalTime());
    EXPECT_TRUE(std::isinf(timedItem->takeTimeToNextIteration()));

    // Normal iteration.
    timedItem->updateInheritedTime(1.75);
    EXPECT_EQ(1.75, timedItem->takeLocalTime());
    EXPECT_NEAR(0.05, timedItem->takeTimeToNextIteration(), 0.000000000000001);

    // Reverse iteration.
    timedItem->updateInheritedTime(2.75);
    EXPECT_EQ(2.75, timedItem->takeLocalTime());
    EXPECT_NEAR(0.05, timedItem->takeTimeToNextIteration(), 0.000000000000001);

    // Item ends before iteration finishes.
    timedItem->updateInheritedTime(3.4);
    EXPECT_EQ(TimedItem::PhaseActive, timedItem->phase());
    EXPECT_EQ(3.4, timedItem->takeLocalTime());
    EXPECT_TRUE(std::isinf(timedItem->takeTimeToNextIteration()));

    // Item has finished.
    timedItem->updateInheritedTime(3.5);
    EXPECT_EQ(TimedItem::PhaseAfter, timedItem->phase());
    EXPECT_EQ(3.5, timedItem->takeLocalTime());
    EXPECT_TRUE(std::isinf(timedItem->takeTimeToNextIteration()));
}

}
