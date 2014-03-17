/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerMessagingProxy_h
#define WorkerMessagingProxy_h

#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

    class WorkerObjectProxy;
    class DedicatedWorkerThread;
    class ExecutionContext;
    class Worker;
    class WorkerClients;

    class WorkerMessagingProxy : public WorkerGlobalScopeProxy, public WorkerLoaderProxy {
        WTF_MAKE_NONCOPYABLE(WorkerMessagingProxy); WTF_MAKE_FAST_ALLOCATED;
    public:
        WorkerMessagingProxy(Worker*, PassOwnPtr<WorkerClients>);

        // Implementations of WorkerGlobalScopeProxy.
        // (Only use these methods in the worker object thread.)
        virtual void startWorkerGlobalScope(const KURL& scriptURL, const String& userAgent, const String& sourceCode, WorkerThreadStartMode) OVERRIDE;
        virtual void terminateWorkerGlobalScope() OVERRIDE;
        virtual void postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue>, PassOwnPtr<MessagePortChannelArray>) OVERRIDE;
        virtual bool hasPendingActivity() const OVERRIDE;
        virtual void workerObjectDestroyed() OVERRIDE;
        virtual void connectToInspector(WorkerGlobalScopeProxy::PageInspector*) OVERRIDE;
        virtual void disconnectFromInspector() OVERRIDE;
        virtual void sendMessageToInspector(const String&) OVERRIDE;

        // These methods come from worker context thread via WorkerObjectProxy
        // and are called on the worker object thread (e.g. main thread).
        void postMessageToWorkerObject(PassRefPtr<SerializedScriptValue>, PassOwnPtr<MessagePortChannelArray>);
        void reportException(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL);
        void reportConsoleMessage(MessageSource, MessageLevel, const String& message, int lineNumber, const String& sourceURL);
        void postMessageToPageInspector(const String&);
        void confirmMessageFromWorkerObject(bool hasPendingActivity);
        void reportPendingActivity(bool hasPendingActivity);
        void workerGlobalScopeClosed();
        void workerGlobalScopeDestroyed();

        // Implementation of WorkerLoaderProxy.
        // These methods are called on different threads to schedule loading
        // requests and to send callbacks back to WorkerGlobalScope.
        virtual void postTaskToLoader(PassOwnPtr<ExecutionContextTask>) OVERRIDE;
        virtual bool postTaskForModeToWorkerGlobalScope(PassOwnPtr<ExecutionContextTask>, const String& mode) OVERRIDE;

        void workerThreadCreated(PassRefPtr<DedicatedWorkerThread>);

    protected:
        virtual ~WorkerMessagingProxy();

    private:
        static void workerObjectDestroyedInternal(ExecutionContext*, WorkerMessagingProxy*);

        RefPtr<ExecutionContext> m_executionContext;
        OwnPtr<WorkerObjectProxy> m_workerObjectProxy;
        Worker* m_workerObject;
        bool m_mayBeDestroyed;
        RefPtr<DedicatedWorkerThread> m_workerThread;

        unsigned m_unconfirmedMessageCount; // Unconfirmed messages from worker object to worker thread.
        bool m_workerThreadHadPendingActivity; // The latest confirmation from worker thread reported that it was still active.

        bool m_askedToTerminate;

        Vector<OwnPtr<ExecutionContextTask> > m_queuedEarlyTasks; // Tasks are queued here until there's a thread object created.
        WorkerGlobalScopeProxy::PageInspector* m_pageInspector;

        OwnPtr<WorkerClients> m_workerClients;
    };

} // namespace WebCore

#endif // WorkerMessagingProxy_h
