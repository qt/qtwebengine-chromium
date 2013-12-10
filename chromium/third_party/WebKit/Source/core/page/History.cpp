/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 */

#include "config.h"
#include "core/page/History.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/history/BackForwardController.h"
#include "core/history/HistoryItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/MainThread.h"

namespace WebCore {

History::History(Frame* frame)
    : DOMWindowProperty(frame)
    , m_lastStateObjectRequested(0)
{
    ScriptWrappable::init(this);
}

unsigned History::length() const
{
    if (!m_frame)
        return 0;
    if (!m_frame->page())
        return 0;
    return m_frame->page()->backForward().count();
}

SerializedScriptValue* History::state()
{
    m_lastStateObjectRequested = stateInternal();
    return m_lastStateObjectRequested.get();
}

SerializedScriptValue* History::stateInternal() const
{
    if (!m_frame)
        return 0;

    if (HistoryItem* historyItem = m_frame->loader()->history()->currentItem())
        return historyItem->stateObject();

    return 0;
}

bool History::stateChanged() const
{
    return m_lastStateObjectRequested != stateInternal();
}

bool History::isSameAsCurrentState(SerializedScriptValue* state) const
{
    return state == stateInternal();
}

void History::back()
{
    go(-1);
}

void History::back(ScriptExecutionContext* context)
{
    go(context, -1);
}

void History::forward()
{
    go(1);
}

void History::forward(ScriptExecutionContext* context)
{
    go(context, 1);
}

void History::go(int distance)
{
    if (!m_frame)
        return;

    m_frame->navigationScheduler()->scheduleHistoryNavigation(distance);
}

void History::go(ScriptExecutionContext* context, int distance)
{
    if (!m_frame)
        return;

    ASSERT(isMainThread());
    Document* activeDocument = toDocument(context);
    if (!activeDocument)
        return;

    if (!activeDocument->canNavigate(m_frame))
        return;

    m_frame->navigationScheduler()->scheduleHistoryNavigation(distance);
}

KURL History::urlForState(const String& urlString)
{
    KURL baseURL = m_frame->document()->baseURL();
    if (urlString.isEmpty())
        return baseURL;

    return KURL(baseURL, urlString);
}

void History::stateObjectAdded(PassRefPtr<SerializedScriptValue> data, const String& title, const String& urlString, SameDocumentNavigationSource sameDocumentNavigationSource, ExceptionState& es)
{
    if (!m_frame || !m_frame->page())
        return;

    KURL fullURL = urlForState(urlString);
    if (!fullURL.isValid() || !m_frame->document()->securityOrigin()->canRequest(fullURL)) {
        // We can safely expose the URL to JavaScript, as a) no redirection takes place: JavaScript already had this URL, b) JavaScript can only access a same-origin History object.
        es.throwSecurityError("A history state object with URL '" + fullURL.elidedString() + "' cannot be created in a document with origin '" + m_frame->document()->securityOrigin()->toString() + "'.");
        return;
    }
    m_frame->loader()->updateForSameDocumentNavigation(fullURL, sameDocumentNavigationSource, data, title, FrameLoader::DoNotUpdateBackForwardList);
}

} // namespace WebCore
