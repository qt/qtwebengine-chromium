/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef MediaStreamSource_h
#define MediaStreamSource_h

#include "platform/PlatformExport.h"
#include "platform/audio/AudioDestinationConsumer.h"
#include "platform/mediastream/MediaConstraints.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class PLATFORM_EXPORT MediaStreamSource : public RefCounted<MediaStreamSource> {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void sourceChangedState() = 0;
    };

    class ExtraData {
    public:
        virtual ~ExtraData() { }
    };

    enum Type {
        TypeAudio,
        TypeVideo
    };

    enum ReadyState {
        ReadyStateLive = 0,
        ReadyStateMuted = 1,
        ReadyStateEnded = 2
    };

    static PassRefPtr<MediaStreamSource> create(const String& id, Type, const String& name, ReadyState = ReadyStateLive, bool requiresConsumer = false);

    const String& id() const { return m_id; }
    Type type() const { return m_type; }
    const String& name() const { return m_name; }

    void setReadyState(ReadyState);
    ReadyState readyState() const { return m_readyState; }

    void addObserver(Observer*);
    void removeObserver(Observer*);

    ExtraData* extraData() const { return m_extraData.get(); }
    void setExtraData(ExtraData* extraData) { m_extraData = adoptPtr(extraData); }

    void setConstraints(PassRefPtr<MediaConstraints> constraints) { m_constraints = constraints; }
    MediaConstraints* constraints() { return m_constraints.get(); }

    void setAudioFormat(size_t numberOfChannels, float sampleRate);
    void consumeAudio(AudioBus*, size_t numberOfFrames);

    bool requiresAudioConsumer() const { return m_requiresConsumer; }
    void addAudioConsumer(PassRefPtr<AudioDestinationConsumer>);
    bool removeAudioConsumer(AudioDestinationConsumer*);
    const Vector<RefPtr<AudioDestinationConsumer> >& audioConsumers() { return m_audioConsumers; }

private:
    MediaStreamSource(const String& id, Type, const String& name, ReadyState, bool requiresConsumer);

    String m_id;
    Type m_type;
    String m_name;
    ReadyState m_readyState;
    bool m_requiresConsumer;
    Vector<Observer*> m_observers;
    Mutex m_audioConsumersLock;
    Vector<RefPtr<AudioDestinationConsumer> > m_audioConsumers;
    OwnPtr<ExtraData> m_extraData;
    RefPtr<MediaConstraints> m_constraints;
};

typedef Vector<RefPtr<MediaStreamSource> > MediaStreamSourceVector;

} // namespace WebCore

#endif // MediaStreamSource_h
