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
#include "V8CanvasRenderingContext2D.h"

#include "V8CanvasGradient.h"
#include "V8CanvasPattern.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLImageElement.h"
#include "V8HTMLVideoElement.h"
#include "V8ImageData.h"
#include "bindings/v8/V8Binding.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"
#include "core/html/canvas/CanvasStyle.h"
#include "platform/geometry/FloatRect.h"

namespace WebCore {

static v8::Handle<v8::Value> toV8Object(CanvasStyle* style, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (style->canvasGradient())
        return toV8(style->canvasGradient(), creationContext, isolate);

    if (style->canvasPattern())
        return toV8(style->canvasPattern(), creationContext, isolate);

    return v8String(isolate, style->color());
}

static PassRefPtr<CanvasStyle> toCanvasStyle(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (V8CanvasGradient::hasInstance(value, isolate, worldType(isolate)))
        return CanvasStyle::createFromGradient(V8CanvasGradient::toNative(v8::Handle<v8::Object>::Cast(value)));

    if (V8CanvasPattern::hasInstance(value, isolate, worldType(isolate)))
        return CanvasStyle::createFromPattern(V8CanvasPattern::toNative(v8::Handle<v8::Object>::Cast(value)));

    return 0;
}

void V8CanvasRenderingContext2D::strokeStyleAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    CanvasRenderingContext2D* impl = V8CanvasRenderingContext2D::toNative(info.Holder());
    v8SetReturnValue(info, toV8Object(impl->strokeStyle(), info.Holder(), info.GetIsolate()));
}

void V8CanvasRenderingContext2D::strokeStyleAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    CanvasRenderingContext2D* impl = V8CanvasRenderingContext2D::toNative(info.Holder());
    if (value->IsString())
        impl->setStrokeColor(toCoreString(value.As<v8::String>()));
    else
        impl->setStrokeStyle(toCanvasStyle(value, info.GetIsolate()));
}

void V8CanvasRenderingContext2D::fillStyleAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    CanvasRenderingContext2D* impl = V8CanvasRenderingContext2D::toNative(info.Holder());
    v8SetReturnValue(info, toV8Object(impl->fillStyle(), info.Holder(), info.GetIsolate()));
}

void V8CanvasRenderingContext2D::fillStyleAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    CanvasRenderingContext2D* impl = V8CanvasRenderingContext2D::toNative(info.Holder());
    if (value->IsString())
        impl->setFillColor(toCoreString(value.As<v8::String>()));
    else
        impl->setFillStyle(toCanvasStyle(value, info.GetIsolate()));
}

} // namespace WebCore
