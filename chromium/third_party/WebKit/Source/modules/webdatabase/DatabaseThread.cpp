/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/webdatabase/DatabaseThread.h"

#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/SQLTransactionClient.h"
#include "modules/webdatabase/SQLTransactionCoordinator.h"
#include "platform/Logging.h"
#include "public/platform/Platform.h"
#include "wtf/AutodrainedPool.h"

namespace WebCore {

DatabaseThread::DatabaseThread()
    : m_transactionClient(adoptPtr(new SQLTransactionClient()))
    , m_transactionCoordinator(adoptPtr(new SQLTransactionCoordinator()))
    , m_cleanupSync(0)
    , m_terminationRequested(false)
{
}

DatabaseThread::~DatabaseThread()
{
    if (!m_terminationRequested)
        requestTermination(0);
    m_thread.clear();
}

void DatabaseThread::start()
{
    if (m_thread)
        return;
    m_thread = adoptPtr(blink::Platform::current()->createThread("WebCore: Database"));
}

void DatabaseThread::requestTermination(DatabaseTaskSynchronizer *cleanupSync)
{
    ASSERT(!m_terminationRequested);
    m_terminationRequested = true;
    m_cleanupSync = cleanupSync;
    WTF_LOG(StorageAPI, "DatabaseThread %p was asked to terminate\n", this);
    m_thread->postTask(new Task(WTF::bind(&DatabaseThread::cleanupDatabaseThread, this)));
}

bool DatabaseThread::terminationRequested(DatabaseTaskSynchronizer* taskSynchronizer) const
{
#ifndef NDEBUG
    if (taskSynchronizer)
        taskSynchronizer->setHasCheckedForTermination();
#endif

    return m_terminationRequested;
}

void DatabaseThread::cleanupDatabaseThread()
{
    WTF_LOG(StorageAPI, "Cleaning up DatabaseThread %p", this);

    // Clean up the list of all pending transactions on this database thread
    m_transactionCoordinator->shutdown();

    // Close the databases that we ran transactions on. This ensures that if any transactions are still open, they are rolled back and we don't leave the database in an
    // inconsistent or locked state.
    if (m_openDatabaseSet.size() > 0) {
        // As the call to close will modify the original set, we must take a copy to iterate over.
        DatabaseSet openSetCopy;
        openSetCopy.swap(m_openDatabaseSet);
        DatabaseSet::iterator end = openSetCopy.end();
        for (DatabaseSet::iterator it = openSetCopy.begin(); it != end; ++it)
            (*it).get()->close();
    }

    if (m_cleanupSync) // Someone wanted to know when we were done cleaning up.
        m_thread->postTask(new Task(WTF::bind(&DatabaseTaskSynchronizer::taskCompleted, m_cleanupSync)));
}

void DatabaseThread::recordDatabaseOpen(DatabaseBackend* database)
{
    ASSERT(isDatabaseThread());
    ASSERT(database);
    ASSERT(!m_openDatabaseSet.contains(database));
    m_openDatabaseSet.add(database);
}

void DatabaseThread::recordDatabaseClosed(DatabaseBackend* database)
{
    ASSERT(isDatabaseThread());
    ASSERT(database);
    ASSERT(m_terminationRequested || m_openDatabaseSet.contains(database));
    m_openDatabaseSet.remove(database);
}

bool DatabaseThread::isDatabaseOpen(DatabaseBackend* database)
{
    ASSERT(isDatabaseThread());
    ASSERT(database);
    return !m_terminationRequested && m_openDatabaseSet.contains(database);
}

void DatabaseThread::scheduleTask(PassOwnPtr<DatabaseTask> task)
{
    ASSERT(m_thread);
    ASSERT(!task->hasSynchronizer() || task->hasCheckedForTermination());
    // WebThread takes ownership of the task.
    m_thread->postTask(task.leakPtr());
}

} // namespace WebCore
