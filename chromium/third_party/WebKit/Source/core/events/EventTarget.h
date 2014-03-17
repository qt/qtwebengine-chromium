/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef EventTarget_h
#define EventTarget_h

#include "core/events/EventListenerMap.h"
#include "core/events/ThreadLocalEventNames.h"
#include "wtf/Forward.h"

namespace WebCore {

class ApplicationCache;
class AudioContext;
class DOMWindow;
class DedicatedWorkerGlobalScope;
class Event;
class EventListener;
class EventSource;
class ExceptionState;
class FileReader;
class FileWriter;
class IDBDatabase;
class IDBRequest;
class IDBTransaction;
class MIDIAccess;
class MIDIInput;
class MIDIPort;
class MediaController;
class MediaStream;
class MessagePort;
class NamedFlow;
class Node;
class Notification;
class SVGElementInstance;
class ExecutionContext;
class ScriptProcessorNode;
class SharedWorker;
class SharedWorkerGlobalScope;
class TextTrack;
class TextTrackCue;
class WebSocket;
class Worker;
class XMLHttpRequest;
class XMLHttpRequestUpload;

struct FiringEventIterator {
    FiringEventIterator(const AtomicString& eventType, size_t& iterator, size_t& end)
        : eventType(eventType)
        , iterator(iterator)
        , end(end)
    {
    }

    const AtomicString& eventType;
    size_t& iterator;
    size_t& end;
};
typedef Vector<FiringEventIterator, 1> FiringEventIteratorVector;

struct EventTargetData {
    WTF_MAKE_NONCOPYABLE(EventTargetData); WTF_MAKE_FAST_ALLOCATED;
public:
    EventTargetData();
    ~EventTargetData();

    EventListenerMap eventListenerMap;
    OwnPtr<FiringEventIteratorVector> firingEventIterators;
};

class EventTarget {
public:
    void ref() { refEventTarget(); }
    void deref() { derefEventTarget(); }

    virtual const AtomicString& interfaceName() const = 0;
    virtual ExecutionContext* executionContext() const = 0;

    virtual Node* toNode();
    virtual DOMWindow* toDOMWindow();
    virtual MessagePort* toMessagePort();

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual void removeAllEventListeners();
    virtual bool dispatchEvent(PassRefPtr<Event>);
    bool dispatchEvent(PassRefPtr<Event>, ExceptionState&); // DOM API
    virtual void uncaughtExceptionInEventHandler();

    // Used for legacy "onEvent" attribute APIs.
    bool setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, DOMWrapperWorld* isolatedWorld = 0);
    EventListener* getAttributeEventListener(const AtomicString& eventType, DOMWrapperWorld* isolatedWorld = 0);

    bool hasEventListeners() const;
    bool hasEventListeners(const AtomicString& eventType) const;
    bool hasCapturingEventListeners(const AtomicString& eventType);
    const EventListenerVector& getEventListeners(const AtomicString& eventType);

    bool fireEventListeners(Event*);
    bool isFiringEventListeners();

protected:
    virtual ~EventTarget();

    // Subclasses should likely not override these themselves; instead, they should subclass EventTargetWithInlineData.
    virtual EventTargetData* eventTargetData() = 0;
    virtual EventTargetData& ensureEventTargetData() = 0;

private:
    // Subclasses should likely not override these themselves; instead, they should use the REFCOUNTED_EVENT_TARGET() macro.
    virtual void refEventTarget() = 0;
    virtual void derefEventTarget() = 0;

    DOMWindow* executingWindow();
    void fireEventListeners(Event*, EventTargetData*, EventListenerVector&);
    void countLegacyEvents(const AtomicString& legacyTypeName, EventListenerVector*, EventListenerVector*);

    bool clearAttributeEventListener(const AtomicString& eventType, DOMWrapperWorld* isolatedWorld);

    friend class EventListenerIterator;
};

class EventTargetWithInlineData : public EventTarget {
protected:
    virtual EventTargetData* eventTargetData() OVERRIDE FINAL { return &m_eventTargetData; }
    virtual EventTargetData& ensureEventTargetData() OVERRIDE FINAL { return m_eventTargetData; }
private:
    EventTargetData m_eventTargetData;
};

// FIXME: These macros should be split into separate DEFINE and DECLARE
// macros to avoid causing so many header includes.
#define DEFINE_ATTRIBUTE_EVENT_LISTENER(attribute) \
    EventListener* on##attribute(DOMWrapperWorld* isolatedWorld) { return getAttributeEventListener(EventTypeNames::attribute, isolatedWorld); } \
    void setOn##attribute(PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld = 0) { setAttributeEventListener(EventTypeNames::attribute, listener, isolatedWorld); } \

