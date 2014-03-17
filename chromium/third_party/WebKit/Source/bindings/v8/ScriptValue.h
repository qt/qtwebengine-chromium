/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef ScriptValue_h
#define ScriptValue_h

#include "bindings/v8/SharedPersistent.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace WebCore {

class JSONValue;
class ScriptState;

class ScriptValue {
public:
    ScriptValue()
        : m_isolate(0)
    { }

    virtual ~ScriptValue();

    ScriptValue(v8::Handle<v8::Value> value, v8::Isolate* isolate)
        : m_isolate(isolate)
        , m_value(value.IsEmpty() ? 0 : SharedPersistent<v8::Value>::create(value, isolate))
    {
    }

    ScriptValue(const ScriptValue& value)
        : m_isolate(value.m_isolate)
        , m_value(value.m_value)
    {
    }

    v8::Isolate* isolate() const
    {
        if (!m_isolate)
            m_isolate = v8::Isolate::GetCurrent();
        return m_isolate;
    }

    static ScriptValue createUndefined(v8::Isolate* isolate)
    {
        return ScriptValue(v8::Undefined(isolate), isolate);
    }

    static ScriptValue createNull()
    {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        return ScriptValue(v8::Null(isolate), isolate);
    }
    static ScriptValue createBoolean(bool b)
    {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        return ScriptValue(b ? v8::True(isolate) : v8::False(isolate), isolate);
    }

    ScriptValue& operator=(const ScriptValue& value)
    {
        if (this != &value) {
            m_value = value.m_value;
            m_isolate = value.m_isolate;
        }
        return *this;
    }

    bool operator==(const ScriptValue& value) const
    {
        if (hasNoValue())
            return value.hasNoValue();
        if (value.hasNoValue())
            return false;
        return *m_value == *value.m_value;
    }

    bool isEqual(ScriptState*, const ScriptValue& value) const
    {
        return operator==(value);
    }

    // Note: This creates a new local Handle; not to be used in cases where is
    // is an efficiency problem.
    bool isFunction() const
    {
        ASSERT(!hasNoValue());
        v8::Handle<v8::Value> value = v8Value();
        return !value.IsEmpty() && value->IsFunction();
    }

    bool operator!=(const ScriptValue& value) const
    {
        return !operator==(value);
    }

    // Note: This creates a new local Handle; not to be used in cases where is
    // is an efficiency problem.
    bool isNull() const
    {
        ASSERT(!hasNoValue());
        v8::Handle<v8::Value> value = v8Value();
        return !value.IsEmpty() && value->IsNull();
    }

    // Note: This creates a new local Handle; not to be used in cases where is
    // is an efficiency problem.
    bool isUndefined() const
    {
        ASSERT(!hasNoValue());
        v8::Handle<v8::Value> value = v8Value();
        return !value.IsEmpty() && value->IsUndefined();
    }

    // Note: This creates a new local Handle; not to be used in cases where is
    // is an efficiency problem.
    bool isObject() const
    {
        ASSERT(!hasNoValue());
        v8::Handle<v8::Value> value = v8Value();
        return !value.IsEmpty() && value->IsObject();
    }

    bool hasNoValue() const
    {
        return !m_value.get() || m_value->isEmpty();
    }

    void clear()
    {
        m_value = 0;
    }

    v8::Handle<v8::Value> v8Value() const
    {
        return m_value.get() ? m_value->newLocal(m_isolate) : v8::Handle<v8::Value>();
    }

    bool getString(String& result) const;
    String toString() const;

    PassRefPtr<JSONValue> toJSONValue(ScriptState*) const;

private:
    mutable v8::Isolate* m_isolate;
    RefPtr<SharedPersistent<v8::Value> > m_value;
};

} // namespace WebCore

#endif // ScriptValue_h
