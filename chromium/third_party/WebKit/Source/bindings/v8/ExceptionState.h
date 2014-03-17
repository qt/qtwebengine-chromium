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

#ifndef ExceptionState_h
#define ExceptionState_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/V8ThrowException.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace WebCore {

typedef int ExceptionCode;

class ExceptionState {
    WTF_MAKE_NONCOPYABLE(ExceptionState);
public:
    enum Context {
        ConstructionContext,
        ExecutionContext,
        DeletionContext,
        GetterContext,
        SetterContext,
        UnknownContext, // FIXME: Remove this once we've flipped over to the new API.
    };

    explicit ExceptionState(const v8::Handle<v8::Object>& creationContext, v8::Isolate* isolate)
        : m_code(0)
        , m_context(UnknownContext)
        , m_propertyName(0)
        , m_interfaceName(0)
        , m_creationContext(creationContext)
        , m_isolate(isolate) { }

    ExceptionState(Context context, const char* propertyName, const char* interfaceName, const v8::Handle<v8::Object>& creationContext, v8::Isolate* isolate)
        : m_code(0)
        , m_context(context)
        , m_propertyName(propertyName)
        , m_interfaceName(interfaceName)
        , m_creationContext(creationContext)
        , m_isolate(isolate) { }

    ExceptionState(Context context, const char* interfaceName, const v8::Handle<v8::Object>& creationContext, v8::Isolate* isolate)
        : m_code(0)
        , m_context(context)
        , m_propertyName(0)
        , m_interfaceName(interfaceName)
        , m_creationContext(creationContext)
        , m_isolate(isolate) { ASSERT(m_context == ConstructionContext); }

    virtual void throwDOMException(const ExceptionCode&, const String& message);
    virtual void throwTypeError(const String& message);
    virtual void throwSecurityError(const String& sanitizedMessage, const String& unsanitizedMessage = String());

    // Please don't use these methods. Use ::throwDOMException and ::throwTypeError, and pass in a useful exception message.
    virtual void throwUninformativeAndGenericDOMException(const ExceptionCode& ec) { throwDOMException(ec, String()); }
    virtual void throwUninformativeAndGenericTypeError() { throwTypeError(String()); }

    bool hadException() const { return !m_exception.isEmpty() || m_code; }
    void clearException();

    ExceptionCode code() const { return m_code; }

    bool throwIfNeeded()
    {
        if (m_exception.isEmpty()) {
            if (!m_code)
                return false;
            throwUninformativeAndGenericDOMException(m_code);
        }

        V8ThrowException::throwError(m_exception.newLocal(m_isolate), m_isolate);
        return true;
    }

    Context context() const { return m_context; }
    const char* propertyName() const { return m_propertyName; }
    const char* interfaceName() const { return m_interfaceName; }

protected:
    ExceptionCode m_code;
    Context m_context;
    const char* m_propertyName;
    const char* m_interfaceName;

private:
    void setException(v8::Handle<v8::Value>);

    String addExceptionContext(const String&) const;

    ScopedPersistent<v8::Value> m_exception;
    v8::Handle<v8::Object> m_creationContext;
    v8::Isolate* m_isolate;
};

class TrackExceptionState : public ExceptionState {
public:
    TrackExceptionState(): ExceptionState(v8::Handle<v8::Object>(), 0) { }
    virtual void throwDOMException(const ExceptionCode&, const String& message) OVERRIDE FINAL;
    virtual void throwTypeError(const String& message = String()) OVERRIDE FINAL;
    virtual void throwSecurityError(const String& sanitizedMessage, const String& unsanitizedMessage = String()) OVERRIDE FINAL;
};

} // namespace WebCore

#endif // ExceptionState_h
