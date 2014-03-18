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

#ifndef CallbackPromiseAdapter_h
#define CallbackPromiseAdapter_h

#include "bindings/v8/DOMRequestState.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "public/platform/WebCallbacks.h"

namespace WebCore {

// This class provides an easy way to convert from a Script-exposed
// class (i.e. a class that has a toV8() overload) that uses Promises
// to a WebKit API class that uses WebCallbacks. You can define
// seperate Success and Error classes, but this example just uses one
// object for both.
//
// To use:
//
// class MyClass ... {
//    typedef blink::WebMyClass WebType;
//    static PassRefPtr<MyClass> from(blink::WebMyClass* webInstance) {
//        // convert/create as appropriate, but often it's just:
//        return MyClass::create(adoptPtr(webInstance));
//    }
//
// Now when calling into a WebKit API that requires a WebCallbacks<blink::WebMyClass, blink::WebMyClass>*:
//
//    // call signature: callSomeMethod(WebCallbacks<MyClass, MyClass>* callbacks)
//    webObject->callSomeMethod(new CallbackPromiseAdapter<MyClass, MyClass>(resolver, scriptExecutionContext));
//
// Note that this class does not manage its own lifetime. In this
// example that ownership of the WebCallbacks instance is being passed
// in and it is up to the callee to free the WebCallbacks instace.
template<typename S, typename T>
class CallbackPromiseAdapter : public blink::WebCallbacks<typename S::WebType, typename T::WebType> {
public:
    explicit CallbackPromiseAdapter(PassRefPtr<ScriptPromiseResolver> resolver, ExecutionContext* context)
        : m_resolver(resolver)
        , m_requestState(context)
    {
    }
    virtual ~CallbackPromiseAdapter() { }

    virtual void onSuccess(typename S::WebType* result) OVERRIDE
    {
        DOMRequestState::Scope scope(m_requestState);
        m_resolver->resolve(S::from(result));
    }
    void onError(typename T::WebType* error) OVERRIDE
    {
        DOMRequestState::Scope scope(m_requestState);
        m_resolver->reject(T::from(error));
    }
private:
    RefPtr<ScriptPromiseResolver> m_resolver;
    DOMRequestState m_requestState;
    WTF_MAKE_NONCOPYABLE(CallbackPromiseAdapter);
};

} // namespace WebCore

#endif
