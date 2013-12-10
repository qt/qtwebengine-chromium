/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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

#include "config.h"
#include "modules/filesystem/WorkerGlobalScopeFileSystem.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/FileError.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/DOMFileSystemSync.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileEntrySync.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/FileSystemType.h"
#include "modules/filesystem/SyncCallbackHelper.h"
#include "modules/filesystem/WorkerLocalFileSystem.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

void WorkerGlobalScopeFileSystem::webkitRequestFileSystem(WorkerGlobalScope* worker, int type, long long size, PassRefPtr<FileSystemCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ScriptExecutionContext* secureContext = worker->scriptExecutionContext();
    if (!secureContext->securityOrigin()->canAccessFileSystem()) {
        DOMFileSystem::scheduleCallback(worker, errorCallback, FileError::create(FileError::SECURITY_ERR));
        return;
    }

    FileSystemType fileSystemType = static_cast<FileSystemType>(type);
    if (!DOMFileSystemBase::isValidType(fileSystemType)) {
        DOMFileSystem::scheduleCallback(worker, errorCallback, FileError::create(FileError::INVALID_MODIFICATION_ERR));
        return;
    }

    WorkerLocalFileSystem::from(worker)->requestFileSystem(worker, fileSystemType, size, FileSystemCallbacks::create(successCallback, errorCallback, worker, fileSystemType));
}

PassRefPtr<DOMFileSystemSync> WorkerGlobalScopeFileSystem::webkitRequestFileSystemSync(WorkerGlobalScope* worker, int type, long long size, ExceptionState& es)
{
    ScriptExecutionContext* secureContext = worker->scriptExecutionContext();
    if (!secureContext->securityOrigin()->canAccessFileSystem()) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("webkitRequestFileSystemSync", "WorkerGlobalScopeFileSystem", FileError::securityErrorMessage));
        return 0;
    }

    FileSystemType fileSystemType = static_cast<FileSystemType>(type);
    if (!DOMFileSystemBase::isValidType(fileSystemType)) {
        es.throwDOMException(InvalidModificationError, ExceptionMessages::failedToExecute("webkitRequestFileSystemSync", "WorkerGlobalScopeFileSystem", "the type must be TEMPORARY or PERSISTENT."));
        return 0;
    }

    FileSystemSyncCallbackHelper helper;
    OwnPtr<AsyncFileSystemCallbacks> callbacks = FileSystemCallbacks::create(helper.successCallback(), helper.errorCallback(), worker, fileSystemType);
    callbacks->setShouldBlockUntilCompletion(true);

    WorkerLocalFileSystem::from(worker)->requestFileSystem(worker, fileSystemType, size, callbacks.release());
    return helper.getResult(es);
}

void WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemURL(WorkerGlobalScope* worker, const String& url, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    KURL completedURL = worker->completeURL(url);
    ScriptExecutionContext* secureContext = worker->scriptExecutionContext();
    if (!secureContext->securityOrigin()->canAccessFileSystem() || !secureContext->securityOrigin()->canRequest(completedURL)) {
        DOMFileSystem::scheduleCallback(worker, errorCallback, FileError::create(FileError::SECURITY_ERR));
        return;
    }

    FileSystemType type;
    String filePath;
    if (!completedURL.isValid() || !DOMFileSystemBase::crackFileSystemURL(completedURL, type, filePath)) {
        DOMFileSystem::scheduleCallback(worker, errorCallback, FileError::create(FileError::ENCODING_ERR));
        return;
    }

    WorkerLocalFileSystem::from(worker)->readFileSystem(worker, type, ResolveURICallbacks::create(successCallback, errorCallback, worker, type, filePath));
}

PassRefPtr<EntrySync> WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemSyncURL(WorkerGlobalScope* worker, const String& url, ExceptionState& es)
{
    KURL completedURL = worker->completeURL(url);
    ScriptExecutionContext* secureContext = worker->scriptExecutionContext();
    if (!secureContext->securityOrigin()->canAccessFileSystem() || !secureContext->securityOrigin()->canRequest(completedURL)) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("webkitResolveLocalFileSystemSyncURL", "WorkerGlobalScopeFileSystem", FileError::securityErrorMessage));
        return 0;
    }

    FileSystemType type;
    String filePath;
    if (!completedURL.isValid() || !DOMFileSystemBase::crackFileSystemURL(completedURL, type, filePath)) {
        es.throwDOMException(EncodingError, ExceptionMessages::failedToExecute("webkitResolveLocalFileSystemSyncURL", "WorkerGlobalScopeFileSystem", "the URL '" + url + "' is invalid."));
        return 0;
    }

    FileSystemSyncCallbackHelper readFileSystemHelper;
    OwnPtr<AsyncFileSystemCallbacks> callbacks = FileSystemCallbacks::create(readFileSystemHelper.successCallback(), readFileSystemHelper.errorCallback(), worker, type);
    callbacks->setShouldBlockUntilCompletion(true);

    WorkerLocalFileSystem::from(worker)->readFileSystem(worker, type, callbacks.release());
    RefPtr<DOMFileSystemSync> fileSystem = readFileSystemHelper.getResult(es);
    if (!fileSystem)
        return 0;

    RefPtr<EntrySync> entry = fileSystem->root()->getDirectory(filePath, Dictionary(), es);
    if (es.code() == TypeMismatchError) {
        es.clearException();
        return fileSystem->root()->getFile(filePath, Dictionary(), es);
    }

    return entry.release();
}

COMPILE_ASSERT(static_cast<int>(WorkerGlobalScopeFileSystem::TEMPORARY) == static_cast<int>(FileSystemTypeTemporary), enum_mismatch);
COMPILE_ASSERT(static_cast<int>(WorkerGlobalScopeFileSystem::PERSISTENT) == static_cast<int>(FileSystemTypePersistent), enum_mismatch);

} // namespace WebCore
