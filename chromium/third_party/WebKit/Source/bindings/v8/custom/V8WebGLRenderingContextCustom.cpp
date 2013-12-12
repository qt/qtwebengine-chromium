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
#include "V8WebGLRenderingContext.h"

#include "V8ANGLEInstancedArrays.h"
#include "V8EXTFragDepth.h"
#include "V8EXTTextureFilterAnisotropic.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLImageElement.h"
#include "V8HTMLVideoElement.h"
#include "V8ImageData.h"
#include "V8OESElementIndexUint.h"
#include "V8OESStandardDerivatives.h"
#include "V8OESTextureFloat.h"
#include "V8OESTextureFloatLinear.h"
#include "V8OESTextureHalfFloat.h"
#include "V8OESTextureHalfFloatLinear.h"
#include "V8OESVertexArrayObject.h"
#include "V8WebGLBuffer.h"
#include "V8WebGLCompressedTextureATC.h"
#include "V8WebGLCompressedTexturePVRTC.h"
#include "V8WebGLCompressedTextureS3TC.h"
#include "V8WebGLDebugRendererInfo.h"
#include "V8WebGLDebugShaders.h"
#include "V8WebGLDepthTexture.h"
#include "V8WebGLDrawBuffers.h"
#include "V8WebGLFramebuffer.h"
#include "V8WebGLLoseContext.h"
#include "V8WebGLProgram.h"
#include "V8WebGLRenderbuffer.h"
#include "V8WebGLShader.h"
#include "V8WebGLTexture.h"
#include "V8WebGLUniformLocation.h"
#include "V8WebGLVertexArrayObjectOES.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "bindings/v8/custom/V8Float32ArrayCustom.h"
#include "bindings/v8/custom/V8Int16ArrayCustom.h"
#include "bindings/v8/custom/V8Int32ArrayCustom.h"
#include "bindings/v8/custom/V8Int8ArrayCustom.h"
#include "bindings/v8/custom/V8Uint16ArrayCustom.h"
#include "bindings/v8/custom/V8Uint32ArrayCustom.h"
#include "bindings/v8/custom/V8Uint8ArrayCustom.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/canvas/WebGLRenderingContext.h"
#include "core/platform/NotImplemented.h"
#include "wtf/FastMalloc.h"
#include <limits>

