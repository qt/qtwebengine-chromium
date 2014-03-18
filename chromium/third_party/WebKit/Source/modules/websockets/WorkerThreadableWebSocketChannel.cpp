/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#include "modules/websockets/WorkerThreadableWebSocketChannel.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/frame/Settings.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerRunLoop.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/MainThreadWebSocketChannel.h"
#include "modules/websockets/ThreadableWebSocketChannelClientWrapper.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/MainThread.h"

namespace WebCore {

WorkerThreadableWebSocketChannel::WorkerThreadableWebSocketChannel(WorkerGlobalScope* context, WebSocketChannelClient* client, const String& taskMode, const String& sourceURL, unsigned lineNumber)
    : m_workerGlobalScope(context)
    , m_workerClientWrapper(ThreadableWebSocketChannelClientWrapper::create(context, client))
    , m_bridge(Bridge::create(m_workerClientWrapper, m_workerGlobalScope, taskMode))
    , m_sourceURLAtConnection(sourceURL)
    , m_lineNumberAtConnection(lineNumber)
{
    m_bridge->initialize(sourceURL, lineNumber);
}

WorkerThreadableWebSocketChannel::~WorkerThreadableWebSocketChannel()
{
    if (m_bridge)
        m_bridge->disconnect();
}

void WorkerThreadableWebSocketChannel::connect(const KURL& url, const String& protocol)
{
    if (m_bridge)
        m_bridge->connect(url, protocol);
}

String WorkerThreadableWebSocketChannel::subprotocol()
{
    ASSERT(m_workerClientWrapper);
    return m_workerClientWrapper->subprotocol();
}

String WorkerThreadableWebSocketChannel::extensions()
{
    ASSERT(m_workerClientWrapper);
    return m_workerClientWrapper->extensions();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(const String& message)
{
    if (!m_bridge)
        return WebSocketChannel::SendFail;
    return m_bridge->send(message);
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!m_bridge)
        return WebSocketChannel::SendFail;
    return m_bridge->send(binaryData, byteOffset, byteLength);
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(PassRefPtr<BlobDataHandle> blobData)
{
    if (!m_bridge)
        return WebSocketChannel::SendFail;
    return m_bridge->send(blobData);
}

unsigned long WorkerThreadableWebSocketChannel::bufferedAmount() const
{
    if (!m_bridge)
        return 0;
    return m_bridge->bufferedAmount();
}

void WorkerThreadableWebSocketChannel::close(int code, const String& reason)
{
    if (m_bridge)
        m_bridge->close(code, reason);
}

void WorkerThreadableWebSocketChannel::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    if (!m_bridge)
        return;

    RefPtr<ScriptCallStack> callStack = createScriptCallStack(1, true);
    if (callStack && callStack->size())  {
        // In order to emulate the ConsoleMessage behavior,
        // we should ignore the specified url and line number if
        // we can get the JavaScript context.
        m_bridge->fail(reason, level, callStack->at(0).sourceURL(), callStack->at(0).lineNumber());
    } else if (sourceURL.isEmpty() && !lineNumber) {
        // No information is specified by the caller - use the url
        // and the line number at the connection.
        m_bridge->fail(reason, level, m_sourceURLAtConnection, m_lineNumberAtConnection);
    } else {
        // Use the specified information.
        m_bridge->fail(reason, level, sourceURL, lineNumber);
    }
}

void WorkerThreadableWebSocketChannel::disconnect()
{
    m_bridge->disconnect();
    m_bridge.clear();
}

void WorkerThreadableWebSocketChannel::suspend()
{
    m_workerClientWrapper->suspend();
    if (m_bridge)
        m_bridge->suspend();
}

void WorkerThreadableWebSocketChannel::resume()
{
    m_workerClientWrapper->resume();
    if (m_bridge)
        m_bridge->resume();
}

WorkerThreadableWebSocketChannel::Peer::Peer(PassRefPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper, WorkerLoaderProxy& loaderProxy, ExecutionContext* context, const String& taskMode, const String& sourceURL, unsigned lineNumber)
    : m_workerClientWrapper(clientWrapper)
    , m_loaderProxy(loaderProxy)
    , m_mainWebSocketChannel(0)
    , m_taskMode(taskMode)
{
    Document* document = toDocument(context);
    Settings* settings = document->settings();
    if (settings && settings->experimentalWebSocketEnabled()) {
        // FIXME: Create an "experimental" WebSocketChannel instead of a MainThreadWebSocketChannel.
        m_mainWebSocketChannel = MainThreadWebSocketChannel::create(document, this, sourceURL, lineNumber);
    } else
        m_mainWebSocketChannel = MainThreadWebSocketChannel::create(document, this, sourceURL, lineNumber);
    ASSERT(isMainThread());
}

WorkerThreadableWebSocketChannel::Peer::~Peer()
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->disconnect();
}

