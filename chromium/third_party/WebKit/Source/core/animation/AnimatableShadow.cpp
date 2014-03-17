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
#include "core/animation/AnimatableShadow.h"

namespace WebCore {

PassRefPtr<AnimatableValue> AnimatableShadow::interpolateTo(const AnimatableValue* value, double fraction) const
{
    const AnimatableShadow* shadowList = toAnimatableShadow(value);
    return AnimatableShadow::create(ShadowList::blend(m_shadowList.get(), shadowList->m_shadowList.get(), fraction));
}

PassRefPtr<AnimatableValue> AnimatableShadow::addWith(const AnimatableValue* value) const
{
    // FIXME: The spec doesn't specify anything for shadow in particular, but
    // the default behaviour is probably not what one would expect.
    return AnimatableValue::defaultAddWith(this, value);
}

bool AnimatableShadow::equalTo(const AnimatableValue* value) const
{
    const ShadowList* shadowList = toAnimatableShadow(value)->m_shadowList.get();
    return m_shadowList == shadowList || (m_shadowList && shadowList && *m_shadowList == *shadowList);
}

} // namespace WebCore