namespace WebCore {

// Allocates new storage via fastMalloc.
// Returns NULL if array failed to convert for any reason.
static float* jsArrayToFloatArray(v8::Handle<v8::Array> array, uint32_t len)
{
    // Convert the data element-by-element.
    if (len > std::numeric_limits<uint32_t>::max() / sizeof(float))
        return 0;
    float* data = static_cast<float*>(fastMalloc(len * sizeof(float)));

    for (uint32_t i = 0; i < len; i++) {
        v8::Local<v8::Value> val = array->Get(i);
        if (!val->IsNumber()) {
            fastFree(data);
            return 0;
        }
        data[i] = toFloat(val);
    }
    return data;
}

// Allocates new storage via fastMalloc.
// Returns NULL if array failed to convert for any reason.
static int* jsArrayToIntArray(v8::Handle<v8::Array> array, uint32_t len)
{
    // Convert the data element-by-element.
    if (len > std::numeric_limits<uint32_t>::max() / sizeof(int))
        return 0;
    int* data = static_cast<int*>(fastMalloc(len * sizeof(int)));

    for (uint32_t i = 0; i < len; i++) {
        v8::Local<v8::Value> val = array->Get(i);
        bool ok;
        int ival = toInt32(val, ok);
        if (!ok) {
            fastFree(data);
            return 0;
        }
        data[i] = ival;
    }
    return data;
}

static v8::Handle<v8::Value> toV8Object(const WebGLGetInfo& info, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return v8::Boolean::New(info.getBool());
    case WebGLGetInfo::kTypeBoolArray: {
        const Vector<bool>& value = info.getBoolArray();
        v8::Local<v8::Array> array = v8::Array::New(value.size());
        for (size_t ii = 0; ii < value.size(); ++ii)
            array->Set(v8::Integer::New(ii, isolate), v8::Boolean::New(value[ii]));
        return array;
    }
    case WebGLGetInfo::kTypeFloat:
        return v8::Number::New(isolate, info.getFloat());
    case WebGLGetInfo::kTypeInt:
        return v8::Integer::New(info.getInt(), isolate);
    case WebGLGetInfo::kTypeNull:
        return v8::Null(isolate);
    case WebGLGetInfo::kTypeString:
        return v8String(info.getString(), isolate);
    case WebGLGetInfo::kTypeUnsignedInt:
        return v8::Integer::NewFromUnsigned(info.getUnsignedInt(), isolate);
    case WebGLGetInfo::kTypeWebGLBuffer:
        return toV8(info.getWebGLBuffer(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLFloatArray:
        return toV8(info.getWebGLFloatArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLFramebuffer:
        return toV8(info.getWebGLFramebuffer(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLIntArray:
        return toV8(info.getWebGLIntArray(), creationContext, isolate);
    // FIXME: implement WebGLObjectArray
    // case WebGLGetInfo::kTypeWebGLObjectArray:
    case WebGLGetInfo::kTypeWebGLProgram:
        return toV8(info.getWebGLProgram(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLRenderbuffer:
        return toV8(info.getWebGLRenderbuffer(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLTexture:
        return toV8(info.getWebGLTexture(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLUnsignedByteArray:
        return toV8(info.getWebGLUnsignedByteArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLUnsignedIntArray:
        return toV8(info.getWebGLUnsignedIntArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLVertexArrayObjectOES:
        return toV8(info.getWebGLVertexArrayObjectOES(), creationContext, isolate);
    default:
        notImplemented();
        return v8::Undefined(isolate);
    }
}

static v8::Handle<v8::Value> toV8Object(WebGLExtension* extension, v8::Handle<v8::Object> contextObject, v8::Isolate* isolate)
{
    if (!extension)
        return v8::Null(isolate);
    v8::Handle<v8::Value> extensionObject;
    const char* referenceName = 0;
    switch (extension->name()) {
    case WebGLExtension::ANGLEInstancedArraysName:
        extensionObject = toV8(static_cast<ANGLEInstancedArrays*>(extension), contextObject, isolate);
        referenceName = "angleInstancedArraysName";
        break;
    case WebGLExtension::EXTFragDepthName:
        extensionObject = toV8(static_cast<EXTFragDepth*>(extension), contextObject, isolate);
        referenceName = "extFragDepthName";
        break;
    case WebGLExtension::EXTTextureFilterAnisotropicName:
        extensionObject = toV8(static_cast<EXTTextureFilterAnisotropic*>(extension), contextObject, isolate);
        referenceName = "extTextureFilterAnisotropicName";
        break;
    case WebGLExtension::OESElementIndexUintName:
        extensionObject = toV8(static_cast<OESElementIndexUint*>(extension), contextObject, isolate);
        referenceName = "oesElementIndexUintName";
        break;
    case WebGLExtension::OESStandardDerivativesName:
        extensionObject = toV8(static_cast<OESStandardDerivatives*>(extension), contextObject, isolate);
        referenceName = "oesStandardDerivativesName";
        break;
    case WebGLExtension::OESTextureFloatName:
        extensionObject = toV8(static_cast<OESTextureFloat*>(extension), contextObject, isolate);
        referenceName = "oesTextureFloatName";
        break;
    case WebGLExtension::OESTextureFloatLinearName:
        extensionObject = toV8(static_cast<OESTextureFloatLinear*>(extension), contextObject, isolate);
        referenceName = "oesTextureFloatLinearName";
        break;
    case WebGLExtension::OESTextureHalfFloatName:
        extensionObject = toV8(static_cast<OESTextureHalfFloat*>(extension), contextObject, isolate);
        referenceName = "oesTextureHalfFloatName";
        break;
    case WebGLExtension::OESTextureHalfFloatLinearName:
        extensionObject = toV8(static_cast<OESTextureHalfFloatLinear*>(extension), contextObject, isolate);
        referenceName = "oesTextureHalfFloatLinearName";
        break;
    case WebGLExtension::OESVertexArrayObjectName:
        extensionObject = toV8(static_cast<OESVertexArrayObject*>(extension), contextObject, isolate);
        referenceName = "oesVertexArrayObjectName";
        break;
    case WebGLExtension::WebGLCompressedTextureATCName:
        extensionObject = toV8(static_cast<WebGLCompressedTextureATC*>(extension), contextObject, isolate);
        referenceName = "webGLCompressedTextureATCName";
    case WebGLExtension::WebGLCompressedTexturePVRTCName:
        extensionObject = toV8(static_cast<WebGLCompressedTexturePVRTC*>(extension), contextObject, isolate);
        referenceName = "webGLCompressedTexturePVRTCName";
        break;
    case WebGLExtension::WebGLCompressedTextureS3TCName:
        extensionObject = toV8(static_cast<WebGLCompressedTextureS3TC*>(extension), contextObject, isolate);
        referenceName = "webGLCompressedTextureS3TCName";
        break;
    case WebGLExtension::WebGLDebugRendererInfoName:
        extensionObject = toV8(static_cast<WebGLDebugRendererInfo*>(extension), contextObject, isolate);
        referenceName = "webGLDebugRendererInfoName";
        break;
    case WebGLExtension::WebGLDebugShadersName:
        extensionObject = toV8(static_cast<WebGLDebugShaders*>(extension), contextObject, isolate);
        referenceName = "webGLDebugShadersName";
        break;
    case WebGLExtension::WebGLDepthTextureName:
        extensionObject = toV8(static_cast<WebGLDepthTexture*>(extension), contextObject, isolate);
        referenceName = "webGLDepthTextureName";
        break;
    case WebGLExtension::WebGLDrawBuffersName:
        extensionObject = toV8(static_cast<WebGLDrawBuffers*>(extension), contextObject, isolate);
        referenceName = "webGLDrawBuffersName";
        break;
    case WebGLExtension::WebGLLoseContextName:
        extensionObject = toV8(static_cast<WebGLLoseContext*>(extension), contextObject, isolate);
        referenceName = "webGLLoseContextName";
        break;
    }
    ASSERT(!extensionObject.IsEmpty());
    V8HiddenPropertyName::setNamedHiddenReference(contextObject, referenceName, extensionObject);
    return extensionObject;
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static void getObjectParameter(const v8::FunctionCallbackInfo<v8::Value>& args, ObjectType objectType)
{
    if (args.Length() != 2) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    unsigned target = toInt32(args[0]);
    unsigned pname = toInt32(args[1]);
    WebGLGetInfo info;
    switch (objectType) {
    case kBuffer:
        info = context->getBufferParameter(target, pname);
        break;
    case kRenderbuffer:
        info = context->getRenderbufferParameter(target, pname);
        break;
    case kTexture:
        info = context->getTexParameter(target, pname);
        break;
    case kVertexAttrib:
        // target => index
        info = context->getVertexAttrib(target, pname);
        break;
    default:
        notImplemented();
        break;
    }
    v8SetReturnValue(args, toV8Object(info, args.Holder(), args.GetIsolate()));
}

static WebGLUniformLocation* toWebGLUniformLocation(v8::Handle<v8::Value> value, bool& ok, v8::Isolate* isolate)
{
    ok = false;
    WebGLUniformLocation* location = 0;
    if (V8WebGLUniformLocation::HasInstance(value, isolate, worldType(isolate))) {
        location = V8WebGLUniformLocation::toNative(value->ToObject());
        ok = true;
    }
    return location;
}

enum WhichProgramCall {
    kProgramParameter, kUniform
};

void V8WebGLRenderingContext::getAttachedShadersMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLProgram::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Vector<RefPtr<WebGLShader> > shaders;
    bool succeed = context->getAttachedShaders(program, shaders);
    if (!succeed) {
        v8SetReturnValueNull(args);
        return;
    }
    v8::Local<v8::Array> array = v8::Array::New(shaders.size());
    for (size_t ii = 0; ii < shaders.size(); ++ii)
        array->Set(v8::Integer::New(ii, args.GetIsolate()), toV8(shaders[ii].get(), args.Holder(), args.GetIsolate()));
    v8SetReturnValue(args, array);
}

void V8WebGLRenderingContext::getBufferParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    getObjectParameter(args, kBuffer);
}

void V8WebGLRenderingContext::getExtensionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    WebGLRenderingContext* imp = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() < 1) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, name, args[0]);
    RefPtr<WebGLExtension> extension(imp->getExtension(name));
    v8SetReturnValue(args, toV8Object(extension.get(), args.Holder(), args.GetIsolate()));
}

void V8WebGLRenderingContext::getFramebufferAttachmentParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() != 3) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    unsigned target = toInt32(args[0]);
    unsigned attachment = toInt32(args[1]);
    unsigned pname = toInt32(args[2]);
    WebGLGetInfo info = context->getFramebufferAttachmentParameter(target, attachment, pname);
    v8SetReturnValue(args, toV8Object(info, args.Holder(), args.GetIsolate()));
}

void V8WebGLRenderingContext::getParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() != 1) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    unsigned pname = toInt32(args[0]);
    WebGLGetInfo info = context->getParameter(pname);
    v8SetReturnValue(args, toV8Object(info, args.Holder(), args.GetIsolate()));
}

void V8WebGLRenderingContext::getProgramParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() != 2) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLProgram::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    unsigned pname = toInt32(args[1]);
    WebGLGetInfo info = context->getProgramParameter(program, pname);
    v8SetReturnValue(args, toV8Object(info, args.Holder(), args.GetIsolate()));
}

void V8WebGLRenderingContext::getRenderbufferParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    getObjectParameter(args, kRenderbuffer);
}

void V8WebGLRenderingContext::getShaderParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() != 2) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLShader::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    WebGLShader* shader = V8WebGLShader::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8WebGLShader::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    unsigned pname = toInt32(args[1]);
    WebGLGetInfo info = context->getShaderParameter(shader, pname);
    v8SetReturnValue(args, toV8Object(info, args.Holder(), args.GetIsolate()));
}

