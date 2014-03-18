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

#ifndef AnimatableLengthPoint_h
#define AnimatableLengthPoint_h

#include "core/animation/AnimatableValue.h"

namespace WebCore {

class AnimatableLengthPoint : public AnimatableValue {
public:
    virtual ~AnimatableLengthPoint() { }
    static PassRefPtr<AnimatableLengthPoint> create(PassRefPtr<AnimatableValue> x, PassRefPtr<AnimatableValue> y)
    {
        return adoptRef(new AnimatableLengthPoint(x, y));
    }
    const AnimatableValue* x() const { return m_x.get(); }
    const AnimatableValue* y() const { return m_y.get(); }

protected:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;

private:
    AnimatableLengthPoint(PassRefPtr<AnimatableValue> x, PassRefPtr<AnimatableValue> y)
        : m_x(x)
        , m_y(y)
    {
    }
    virtual AnimatableType type() const OVERRIDE { return TypeLengthPoint; }
    virtual bool equalTo(const AnimatableValue*) const OVERRIDE;

    RefPtr<AnimatableValue> m_x;
    RefPtr<AnimatableValue> m_y;
};

inline const AnimatableLengthPoint* toAnimatableLengthPoint(const AnimatableValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value && value->isLengthPoint());
    return static_cast<const AnimatableLengthPoint*>(value);
}

} // namespace WebCore

#endif // AnimatableLengthPoint_h
