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

#ifndef WorkerRunLoop_h
#define WorkerRunLoop_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "wtf/Functional.h"
#include "wtf/MessageQueue.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

    class ModePredicate;
    class WorkerGlobalScope;
    class WorkerSharedTimer;

    class WorkerRunLoop {
    public:
        WorkerRunLoop();
        ~WorkerRunLoop();

        // Blocking call. Waits for tasks and timers, invokes the callbacks.
        void run(WorkerGlobalScope*);

        enum WaitMode { WaitForMessage, DontWaitForMessage };

        // Waits for a single task and returns.
        MessageQueueWaitResult runInMode(WorkerGlobalScope*, const String& mode, WaitMode = WaitForMessage);

        void terminate();
        bool terminated() const { return m_messageQueue.killed(); }

        // Returns true if the loop is still alive, false if it has been terminated.
        bool postTask(PassOwnPtr<ExecutionContextTask>);
        bool postTask(const Closure&);

        void postTaskAndTerminate(PassOwnPtr<ExecutionContextTask>);

        // Returns true if the loop is still alive, false if it has been terminated.
        bool postTaskForMode(PassOwnPtr<ExecutionContextTask>, const String& mode);
        bool postTaskForMode(const Closure&, const String& mode);

        unsigned long createUniqueId() { return ++m_uniqueId; }

        static String defaultMode();

        class Task;

    private:
        friend class RunLoopSetup;
        MessageQueueWaitResult runInMode(WorkerGlobalScope*, const ModePredicate&, WaitMode);

        // Runs any clean up tasks that are currently in the queue and returns.
        // This should only be called when the context is closed or loop has been terminated.
        void runCleanupTasks(WorkerGlobalScope*);

        MessageQueue<Task> m_messageQueue;
        OwnPtr<WorkerSharedTimer> m_sharedTimer;
        int m_nestedCount;
        unsigned long m_uniqueId;
    };

} // namespace WebCore

#endif // WorkerRunLoop_h
