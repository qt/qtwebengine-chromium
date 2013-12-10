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

#ifndef LocalFileSystem_h
#define LocalFileSystem_h

#include "core/page/Page.h"
#include "modules/filesystem/FileSystemType.h"
#include "wtf/Forward.h"

namespace WebCore {

class AsyncFileSystemCallbacks;
class FileSystemClient;
class ScriptExecutionContext;

// Base class of LocalFileSystem and WorkerLocalFileSystem.
class LocalFileSystemBase {
    WTF_MAKE_NONCOPYABLE(LocalFileSystemBase);
public:
    virtual ~LocalFileSystemBase();

    // Does not create the new file system if it doesn't exist, just reads it if available.
    void readFileSystem(ScriptExecutionContext*, FileSystemType, PassOwnPtr<AsyncFileSystemCallbacks>);

    void requestFileSystem(ScriptExecutionContext*, FileSystemType, long long size, PassOwnPtr<AsyncFileSystemCallbacks>);

    void deleteFileSystem(ScriptExecutionContext*, FileSystemType, PassOwnPtr<AsyncFileSystemCallbacks>);

    FileSystemClient* client() { return m_client.get(); }

protected:
    explicit LocalFileSystemBase(PassOwnPtr<FileSystemClient>);

    OwnPtr<FileSystemClient> m_client;
};

class LocalFileSystem : public LocalFileSystemBase, public Supplement<Page> {
public:
    static PassOwnPtr<LocalFileSystem> create(PassOwnPtr<FileSystemClient>);
    static const char* supplementName();
    static LocalFileSystem* from(ScriptExecutionContext*);
    virtual ~LocalFileSystem();

private:
    explicit LocalFileSystem(PassOwnPtr<FileSystemClient>);
};


} // namespace WebCore

#endif // LocalFileSystem_h
