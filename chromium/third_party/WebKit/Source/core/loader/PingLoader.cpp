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
 *
 */

#include "config.h"
#include "core/loader/PingLoader.h"

#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/UniqueIdentifier.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/network/FormData.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoader.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

void PingLoader::loadImage(Frame* frame, const KURL& url)
{
    if (!frame->document()->securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url.string());
        return;
    }

    ResourceRequest request(url);
    request.setTargetType(ResourceRequest::TargetIsPing);
    request.setHTTPHeaderField("Cache-Control", "max-age=0");
    String referrer = SecurityPolicy::generateReferrerHeader(frame->document()->referrerPolicy(), request.url(), frame->document()->outgoingReferrer());
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);
    frame->loader().addExtraFieldsToRequest(request);
    OwnPtr<PingLoader> pingLoader = adoptPtr(new PingLoader(frame, request));

    // Leak the ping loader, since it will kill itself as soon as it receives a response.
    PingLoader* ALLOW_UNUSED leakedPingLoader = pingLoader.leakPtr();
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::sendPing(Frame* frame, const KURL& pingURL, const KURL& destinationURL)
{
    ResourceRequest request(pingURL);
    request.setTargetType(ResourceRequest::TargetIsPing);
    request.setHTTPMethod("POST");
    request.setHTTPContentType("text/ping");
    request.setHTTPBody(FormData::create("PING"));
    request.setHTTPHeaderField("Cache-Control", "max-age=0");
    frame->loader().addExtraFieldsToRequest(request);

    SecurityOrigin* sourceOrigin = frame->document()->securityOrigin();
    RefPtr<SecurityOrigin> pingOrigin = SecurityOrigin::create(pingURL);
    FrameLoader::addHTTPOriginIfNeeded(request, sourceOrigin->toString());
    request.setHTTPHeaderField("Ping-To", destinationURL.string());
    if (!SecurityPolicy::shouldHideReferrer(pingURL, frame->document()->outgoingReferrer())) {
        request.setHTTPHeaderField("Ping-From", frame->document()->url().string());
        if (!sourceOrigin->isSameSchemeHostPort(pingOrigin.get())) {
            String referrer = SecurityPolicy::generateReferrerHeader(frame->document()->referrerPolicy(), pingURL, frame->document()->outgoingReferrer());
            if (!referrer.isEmpty())
                request.setHTTPReferrer(referrer);
        }
    }
    OwnPtr<PingLoader> pingLoader = adoptPtr(new PingLoader(frame, request));

    // Leak the ping loader, since it will kill itself as soon as it receives a response.
    PingLoader* ALLOW_UNUSED leakedPingLoader = pingLoader.leakPtr();
}

void PingLoader::sendViolationReport(Frame* frame, const KURL& reportURL, PassRefPtr<FormData> report, ViolationReportType type)
{
    ResourceRequest request(reportURL);
    request.setTargetType(ResourceRequest::TargetIsSubresource);
    request.setHTTPMethod("POST");
    request.setHTTPContentType(type == ContentSecurityPolicyViolationReport ? "application/csp-report" : "application/json");
    request.setHTTPBody(report);
    frame->loader().addExtraFieldsToRequest(request);

    String referrer = SecurityPolicy::generateReferrerHeader(frame->document()->referrerPolicy(), reportURL, frame->document()->outgoingReferrer());
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);
    OwnPtr<PingLoader> pingLoader = adoptPtr(new PingLoader(frame, request, SecurityOrigin::create(reportURL)->isSameSchemeHostPort(frame->document()->securityOrigin()) ? AllowStoredCredentials : DoNotAllowStoredCredentials));

    // Leak the ping loader, since it will kill itself as soon as it receives a response.
    PingLoader* ALLOW_UNUSED leakedPingLoader = pingLoader.leakPtr();
}

PingLoader::PingLoader(Frame* frame, ResourceRequest& request, StoredCredentials credentialsAllowed)
    : m_timeout(this, &PingLoader::timeout)
{
    frame->loader().client()->didDispatchPingLoader(request.url());

    unsigned long identifier = createUniqueIdentifier();
    m_loader = adoptPtr(blink::Platform::current()->createURLLoader());
    ASSERT(m_loader);
    blink::WrappedResourceRequest wrappedRequest(request);
    wrappedRequest.setAllowStoredCredentials(credentialsAllowed == AllowStoredCredentials);
    m_loader->loadAsynchronously(wrappedRequest, this);

    InspectorInstrumentation::continueAfterPingLoader(frame, identifier, frame->loader().activeDocumentLoader(), request, ResourceResponse());

    // If the server never responds, FrameLoader won't be able to cancel this load and
    // we'll sit here waiting forever. Set a very generous timeout, just in case.
    m_timeout.startOneShot(60000);
}

PingLoader::~PingLoader()
{
    if (m_loader)
        m_loader->cancel();
}

}
