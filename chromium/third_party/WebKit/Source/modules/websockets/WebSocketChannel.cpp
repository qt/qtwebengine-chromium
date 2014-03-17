/*
 * Copyright (C) 2009, 2012 Google Inc.  All rights reserved.
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

#include "modules/websockets/WebSocketChannel.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/frame/Settings.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerRunLoop.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/MainThreadWebSocketChannel.h"
#include "modules/websockets/ThreadableWebSocketChannelClientWrapper.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "modules/websockets/WorkerThreadableWebSocketChannel.h"

namespace WebCore {

static const char webSocketChannelMode[] = "webSocketChannelMode";

PassRefPtr<WebSocketChannel> WebSocketChannel::create(ExecutionContext* context, WebSocketChannelClient* client)
{
    ASSERT(context);
    ASSERT(client);

    String sourceURL;
    unsigned lineNumber = 0;
    RefPtr<ScriptCallStack> callStack = createScriptCallStack(1, true);
    if (callStack && callStack->size()) {
        sourceURL = callStack->at(0).sourceURL();
        lineNumber = callStack->at(0).lineNumber();
    }

    if (context->isWorkerGlobalScope()) {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        WorkerRunLoop& runLoop = workerGlobalScope->thread()->runLoop();
        String mode = webSocketChannelMode;
        mode.append(String::number(runLoop.createUniqueId()));
        return WorkerThreadableWebSocketChannel::create(workerGlobalScope, client, mode, sourceURL, lineNumber);
    }

    Document* document = toDocument(context);
    Settings* settings = document->settings();
    if (settings && settings->experimentalWebSocketEnabled()) {
        // FIXME: Create and return an "experimental" WebSocketChannel instead of a MainThreadWebSocketChannel.
        return MainThreadWebSocketChannel::create(document, client, sourceURL, lineNumber);
    }
    return MainThreadWebSocketChannel::create(document, client, sourceURL, lineNumber);
}

} // namespace WebCore