void V8WebGLRenderingContext::getSupportedExtensionsMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    WebGLRenderingContext* imp = V8WebGLRenderingContext::toNative(args.Holder());
    if (imp->isContextLost()) {
        v8SetReturnValueNull(args);
        return;
    }

    Vector<String> value = imp->getSupportedExtensions();
    v8::Local<v8::Array> array = v8::Array::New(value.size());
    for (size_t ii = 0; ii < value.size(); ++ii)
        array->Set(v8::Integer::New(ii, args.GetIsolate()), v8String(value[ii], args.GetIsolate()));
    v8SetReturnValue(args, array);
}

void V8WebGLRenderingContext::getTexParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    getObjectParameter(args, kTexture);
}

void V8WebGLRenderingContext::getUniformMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() != 2) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLProgram::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;

    if (args.Length() > 1 && !isUndefinedOrNull(args[1]) && !V8WebGLUniformLocation::HasInstance(args[1], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[1], ok, args.GetIsolate());

    WebGLGetInfo info = context->getUniform(program, location);
    v8SetReturnValue(args, toV8Object(info, args.Holder(), args.GetIsolate()));
}

void V8WebGLRenderingContext::getVertexAttribMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    getObjectParameter(args, kVertexAttrib);
}

enum FunctionToCall {
    kUniform1v, kUniform2v, kUniform3v, kUniform4v,
    kVertexAttrib1v, kVertexAttrib2v, kVertexAttrib3v, kVertexAttrib4v
};