void WorkerThreadableWebSocketChannel::Peer::connect(const KURL& url, const String& protocol)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->connect(url, protocol);
}

static void workerGlobalScopeDidSend(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, WebSocketChannel::SendResult sendRequestResult)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->setSendRequestResult(sendRequestResult);
}

void WorkerThreadableWebSocketChannel::Peer::send(const String& message)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel || !m_workerClientWrapper)
        return;
    WebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(message);
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidSend, m_workerClientWrapper, sendRequestResult), m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::send(const ArrayBuffer& binaryData)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel || !m_workerClientWrapper)
        return;
    WebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(binaryData, 0, binaryData.byteLength());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidSend, m_workerClientWrapper, sendRequestResult), m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::send(PassRefPtr<BlobDataHandle> blobData)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel || !m_workerClientWrapper)
        return;
    WebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(blobData);
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidSend, m_workerClientWrapper, sendRequestResult), m_taskMode);
}

static void workerGlobalScopeDidGetBufferedAmount(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, unsigned long bufferedAmount)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->setBufferedAmount(bufferedAmount);
}

void WorkerThreadableWebSocketChannel::Peer::bufferedAmount()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel || !m_workerClientWrapper)
        return;
    unsigned long bufferedAmount = m_mainWebSocketChannel->bufferedAmount();
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidGetBufferedAmount, m_workerClientWrapper, bufferedAmount), m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::close(int code, const String& reason)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->close(code, reason);
}

void WorkerThreadableWebSocketChannel::Peer::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->fail(reason, level, sourceURL, lineNumber);
}

void WorkerThreadableWebSocketChannel::Peer::disconnect()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->disconnect();
    m_mainWebSocketChannel = 0;
}

void WorkerThreadableWebSocketChannel::Peer::suspend()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->suspend();
}

void WorkerThreadableWebSocketChannel::Peer::resume()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->resume();
}

static void workerGlobalScopeDidConnect(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, const String& subprotocol, const String& extensions)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->setSubprotocol(subprotocol);
    workerClientWrapper->setExtensions(extensions);
    workerClientWrapper->didConnect();
}

void WorkerThreadableWebSocketChannel::Peer::didConnect()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidConnect, m_workerClientWrapper, m_mainWebSocketChannel->subprotocol(), m_mainWebSocketChannel->extensions()), m_taskMode);
}

static void workerGlobalScopeDidReceiveMessage(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, const String& message)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveMessage(message);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessage(const String& message)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveMessage, m_workerClientWrapper, message), m_taskMode);
}

static void workerGlobalScopeDidReceiveBinaryData(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveBinaryData(binaryData);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveBinaryData, m_workerClientWrapper, binaryData), m_taskMode);
}

static void workerGlobalScopeDidUpdateBufferedAmount(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, unsigned long bufferedAmount)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didUpdateBufferedAmount(bufferedAmount);
}

void WorkerThreadableWebSocketChannel::Peer::didUpdateBufferedAmount(unsigned long bufferedAmount)
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidUpdateBufferedAmount, m_workerClientWrapper, bufferedAmount), m_taskMode);
}

static void workerGlobalScopeDidStartClosingHandshake(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didStartClosingHandshake();
}

