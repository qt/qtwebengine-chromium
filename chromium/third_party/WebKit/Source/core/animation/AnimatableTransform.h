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

#ifndef AnimatableTransform_h
#define AnimatableTransform_h

#include "core/animation/AnimatableValue.h"
#include "platform/transforms/TransformOperations.h"

namespace WebCore {

class AnimatableTransform : public AnimatableValue {
public:
    virtual ~AnimatableTransform() { }
    static PassRefPtr<AnimatableTransform> create(const TransformOperations&);
    const TransformOperations& transformOperations() const
    {
        return m_transform;
    }

protected:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;

private:
    explicit AnimatableTransform(const TransformOperations& transform)
        : m_transform(transform)
    {
    }
    virtual AnimatableType type() const OVERRIDE { return TypeTransform; }
    virtual bool equalTo(const AnimatableValue*) const OVERRIDE;
    const TransformOperations m_transform;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableTransform, isTransform());

} // namespace WebCore

#endif // AnimatableTransform_h

