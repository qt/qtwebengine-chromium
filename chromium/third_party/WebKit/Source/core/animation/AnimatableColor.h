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

#ifndef AnimatableColor_h
#define AnimatableColor_h

#include "core/animation/AnimatableValue.h"
#include "core/platform/graphics/Color.h"

namespace WebCore {

class AnimatableColorImpl {
public:
    AnimatableColorImpl(float red, float green, float blue, float alpha);
    AnimatableColorImpl(Color);
    Color toColor() const;
    AnimatableColorImpl interpolateTo(const AnimatableColorImpl&, double fraction) const;
    AnimatableColorImpl addWith(const AnimatableColorImpl&) const;

private:
    float m_alpha;
    float m_red;
    float m_green;
    float m_blue;
};

class AnimatableColor : public AnimatableValue {
public:
    static PassRefPtr<AnimatableColor> create(const AnimatableColorImpl&, const AnimatableColorImpl& visitedLinkColor);
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;
    Color color() const { return m_color.toColor(); }
    Color visitedLinkColor() const { return m_visitedLinkColor.toColor(); }

private:
    AnimatableColor(const AnimatableColorImpl& color, const AnimatableColorImpl& visitedLinkColor)
        : AnimatableValue(TypeColor)
        , m_color(color)
        , m_visitedLinkColor(visitedLinkColor)
    {
    }
    const AnimatableColorImpl m_color;
    const AnimatableColorImpl m_visitedLinkColor;
};

inline const AnimatableColor* toAnimatableColor(const AnimatableValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value && value->isColor());
    return static_cast<const AnimatableColor*>(value);
}

} // namespace WebCore

#endif // AnimatableColor_h
