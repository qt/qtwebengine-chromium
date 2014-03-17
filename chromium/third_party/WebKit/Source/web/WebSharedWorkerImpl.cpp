/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebSharedWorkerImpl.h"

#include "DatabaseClientImpl.h"
#include "LocalFileSystemClient.h"
#include "WebDataSourceImpl.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebRuntimeFeatures.h"
#include "WebSettings.h"
#include "WebView.h"
#include "WorkerPermissionClient.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/events/MessageEvent.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebFileError.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebWorkerPermissionClientProxy.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"

using namespace WebCore;

namespace blink {

// This function is called on the main thread to force to initialize some static
// values used in WebKit before any worker thread is started. This is because in
// our worker processs, we do not run any WebKit code in main thread and thus
// when multiple workers try to start at the same time, we might hit crash due
// to contention for initializing static values.
static void initializeWebKitStaticValues()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // Note that we have to pass a URL with valid protocol in order to follow
        // the path to do static value initializations.
        RefPtr<SecurityOrigin> origin =
            SecurityOrigin::create(KURL(ParsedURLString, "http://localhost"));
        origin.release();
    }
}

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : m_webView(0)
    , m_mainFrame(0)
    , m_askedToTerminate(false)
    , m_client(WeakReference<WebSharedWorkerClient>::create(client))
    , m_clientWeakPtr(WeakPtr<WebSharedWorkerClient>(m_client))
    , m_pauseWorkerContextOnStart(false)
{
    initializeWebKitStaticValues();
}

WebSharedWorkerImpl::~WebSharedWorkerImpl()
{
    ASSERT(m_webView);
    // Detach the client before closing the view to avoid getting called back.
    toWebFrameImpl(m_mainFrame)->setClient(0);

    m_webView->close();
    m_mainFrame->close();
}

void WebSharedWorkerImpl::stopWorkerThread()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;
    if (m_workerThread)
        m_workerThread->stop();
}

void WebSharedWorkerImpl::initializeLoader(const WebURL& url)
{
    // Create 'shadow page'. This page is never displayed, it is used to proxy the
    // loading requests from the worker context to the rest of WebKit and Chromium
    // infrastructure.
    ASSERT(!m_webView);
    m_webView = WebView::create(0);
    m_webView->settings()->setOfflineWebApplicationCacheEnabled(WebRuntimeFeatures::isApplicationCacheEnabled());
    // FIXME: Settings information should be passed to the Worker process from Browser process when the worker
    // is created (similar to RenderThread::OnCreateNewView).
    m_mainFrame = WebFrame::create(this);
    m_webView->setMainFrame(m_mainFrame);

    WebFrameImpl* webFrame = toWebFrameImpl(m_webView->mainFrame());

    // Construct substitute data source for the 'shadow page'. We only need it
    // to have same origin as the worker so the loading checks work correctly.
    CString content("");
    int length = static_cast<int>(content.length());
    RefPtr<SharedBuffer> buffer(SharedBuffer::create(content.data(), length));
    webFrame->frame()->loader().load(FrameLoadRequest(0, ResourceRequest(url), SubstituteData(buffer, "text/html", "UTF-8", KURL())));

    // This document will be used as 'loading context' for the worker.
    m_loadingDocument = webFrame->frame()->document();
}

void WebSharedWorkerImpl::didCreateDataSource(WebFrame*, WebDataSource* ds)
{
    // Tell the loader to load the data into the 'shadow page' synchronously,
    // so we can grab the resulting Document right after load.
    static_cast<WebDataSourceImpl*>(ds)->setDeferMainResourceDataLoad(false);
}

WebApplicationCacheHost* WebSharedWorkerImpl::createApplicationCacheHost(WebFrame*, WebApplicationCacheHostClient* appcacheHostClient)
{
    if (client())
        return client()->createApplicationCacheHost(appcacheHostClient);
    return 0;
}

// WorkerReportingProxy --------------------------------------------------------

void WebSharedWorkerImpl::reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    // Not suppported in SharedWorker.
}

void WebSharedWorkerImpl::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, int lineNumber, const String& sourceURL)
{
    // Not supported in SharedWorker.
}

void WebSharedWorkerImpl::postMessageToPageInspector(const String& message)
{
    callOnMainThread(bind(&WebSharedWorkerClient::dispatchDevToolsMessage, m_clientWeakPtr, message.isolatedCopy()));
}

void WebSharedWorkerImpl::updateInspectorStateCookie(const String& cookie)
{
    callOnMainThread(bind(&WebSharedWorkerClient::saveDevToolsAgentState, m_clientWeakPtr, cookie.isolatedCopy()));
}

void WebSharedWorkerImpl::workerGlobalScopeClosed()
{
    callOnMainThread(bind(&WebSharedWorkerImpl::workerGlobalScopeClosedOnMainThread, this));
}

void WebSharedWorkerImpl::workerGlobalScopeClosedOnMainThread()
{
    if (client())
        client()->workerContextClosed();

    stopWorkerThread();
}

void WebSharedWorkerImpl::workerGlobalScopeStarted()
{
}

void WebSharedWorkerImpl::workerGlobalScopeDestroyed()
{
    callOnMainThread(bind(&WebSharedWorkerImpl::workerGlobalScopeDestroyedOnMainThread, this));
}

