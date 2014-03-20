/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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


#include "core/inspector/InjectedScript.h"

#include "bindings/v8/ScriptFunctionCall.h"
#include "core/inspector/InjectedScriptHost.h"
#include "platform/JSONValues.h"
#include "wtf/text/WTFString.h"

using WebCore::TypeBuilder::Array;
using WebCore::TypeBuilder::Debugger::CallFrame;
using WebCore::TypeBuilder::Runtime::PropertyDescriptor;
using WebCore::TypeBuilder::Runtime::InternalPropertyDescriptor;
using WebCore::TypeBuilder::Debugger::FunctionDetails;
using WebCore::TypeBuilder::Runtime::RemoteObject;

namespace WebCore {

InjectedScript::InjectedScript()
    : InjectedScriptBase("InjectedScript")
{
}

InjectedScript::InjectedScript(ScriptObject injectedScriptObject, InspectedStateAccessCheck accessCheck)
    : InjectedScriptBase("InjectedScript", injectedScriptObject, accessCheck)
{
}

void InjectedScript::evaluate(ErrorString* errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptFunctionCall function(injectedScriptObject(), "evaluate");
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptFunctionCall function(injectedScriptObject(), "callFunctionOn");
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, const ScriptValue& callFrames, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptFunctionCall function(injectedScriptObject(), "evaluateOnCallFrame");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::restartFrame(ErrorString* errorString, const ScriptValue& callFrames, const String& callFrameId, RefPtr<JSONObject>* result)
{
    ScriptFunctionCall function(injectedScriptObject(), "restartFrame");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (resultValue) {
        if (resultValue->type() == JSONValue::TypeString) {
            resultValue->asString(errorString);
            return;
        }
        if (resultValue->type() == JSONValue::TypeObject) {
            *result = resultValue->asObject();
            return;
        }
    }
    *errorString = "Internal error";
}

void InjectedScript::getStepInPositions(ErrorString* errorString, const ScriptValue& callFrames, const String& callFrameId, RefPtr<Array<TypeBuilder::Debugger::Location> >& positions)
{
    ScriptFunctionCall function(injectedScriptObject(), "getStepInPositions");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (resultValue) {
        if (resultValue->type() == JSONValue::TypeString) {
            resultValue->asString(errorString);
            return;
        }
        if (resultValue->type() == JSONValue::TypeArray) {
            positions = Array<TypeBuilder::Debugger::Location>::runtimeCast(resultValue);
            return;
        }
    }
    *errorString = "Internal error";
}

void InjectedScript::setVariableValue(ErrorString* errorString, const ScriptValue& callFrames, const String* callFrameIdOpt, const String* functionObjectIdOpt, int scopeNumber, const String& variableName, const String& newValueStr)
{
    ScriptFunctionCall function(injectedScriptObject(), "setVariableValue");
    if (callFrameIdOpt) {
        function.appendArgument(callFrames);
        function.appendArgument(*callFrameIdOpt);
    } else {
        function.appendArgument(false);
        function.appendArgument(false);
    }
    if (functionObjectIdOpt)
        function.appendArgument(*functionObjectIdOpt);
    else
        function.appendArgument(false);
    function.appendArgument(scopeNumber);
    function.appendArgument(variableName);
    function.appendArgument(newValueStr);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue) {
        *errorString = "Internal error";
        return;
    }
    if (resultValue->type() == JSONValue::TypeString) {
        resultValue->asString(errorString);
        return;
    }
    // Normal return.
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>* result)
{
    ScriptFunctionCall function(injectedScriptObject(), "getFunctionDetails");
    function.appendArgument(functionId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue || resultValue->type() != JSONValue::TypeObject) {
        if (!resultValue->asString(errorString))
            *errorString = "Internal error";
        return;
    }
    *result = FunctionDetails::runtimeCast(resultValue);
}

void InjectedScript::getProperties(ErrorString* errorString, const String& objectId, bool ownProperties, bool accessorPropertiesOnly, RefPtr<Array<PropertyDescriptor> >* properties)
{
    ScriptFunctionCall function(injectedScriptObject(), "getProperties");
    function.appendArgument(objectId);
    function.appendArgument(ownProperties);
    function.appendArgument(accessorPropertiesOnly);

    RefPtr<JSONValue> result;
    makeCall(function, &result);
    if (!result || result->type() != JSONValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    *properties = Array<PropertyDescriptor>::runtimeCast(result);
}

void InjectedScript::getInternalProperties(ErrorString* errorString, const String& objectId, RefPtr<Array<InternalPropertyDescriptor> >* properties)
{
    ScriptFunctionCall function(injectedScriptObject(), "getInternalProperties");
    function.appendArgument(objectId);

    RefPtr<JSONValue> result;
    makeCall(function, &result);
    if (!result || result->type() != JSONValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    RefPtr<Array<InternalPropertyDescriptor> > array = Array<InternalPropertyDescriptor>::runtimeCast(result);
    if (array->length() > 0)
        *properties = array;
}

Node* InjectedScript::nodeForObjectId(const String& objectId)
{
    if (hasNoValue() || !canAccessInspectedWindow())
        return 0;

    ScriptFunctionCall function(injectedScriptObject(), "nodeForObjectId");
    function.appendArgument(objectId);

    bool hadException = false;
    ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);

    return InjectedScriptHost::scriptValueAsNode(resultValue);
}

void InjectedScript::releaseObject(const String& objectId)
{
    ScriptFunctionCall function(injectedScriptObject(), "releaseObject");
    function.appendArgument(objectId);
    RefPtr<JSONValue> result;
    makeCall(function, &result);
}

PassRefPtr<Array<CallFrame> > InjectedScript::wrapCallFrames(const ScriptValue& callFrames)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(injectedScriptObject(), "wrapCallFrames");
    function.appendArgument(callFrames);
    bool hadException = false;
    ScriptValue callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    RefPtr<JSONValue> result = callFramesValue.toJSONValue(scriptState());
    if (result->type() == JSONValue::TypeArray)
        return Array<CallFrame>::runtimeCast(result);
    return Array<CallFrame>::create();
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapObject(const ScriptValue& value, const String& groupName, bool generatePreview) const
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapObject");
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(canAccessInspectedWindow());
    wrapFunction.appendArgument(generatePreview);
    bool hadException = false;
    ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return 0;
    RefPtr<JSONObject> rawResult = r.toJSONValue(scriptState())->asObject();
    return TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapTable(const ScriptValue& table, const ScriptValue& columns) const
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapTable");
    wrapFunction.appendArgument(canAccessInspectedWindow());
    wrapFunction.appendArgument(table);
    if (columns.hasNoValue())
        wrapFunction.appendArgument(false);
    else
        wrapFunction.appendArgument(columns);
    bool hadException = false;
    ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return 0;
    RefPtr<JSONObject> rawResult = r.toJSONValue(scriptState())->asObject();
    return TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapNode(Node* node, const String& groupName)
{
    return wrapObject(nodeAsScriptValue(node), groupName);
}

ScriptValue InjectedScript::findObjectById(const String& objectId) const
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(injectedScriptObject(), "findObjectById");
    function.appendArgument(objectId);

    bool hadException = false;
    ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    return resultValue;
}

ScriptValue InjectedScript::findCallFrameById(ErrorString* errorString, const ScriptValue& topCallFrame, const String& callFrameId)
{
    ScriptFunctionCall function(injectedScriptObject(), "callFrameForId");
    function.appendArgument(topCallFrame);
    function.appendArgument(callFrameId);
    bool hadException = false;
    ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    if (hadException || resultValue.hasNoValue() || !resultValue.isObject()) {
        *errorString = "Internal error";
        return ScriptValue();
    }
    return resultValue;
}

void InjectedScript::inspectNode(Node* node)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(injectedScriptObject(), "inspectNode");
    function.appendArgument(nodeAsScriptValue(node));
    RefPtr<JSONValue> result;
    makeCall(function, &result);
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall releaseFunction(injectedScriptObject(), "releaseObjectGroup");
    releaseFunction.appendArgument(objectGroup);
    bool hadException = false;
    callFunctionWithEvalEnabled(releaseFunction, hadException);
    ASSERT(!hadException);
}

ScriptValue InjectedScript::nodeAsScriptValue(Node* node)
{
    return InjectedScriptHost::nodeAsScriptValue(scriptState(), node);
}

} // namespace WebCore

