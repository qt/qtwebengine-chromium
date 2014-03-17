/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InjectedScriptHost.h"

#include "core/inspector/InspectorAgent.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMStorageAgent.h"
#include "core/inspector/InspectorDatabaseAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "platform/JSONValues.h"

#include "wtf/RefPtr.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

PassRefPtr<InjectedScriptHost> InjectedScriptHost::create()
{
    return adoptRef(new InjectedScriptHost());
}

InjectedScriptHost::InjectedScriptHost()
    : m_instrumentingAgents(0)
    , m_scriptDebugServer(0)
{
    ScriptWrappable::init(this);
    m_defaultInspectableObject = adoptPtr(new InspectableObject());
}

InjectedScriptHost::~InjectedScriptHost()
{
}

void InjectedScriptHost::disconnect()
{
    m_instrumentingAgents = 0;
    m_scriptDebugServer = 0;
}

void InjectedScriptHost::inspectImpl(PassRefPtr<JSONValue> object, PassRefPtr<JSONValue> hints)
{
    if (InspectorAgent* inspectorAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorAgent() : 0) {
        RefPtr<TypeBuilder::Runtime::RemoteObject> remoteObject = TypeBuilder::Runtime::RemoteObject::runtimeCast(object);
        inspectorAgent->inspect(remoteObject, hints->asObject());
    }
}

void InjectedScriptHost::getEventListenersImpl(Node* node, Vector<EventListenerInfo>& listenersArray)
{
    InspectorDOMAgent::getEventListeners(node, listenersArray, false);
}

void InjectedScriptHost::clearConsoleMessages()
{
    if (InspectorConsoleAgent* consoleAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorConsoleAgent() : 0) {
        ErrorString error;
        consoleAgent->clearMessages(&error);
    }
}

ScriptValue InjectedScriptHost::InspectableObject::get(ScriptState*)
{
    return ScriptValue();
};

void InjectedScriptHost::addInspectedObject(PassOwnPtr<InjectedScriptHost::InspectableObject> object)
{
    m_inspectedObjects.prepend(object);
    while (m_inspectedObjects.size() > 5)
        m_inspectedObjects.removeLast();
}

void InjectedScriptHost::clearInspectedObjects()
{
    m_inspectedObjects.clear();
}

InjectedScriptHost::InspectableObject* InjectedScriptHost::inspectedObject(unsigned int num)
{
    if (num >= m_inspectedObjects.size())
        return m_defaultInspectableObject.get();
    return m_inspectedObjects[num].get();
}

String InjectedScriptHost::databaseIdImpl(Database* database)
{
    if (InspectorDatabaseAgent* databaseAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorDatabaseAgent() : 0)
        return databaseAgent->databaseId(database);
    return String();
}

String InjectedScriptHost::storageIdImpl(Storage* storage)
{
    if (InspectorDOMStorageAgent* domStorageAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorDOMStorageAgent() : 0)
        return domStorageAgent->storageId(storage);
    return String();
}

void InjectedScriptHost::debugFunction(const String& scriptId, int lineNumber, int columnNumber)
{
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorDebuggerAgent() : 0)
        debuggerAgent->setBreakpoint(scriptId, lineNumber, columnNumber, InspectorDebuggerAgent::DebugCommandBreakpointSource);
}

void InjectedScriptHost::undebugFunction(const String& scriptId, int lineNumber, int columnNumber)
{
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorDebuggerAgent() : 0)
        debuggerAgent->removeBreakpoint(scriptId, lineNumber, columnNumber, InspectorDebuggerAgent::DebugCommandBreakpointSource);
}

void InjectedScriptHost::monitorFunction(const String& scriptId, int lineNumber, int columnNumber, const String& functionName)
{
    StringBuilder builder;
    builder.appendLiteral("console.log(\"function ");
    if (functionName.isEmpty())
        builder.appendLiteral("(anonymous function)");
    else
        builder.append(functionName);
    builder.appendLiteral(" called\" + (arguments.length > 0 ? \" with arguments: \" + Array.prototype.join.call(arguments, \", \") : \"\")) && false");
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorDebuggerAgent() : 0)
        debuggerAgent->setBreakpoint(scriptId, lineNumber, columnNumber, InspectorDebuggerAgent::MonitorCommandBreakpointSource, builder.toString());
}

void InjectedScriptHost::unmonitorFunction(const String& scriptId, int lineNumber, int columnNumber)
{
    if (InspectorDebuggerAgent* debuggerAgent = m_instrumentingAgents ? m_instrumentingAgents->inspectorDebuggerAgent() : 0)
        debuggerAgent->removeBreakpoint(scriptId, lineNumber, columnNumber, InspectorDebuggerAgent::MonitorCommandBreakpointSource);
}

} // namespace WebCore

