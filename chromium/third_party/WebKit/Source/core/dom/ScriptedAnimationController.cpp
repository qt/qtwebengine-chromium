/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/dom/ScriptedAnimationController.h"

#include "core/dom/Document.h"
#include "core/dom/RequestAnimationFrameCallback.h"
#include "core/events/Event.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"

namespace WebCore {

std::pair<EventTarget*, StringImpl*> eventTargetKey(const Event* event)
{
    return std::make_pair(event->target(), event->type().impl());
}

ScriptedAnimationController::ScriptedAnimationController(Document* document)
    : m_document(document)
    , m_nextCallbackId(0)
    , m_suspendCount(0)
{
}

ScriptedAnimationController::~ScriptedAnimationController()
{
}

void ScriptedAnimationController::suspend()
{
    ++m_suspendCount;
}

void ScriptedAnimationController::resume()
{
    // It would be nice to put an ASSERT(m_suspendCount > 0) here, but in WK1 resume() can be called
    // even when suspend hasn't (if a tab was created in the background).
    if (m_suspendCount > 0)
        --m_suspendCount;
    scheduleAnimationIfNeeded();
}

ScriptedAnimationController::CallbackId ScriptedAnimationController::registerCallback(PassOwnPtr<RequestAnimationFrameCallback> callback)
{
    ScriptedAnimationController::CallbackId id = ++m_nextCallbackId;
    callback->m_cancelled = false;
    callback->m_id = id;
    m_callbacks.append(callback);
    scheduleAnimationIfNeeded();

    InspectorInstrumentation::didRequestAnimationFrame(m_document, id);

    return id;
}

void ScriptedAnimationController::cancelCallback(CallbackId id)
{
    for (size_t i = 0; i < m_callbacks.size(); ++i) {
        if (m_callbacks[i]->m_id == id) {
            InspectorInstrumentation::didCancelAnimationFrame(m_document, id);
            m_callbacks.remove(i);
            return;
        }
    }
    for (size_t i = 0; i < m_callbacksToInvoke.size(); ++i) {
        if (m_callbacksToInvoke[i]->m_id == id) {
            InspectorInstrumentation::didCancelAnimationFrame(m_document, id);
            m_callbacksToInvoke[i]->m_cancelled = true;
            // will be removed at the end of executeCallbacks()
            return;
        }
    }
}

void ScriptedAnimationController::dispatchEvents()
{
    Vector<RefPtr<Event> > events;
    events.swap(m_eventQueue);
    m_perFrameEvents.clear();

    for (size_t i = 0; i < events.size(); ++i) {
        EventTarget* eventTarget = events[i]->target();
        // FIXME: we should figure out how to make dispatchEvent properly virtual to avoid
        // special casting window.
        // FIXME: We should not fire events for nodes that are no longer in the tree.
        if (DOMWindow* window = eventTarget->toDOMWindow())
            window->dispatchEvent(events[i], 0);
        else
            eventTarget->dispatchEvent(events[i]);
    }
}

void ScriptedAnimationController::executeCallbacks(double monotonicTimeNow)
{
    // dispatchEvents() runs script which can cause the document to be destroyed.
    if (!m_document)
        return;

    double highResNowMs = 1000.0 * m_document->loader()->timing()->monotonicTimeToZeroBasedDocumentTime(monotonicTimeNow);
    double legacyHighResNowMs = 1000.0 * m_document->loader()->timing()->monotonicTimeToPseudoWallTime(monotonicTimeNow);

    // First, generate a list of callbacks to consider.  Callbacks registered from this point
    // on are considered only for the "next" frame, not this one.
    ASSERT(m_callbacksToInvoke.isEmpty());
    m_callbacksToInvoke.swap(m_callbacks);

    for (size_t i = 0; i < m_callbacksToInvoke.size(); ++i) {
        RequestAnimationFrameCallback* callback = m_callbacksToInvoke[i].get();
        if (!callback->m_cancelled) {
            InspectorInstrumentationCookie cookie = InspectorInstrumentation::willFireAnimationFrame(m_document, callback->m_id);
            if (callback->m_useLegacyTimeBase)
                callback->handleEvent(legacyHighResNowMs);
            else
                callback->handleEvent(highResNowMs);
            InspectorInstrumentation::didFireAnimationFrame(cookie);
        }
    }

    m_callbacksToInvoke.clear();
}

void ScriptedAnimationController::serviceScriptedAnimations(double monotonicTimeNow)
{
    if (!m_callbacks.size() && !m_eventQueue.size())
        return;

    if (m_suspendCount)
        return;

    RefPtr<ScriptedAnimationController> protect(this);

    dispatchEvents();
    executeCallbacks(monotonicTimeNow);

    scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::enqueueEvent(PassRefPtr<Event> event)
{
    m_eventQueue.append(event);
    scheduleAnimationIfNeeded();
}

void ScriptedAnimationController::enqueuePerFrameEvent(PassRefPtr<Event> event)
{
    if (!m_perFrameEvents.add(eventTargetKey(event.get())).isNewEntry)
        return;
    enqueueEvent(event);
}

void ScriptedAnimationController::scheduleAnimationIfNeeded()
{
    if (!m_document)
        return;

    if (m_suspendCount)
        return;

    if (!m_callbacks.size() && !m_eventQueue.size())
        return;

    if (FrameView* frameView = m_document->view())
        frameView->scheduleAnimation();
}

}