void WorkerThreadableWebSocketChannel::Peer::didStartClosingHandshake()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidStartClosingHandshake, m_workerClientWrapper), m_taskMode);
}

static void workerGlobalScopeDidClose(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, unsigned long unhandledBufferedAmount, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didClose(unhandledBufferedAmount, closingHandshakeCompletion, code, reason);
}

void WorkerThreadableWebSocketChannel::Peer::didClose(unsigned long unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT(isMainThread());
    m_mainWebSocketChannel = 0;
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidClose, m_workerClientWrapper, unhandledBufferedAmount, closingHandshakeCompletion, code, reason), m_taskMode);
}

static void workerGlobalScopeDidReceiveMessageError(ExecutionContext* context, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    workerClientWrapper->didReceiveMessageError();
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessageError()
{
    ASSERT(isMainThread());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope(createCallbackTask(&workerGlobalScopeDidReceiveMessageError, m_workerClientWrapper), m_taskMode);
}

WorkerThreadableWebSocketChannel::Bridge::Bridge(PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper, PassRefPtr<WorkerGlobalScope> workerGlobalScope, const String& taskMode)
    : m_workerClientWrapper(workerClientWrapper)
    , m_workerGlobalScope(workerGlobalScope)
    , m_loaderProxy(m_workerGlobalScope->thread()->workerLoaderProxy())
    , m_taskMode(taskMode)
    , m_peer(0)
{
    ASSERT(m_workerClientWrapper.get());
}

WorkerThreadableWebSocketChannel::Bridge::~Bridge()
{
    disconnect();
}

class WorkerThreadableWebSocketChannel::WorkerGlobalScopeDidInitializeTask : public ExecutionContextTask {
public:
    static PassOwnPtr<ExecutionContextTask> create(WorkerThreadableWebSocketChannel::Peer* peer, WorkerLoaderProxy* loaderProxy, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
    {
        return adoptPtr(new WorkerGlobalScopeDidInitializeTask(peer, loaderProxy, workerClientWrapper));
    }

    virtual ~WorkerGlobalScopeDidInitializeTask() { }
    virtual void performTask(ExecutionContext* context) OVERRIDE
    {
        ASSERT_UNUSED(context, context->isWorkerGlobalScope());
        if (m_workerClientWrapper->failedWebSocketChannelCreation()) {
            // If Bridge::initialize() quitted earlier, we need to kick mainThreadDestroy() to delete the peer.
            OwnPtr<WorkerThreadableWebSocketChannel::Peer> peer = adoptPtr(m_peer);
            m_peer = 0;
            m_loaderProxy->postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadDestroy, peer.release()));
        } else
            m_workerClientWrapper->didCreateWebSocketChannel(m_peer);
    }
    virtual bool isCleanupTask() const OVERRIDE { return true; }

private:
    WorkerGlobalScopeDidInitializeTask(WorkerThreadableWebSocketChannel::Peer* peer, WorkerLoaderProxy* loaderProxy, PassRefPtr<ThreadableWebSocketChannelClientWrapper> workerClientWrapper)
        : m_peer(peer)
        , m_loaderProxy(loaderProxy)
        , m_workerClientWrapper(workerClientWrapper)
    {
    }

    WorkerThreadableWebSocketChannel::Peer* m_peer;
    WorkerLoaderProxy* m_loaderProxy;
    RefPtr<ThreadableWebSocketChannelClientWrapper> m_workerClientWrapper;
};

void WorkerThreadableWebSocketChannel::Bridge::mainThreadInitialize(ExecutionContext* context, WorkerLoaderProxy* loaderProxy, PassRefPtr<ThreadableWebSocketChannelClientWrapper> prpClientWrapper, const String& taskMode, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());

    RefPtr<ThreadableWebSocketChannelClientWrapper> clientWrapper = prpClientWrapper;

    Peer* peer = Peer::create(clientWrapper, *loaderProxy, context, taskMode, sourceURL, lineNumber);
    bool sent = loaderProxy->postTaskForModeToWorkerGlobalScope(
        WorkerThreadableWebSocketChannel::WorkerGlobalScopeDidInitializeTask::create(peer, loaderProxy, clientWrapper), taskMode);
    if (!sent) {
        clientWrapper->clearPeer();
        delete peer;
    }
}