void WebSharedWorkerImpl::workerGlobalScopeDestroyedOnMainThread()
{
    if (client())
        client()->workerContextDestroyed();
    // The lifetime of this proxy is controlled by the worker context.
    delete this;
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebSharedWorkerImpl::postTaskToLoader(PassOwnPtr<ExecutionContextTask> task)
{
    ASSERT(m_loadingDocument->isDocument());
    m_loadingDocument->postTask(task);
}

bool WebSharedWorkerImpl::postTaskForModeToWorkerGlobalScope(
    PassOwnPtr<ExecutionContextTask> task, const String& mode)
{
    m_workerThread->runLoop().postTaskForMode(task, mode);
    return true;
}

bool WebSharedWorkerImpl::isStarted()
{
    // Should not ever be called from the worker thread (this API is only called on WebSharedWorkerProxy on the renderer thread).
    ASSERT_NOT_REACHED();
    return workerThread();
}

void WebSharedWorkerImpl::connect(WebMessagePortChannel* webChannel, ConnectListener* listener)
{
    workerThread()->runLoop().postTask(
        createCallbackTask(&connectTask, adoptPtr(webChannel)));
    if (listener)
        listener->connected();
}

void WebSharedWorkerImpl::connectTask(ExecutionContext* context, PassOwnPtr<WebMessagePortChannel> channel)
{
    // Wrap the passed-in channel in a MessagePort, and send it off via a connect event.
    RefPtr<MessagePort> port = MessagePort::create(*context);
    port->entangle(channel);
    WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
    ASSERT_WITH_SECURITY_IMPLICATION(workerGlobalScope->isSharedWorkerGlobalScope());
    workerGlobalScope->dispatchEvent(createConnectEvent(port));
}

void WebSharedWorkerImpl::startWorkerContext(const WebURL& url, const WebString& name, const WebString& userAgent, const WebString& sourceCode, const WebString& contentSecurityPolicy, WebContentSecurityPolicyType policyType, long long)
{
    initializeLoader(url);

    WorkerThreadStartMode startMode = m_pauseWorkerContextOnStart ? PauseWorkerGlobalScopeOnStart : DontPauseWorkerGlobalScopeOnStart;
    OwnPtr<WorkerClients> workerClients = WorkerClients::create();
    provideLocalFileSystemToWorker(workerClients.get(), LocalFileSystemClient::create());
    provideDatabaseClientToWorker(workerClients.get(), DatabaseClientImpl::create());
    WebSecurityOrigin webSecurityOrigin(m_loadingDocument->securityOrigin());
    providePermissionClientToWorker(workerClients.get(), adoptPtr(client()->createWorkerPermissionClientProxy(webSecurityOrigin)));
    OwnPtr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(url, userAgent, sourceCode, startMode, contentSecurityPolicy, static_cast<WebCore::ContentSecurityPolicy::HeaderType>(policyType), workerClients.release());
    setWorkerThread(SharedWorkerThread::create(name, *this, *this, startupData.release()));

    workerThread()->start();
}

void WebSharedWorkerImpl::terminateWorkerContext()
{
    stopWorkerThread();
}

void WebSharedWorkerImpl::clientDestroyed()
{
    m_client.clear();
}

void WebSharedWorkerImpl::pauseWorkerContextOnStart()
{
    m_pauseWorkerContextOnStart = true;
}

static void resumeWorkerContextTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->resume();
}

void WebSharedWorkerImpl::resumeWorkerContext()
{
    m_pauseWorkerContextOnStart = false;
    if (workerThread())
        workerThread()->runLoop().postTaskForMode(createCallbackTask(resumeWorkerContextTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void connectToWorkerContextInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->connectFrontend();
}

void WebSharedWorkerImpl::attachDevTools()
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(connectToWorkerContextInspectorTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void reconnectToWorkerContextInspectorTask(ExecutionContext* context, const String& savedState)
{
    WorkerInspectorController* ic = toWorkerGlobalScope(context)->workerInspectorController();
    ic->restoreInspectorStateFromCookie(savedState);
    ic->resume();
}

void WebSharedWorkerImpl::reattachDevTools(const WebString& savedState)
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(reconnectToWorkerContextInspectorTask, String(savedState)), WorkerDebuggerAgent::debuggerTaskMode);
}

static void disconnectFromWorkerContextInspectorTask(ExecutionContext* context, bool)
{
    toWorkerGlobalScope(context)->workerInspectorController()->disconnectFrontend();
}

void WebSharedWorkerImpl::detachDevTools()
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(disconnectFromWorkerContextInspectorTask, true), WorkerDebuggerAgent::debuggerTaskMode);
}

static void dispatchOnInspectorBackendTask(ExecutionContext* context, const String& message)
{
    toWorkerGlobalScope(context)->workerInspectorController()->dispatchMessageFromFrontend(message);
}

void WebSharedWorkerImpl::dispatchDevToolsMessage(const WebString& message)
{
    workerThread()->runLoop().postTaskForMode(createCallbackTask(dispatchOnInspectorBackendTask, String(message)), WorkerDebuggerAgent::debuggerTaskMode);
    WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(workerThread());
}

WebSharedWorker* WebSharedWorker::create(WebSharedWorkerClient* client)
{
    return new WebSharedWorkerImpl(client);
}

} // namespace blink