bool isFunctionToCallForAttribute(FunctionToCall functionToCall)
{
    switch (functionToCall) {
    case kVertexAttrib1v:
    case kVertexAttrib2v:
    case kVertexAttrib3v:
    case kVertexAttrib4v:
        return true;
    default:
        break;
    }
    return false;
}

static void vertexAttribAndUniformHelperf(const v8::FunctionCallbackInfo<v8::Value>& args, FunctionToCall functionToCall)
{
    // Forms:
    // * glUniform1fv(WebGLUniformLocation location, Array data);
    // * glUniform1fv(WebGLUniformLocation location, Float32Array data);
    // * glUniform2fv(WebGLUniformLocation location, Array data);
    // * glUniform2fv(WebGLUniformLocation location, Float32Array data);
    // * glUniform3fv(WebGLUniformLocation location, Array data);
    // * glUniform3fv(WebGLUniformLocation location, Float32Array data);
    // * glUniform4fv(WebGLUniformLocation location, Array data);
    // * glUniform4fv(WebGLUniformLocation location, Float32Array data);
    // * glVertexAttrib1fv(GLint index, Array data);
    // * glVertexAttrib1fv(GLint index, Float32Array data);
    // * glVertexAttrib2fv(GLint index, Array data);
    // * glVertexAttrib2fv(GLint index, Float32Array data);
    // * glVertexAttrib3fv(GLint index, Array data);
    // * glVertexAttrib3fv(GLint index, Float32Array data);
    // * glVertexAttrib4fv(GLint index, Array data);
    // * glVertexAttrib4fv(GLint index, Float32Array data);

    if (args.Length() != 2) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    bool ok = false;
    int index = -1;
    WebGLUniformLocation* location = 0;

    if (isFunctionToCallForAttribute(functionToCall))
        index = toInt32(args[0]);
    else {
        if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLUniformLocation::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
            throwTypeError(args.GetIsolate());
            return;
        }
        location = toWebGLUniformLocation(args[0], ok, args.GetIsolate());
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());

    if (V8Float32Array::HasInstance(args[1], args.GetIsolate(), worldType(args.GetIsolate()))) {
        Float32Array* array = V8Float32Array::toNative(args[1]->ToObject());
        ASSERT(array != NULL);
        switch (functionToCall) {
        case kUniform1v: context->uniform1fv(location, array); break;
        case kUniform2v: context->uniform2fv(location, array); break;
        case kUniform3v: context->uniform3fv(location, array); break;
        case kUniform4v: context->uniform4fv(location, array); break;
        case kVertexAttrib1v: context->vertexAttrib1fv(index, array); break;
        case kVertexAttrib2v: context->vertexAttrib2fv(index, array); break;
        case kVertexAttrib3v: context->vertexAttrib3fv(index, array); break;
        case kVertexAttrib4v: context->vertexAttrib4fv(index, array); break;
        default: ASSERT_NOT_REACHED(); break;
        }
        return;
    }

    if (args[1].IsEmpty() || !args[1]->IsArray()) {
        throwTypeError(args.GetIsolate());
        return;
    }
    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[1]);
    uint32_t len = array->Length();
    float* data = jsArrayToFloatArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        setDOMException(SyntaxError, args.GetIsolate());
        return;
    }
    switch (functionToCall) {
    case kUniform1v: context->uniform1fv(location, data, len); break;
    case kUniform2v: context->uniform2fv(location, data, len); break;
    case kUniform3v: context->uniform3fv(location, data, len); break;
    case kUniform4v: context->uniform4fv(location, data, len); break;
    case kVertexAttrib1v: context->vertexAttrib1fv(index, data, len); break;
    case kVertexAttrib2v: context->vertexAttrib2fv(index, data, len); break;
    case kVertexAttrib3v: context->vertexAttrib3fv(index, data, len); break;
    case kVertexAttrib4v: context->vertexAttrib4fv(index, data, len); break;
    default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
}