void WorkerThreadableWebSocketChannel::Bridge::initialize(const String& sourceURL, unsigned lineNumber)
{
    ASSERT(!m_peer);
    setMethodNotCompleted();
    RefPtr<Bridge> protect(this);
    m_loaderProxy.postTaskToLoader(
        createCallbackTask(&Bridge::mainThreadInitialize, AllowCrossThreadAccess(&m_loaderProxy), m_workerClientWrapper, m_taskMode, sourceURL, lineNumber));
    waitForMethodCompletion();
    // m_peer may be null when the nested runloop exited before a peer has created.
    m_peer = m_workerClientWrapper->peer();
    if (!m_peer)
        m_workerClientWrapper->setFailedWebSocketChannelCreation();
}

void WorkerThreadableWebSocketChannel::mainThreadConnect(ExecutionContext* context, Peer* peer, const KURL& url, const String& protocol)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->connect(url, protocol);
}

void WorkerThreadableWebSocketChannel::Bridge::connect(const KURL& url, const String& protocol)
{
    ASSERT(m_workerClientWrapper);
    if (!m_peer)
        return;
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadConnect, AllowCrossThreadAccess(m_peer), url, protocol));
}

void WorkerThreadableWebSocketChannel::mainThreadSend(ExecutionContext* context, Peer* peer, const String& message)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->send(message);
}

void WorkerThreadableWebSocketChannel::mainThreadSendArrayBuffer(ExecutionContext* context, Peer* peer, PassOwnPtr<Vector<char> > data)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    RefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::create(data->data(), data->size());
    peer->send(*arrayBuffer);
}

void WorkerThreadableWebSocketChannel::mainThreadSendBlob(ExecutionContext* context, Peer* peer, PassRefPtr<BlobDataHandle> data)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);
    peer->send(data);
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const String& message)
{
    if (!m_workerClientWrapper || !m_peer)
        return WebSocketChannel::SendFail;
    setMethodNotCompleted();
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadSend, AllowCrossThreadAccess(m_peer), message));
    RefPtr<Bridge> protect(this);
    waitForMethodCompletion();
    ThreadableWebSocketChannelClientWrapper* clientWrapper = m_workerClientWrapper.get();
    if (!clientWrapper)
        return WebSocketChannel::SendFail;
    return clientWrapper->sendRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!m_workerClientWrapper || !m_peer)
        return WebSocketChannel::SendFail;
    // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied into Vector<char>.
    OwnPtr<Vector<char> > data = adoptPtr(new Vector<char>(byteLength));
    if (binaryData.byteLength())
        memcpy(data->data(), static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);
    setMethodNotCompleted();
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadSendArrayBuffer, AllowCrossThreadAccess(m_peer), data.release()));
    RefPtr<Bridge> protect(this);
    waitForMethodCompletion();
    ThreadableWebSocketChannelClientWrapper* clientWrapper = m_workerClientWrapper.get();
    if (!clientWrapper)
        return WebSocketChannel::SendFail;
    return clientWrapper->sendRequestResult();
}

WebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(PassRefPtr<BlobDataHandle> data)
{
    if (!m_workerClientWrapper || !m_peer)
        return WebSocketChannel::SendFail;
    setMethodNotCompleted();
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadSendBlob, AllowCrossThreadAccess(m_peer), data));
    RefPtr<Bridge> protect(this);
    waitForMethodCompletion();
    ThreadableWebSocketChannelClientWrapper* clientWrapper = m_workerClientWrapper.get();
    if (!clientWrapper)
        return WebSocketChannel::SendFail;
    return clientWrapper->sendRequestResult();
}

void WorkerThreadableWebSocketChannel::mainThreadBufferedAmount(ExecutionContext* context, Peer* peer)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->bufferedAmount();
}

