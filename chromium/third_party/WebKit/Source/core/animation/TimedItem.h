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

#ifndef TimedItem_h
#define TimedItem_h

#include "core/animation/Timing.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class Player;
class TimedItem;

static inline bool isNull(double value)
{
    return std::isnan(value);
}

static inline double nullValue()
{
    return std::numeric_limits<double>::quiet_NaN();
}

class TimedItem : public RefCounted<TimedItem> {
    friend class Player; // Calls attach/detach, updateInheritedTime.
public:
    // Note that logic in CSSAnimations depends on the order of these values.
    enum Phase {
        PhaseBefore,
        PhaseActive,
        PhaseAfter,
        PhaseNone,
    };

    class EventDelegate {
    public:
        virtual ~EventDelegate() { };
        virtual void onEventCondition(const TimedItem*, bool isFirstSample, Phase previousPhase, double previousIteration) = 0;
    };

    virtual ~TimedItem() { }

    virtual bool isAnimation() const { return false; }

    Phase phase() const { return ensureCalculated().phase; }
    bool isCurrent() const { return ensureCalculated().isCurrent; }
    bool isInEffect() const { return ensureCalculated().isInEffect; }
    bool isInPlay() const { return ensureCalculated().isInPlay; }
    double timeToEffectChange() const { return ensureCalculated().timeToEffectChange; }

    double currentIteration() const { return ensureCalculated().currentIteration; }
    double activeDuration() const { return ensureCalculated().activeDuration; }
    double timeFraction() const { return ensureCalculated().timeFraction; }
    double startTime() const { return m_startTime; }
    const Player* player() const { return m_player; }
    Player* player() { return m_player; }
    const Timing& specified() const { return m_specified; }

protected:
    TimedItem(const Timing&, PassOwnPtr<EventDelegate> = nullptr);

    // When TimedItem receives a new inherited time via updateInheritedTime
    // it will (if necessary) recalculate timings and (if necessary) call
    // updateChildrenAndEffects.
    // Returns whether style recalc was triggered.
    bool updateInheritedTime(double inheritedTime) const;
    void invalidate() const { m_needsUpdate = true; };

private:
    // Returns whether style recalc was triggered.
    virtual bool updateChildrenAndEffects() const = 0;
    virtual double intrinsicIterationDuration() const { return 0; };
    virtual double calculateTimeToEffectChange(double localTime, double timeToNextIteration) const = 0;
    virtual void didAttach() { };
    virtual void willDetach() { };

    void attach(Player* player)
    {
        m_player = player;
        didAttach();
    };

    void detach()
    {
        ASSERT(m_player);
        willDetach();
        m_player = 0;
    };

    // FIXME: m_parent and m_startTime are placeholders, they depend on timing groups.
    TimedItem* const m_parent;
    const double m_startTime;
    Player* m_player;
    Timing m_specified;
    OwnPtr<EventDelegate> m_eventDelegate;

    // FIXME: Should be versioned by monotonic value on player.
    mutable struct CalculatedTiming {
        double activeDuration;
        Phase phase;
        double currentIteration;
        double timeFraction;
        bool isCurrent;
        bool isInEffect;
        bool isInPlay;
        double timeToEffectChange;
    } m_calculated;
    mutable bool m_isFirstSample;
    mutable bool m_needsUpdate;
    mutable double m_lastUpdateTime;

    // FIXME: Should check the version and reinherit time if inconsistent.
    const CalculatedTiming& ensureCalculated() const { return m_calculated; }
};

} // namespace WebCore

#endif
