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
#include "V8SVGLength.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/properties/SVGPropertyTearOff.h"

namespace WebCore {

void V8SVGLength::valueAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    SVGLength& imp = wrapper->propertyReference();
    ExceptionState exceptionState(info.Holder(), info.GetIsolate());
    SVGLengthContext lengthContext(wrapper->contextElement());
    float value = imp.value(lengthContext, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8SetReturnValue(info, value);
}

void V8SVGLength::valueAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    if (wrapper->isReadOnly()) {
        setDOMException(NoModificationAllowedError, info.GetIsolate());
        return;
    }

    if (!isUndefinedOrNull(value) && !value->IsNumber() && !value->IsBoolean()) {
        throwUninformativeAndGenericTypeError(info.GetIsolate());
        return;
    }

    SVGLength& imp = wrapper->propertyReference();
    ExceptionState exceptionState(info.Holder(), info.GetIsolate());
    SVGLengthContext lengthContext(wrapper->contextElement());
    imp.setValue(static_cast<float>(value->NumberValue()), lengthContext, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    wrapper->commitChange();
}

void V8SVGLength::convertToSpecifiedUnitsMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "convertToSpecifiedUnits", "SVGLength", info.Holder(), info.GetIsolate());
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    if (wrapper->isReadOnly()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "The length is read only.");
        exceptionState.throwIfNeeded();
        return;
    }

    if (info.Length() < 1) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(1, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    SVGLength& imp = wrapper->propertyReference();
    V8TRYCATCH_VOID(int, unitType, toUInt32(info[0]));
    SVGLengthContext lengthContext(wrapper->contextElement());
    imp.convertToSpecifiedUnits(unitType, lengthContext, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    wrapper->commitChange();
}

} // namespace WebCore
