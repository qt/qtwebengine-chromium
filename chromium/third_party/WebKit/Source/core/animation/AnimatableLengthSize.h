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

#ifndef AnimatableLengthSize_h
#define AnimatableLengthSize_h

#include "core/animation/AnimatableValue.h"

namespace WebCore {

class AnimatableLengthSize : public AnimatableValue {
public:
    virtual ~AnimatableLengthSize() { }
    static PassRefPtr<AnimatableLengthSize> create(PassRefPtr<AnimatableValue> width, PassRefPtr<AnimatableValue> height)
    {
        return adoptRef(new AnimatableLengthSize(width, height));
    }
    const AnimatableValue* width() const { return m_width.get(); }
    const AnimatableValue* height() const { return m_height.get(); }

protected:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;

private:
    AnimatableLengthSize(PassRefPtr<AnimatableValue> width, PassRefPtr<AnimatableValue> height)
        : m_width(width)
        , m_height(height)
    {
    }
    virtual AnimatableType type() const OVERRIDE { return TypeLengthSize; }
    virtual bool equalTo(const AnimatableValue*) const OVERRIDE;

    RefPtr<AnimatableValue> m_width;
    RefPtr<AnimatableValue> m_height;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableLengthSize, isLengthSize());

} // namespace WebCore

#endif // AnimatableLengthSize_h
