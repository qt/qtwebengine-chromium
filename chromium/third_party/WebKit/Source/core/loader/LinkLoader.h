/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef LinkLoader_h
#define LinkLoader_h

#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceOwner.h"
#include "core/loader/LinkLoaderClient.h"
#include "platform/PrerenderClient.h"
#include "platform/Timer.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class Document;
class LinkRelAttribute;
class PrerenderHandle;

// The LinkLoader can load link rel types icon, dns-prefetch, subresource, prefetch and prerender.
class LinkLoader : public ResourceOwner<Resource, ResourceClient>, public PrerenderClient {

public:
    explicit LinkLoader(LinkLoaderClient*);
    virtual ~LinkLoader();

    // from ResourceClient
    virtual void notifyFinished(Resource*);

    // from PrerenderClient
    virtual void didStartPrerender() OVERRIDE;
    virtual void didStopPrerender() OVERRIDE;
    virtual void didSendLoadForPrerender() OVERRIDE;
    virtual void didSendDOMContentLoadedForPrerender() OVERRIDE;

    void released();
    bool loadLink(const LinkRelAttribute&, const String& type, const KURL&, Document&);

private:
    void linkLoadTimerFired(Timer<LinkLoader>*);
    void linkLoadingErrorTimerFired(Timer<LinkLoader>*);

    LinkLoaderClient* m_client;

    Timer<LinkLoader> m_linkLoadTimer;
    Timer<LinkLoader> m_linkLoadingErrorTimer;

    OwnPtr<PrerenderHandle> m_prerender;
};

}

#endif
