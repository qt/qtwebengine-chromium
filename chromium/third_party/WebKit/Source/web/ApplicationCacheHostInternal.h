/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "WebApplicationCacheHostClient.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "public/platform/WebURL.h"

namespace WebCore {

class ApplicationCacheHostInternal : public blink::WebApplicationCacheHostClient {
public:
    ApplicationCacheHostInternal(ApplicationCacheHost* host)
        : m_innerHost(host)
    {
        blink::WebFrameImpl* webFrame = blink::WebFrameImpl::fromFrame(host->m_documentLoader->frame());
        ASSERT(webFrame);
        m_outerHost = adoptPtr(webFrame->client()->createApplicationCacheHost(webFrame, this));
    }

    virtual void didChangeCacheAssociation()
    {
        // FIXME: Prod the inspector to update it's notion of what cache the page is using.
    }

    virtual void notifyEventListener(blink::WebApplicationCacheHost::EventID eventID)
    {
        m_innerHost->notifyApplicationCache(static_cast<ApplicationCacheHost::EventID>(eventID), 0, 0);
    }

    virtual void notifyProgressEventListener(const blink::WebURL&, int progressTotal, int progressDone)
    {
        m_innerHost->notifyApplicationCache(ApplicationCacheHost::PROGRESS_EVENT, progressTotal, progressDone);
    }

    static blink::WebApplicationCacheHost* toWebApplicationCacheHost(ApplicationCacheHost* innerHost)
    {
        if (innerHost && innerHost->m_internal)
            return innerHost->m_internal->m_outerHost.get();
        return 0;
    }

private:
    friend class ApplicationCacheHost;
    ApplicationCacheHost* m_innerHost;
    OwnPtr<blink::WebApplicationCacheHost> m_outerHost;
};

}
