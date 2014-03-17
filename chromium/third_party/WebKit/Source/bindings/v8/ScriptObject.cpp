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
#include "bindings/v8/ScriptObject.h"

#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/ScriptState.h"

#include "V8InspectorFrontendHost.h"

#include <v8.h>

namespace WebCore {

ScriptObject::ScriptObject(ScriptState* scriptState, v8::Handle<v8::Object> v8Object)
    : ScriptValue(v8Object, scriptState->isolate())
    , m_scriptState(scriptState)
{
}

ScriptObject::ScriptObject(ScriptState* scriptState, const ScriptValue& scriptValue)
    : ScriptValue(scriptValue)
    , m_scriptState(scriptState)
{
}

v8::Handle<v8::Object> ScriptObject::v8Object() const
{
    ASSERT(v8Value()->IsObject());
    return v8::Handle<v8::Object>::Cast(v8Value());
}

bool ScriptGlobalObject::set(ScriptState* scriptState, const char* name, InspectorFrontendHost* value)
{
    ScriptScope scope(scriptState);
    scope.global()->Set(v8AtomicString(scriptState->isolate(), name), toV8(value, v8::Handle<v8::Object>(), scriptState->isolate()));
    return scope.success();
}

bool ScriptGlobalObject::get(ScriptState* scriptState, const char* name, ScriptObject& value)
{
    ScriptScope scope(scriptState);
    v8::Local<v8::Value> v8Value = scope.global()->Get(v8AtomicString(scriptState->isolate(), name));
    if (v8Value.IsEmpty())
        return false;

    if (!v8Value->IsObject())
        return false;

    value = ScriptObject(scriptState, v8::Handle<v8::Object>::Cast(v8Value));
    return true;
}

} // namespace WebCore
