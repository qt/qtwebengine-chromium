/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/platform/LifecycleNotifier.h"

namespace WebCore {

LifecycleNotifier::LifecycleNotifier(LifecycleContext* context)
    : m_context(context)
    , m_inDestructor(false)
    , m_iterating(IteratingNone)
{
}

LifecycleNotifier::~LifecycleNotifier()
{
    m_inDestructor = true;
    for (ObserverSet::iterator it = m_observers.begin(); it != m_observers.end(); it = m_observers.begin()) {
        LifecycleObserver* observer = *it;
        m_observers.remove(observer);
        ASSERT(observer->lifecycleContext() == m_context);
        observer->contextDestroyed();
    }
}

void LifecycleNotifier::addObserver(LifecycleObserver* observer)
{
    RELEASE_ASSERT(!m_inDestructor);
    m_observers.add(observer);
}

void LifecycleNotifier::removeObserver(LifecycleObserver* observer)
{
    RELEASE_ASSERT(!m_inDestructor);
    m_observers.remove(observer);
}

LifecycleContext* LifecycleNotifier::context()
{
    return m_context;
}

} // namespace WebCore
