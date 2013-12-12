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

#include "config.h"
#include "MediaSourcePrivateImpl.h"

#include "SourceBufferPrivateImpl.h"
#include "WebSourceBuffer.h"

#include "public/platform/WebMediaSource.h"

#include <algorithm>
#include <limits>
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebKit {

MediaSourcePrivateImpl::MediaSourcePrivateImpl(PassOwnPtr<WebKit::WebMediaSource> webMediaSource)
    : m_webMediaSource(webMediaSource)
{
}

WebCore::MediaSourcePrivate::AddStatus MediaSourcePrivateImpl::addSourceBuffer(const String& type, const CodecsArray& codecs,
    OwnPtr<WebCore::SourceBufferPrivate>* sourceBuffer)
{
    if (!m_webMediaSource)
        return WebCore::MediaSourcePrivate::NotSupported;

    WebSourceBuffer* webSourceBuffer = 0;
    WebCore::MediaSourcePrivate::AddStatus result =
        static_cast<WebCore::MediaSourcePrivate::AddStatus>(m_webMediaSource->addSourceBuffer(type, codecs, &webSourceBuffer));

    if (result == WebCore::MediaSourcePrivate::Ok) {
        ASSERT(webSourceBuffer);
        *sourceBuffer = adoptPtr(new SourceBufferPrivateImpl(adoptPtr(webSourceBuffer)));
    }
    return result;
}

double MediaSourcePrivateImpl::duration()
{
    if (!m_webMediaSource)
        return std::numeric_limits<float>::quiet_NaN();

    return m_webMediaSource->duration();
}

void MediaSourcePrivateImpl::setDuration(double duration)
{
    if (m_webMediaSource)
        m_webMediaSource->setDuration(duration);
}

void MediaSourcePrivateImpl::markEndOfStream(WebCore::MediaSourcePrivate::EndOfStreamStatus status)
{
    if (m_webMediaSource)
        m_webMediaSource->markEndOfStream(static_cast<WebMediaSource::EndOfStreamStatus>(status));
}

void MediaSourcePrivateImpl::unmarkEndOfStream()
{
    if (m_webMediaSource)
        m_webMediaSource->unmarkEndOfStream();
}

}
