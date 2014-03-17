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

#ifndef WebMediaStreamTrackSourcesRequest_h
#define WebMediaStreamTrackSourcesRequest_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

namespace WebCore {
class MediaStreamTrackSourcesRequest;
}

namespace blink {
class WebSourceInfo;

class WebMediaStreamTrackSourcesRequest {
public:
    class ExtraData {
    public:
        virtual ~ExtraData() { }
    };

    WebMediaStreamTrackSourcesRequest() { }
    WebMediaStreamTrackSourcesRequest(const WebMediaStreamTrackSourcesRequest& other) { assign(other); }
    ~WebMediaStreamTrackSourcesRequest() { reset(); }

    WebMediaStreamTrackSourcesRequest& operator=(const WebMediaStreamTrackSourcesRequest& other)
    {
        assign(other);
        return *this;
    }

    BLINK_EXPORT void assign(const WebMediaStreamTrackSourcesRequest&);

    BLINK_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    BLINK_EXPORT WebString origin() const;
    BLINK_EXPORT void requestSucceeded(const WebVector<WebSourceInfo>&) const;

    // Extra data associated with this object.
    // If non-null, the extra data pointer will be deleted when the object is destroyed.
    // Setting the extra data pointer will cause any existing non-null
    // extra data pointer to be deleted.
    BLINK_EXPORT ExtraData* extraData() const;
    BLINK_EXPORT void setExtraData(ExtraData*);

#if BLINK_IMPLEMENTATION
    WebMediaStreamTrackSourcesRequest(const WTF::PassRefPtr<WebCore::MediaStreamTrackSourcesRequest>&);
#endif

private:
    WebPrivatePtr<WebCore::MediaStreamTrackSourcesRequest> m_private;
};

} // namespace blink

#endif // WebMediaStreamTrackSourcesRequest_h
