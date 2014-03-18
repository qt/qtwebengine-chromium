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

#ifndef WebSharedWorkerClient_h
#define WebSharedWorkerClient_h

#include "../platform/WebMessagePortChannel.h"

namespace blink {

class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebNotificationPresenter;
class WebSecurityOrigin;
class WebString;
class WebWorker;
class WebWorkerPermissionClientProxy;

// Provides an interface back to the in-page script object for a worker.
// All functions are expected to be called back on the thread that created
// the Worker object, unless noted.
class WebSharedWorkerClient {
public:
    virtual void workerContextClosed() = 0;
    virtual void workerContextDestroyed() = 0;

    // Returns the notification presenter for this worker context. Pointer
    // is owned by the object implementing WebSharedWorkerClient.
    virtual WebNotificationPresenter* notificationPresenter() = 0;

    // Called on the main webkit thread in the worker process during
    // initialization.
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebApplicationCacheHostClient*) = 0;

    // Called on the main webkit thread in the worker process during
    // initialization.
    // WebWorkerPermissionClientProxy should not retain the given
    // WebSecurityOrigin, as the proxy instance is passed to worker thread
    // while WebSecurityOrigin is not thread safe.
    virtual WebWorkerPermissionClientProxy* createWorkerPermissionClientProxy(const WebSecurityOrigin&) { return 0; }

    virtual void dispatchDevToolsMessage(const WebString&) { }
    virtual void saveDevToolsAgentState(const WebString&) { }

protected:
    ~WebSharedWorkerClient() { }
};

} // namespace blink

#endif