static void uniformHelperi(const v8::FunctionCallbackInfo<v8::Value>& args, FunctionToCall functionToCall)
{
    // Forms:
    // * glUniform1iv(GLUniformLocation location, Array data);
    // * glUniform1iv(GLUniformLocation location, Int32Array data);
    // * glUniform2iv(GLUniformLocation location, Array data);
    // * glUniform2iv(GLUniformLocation location, Int32Array data);
    // * glUniform3iv(GLUniformLocation location, Array data);
    // * glUniform3iv(GLUniformLocation location, Int32Array data);
    // * glUniform4iv(GLUniformLocation location, Array data);
    // * glUniform4iv(GLUniformLocation location, Int32Array data);

    if (args.Length() != 2) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLUniformLocation::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[0], ok, args.GetIsolate());

    if (V8Int32Array::HasInstance(args[1], args.GetIsolate(), worldType(args.GetIsolate()))) {
        Int32Array* array = V8Int32Array::toNative(args[1]->ToObject());
        ASSERT(array != NULL);
        switch (functionToCall) {
        case kUniform1v: context->uniform1iv(location, array); break;
        case kUniform2v: context->uniform2iv(location, array); break;
        case kUniform3v: context->uniform3iv(location, array); break;
        case kUniform4v: context->uniform4iv(location, array); break;
        default: ASSERT_NOT_REACHED(); break;
        }
        return;
    }

    if (args[1].IsEmpty() || !args[1]->IsArray()) {
        throwTypeError(args.GetIsolate());
        return;
    }
    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[1]);
    uint32_t len = array->Length();
    int* data = jsArrayToIntArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        setDOMException(SyntaxError, args.GetIsolate());
        return;
    }
    switch (functionToCall) {
    case kUniform1v: context->uniform1iv(location, data, len); break;
    case kUniform2v: context->uniform2iv(location, data, len); break;
    case kUniform3v: context->uniform3iv(location, data, len); break;
    case kUniform4v: context->uniform4iv(location, data, len); break;
    default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
}

