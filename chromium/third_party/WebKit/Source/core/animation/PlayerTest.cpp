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
#include "core/animation/Player.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "platform/weborigin/KURL.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class AnimationPlayerTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        timeline = DocumentTimeline::create(document.get());
        player = Player::create(*timeline, 0);
        player->setStartTime(0);
        timeline->setZeroTime(0);
    }

    bool updateTimeline(double time, double* timeToEffectChange = 0)
    {
        document->animationClock().updateTime(time);
        // The timeline does not know about our player, so we have to explicitly call update().
        return player->update(timeToEffectChange);
    }

    RefPtr<Document> document;
    RefPtr<DocumentTimeline> timeline;
    RefPtr<Player> player;
};

TEST_F(AnimationPlayerTest, InitialState)
{
    EXPECT_EQ(0, timeline->currentTime());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(1, player->playbackRate());
    EXPECT_EQ(0, player->startTime());
    EXPECT_EQ(0, player->timeDrift());
}

TEST_F(AnimationPlayerTest, PauseUnpause)
{
    updateTimeline(200);
    player->setPaused(true);
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(200, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());

    updateTimeline(400);
    player->setPaused(false);
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(200, player->currentTime());
    EXPECT_EQ(200, player->timeDrift());

    updateTimeline(600);
    EXPECT_EQ(400, player->currentTime());
    EXPECT_EQ(200, player->timeDrift());
}

TEST_F(AnimationPlayerTest, PauseBeforeTimelineStarted)
{
    player->setPaused(true);
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());

    player->setPaused(false);
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());

    player->setPaused(true);
    updateTimeline(100);
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(100, player->timeDrift());

    player->setPaused(false);
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(100, player->timeDrift());
}

TEST_F(AnimationPlayerTest, PauseBeforeStartTimeSet)
{
    player = Player::create(*timeline, 0);
    updateTimeline(100);
    EXPECT_EQ(0, player->currentTime());

    player->setPaused(true);
    updateTimeline(200);
    EXPECT_EQ(0, player->currentTime());

    player->setStartTime(150);
    EXPECT_EQ(0, player->currentTime());

    player->setPaused(false);
    EXPECT_EQ(0, player->currentTime());

    updateTimeline(300);
    EXPECT_EQ(100, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTime)
{
    updateTimeline(0);
    player->setCurrentTime(250);
    EXPECT_EQ(250, player->currentTime());
    EXPECT_EQ(-250, player->timeDrift());
}

TEST_F(AnimationPlayerTest, SetStartTime)
{
    updateTimeline(0);
    player = Player::create(*timeline, 0);
    EXPECT_FALSE(player->hasStartTime());
    EXPECT_TRUE(isNull(player->startTime()));
    EXPECT_EQ(0, player->currentTime());

    updateTimeline(100);
    player->setStartTime(50);
    EXPECT_TRUE(player->hasStartTime());
    EXPECT_EQ(50, player->startTime());
    EXPECT_EQ(50, player->currentTime());

    updateTimeline(200);
    EXPECT_EQ(150, player->currentTime());
}


TEST_F(AnimationPlayerTest, SetCurrentTimeBeforeTimelineStarted)
{
    player->setCurrentTime(250);
    EXPECT_EQ(250, player->currentTime());
    EXPECT_EQ(-250, player->timeDrift());

    updateTimeline(0);
    EXPECT_EQ(250, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeBeforeStartTimeSet)
{
    updateTimeline(0);
    player = Player::create(*timeline, 0);

    player->setCurrentTime(250);
    EXPECT_EQ(250, player->currentTime());
    EXPECT_EQ(-250, player->timeDrift());

    updateTimeline(100);
    player->setStartTime(50);
    EXPECT_EQ(300, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetPlaybackRate)
{
    updateTimeline(0);
    player->setPlaybackRate(2);
    EXPECT_EQ(2, player->playbackRate());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());

    updateTimeline(100);
    EXPECT_EQ(200, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateBeforeTimelineStarted)
{
    player->setPlaybackRate(2);
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());

    updateTimeline(100);
    EXPECT_EQ(200, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateWhilePaused)
{
    updateTimeline(100);
    player->setPaused(true);
    player->setPlaybackRate(2);
    EXPECT_EQ(100, player->currentTime());
    EXPECT_EQ(100, player->timeDrift());

    updateTimeline(200);
    player->setPaused(false);
    EXPECT_EQ(100, player->currentTime());
    EXPECT_EQ(300, player->timeDrift());

    updateTimeline(250);
    EXPECT_EQ(200, player->currentTime());
    EXPECT_EQ(300, player->timeDrift());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateNaN)
{
    updateTimeline(0);
    player->setPlaybackRate(nullValue());
    EXPECT_TRUE(isNull(player->playbackRate()));
    EXPECT_TRUE(isNull(player->currentTime()));
    EXPECT_TRUE(isNull(player->timeDrift()));

    updateTimeline(100);
    EXPECT_TRUE(isNull(player->currentTime()));
    EXPECT_TRUE(isNull(player->timeDrift()));
}

TEST_F(AnimationPlayerTest, SetPlaybackRateInfinity)
{
    updateTimeline(0);
    player->setPlaybackRate(std::numeric_limits<double>::infinity());
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->playbackRate());
    EXPECT_TRUE(isNull(player->currentTime()));
    EXPECT_TRUE(isNull(player->timeDrift()));

    updateTimeline(100);
    EXPECT_TRUE(isNull(player->currentTime()));
    EXPECT_TRUE(isNull(player->timeDrift()));
}

TEST_F(AnimationPlayerTest, SetPlaybackRateMax)
{
    updateTimeline(0);
    player->setPlaybackRate(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), player->playbackRate());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_EQ(0, player->timeDrift());

    updateTimeline(100);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeNan)
{
    updateTimeline(0);
    player->setCurrentTime(nullValue());
    EXPECT_TRUE(isNull(player->currentTime()));
    EXPECT_TRUE(isNull(player->timeDrift()));

    updateTimeline(100);
    EXPECT_TRUE(isNull(player->currentTime()));
    EXPECT_TRUE(isNull(player->timeDrift()));
}

TEST_F(AnimationPlayerTest, SetCurrentTimeInfinity)
{
    updateTimeline(0);
    player->setCurrentTime(std::numeric_limits<double>::infinity());
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->currentTime());
    EXPECT_EQ(-std::numeric_limits<double>::infinity(), player->timeDrift());

    updateTimeline(100);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->currentTime());
    EXPECT_EQ(-std::numeric_limits<double>::infinity(), player->timeDrift());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeMax)
{
    updateTimeline(0);
    player->setCurrentTime(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), player->currentTime());
    EXPECT_EQ(-std::numeric_limits<double>::max(), player->timeDrift());

    updateTimeline(100);
    EXPECT_EQ(std::numeric_limits<double>::max(), player->currentTime());
    EXPECT_EQ(-std::numeric_limits<double>::max(), player->timeDrift());
}

