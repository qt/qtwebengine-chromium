/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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

#ifndef UserMediaRequest_h
#define UserMediaRequest_h

#include "core/dom/ActiveDOMObject.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Dictionary;
class Document;
class ExceptionState;
class MediaConstraints;
class MediaConstraintsImpl;
class MediaStreamDescriptor;
class UserMediaController;

class UserMediaRequest : public RefCounted<UserMediaRequest>, public ContextLifecycleObserver {
public:
    static PassRefPtr<UserMediaRequest> create(ExecutionContext*, UserMediaController*, const Dictionary& options, PassOwnPtr<NavigatorUserMediaSuccessCallback>, PassOwnPtr<NavigatorUserMediaErrorCallback>, ExceptionState&);
    ~UserMediaRequest();

    NavigatorUserMediaSuccessCallback* successCallback() const { return m_successCallback.get(); }
    NavigatorUserMediaErrorCallback* errorCallback() const { return m_errorCallback.get(); }
    Document* ownerDocument();

    void start();

    void succeed(PassRefPtr<MediaStreamDescriptor>);
    void fail(const String& description);
    void failConstraint(const String& constraintName, const String& description);

    bool audio() const;
    bool video() const;
    MediaConstraints* audioConstraints() const;
    MediaConstraints* videoConstraints() const;

    // ContextLifecycleObserver
    virtual void contextDestroyed();

private:
    UserMediaRequest(ExecutionContext*, UserMediaController*, PassRefPtr<MediaConstraintsImpl> audio, PassRefPtr<MediaConstraintsImpl> video, PassOwnPtr<NavigatorUserMediaSuccessCallback>, PassOwnPtr<NavigatorUserMediaErrorCallback>);

    RefPtr<MediaConstraintsImpl> m_audio;
    RefPtr<MediaConstraintsImpl> m_video;

    UserMediaController* m_controller;

    OwnPtr<NavigatorUserMediaSuccessCallback> m_successCallback;
    OwnPtr<NavigatorUserMediaErrorCallback> m_errorCallback;
};

} // namespace WebCore

#endif // UserMediaRequest_h
