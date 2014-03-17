/*
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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

#ifndef PlatformSpeechSynthesizer_h
#define PlatformSpeechSynthesizer_h

#include "platform/PlatformExport.h"
#include "platform/speech/PlatformSpeechSynthesisVoice.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {
class WebSpeechSynthesizer;
class WebSpeechSynthesizerClient;
}

namespace WebCore {

enum SpeechBoundary {
    SpeechWordBoundary,
    SpeechSentenceBoundary
};

class PlatformSpeechSynthesisUtterance;

class PlatformSpeechSynthesizerClient {
public:
    virtual void didStartSpeaking(PassRefPtr<PlatformSpeechSynthesisUtterance>) = 0;
    virtual void didFinishSpeaking(PassRefPtr<PlatformSpeechSynthesisUtterance>) = 0;
    virtual void didPauseSpeaking(PassRefPtr<PlatformSpeechSynthesisUtterance>) = 0;
    virtual void didResumeSpeaking(PassRefPtr<PlatformSpeechSynthesisUtterance>) = 0;
    virtual void speakingErrorOccurred(PassRefPtr<PlatformSpeechSynthesisUtterance>) = 0;
    virtual void boundaryEventOccurred(PassRefPtr<PlatformSpeechSynthesisUtterance>, SpeechBoundary, unsigned charIndex) = 0;
    virtual void voicesDidChange() = 0;
protected:
    virtual ~PlatformSpeechSynthesizerClient() { }
};

class PLATFORM_EXPORT PlatformSpeechSynthesizer {
    WTF_MAKE_NONCOPYABLE(PlatformSpeechSynthesizer);
public:
    static PassOwnPtr<PlatformSpeechSynthesizer> create(PlatformSpeechSynthesizerClient*);

    virtual ~PlatformSpeechSynthesizer();

    const Vector<RefPtr<PlatformSpeechSynthesisVoice> >& voiceList() const { return m_voiceList; }
    virtual void speak(PassRefPtr<PlatformSpeechSynthesisUtterance>);
    virtual void pause();
    virtual void resume();
    virtual void cancel();

    PlatformSpeechSynthesizerClient* client() const { return m_speechSynthesizerClient; }

    void setVoiceList(Vector<RefPtr<PlatformSpeechSynthesisVoice> >&);

protected:
    virtual void initializeVoiceList();
    explicit PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient*);
    Vector<RefPtr<PlatformSpeechSynthesisVoice> > m_voiceList;

private:
    PlatformSpeechSynthesizerClient* m_speechSynthesizerClient;

    OwnPtr<blink::WebSpeechSynthesizer> m_webSpeechSynthesizer;
    OwnPtr<blink::WebSpeechSynthesizerClient> m_webSpeechSynthesizerClient;
};

} // namespace WebCore

#endif // PlatformSpeechSynthesizer_h
