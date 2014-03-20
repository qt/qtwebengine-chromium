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

#include "config.h"
#include "bindings/v8/V8HiddenPropertyName.h"

#include "bindings/v8/V8Binding.h"
#include <string.h>
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace WebCore {

#define V8_AS_STRING(x) V8_AS_STRING_IMPL(x)
#define V8_AS_STRING_IMPL(x) #x

#define V8_HIDDEN_PROPERTY_PREFIX "WebCore::HiddenProperty::"

#define V8_DEFINE_HIDDEN_PROPERTY(name) \
v8::Handle<v8::String> V8HiddenPropertyName::name(v8::Isolate* isolate) \
{ \
    V8HiddenPropertyName* hiddenPropertyName = V8PerIsolateData::from(isolate)->hiddenPropertyName(); \
    if (hiddenPropertyName->m_##name.IsEmpty()) { \
        createString(V8_HIDDEN_PROPERTY_PREFIX V8_AS_STRING(name), &(hiddenPropertyName->m_##name), isolate); \
    } \
    return v8::Local<v8::String>::New(isolate, hiddenPropertyName->m_##name); \
}

V8_HIDDEN_PROPERTIES(V8_DEFINE_HIDDEN_PROPERTY);

static v8::Handle<v8::String> hiddenReferenceName(const char* name, unsigned length)
{
    ASSERT(length);
    Vector<char, 64> prefixedName;
    prefixedName.append(V8_HIDDEN_PROPERTY_PREFIX, sizeof(V8_HIDDEN_PROPERTY_PREFIX) - 1);
    prefixedName.append(name, length);
    return v8AtomicString(v8::Isolate::GetCurrent(), prefixedName.data(), static_cast<int>(prefixedName.size()));
}

void V8HiddenPropertyName::setNamedHiddenReference(v8::Handle<v8::Object> parent, const char* name, v8::Handle<v8::Value> child)
{
    ASSERT(name);
    parent->SetHiddenValue(hiddenReferenceName(name, strlen(name)), child);
}

void V8HiddenPropertyName::createString(const char* key, v8::Persistent<v8::String>* handle, v8::Isolate* isolate)
{
    v8::HandleScope scope(isolate);
    handle->Reset(isolate, v8AtomicString(isolate, key));
}

}  // namespace WebCore
