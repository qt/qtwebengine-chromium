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
#include "ServiceWorkerGlobalScopeProxy.h"

#include "WebEmbeddedWorkerImpl.h"
#include "WebServiceWorkerContextClient.h"
#include "core/dom/ExecutionContext.h"
#include "platform/NotImplemented.h"
#include "wtf/Functional.h"
#include "wtf/PassOwnPtr.h"

using namespace WebCore;

namespace blink {

PassOwnPtr<ServiceWorkerGlobalScopeProxy> ServiceWorkerGlobalScopeProxy::create(WebEmbeddedWorkerImpl& embeddedWorker, ExecutionContext& executionContext, PassOwnPtr<WebServiceWorkerContextClient> client)
{
    return adoptPtr(new ServiceWorkerGlobalScopeProxy(embeddedWorker, executionContext, client));
}

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy()
{
}

void ServiceWorkerGlobalScopeProxy::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    notImplemented();
}

void ServiceWorkerGlobalScopeProxy::reportConsoleMessage(MessageSource, MessageLevel, const String& message, int lineNumber, const String& sourceURL)
{
    notImplemented();
}

void ServiceWorkerGlobalScopeProxy::postMessageToPageInspector(const String& message)
{
    m_client->dispatchDevToolsMessage(message);
}

void ServiceWorkerGlobalScopeProxy::updateInspectorStateCookie(const String& message)
{
    m_client->saveDevToolsAgentState(message);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeStarted()
{
    m_client->workerContextStarted(this);
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeClosed()
{
    m_executionContext.postTask(bind(&WebEmbeddedWorkerImpl::terminateWorkerContext, &m_embeddedWorker));
}

void ServiceWorkerGlobalScopeProxy::workerGlobalScopeDestroyed()
{
    m_client->workerContextDestroyed();
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(WebEmbeddedWorkerImpl& embeddedWorker, ExecutionContext& executionContext, PassOwnPtr<WebServiceWorkerContextClient> client)
    : m_embeddedWorker(embeddedWorker)
    , m_executionContext(executionContext)
    , m_client(client)
{
    ASSERT(m_client);
}

} // namespace blink
