/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef NavigatorServiceWorker_h
#define NavigatorServiceWorker_h

#include "bindings/v8/ScriptPromise.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"

namespace blink {
class WebServiceWorkerProvider;
class WebServiceWorkerProviderClient;
}

namespace WebCore {

class ExceptionState;
class Navigator;

class NavigatorServiceWorker : public Supplement<Navigator>, DOMWindowProperty {
public:
    virtual ~NavigatorServiceWorker();
    static NavigatorServiceWorker* from(Navigator*);
    static NavigatorServiceWorker* toNavigatorServiceWorker(Navigator* navigator) { return static_cast<NavigatorServiceWorker*>(Supplement<Navigator>::from(navigator, supplementName())); }

    static ScriptPromise registerServiceWorker(ExecutionContext*, Navigator*, const String& pattern, const String& src, ExceptionState&);
    static ScriptPromise unregisterServiceWorker(ExecutionContext*, Navigator*, const String& pattern, ExceptionState&);

private:
    ScriptPromise registerServiceWorker(ExecutionContext*, const String& pattern, const String& src, ExceptionState&);
    ScriptPromise unregisterServiceWorker(ExecutionContext*, const String& pattern, ExceptionState&);

    explicit NavigatorServiceWorker(Navigator*);

    virtual void willDetachGlobalObjectFromFrame() OVERRIDE;

    blink::WebServiceWorkerProvider* ensureProvider();

    static const char* supplementName();

    Navigator* m_navigator;
    OwnPtr<blink::WebServiceWorkerProvider> m_provider;
};

} // namespace WebCore

#endif // NavigatorServiceWorker_h
