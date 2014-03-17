/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDTMFSender_h
#define RTCDTMFSender_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "platform/Timer.h"
#include "platform/mediastream/RTCDTMFSenderHandlerClient.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;
class MediaStreamTrack;
class RTCDTMFSenderHandler;
class RTCPeerConnectionHandler;

class RTCDTMFSender : public RefCounted<RTCDTMFSender>, public ScriptWrappable, public EventTargetWithInlineData, public RTCDTMFSenderHandlerClient, public ActiveDOMObject {
    REFCOUNTED_EVENT_TARGET(RTCDTMFSender);
public:
    static PassRefPtr<RTCDTMFSender> create(ExecutionContext*, RTCPeerConnectionHandler*, PassRefPtr<MediaStreamTrack>, ExceptionState&);
    ~RTCDTMFSender();

    bool canInsertDTMF() const;
    MediaStreamTrack* track() const;
    String toneBuffer() const;
    long duration() const { return m_duration; }
    long interToneGap() const { return m_interToneGap; }

    void insertDTMF(const String& tones, ExceptionState&);
    void insertDTMF(const String& tones, long duration, ExceptionState&);
    void insertDTMF(const String& tones, long duration, long interToneGap, ExceptionState&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(tonechange);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

private:
    RTCDTMFSender(ExecutionContext*, PassRefPtr<MediaStreamTrack>, PassOwnPtr<RTCDTMFSenderHandler>);

    void scheduleDispatchEvent(PassRefPtr<Event>);
    void scheduledEventTimerFired(Timer<RTCDTMFSender>*);

    // RTCDTMFSenderHandlerClient
    virtual void didPlayTone(const String&) OVERRIDE;

    RefPtr<MediaStreamTrack> m_track;
    long m_duration;
    long m_interToneGap;

    OwnPtr<RTCDTMFSenderHandler> m_handler;

    bool m_stopped;

    Timer<RTCDTMFSender> m_scheduledEventTimer;
    Vector<RefPtr<Event> > m_scheduledEvents;
};

} // namespace WebCore

#endif // RTCDTMFSender_h