void V8WebGLRenderingContext::uniform1fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kUniform1v);
}

void V8WebGLRenderingContext::uniform1ivMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformHelperi(args, kUniform1v);
}

void V8WebGLRenderingContext::uniform2fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kUniform2v);
}

void V8WebGLRenderingContext::uniform2ivMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformHelperi(args, kUniform2v);
}

void V8WebGLRenderingContext::uniform3fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kUniform3v);
}

void V8WebGLRenderingContext::uniform3ivMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformHelperi(args, kUniform3v);
}

void V8WebGLRenderingContext::uniform4fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kUniform4v);
}

void V8WebGLRenderingContext::uniform4ivMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformHelperi(args, kUniform4v);
}

static void uniformMatrixHelper(const v8::FunctionCallbackInfo<v8::Value>& args, int matrixSize)
{
    // Forms:
    // * glUniformMatrix2fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix2fv(GLint location, GLboolean transpose, Float32Array data);
    // * glUniformMatrix3fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix3fv(GLint location, GLboolean transpose, Float32Array data);
    // * glUniformMatrix4fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix4fv(GLint location, GLboolean transpose, Float32Array data);
    //
    // FIXME: need to change to accept Float32Array as well.
    if (args.Length() != 3) {
        throwNotEnoughArgumentsError(args.GetIsolate());
        return;
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());

    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLUniformLocation::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        throwTypeError(args.GetIsolate());
        return;
    }
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[0], ok, args.GetIsolate());

    bool transpose = args[1]->BooleanValue();
    if (V8Float32Array::HasInstance(args[2], args.GetIsolate(), worldType(args.GetIsolate()))) {
        Float32Array* array = V8Float32Array::toNative(args[2]->ToObject());
        ASSERT(array != NULL);
        switch (matrixSize) {
        case 2: context->uniformMatrix2fv(location, transpose, array); break;
        case 3: context->uniformMatrix3fv(location, transpose, array); break;
        case 4: context->uniformMatrix4fv(location, transpose, array); break;
        default: ASSERT_NOT_REACHED(); break;
        }
        return;
    }

    if (args[2].IsEmpty() || !args[2]->IsArray()) {
        throwTypeError(args.GetIsolate());
        return;
    }
    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[2]);
    uint32_t len = array->Length();
    float* data = jsArrayToFloatArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        setDOMException(SyntaxError, args.GetIsolate());
        return;
    }
    switch (matrixSize) {
    case 2: context->uniformMatrix2fv(location, transpose, data, len); break;
    case 3: context->uniformMatrix3fv(location, transpose, data, len); break;
    case 4: context->uniformMatrix4fv(location, transpose, data, len); break;
    default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
}

void V8WebGLRenderingContext::uniformMatrix2fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformMatrixHelper(args, 2);
}

void V8WebGLRenderingContext::uniformMatrix3fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformMatrixHelper(args, 3);
}

void V8WebGLRenderingContext::uniformMatrix4fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    uniformMatrixHelper(args, 4);
}

void V8WebGLRenderingContext::vertexAttrib1fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kVertexAttrib1v);
}

void V8WebGLRenderingContext::vertexAttrib2fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kVertexAttrib2v);
}

void V8WebGLRenderingContext::vertexAttrib3fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kVertexAttrib3v);
}

void V8WebGLRenderingContext::vertexAttrib4fvMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    vertexAttribAndUniformHelperf(args, kVertexAttrib4v);
}

} // namespace WebCore
