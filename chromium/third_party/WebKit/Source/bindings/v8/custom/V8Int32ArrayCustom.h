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

#ifndef V8Int32ArrayCustom_h
#define V8Int32ArrayCustom_h

#include "bindings/v8/custom/V8TypedArrayCustom.h"
#include "wtf/Int32Array.h"

namespace WebCore {

template<>
class TypedArrayTraits<Int32Array> {
public:
    typedef v8::Int32Array V8Type;

    static bool IsInstance(v8::Handle<v8::Value> value)
    {
        return value->IsInt32Array();
    }

    static size_t length(v8::Handle<v8::Int32Array> value)
    {
        return value->Length();
    }

    static size_t length(Int32Array* array)
    {
        return array->length();
    }
};

typedef V8TypedArray<Int32Array> V8Int32Array;

template<>
class WrapperTypeTraits<Int32Array> : public TypedArrayWrapperTraits<Int32Array> { };


inline v8::Handle<v8::Object> wrap(Int32Array* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return V8TypedArray<Int32Array>::wrap(impl, creationContext, isolate);
}

inline v8::Handle<v8::Value> toV8(Int32Array* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return V8TypedArray<Int32Array>::toV8(impl, creationContext, isolate);
}

template<class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, Int32Array* impl)
{
    V8TypedArray<Int32Array>::v8SetReturnValue(info, impl);
}

template<class CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& info, Int32Array* impl)
{
    V8TypedArray<Int32Array>::v8SetReturnValueForMainWorld(info, impl);
}

template<class CallbackInfo, class Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& info, Int32Array* impl, Wrappable* wrappable)
{
    V8TypedArray<Int32Array>::v8SetReturnValueFast(info, impl, wrappable);
}

inline v8::Handle<v8::Value> toV8(PassRefPtr< Int32Array > impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

template<class CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, PassRefPtr<Int32Array> impl)
{
    v8SetReturnValue(info, impl.get());
}

template<class CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& info, PassRefPtr<Int32Array> impl)
{
    v8SetReturnValueForMainWorld(info, impl.get());
}

template<class CallbackInfo, class Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& info, PassRefPtr<Int32Array> impl, Wrappable* wrappable)
{
    v8SetReturnValueFast(info, impl.get(), wrappable);
}

} // namespace WebCore

#endif
