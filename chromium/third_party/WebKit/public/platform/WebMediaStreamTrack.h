/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaStreamTrack_h
#define WebMediaStreamTrack_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"

namespace WebCore {
class MediaStreamComponent;
}

namespace blink {
class WebAudioSourceProvider;
class WebMediaStream;
class WebMediaStreamSource;
class WebString;

class WebMediaStreamTrack {
public:
    class ExtraData {
    public:
        virtual ~ExtraData() { }
    };

    WebMediaStreamTrack() { }
    WebMediaStreamTrack(const WebMediaStreamTrack& other) { assign(other); }
    ~WebMediaStreamTrack() { reset(); }

    WebMediaStreamTrack& operator=(const WebMediaStreamTrack& other)
    {
        assign(other);
        return *this;
    }
    BLINK_EXPORT void assign(const WebMediaStreamTrack&);

    BLINK_EXPORT void initialize(const WebMediaStreamSource&);
    BLINK_EXPORT void initialize(const WebString& id, const WebMediaStreamSource&);

    BLINK_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    BLINK_EXPORT WebString id() const;

    BLINK_EXPORT WebMediaStream stream() const;
    BLINK_EXPORT WebMediaStreamSource source() const;
    BLINK_EXPORT bool isEnabled() const;

    // Extra data associated with this WebMediaStream.
    // If non-null, the extra data pointer will be deleted when the object is destroyed.
    // Setting the extra data pointer will cause any existing non-null
    // extra data pointer to be deleted.
    BLINK_EXPORT ExtraData* extraData() const;
    BLINK_EXPORT void setExtraData(ExtraData*);

    // The lifetime of the WebAudioSourceProvider should outlive the
    // WebMediaStreamTrack, and clients are responsible for calling
    // setSourceProvider(0) before the WebMediaStreamTrack is going away.
    BLINK_EXPORT void setSourceProvider(WebAudioSourceProvider*);

#if BLINK_IMPLEMENTATION
    WebMediaStreamTrack(PassRefPtr<WebCore::MediaStreamComponent>);
    WebMediaStreamTrack(WebCore::MediaStreamComponent*);
    WebMediaStreamTrack& operator=(WebCore::MediaStreamComponent*);
    operator WTF::PassRefPtr<WebCore::MediaStreamComponent>() const;
    operator WebCore::MediaStreamComponent*() const;
#endif

private:
    WebPrivatePtr<WebCore::MediaStreamComponent> m_private;
};

} // namespace blink

#endif // WebMediaStreamTrack_h
