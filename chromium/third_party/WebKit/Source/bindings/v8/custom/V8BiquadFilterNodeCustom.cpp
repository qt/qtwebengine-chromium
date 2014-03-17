/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "V8BiquadFilterNode.h"

#include "bindings/v8/V8Binding.h"
#include "modules/webaudio/BiquadFilterNode.h"

namespace WebCore {

void V8BiquadFilterNode::typeAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    BiquadFilterNode* imp = V8BiquadFilterNode::toNative(info.Holder());

    if (value->IsNumber()) {
        bool ok = false;
        uint32_t type = toUInt32(value, ok);
        ASSERT(ok);
        if (!imp->setType(type))
            throwTypeError("Illegal BiquadFilterNode type", info.GetIsolate());
        return;
    }

    if (value->IsString()) {
        String type = toCoreString(value.As<v8::String>());
        if (type == "lowpass" || type == "highpass" || type == "bandpass" || type == "lowshelf" || type == "highshelf" || type == "peaking" || type == "notch" || type == "allpass") {
            imp->setType(type);
            return;
        }
    }

    throwTypeError("Illegal BiquadFilterNode type", info.GetIsolate());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
