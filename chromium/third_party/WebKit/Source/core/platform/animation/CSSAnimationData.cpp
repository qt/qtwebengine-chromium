/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/platform/animation/CSSAnimationData.h"

namespace WebCore {

CSSAnimationData::CSSAnimationData()
    : m_name(initialAnimationName())
    , m_property(CSSPropertyInvalid)
    , m_mode(AnimateAll)
    , m_iterationCount(initialAnimationIterationCount())
    , m_delay(initialAnimationDelay())
    , m_duration(initialAnimationDuration())
    , m_timingFunction(initialAnimationTimingFunction())
    , m_direction(initialAnimationDirection())
    , m_fillMode(initialAnimationFillMode())
    , m_playState(initialAnimationPlayState())
    , m_delaySet(false)
    , m_directionSet(false)
    , m_durationSet(false)
    , m_fillModeSet(false)
    , m_iterationCountSet(false)
    , m_nameSet(false)
    , m_playStateSet(false)
    , m_propertySet(false)
    , m_timingFunctionSet(false)
    , m_isNone(false)
{
}

CSSAnimationData::CSSAnimationData(const CSSAnimationData& o)
    : RefCounted<CSSAnimationData>()
    , m_name(o.m_name)
    , m_property(o.m_property)
    , m_mode(o.m_mode)
    , m_iterationCount(o.m_iterationCount)
    , m_delay(o.m_delay)
    , m_duration(o.m_duration)
    , m_timingFunction(o.m_timingFunction)
    , m_direction(o.m_direction)
    , m_fillMode(o.m_fillMode)
    , m_playState(o.m_playState)
    , m_delaySet(o.m_delaySet)
    , m_directionSet(o.m_directionSet)
    , m_durationSet(o.m_durationSet)
    , m_fillModeSet(o.m_fillModeSet)
    , m_iterationCountSet(o.m_iterationCountSet)
    , m_nameSet(o.m_nameSet)
    , m_playStateSet(o.m_playStateSet)
    , m_propertySet(o.m_propertySet)
    , m_timingFunctionSet(o.m_timingFunctionSet)
    , m_isNone(o.m_isNone)
{
}

CSSAnimationData& CSSAnimationData::operator=(const CSSAnimationData& o)
{
    m_name = o.m_name;
    m_property = o.m_property;
    m_mode = o.m_mode;
    m_iterationCount = o.m_iterationCount;
    m_delay = o.m_delay;
    m_duration = o.m_duration;
    m_timingFunction = o.m_timingFunction;
    m_direction = o.m_direction;
    m_fillMode = o.m_fillMode;
    m_playState = o.m_playState;

    m_delaySet = o.m_delaySet;
    m_directionSet = o.m_directionSet;
    m_durationSet = o.m_durationSet;
    m_fillModeSet = o.m_fillModeSet;
    m_iterationCountSet = o.m_iterationCountSet;
    m_nameSet = o.m_nameSet;
    m_playStateSet = o.m_playStateSet;
    m_propertySet = o.m_propertySet;
    m_timingFunctionSet = o.m_timingFunctionSet;
    m_isNone = o.m_isNone;

    return *this;
}

CSSAnimationData::~CSSAnimationData()
{
}

bool CSSAnimationData::animationsMatchForStyleRecalc(const CSSAnimationData* o) const
{
    if (!o)
        return false;

    return m_name == o->m_name
        && m_playState == o->m_playState
        && m_property == o->m_property
        && m_mode == o->m_mode
        && m_nameSet == o->m_nameSet
        && m_playStateSet == o->m_playStateSet
        && m_propertySet == o->m_propertySet
        && m_isNone == o->m_isNone;
}

const AtomicString& CSSAnimationData::initialAnimationName()
{
    DEFINE_STATIC_LOCAL(const AtomicString, initialValue, ("none"));
    return initialValue;
}

} // namespace WebCore
