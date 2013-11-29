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
#include "core/workers/WorkerRunLoop.h"

#include "core/dom/ScriptExecutionContext.h"
#include "core/platform/SharedTimer.h"
#include "core/platform/ThreadGlobalData.h"
#include "core/platform/ThreadTimers.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

class WorkerSharedTimer : public SharedTimer {
public:
    WorkerSharedTimer()
        : m_sharedTimerFunction(0)
        , m_nextFireTime(0)
    {
    }

    // SharedTimer interface.
    virtual void setFiredFunction(void (*function)()) { m_sharedTimerFunction = function; }
    virtual void setFireInterval(double interval) { m_nextFireTime = interval + currentTime(); }
    virtual void stop() { m_nextFireTime = 0; }

    bool isActive() { return m_sharedTimerFunction && m_nextFireTime; }
    double fireTime() { return m_nextFireTime; }
    void fire() { m_sharedTimerFunction(); }

private:
    void (*m_sharedTimerFunction)();
    double m_nextFireTime;
};

class ModePredicate {
public:
    ModePredicate(const String& mode)
        : m_mode(mode)
        , m_defaultMode(mode == WorkerRunLoop::defaultMode())
    {
    }

    bool isDefaultMode() const
    {
        return m_defaultMode;
    }

    bool operator()(WorkerRunLoop::Task* task) const
    {
        return m_defaultMode || m_mode == task->mode();
    }

private:
    String m_mode;
    bool m_defaultMode;
};

WorkerRunLoop::WorkerRunLoop()
    : m_sharedTimer(adoptPtr(new WorkerSharedTimer))
    , m_nestedCount(0)
    , m_uniqueId(0)
{
}

WorkerRunLoop::~WorkerRunLoop()
{
    ASSERT(!m_nestedCount);
}

String WorkerRunLoop::defaultMode()
{
    return String();
}

class RunLoopSetup {
    WTF_MAKE_NONCOPYABLE(RunLoopSetup);
public:
    RunLoopSetup(WorkerRunLoop& runLoop)
        : m_runLoop(runLoop)
    {
        if (!m_runLoop.m_nestedCount)
            threadGlobalData().threadTimers().setSharedTimer(m_runLoop.m_sharedTimer.get());
        m_runLoop.m_nestedCount++;
    }

    ~RunLoopSetup()
    {
        m_runLoop.m_nestedCount--;
        if (!m_runLoop.m_nestedCount)
            threadGlobalData().threadTimers().setSharedTimer(0);
    }
private:
    WorkerRunLoop& m_runLoop;
};

void WorkerRunLoop::run(WorkerGlobalScope* context)
{
    RunLoopSetup setup(*this);
    ModePredicate modePredicate(defaultMode());
    MessageQueueWaitResult result;
    do {
        result = runInMode(context, modePredicate, WaitForMessage);
    } while (result != MessageQueueTerminated);
    runCleanupTasks(context);
}

MessageQueueWaitResult WorkerRunLoop::runInMode(WorkerGlobalScope* context, const String& mode, WaitMode waitMode)
{
    RunLoopSetup setup(*this);
    ModePredicate modePredicate(mode);
    MessageQueueWaitResult result = runInMode(context, modePredicate, waitMode);
    return result;
}

MessageQueueWaitResult WorkerRunLoop::runInMode(WorkerGlobalScope* context, const ModePredicate& predicate, WaitMode waitMode)
{
    ASSERT(context);
    ASSERT(context->thread());
    ASSERT(context->thread()->isCurrentThread());

    bool nextTimeoutEventIsIdleWatchdog;
    MessageQueueWaitResult result;
    OwnPtr<WorkerRunLoop::Task> task;
    do {
        double absoluteTime = 0.0;
        nextTimeoutEventIsIdleWatchdog = false;
        if (waitMode == WaitForMessage) {
            absoluteTime = (predicate.isDefaultMode() && m_sharedTimer->isActive()) ? m_sharedTimer->fireTime() : MessageQueue<Task>::infiniteTime();

            // Do a script engine idle notification if the next event is distant enough.
            const double kMinIdleTimespan = 0.3; // seconds
            if (m_messageQueue.isEmpty() && absoluteTime > currentTime() + kMinIdleTimespan) {
                bool hasMoreWork = !context->idleNotification();
                if (hasMoreWork) {
                    // Schedule a watchdog, so if there are no events within a particular time interval
                    // idle notifications won't stop firing.
                    const double kWatchdogInterval = 3; // seconds
                    double nextWatchdogTime = currentTime() + kWatchdogInterval;
                    if (absoluteTime > nextWatchdogTime) {
                        absoluteTime = nextWatchdogTime;
                        nextTimeoutEventIsIdleWatchdog = true;
                    }
                }
            }
        }
        task = m_messageQueue.waitForMessageFilteredWithTimeout(result, predicate, absoluteTime);
    } while (result == MessageQueueTimeout && nextTimeoutEventIsIdleWatchdog);

    // If the context is closing, don't execute any further JavaScript tasks (per section 4.1.1 of the Web Workers spec).
    // However, there may be implementation cleanup tasks in the queue, so keep running through it.

    switch (result) {
    case MessageQueueTerminated:
        break;

    case MessageQueueMessageReceived:
        task->performTask(*this, context);
        break;

    case MessageQueueTimeout:
        if (!context->isClosing())
            m_sharedTimer->fire();
        break;
    }

    return result;
}

void WorkerRunLoop::runCleanupTasks(WorkerGlobalScope* context)
{
    ASSERT(context);
    ASSERT(context->thread());
    ASSERT(context->thread()->isCurrentThread());
    ASSERT(m_messageQueue.killed());

    while (true) {
        OwnPtr<WorkerRunLoop::Task> task = m_messageQueue.tryGetMessageIgnoringKilled();
        if (!task)
            return;
        task->performTask(*this, context);
    }
}

void WorkerRunLoop::terminate()
{
    m_messageQueue.kill();
}

bool WorkerRunLoop::postTask(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    return postTaskForMode(task, defaultMode());
}

void WorkerRunLoop::postTaskAndTerminate(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    m_messageQueue.appendAndKill(Task::create(task, defaultMode().isolatedCopy()));
}

bool WorkerRunLoop::postTaskForMode(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    return m_messageQueue.append(Task::create(task, mode.isolatedCopy()));
}

PassOwnPtr<WorkerRunLoop::Task> WorkerRunLoop::Task::create(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
{
    return adoptPtr(new Task(task, mode));
}

void WorkerRunLoop::Task::performTask(const WorkerRunLoop& runLoop, ScriptExecutionContext* context)
{
    WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
    if ((!workerGlobalScope->isClosing() && !runLoop.terminated()) || m_task->isCleanupTask())
        m_task->performTask(context);
}

WorkerRunLoop::Task::Task(PassOwnPtr<ScriptExecutionContext::Task> task, const String& mode)
    : m_task(task)
    , m_mode(mode.isolatedCopy())
{
}

} // namespace WebCore