TEST_F(AnimationPlayerTest, EmptyPlayersDontUpdateEffects)
{
    double timeToNextEffect;
    updateTimeline(0, &timeToNextEffect);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), timeToNextEffect);

    timeToNextEffect = 0;
    updateTimeline(1234, &timeToNextEffect);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), timeToNextEffect);
}

TEST_F(AnimationPlayerTest, PlayersReturnTimeToNextEffect)
{
    Timing timing;
    timing.startDelay = 1;
    timing.iterationDuration = 1;
    timing.hasIterationDuration = true;
    RefPtr<Animation> animation = Animation::create(0, 0, timing);
    player = Player::create(*timeline, animation.get());
    player->setStartTime(0);

    double timeToNextEffect;
    updateTimeline(0, &timeToNextEffect);
    EXPECT_EQ(1, timeToNextEffect);

    updateTimeline(0.5, &timeToNextEffect);
    EXPECT_EQ(0.5, timeToNextEffect);

    updateTimeline(1, &timeToNextEffect);
    EXPECT_EQ(0, timeToNextEffect);

    updateTimeline(1.5, &timeToNextEffect);
    EXPECT_EQ(0, timeToNextEffect);

    updateTimeline(2, &timeToNextEffect);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), timeToNextEffect);

    updateTimeline(3, &timeToNextEffect);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), timeToNextEffect);
}

TEST_F(AnimationPlayerTest, AttachedPlayers)
{
    RefPtr<Element> element = document->createElement("foo", ASSERT_NO_EXCEPTION);

    Timing timing;
    RefPtr<Animation> animation = Animation::create(element, 0, timing);
    RefPtr<Player> player = Player::create(*timeline, animation.get());
    EXPECT_EQ(1U, element->activeAnimations()->players().find(player.get())->value);

    player.release();
    EXPECT_TRUE(element->activeAnimations()->players().isEmpty());
}

}
