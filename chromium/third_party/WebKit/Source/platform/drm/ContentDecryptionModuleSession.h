/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef ContentDecryptionModuleSession_h
#define ContentDecryptionModuleSession_h

#include "platform/PlatformExport.h"
#include "public/platform/WebContentDecryptionModuleSession.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
class WebContentDecryptionModule;
}

namespace WTF {
class Uint8Array;
}

namespace WebCore {

class KURL;

class PLATFORM_EXPORT ContentDecryptionModuleSessionClient {
public:
    enum MediaKeyErrorCode { UnknownError = 1, ClientError };
    virtual void keyAdded() = 0;
    virtual void keyError(MediaKeyErrorCode, unsigned long systemCode) = 0;
    virtual void keyMessage(const unsigned char* message, size_t messageLength, const KURL& destinationURL) = 0;
};

class PLATFORM_EXPORT ContentDecryptionModuleSession : private blink::WebContentDecryptionModuleSession::Client {
    WTF_MAKE_NONCOPYABLE(ContentDecryptionModuleSession);
public:
    static PassOwnPtr<ContentDecryptionModuleSession> create(ContentDecryptionModuleSessionClient*);

    ContentDecryptionModuleSession(blink::WebContentDecryptionModule*, ContentDecryptionModuleSessionClient*);
    ~ContentDecryptionModuleSession();

    String sessionId() const;
    void generateKeyRequest(const String& mimeType, const WTF::Uint8Array& initData);
    void update(const WTF::Uint8Array& key);
    void close();

private:
    // blink::WebContentDecryptionModuleSession::Client
    virtual void keyAdded() OVERRIDE;
    virtual void keyError(MediaKeyErrorCode, unsigned long systemCode) OVERRIDE;
    virtual void keyMessage(const unsigned char* message, size_t messageLength, const blink::WebURL& destinationURL) OVERRIDE;

    OwnPtr<blink::WebContentDecryptionModuleSession> m_session;

    ContentDecryptionModuleSessionClient* m_client;
};

} // namespace WebCore

#endif // ContentDecryptionModuleSession_h