#define DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(attribute) \
    static EventListener* on##attribute(EventTarget* eventTarget, DOMWrapperWorld* isolatedWorld) { return eventTarget->getAttributeEventListener(EventTypeNames::attribute, isolatedWorld); } \
    static void setOn##attribute(EventTarget* eventTarget, PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld = 0) { eventTarget->setAttributeEventListener(EventTypeNames::attribute, listener, isolatedWorld); } \

#define DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute) \
    EventListener* on##attribute(DOMWrapperWorld* isolatedWorld) { return document().getWindowAttributeEventListener(EventTypeNames::attribute, isolatedWorld); } \
    void setOn##attribute(PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld) { document().setWindowAttributeEventListener(EventTypeNames::attribute, listener, isolatedWorld); } \

#define DEFINE_STATIC_WINDOW_ATTRIBUTE_EVENT_LISTENER(attribute) \
    static EventListener* on##attribute(EventTarget* eventTarget, DOMWrapperWorld* isolatedWorld) { \
        if (Node* node = eventTarget->toNode()) \
            return node->document().getWindowAttributeEventListener(EventTypeNames::attribute, isolatedWorld); \
        ASSERT(eventTarget->toDOMWindow()); \
        return eventTarget->getAttributeEventListener(EventTypeNames::attribute, isolatedWorld); \
    } \
    static void setOn##attribute(EventTarget* eventTarget, PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld) { \
        if (Node* node = eventTarget->toNode()) \
            node->document().setWindowAttributeEventListener(EventTypeNames::attribute, listener, isolatedWorld); \
        else { \
            ASSERT(eventTarget->toDOMWindow()); \
            eventTarget->setAttributeEventListener(EventTypeNames::attribute, listener, isolatedWorld); \
        } \
    }

#define DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(attribute, eventName) \
    EventListener* on##attribute(DOMWrapperWorld* isolatedWorld) { return getAttributeEventListener(EventTypeNames::eventName, isolatedWorld); } \
    void setOn##attribute(PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld) { setAttributeEventListener(EventTypeNames::eventName, listener, isolatedWorld); } \

#define DECLARE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(recipient, attribute) \
    EventListener* on##attribute(DOMWrapperWorld* isolatedWorld); \
    void setOn##attribute(PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld);

#define DEFINE_FORWARDING_ATTRIBUTE_EVENT_LISTENER(type, recipient, attribute) \
    EventListener* type::on##attribute(DOMWrapperWorld* isolatedWorld) { return recipient ? recipient->getAttributeEventListener(EventTypeNames::attribute, isolatedWorld) : 0; } \
    void type::setOn##attribute(PassRefPtr<EventListener> listener, DOMWrapperWorld* isolatedWorld) \
    { \
        if (recipient) \
            recipient->setAttributeEventListener(EventTypeNames::attribute, listener, isolatedWorld); \
    }

inline bool EventTarget::isFiringEventListeners()
{
    EventTargetData* d = eventTargetData();
    if (!d)
        return false;
    return d->firingEventIterators && !d->firingEventIterators->isEmpty();
}

inline bool EventTarget::hasEventListeners() const
{
    // FIXME: We should have a const version of eventTargetData.
    if (const EventTargetData* d = const_cast<EventTarget*>(this)->eventTargetData())
        return !d->eventListenerMap.isEmpty();
    return false;
}

inline bool EventTarget::hasEventListeners(const AtomicString& eventType) const
{
    // FIXME: We should have const version of eventTargetData.
    if (const EventTargetData* d = const_cast<EventTarget*>(this)->eventTargetData())
        return d->eventListenerMap.contains(eventType);
    return false;
}

inline bool EventTarget::hasCapturingEventListeners(const AtomicString& eventType)
{
    EventTargetData* d = eventTargetData();
    if (!d)
        return false;
    return d->eventListenerMap.containsCapturing(eventType);
}

} // namespace WebCore

#define DEFINE_EVENT_TARGET_REFCOUNTING(baseClass) \
public: \
    using baseClass::ref; \
    using baseClass::deref; \
private: \
    virtual void refEventTarget() OVERRIDE FINAL { ref(); } \
    virtual void derefEventTarget() OVERRIDE FINAL { deref(); } \
    typedef int thisIsHereToForceASemiColonAfterThisEventTargetMacro

// Use this macro if your EventTarget subclass is also a subclass of WTF::RefCounted.
// A ref-counted class that uses a different method of refcounting should use DEFINE_EVENT_TARGET_REFCOUNTING directly.
// Both of these macros are meant to be placed just before the "public:" section of the class declaration.
#define REFCOUNTED_EVENT_TARGET(className) DEFINE_EVENT_TARGET_REFCOUNTING(RefCounted<className>)

#endif // EventTarget_h
