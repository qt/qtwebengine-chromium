/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ConsoleBase_h
#define ConsoleBase_h

#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/frame/ConsoleTypes.h"
#include "core/frame/DOMWindowProperty.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ScriptArguments;

class ConsoleBase {
public:
    void ref() { refConsole(); }
    void deref() { derefConsole(); }

    void debug(ScriptState*, PassRefPtr<ScriptArguments>);
    void error(ScriptState*, PassRefPtr<ScriptArguments>);
    void info(ScriptState*, PassRefPtr<ScriptArguments>);
    void log(ScriptState*, PassRefPtr<ScriptArguments>);
    void clear(ScriptState*, PassRefPtr<ScriptArguments>);
    void warn(ScriptState*, PassRefPtr<ScriptArguments>);
    void dir(ScriptState*, PassRefPtr<ScriptArguments>);
    void dirxml(ScriptState*, PassRefPtr<ScriptArguments>);
    void table(ScriptState*, PassRefPtr<ScriptArguments>);
    void trace(ScriptState*, PassRefPtr<ScriptArguments>);
    void assertCondition(ScriptState*, PassRefPtr<ScriptArguments>, bool condition);
    void count(ScriptState*, PassRefPtr<ScriptArguments>);
    void markTimeline(const String&);
    void profile(ScriptState*, const String&);
    void profileEnd(ScriptState*, const String&);
    void time(const String&);
    void timeEnd(ScriptState*, const String&);
    void timeStamp(const String&);
    void timeline(ScriptState*, const String&);
    void timelineEnd(ScriptState*, const String&);
    void group(ScriptState*, PassRefPtr<ScriptArguments>);
    void groupCollapsed(ScriptState*, PassRefPtr<ScriptArguments>);
    void groupEnd();

protected:
    virtual ~ConsoleBase();
    virtual ExecutionContext* context() = 0;
    virtual void reportMessageToClient(MessageLevel, const String& message, PassRefPtr<ScriptCallStack>) = 0;

private:
    virtual void refConsole() = 0;
    virtual void derefConsole() = 0;

    void internalAddMessage(MessageType, MessageLevel, ScriptState*, PassRefPtr<ScriptArguments>, bool acceptNoArguments = false, bool printTrace = false);
};

} // namespace WebCore

#endif // ConsoleBase_h
