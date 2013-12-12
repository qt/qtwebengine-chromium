/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/appcache/ApplicationCache.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/EventListener.h"
#include "core/dom/EventNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/Frame.h"

namespace WebCore {

ApplicationCache::ApplicationCache(Frame* frame)
    : DOMWindowProperty(frame)
{
    ScriptWrappable::init(this);
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (cacheHost)
        cacheHost->setApplicationCache(this);
}

void ApplicationCache::willDestroyGlobalObjectInFrame()
{
    if (ApplicationCacheHost* cacheHost = applicationCacheHost())
        cacheHost->setApplicationCache(0);
    DOMWindowProperty::willDestroyGlobalObjectInFrame();
}

ApplicationCacheHost* ApplicationCache::applicationCacheHost() const
{
    if (!m_frame || !m_frame->loader()->documentLoader())
        return 0;
    return m_frame->loader()->documentLoader()->applicationCacheHost();
}

unsigned short ApplicationCache::status() const
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost)
        return ApplicationCacheHost::UNCACHED;
    return cacheHost->status();
}

void ApplicationCache::update(ExceptionState& es)
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost || !cacheHost->update())
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute("update", "ApplicationCache", "there is no application cache to update."));
}

void ApplicationCache::swapCache(ExceptionState& es)
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (!cacheHost || !cacheHost->swapCache())
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute("swapCache", "ApplicationCache", "there is no newer application cache to swap to."));
}

void ApplicationCache::abort()
{
    ApplicationCacheHost* cacheHost = applicationCacheHost();
    if (cacheHost)
        cacheHost->abort();
}

const AtomicString& ApplicationCache::interfaceName() const
{
    return eventNames().interfaceForApplicationCache;
}

ScriptExecutionContext* ApplicationCache::scriptExecutionContext() const
{
    if (m_frame)
        return m_frame->document();
    return 0;
}

const AtomicString& ApplicationCache::toEventType(ApplicationCacheHost::EventID id)
{
    switch (id) {
    case ApplicationCacheHost::CHECKING_EVENT:
        return eventNames().checkingEvent;
    case ApplicationCacheHost::ERROR_EVENT:
        return eventNames().errorEvent;
    case ApplicationCacheHost::NOUPDATE_EVENT:
        return eventNames().noupdateEvent;
    case ApplicationCacheHost::DOWNLOADING_EVENT:
        return eventNames().downloadingEvent;
    case ApplicationCacheHost::PROGRESS_EVENT:
        return eventNames().progressEvent;
    case ApplicationCacheHost::UPDATEREADY_EVENT:
        return eventNames().updatereadyEvent;
    case ApplicationCacheHost::CACHED_EVENT:
        return eventNames().cachedEvent;
    case ApplicationCacheHost::OBSOLETE_EVENT:
        return eventNames().obsoleteEvent;
    }
    ASSERT_NOT_REACHED();
    return eventNames().errorEvent;
}

EventTargetData* ApplicationCache::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* ApplicationCache::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore
