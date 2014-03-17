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
 */

#ifndef DatabaseObserver_h
#define DatabaseObserver_h

#include "wtf/Forward.h"

namespace WebCore {

class DatabaseBackendBase;
class ExecutionContext;

// FIXME: Deprecate this entire class.
// The implementation of this class is in the WebKit API (Chromium source tree)
// in WebKit/chromium/src/DatabaseObserver.cpp.
class DatabaseObserver {
public:
    static void databaseOpened(DatabaseBackendBase*);
    static void databaseModified(DatabaseBackendBase*);
    static void databaseClosed(DatabaseBackendBase*);

    static void reportOpenDatabaseResult(DatabaseBackendBase*, int callsite, int webSqlErrorCode, int sqliteErrorCode);
    static void reportChangeVersionResult(DatabaseBackendBase*, int callsite, int webSqlErrorCode, int sqliteErrorCode);
    static void reportStartTransactionResult(DatabaseBackendBase*, int callsite, int webSqlErrorCode, int sqliteErrorCode);
    static void reportCommitTransactionResult(DatabaseBackendBase*, int callsite, int webSqlErrorCode, int sqliteErrorCode);
    static void reportExecuteStatementResult(DatabaseBackendBase*, int callsite, int webSqlErrorCode, int sqliteErrorCode);
    static void reportVacuumDatabaseResult(DatabaseBackendBase*, int sqliteErrorCode);
};

}

#endif // DatabaseObserver_h
