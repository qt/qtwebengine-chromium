/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "core/frame/Console.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/dom/Document.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/ScriptArguments.h"
#include "platform/TraceEvent.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

ConsoleBase::~ConsoleBase()
{
}

void ConsoleBase::debug(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, DebugMessageLevel, state, arguments);
}

void ConsoleBase::error(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, ErrorMessageLevel, state, arguments);
}

void ConsoleBase::info(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, InfoMessageLevel, state, arguments);
}

void ConsoleBase::log(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, LogMessageLevel, state, arguments);
}

void ConsoleBase::warn(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, WarningMessageLevel, state, arguments);
}

void ConsoleBase::dir(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(DirMessageType, LogMessageLevel, state, arguments);
}

void ConsoleBase::dirxml(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(DirXMLMessageType, LogMessageLevel, state, arguments);
}

void ConsoleBase::table(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(TableMessageType, LogMessageLevel, state, arguments);
}

void ConsoleBase::clear(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(context(), ConsoleAPIMessageSource, ClearMessageType, LogMessageLevel, String(), state, arguments);
}

void ConsoleBase::trace(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(TraceMessageType, LogMessageLevel, state, arguments, true, true);
}

void ConsoleBase::assertCondition(ScriptState* state, PassRefPtr<ScriptArguments> arguments, bool condition)
{
    if (condition)
        return;

    internalAddMessage(AssertMessageType, ErrorMessageLevel, state, arguments, true);
}

void ConsoleBase::count(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleCount(context(), state, arguments);
}

void ConsoleBase::markTimeline(const String& title)
{
    InspectorInstrumentation::consoleTimeStamp(context(), title);
}

void ConsoleBase::profile(ScriptState* state, const String& title)
{
    InspectorInstrumentation::consoleProfile(context(), title, state);
}

void ConsoleBase::profileEnd(ScriptState* state, const String& title)
{
    InspectorInstrumentation::consoleProfileEnd(context(), title);
}

void ConsoleBase::time(const String& title)
{
    InspectorInstrumentation::consoleTime(context(), title);
    TRACE_EVENT_COPY_ASYNC_BEGIN0("webkit.console", title.utf8().data(), this);
}

void ConsoleBase::timeEnd(ScriptState* state, const String& title)
{
    TRACE_EVENT_COPY_ASYNC_END0("webkit.console", title.utf8().data(), this);
    InspectorInstrumentation::consoleTimeEnd(context(), title, state);
}

void ConsoleBase::timeStamp(const String& title)
{
    InspectorInstrumentation::consoleTimeStamp(context(), title);
}

void ConsoleBase::timeline(ScriptState* state, const String& title)
{
    InspectorInstrumentation::consoleTimeline(context(), title, state);
}

void ConsoleBase::timelineEnd(ScriptState* state, const String& title)
{
    InspectorInstrumentation::consoleTimelineEnd(context(), title, state);
}

void ConsoleBase::group(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(context(), ConsoleAPIMessageSource, StartGroupMessageType, LogMessageLevel, String(), state, arguments);
}

void ConsoleBase::groupCollapsed(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(context(), ConsoleAPIMessageSource, StartGroupCollapsedMessageType, LogMessageLevel, String(), state, arguments);
}

void ConsoleBase::groupEnd()
{
    InspectorInstrumentation::addMessageToConsole(context(), ConsoleAPIMessageSource, EndGroupMessageType, LogMessageLevel, String(), 0);
}

void ConsoleBase::internalAddMessage(MessageType type, MessageLevel level, ScriptState* state, PassRefPtr<ScriptArguments> scriptArguments, bool acceptNoArguments, bool printTrace)
{
    if (!context())
        return;

    RefPtr<ScriptArguments> arguments = scriptArguments;
    if (!acceptNoArguments && !arguments->argumentCount())
        return;

    size_t stackSize = printTrace ? ScriptCallStack::maxCallStackSizeToCapture : 1;
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(stackSize));

    String message;
    bool gotStringMessage = arguments->getFirstArgumentAsString(message);
    InspectorInstrumentation::addMessageToConsole(context(), ConsoleAPIMessageSource, type, level, message, state, arguments);
    if (gotStringMessage)
        reportMessageToClient(level, message, callStack);
}

} // namespace WebCore
