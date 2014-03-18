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
#include "core/loader/FrameFetchContext.h"

#include "core/dom/Document.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ProgressTracker.h"
#include "core/frame/Frame.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace WebCore {

FrameFetchContext::FrameFetchContext(Frame* frame)
    : m_frame(frame)
{
}

void FrameFetchContext::reportLocalLoadFailed(const KURL& url)
{
    FrameLoader::reportLocalLoadFailed(m_frame, url.elidedString());
}

void FrameFetchContext::addAdditionalRequestHeaders(Document& document, ResourceRequest& request, Resource::Type type)
{
    if (type != Resource::MainResource) {
        String outgoingReferrer;
        String outgoingOrigin;
        if (request.httpReferrer().isNull()) {
            outgoingReferrer = document.outgoingReferrer();
            outgoingOrigin = document.outgoingOrigin();
        } else {
            outgoingReferrer = request.httpReferrer();
            outgoingOrigin = SecurityOrigin::createFromString(outgoingReferrer)->toString();
        }

        outgoingReferrer = SecurityPolicy::generateReferrerHeader(document.referrerPolicy(), request.url(), outgoingReferrer);
        if (outgoingReferrer.isEmpty())
            request.clearHTTPReferrer();
        else if (!request.httpReferrer())
            request.setHTTPReferrer(outgoingReferrer);

        FrameLoader::addHTTPOriginIfNeeded(request, outgoingOrigin);
    }

    m_frame->loader().addExtraFieldsToRequest(request);
}

CachePolicy FrameFetchContext::cachePolicy(Document* document) const
{
    if (document && document->loadEventFinished())
        return CachePolicyVerify;

    FrameLoadType loadType = m_frame->loader().loadType();
    if (loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    if (Frame* parentFrame = m_frame->tree().parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader().fetchContext().cachePolicy(parentFrame->document());
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    if (loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    DocumentLoader* loader = document ? document->loader() : 0;
    if (loader && loader->request().cachePolicy() == ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;
    return CachePolicyVerify;

}

// FIXME(http://crbug.com/274173):
// |loader| can be null if the resource is loaded from imported document.
// This means inspector, which uses DocumentLoader as an grouping entity,
// cannot see imported documents.
inline DocumentLoader* FrameFetchContext::ensureLoader(DocumentLoader* loader)
{
    return loader ? loader : m_frame->loader().activeDocumentLoader();
}

void FrameFetchContext::dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority loadPriority)
{
    m_frame->loader().client()->dispatchDidChangeResourcePriority(identifier, loadPriority);
}

void FrameFetchContext::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    m_frame->loader().applyUserAgent(request);
    m_frame->loader().client()->dispatchWillSendRequest(loader, identifier, request, redirectResponse);
    InspectorInstrumentation::willSendRequest(m_frame, identifier, ensureLoader(loader), request, redirectResponse, initiatorInfo);
}

void FrameFetchContext::dispatchDidLoadResourceFromMemoryCache(const ResourceRequest& request, const ResourceResponse& response)
{
    m_frame->loader().client()->dispatchDidLoadResourceFromMemoryCache(request, response);
}

void FrameFetchContext::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, ResourceLoader* resourceLoader)
{
    if (Page* page = m_frame->page())
        page->progress().incrementProgress(identifier, r);
    m_frame->loader().client()->dispatchDidReceiveResponse(loader, identifier, r);
    InspectorInstrumentation::didReceiveResourceResponse(m_frame, identifier, ensureLoader(loader), r, resourceLoader);
}

void FrameFetchContext::dispatchDidReceiveData(DocumentLoader*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (Page* page = m_frame->page())
        page->progress().incrementProgress(identifier, data, dataLength);
    InspectorInstrumentation::didReceiveData(m_frame, identifier, data, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidDownloadData(DocumentLoader*, unsigned long identifier, int dataLength, int encodedDataLength)
{
    if (Page* page = m_frame->page())
        page->progress().incrementProgress(identifier, 0, dataLength);
    InspectorInstrumentation::didReceiveData(m_frame, identifier, 0, dataLength, encodedDataLength);
}

void FrameFetchContext::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier, double finishTime)
{
    if (Page* page = m_frame->page())
        page->progress().completeProgress(identifier);
    m_frame->loader().client()->dispatchDidFinishLoading(loader, identifier);

    InspectorInstrumentation::didFinishLoading(m_frame, identifier, ensureLoader(loader), finishTime);
}

void FrameFetchContext::dispatchDidFail(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (Page* page = m_frame->page())
        page->progress().completeProgress(identifier);
    InspectorInstrumentation::didFailLoading(m_frame, identifier, ensureLoader(loader), error);
}

void FrameFetchContext::sendRemainingDelegateMessages(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response, int dataLength)
{
    if (!response.isNull())
        dispatchDidReceiveResponse(ensureLoader(loader), identifier, response);

    if (dataLength > 0)
        dispatchDidReceiveData(ensureLoader(loader), identifier, 0, dataLength, 0);

    dispatchDidFinishLoading(ensureLoader(loader), identifier, 0);
}

} // namespace WebCore
