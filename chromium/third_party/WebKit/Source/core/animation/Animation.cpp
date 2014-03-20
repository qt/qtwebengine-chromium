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
#include "core/animation/Animation.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/KeyframeAnimationEffect.h"
#include "core/animation/Player.h"
#include "core/dom/Element.h"

namespace WebCore {

PassRefPtr<Animation> Animation::create(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect, const Timing& timing, Priority priority, PassOwnPtr<EventDelegate> eventDelegate)
{
    return adoptRef(new Animation(target, effect, timing, priority, eventDelegate));
}

Animation::Animation(PassRefPtr<Element> target, PassRefPtr<AnimationEffect> effect, const Timing& timing, Priority priority, PassOwnPtr<EventDelegate> eventDelegate)
    : TimedItem(timing, eventDelegate)
    , m_target(target)
    , m_effect(effect)
    , m_activeInAnimationStack(false)
    , m_priority(priority)
{
}

void Animation::didAttach()
{
    if (m_target)
        m_target->ensureActiveAnimations()->players().add(player());
}

void Animation::willDetach()
{
    if (m_target)
        m_target->activeAnimations()->players().remove(player());
    if (m_activeInAnimationStack)
        clearEffects();
}

static AnimationStack& ensureAnimationStack(Element* element)
{
    return element->ensureActiveAnimations()->defaultStack();
}

bool Animation::applyEffects(bool previouslyInEffect)
{
    ASSERT(isInEffect());
    if (!m_target || !m_effect)
        return false;

    if (player() && !previouslyInEffect) {
        ensureAnimationStack(m_target.get()).add(this);
        m_activeInAnimationStack = true;
    }

    double iteration = currentIteration();
    ASSERT(iteration >= 0);
    // FIXME: Handle iteration values which overflow int.
    m_compositableValues = m_effect->sample(static_cast<int>(iteration), timeFraction());
    if (player()) {
        m_target->setNeedsAnimationStyleRecalc();
        return true;
    }
    return false;
}

void Animation::clearEffects()
{
    ASSERT(player());
    ASSERT(m_activeInAnimationStack);
    ensureAnimationStack(m_target.get()).remove(this);
    cancelAnimationOnCompositor();
    m_activeInAnimationStack = false;
    m_compositableValues.clear();
    m_target->setNeedsAnimationStyleRecalc();
    invalidate();
}

bool Animation::updateChildrenAndEffects() const
{
    if (!m_effect)
        return false;

    if (isInEffect())
        return const_cast<Animation*>(this)->applyEffects(m_activeInAnimationStack);

    if (m_activeInAnimationStack) {
        const_cast<Animation*>(this)->clearEffects();
        return true;
    }
    return false;
}

double Animation::calculateTimeToEffectChange(double localTime, double timeToNextIteration) const
{
    const double activeStartTime = startTime() + specified().startDelay;
    switch (phase()) {
    case PhaseBefore:
        return activeStartTime - localTime;
    case PhaseActive:
        if (hasActiveAnimationsOnCompositor()) {
            // Need service to apply fill / fire events.
            const double activeEndTime = activeStartTime + activeDuration();
            return std::min(activeEndTime - localTime, timeToNextIteration);
        }
        return 0;
    case PhaseAfter:
        // If this Animation is still in effect then it will need to update
        // when its parent goes out of effect. We have no way of knowing when
        // that will be, however, so the parent will need to supply it.
        return std::numeric_limits<double>::infinity();
    case PhaseNone:
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

bool Animation::isCandidateForAnimationOnCompositor() const
{
    if (!effect() || !m_target)
        return false;
    return CompositorAnimations::instance()->isCandidateForAnimationOnCompositor(specified(), *effect());
}

bool Animation::maybeStartAnimationOnCompositor()
{
    ASSERT(!hasActiveAnimationsOnCompositor());
    if (!isCandidateForAnimationOnCompositor())
        return false;
    if (!CompositorAnimations::instance()->canStartAnimationOnCompositor(*m_target.get()))
        return false;
    if (!CompositorAnimations::instance()->startAnimationOnCompositor(*m_target.get(), specified(), *effect(), m_compositorAnimationIds))
        return false;
    ASSERT(!m_compositorAnimationIds.isEmpty());
    return true;
}

bool Animation::hasActiveAnimationsOnCompositor() const
{
    return !m_compositorAnimationIds.isEmpty();
}

bool Animation::hasActiveAnimationsOnCompositor(CSSPropertyID property) const
{
    return hasActiveAnimationsOnCompositor() && affects(property);
}

bool Animation::affects(CSSPropertyID property) const
{
    return m_effect && m_effect->affects(property);
}

void Animation::cancelAnimationOnCompositor()
{
    if (!hasActiveAnimationsOnCompositor())
        return;
    if (!m_target || !m_target->renderer())
        return;
    for (size_t i = 0; i < m_compositorAnimationIds.size(); ++i)
        CompositorAnimations::instance()->cancelAnimationOnCompositor(*m_target.get(), m_compositorAnimationIds[i]);
    m_compositorAnimationIds.clear();
}

void Animation::pauseAnimationForTestingOnCompositor(double pauseTime)
{
    ASSERT(hasActiveAnimationsOnCompositor());
    if (!m_target || !m_target->renderer())
        return;
    for (size_t i = 0; i < m_compositorAnimationIds.size(); ++i)
        CompositorAnimations::instance()->pauseAnimationForTestingOnCompositor(*m_target.get(), m_compositorAnimationIds[i], pauseTime);
}

} // namespace WebCore