unsigned long WorkerThreadableWebSocketChannel::Bridge::bufferedAmount()
{
    if (!m_workerClientWrapper || !m_peer)
        return 0;
    setMethodNotCompleted();
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadBufferedAmount, AllowCrossThreadAccess(m_peer)));
    RefPtr<Bridge> protect(this);
    waitForMethodCompletion();
    ThreadableWebSocketChannelClientWrapper* clientWrapper = m_workerClientWrapper.get();
    if (clientWrapper)
        return clientWrapper->bufferedAmount();
    return 0;
}

void WorkerThreadableWebSocketChannel::mainThreadClose(ExecutionContext* context, Peer* peer, int code, const String& reason)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->close(code, reason);
}

void WorkerThreadableWebSocketChannel::Bridge::close(int code, const String& reason)
{
    if (!m_peer)
        return;
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadClose, AllowCrossThreadAccess(m_peer), code, reason));
}

void WorkerThreadableWebSocketChannel::mainThreadFail(ExecutionContext* context, Peer* peer, const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->fail(reason, level, sourceURL, lineNumber);
}

void WorkerThreadableWebSocketChannel::Bridge::fail(const String& reason, MessageLevel level, const String& sourceURL, unsigned lineNumber)
{
    if (!m_peer)
        return;
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadFail, AllowCrossThreadAccess(m_peer), reason, level, sourceURL, lineNumber));
}

void WorkerThreadableWebSocketChannel::mainThreadDestroy(ExecutionContext* context, PassOwnPtr<Peer> peer)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT_UNUSED(peer, peer);

    // Peer object will be deleted even if the task does not run in the main thread's cleanup period, because
    // the destructor for the task object (created by createCallbackTask()) will automatically delete the peer.
}

void WorkerThreadableWebSocketChannel::Bridge::disconnect()
{
    clearClientWrapper();
    if (m_peer) {
        OwnPtr<Peer> peer = adoptPtr(m_peer);
        m_peer = 0;
        m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadDestroy, peer.release()));
    }
    m_workerGlobalScope = 0;
}

void WorkerThreadableWebSocketChannel::mainThreadSuspend(ExecutionContext* context, Peer* peer)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->suspend();
}

void WorkerThreadableWebSocketChannel::Bridge::suspend()
{
    if (!m_peer)
        return;
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadSuspend, AllowCrossThreadAccess(m_peer)));
}

void WorkerThreadableWebSocketChannel::mainThreadResume(ExecutionContext* context, Peer* peer)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    ASSERT(peer);

    peer->resume();
}

void WorkerThreadableWebSocketChannel::Bridge::resume()
{
    if (!m_peer)
        return;
    m_loaderProxy.postTaskToLoader(createCallbackTask(&WorkerThreadableWebSocketChannel::mainThreadResume, AllowCrossThreadAccess(m_peer)));
}

void WorkerThreadableWebSocketChannel::Bridge::clearClientWrapper()
{
    m_workerClientWrapper->clearClient();
}

void WorkerThreadableWebSocketChannel::Bridge::setMethodNotCompleted()
{
    ASSERT(m_workerClientWrapper);
    m_workerClientWrapper->clearSyncMethodDone();
}

// Caller of this function should hold a reference to the bridge, because this function may call WebSocket::didClose() in the end,
// which causes the bridge to get disconnected from the WebSocket and deleted if there is no other reference.
void WorkerThreadableWebSocketChannel::Bridge::waitForMethodCompletion()
{
    if (!m_workerGlobalScope)
        return;
    WorkerRunLoop& runLoop = m_workerGlobalScope->thread()->runLoop();
    MessageQueueWaitResult result = MessageQueueMessageReceived;
    ThreadableWebSocketChannelClientWrapper* clientWrapper = m_workerClientWrapper.get();
    while (m_workerGlobalScope && clientWrapper && !clientWrapper->syncMethodDone() && result != MessageQueueTerminated) {
        result = runLoop.runInMode(m_workerGlobalScope.get(), m_taskMode); // May cause this bridge to get disconnected, which makes m_workerGlobalScope become null.
        clientWrapper = m_workerClientWrapper.get();
    }
}

} // namespace WebCore
