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

#ifndef WebDOMActivityLogger_h
#define WebDOMActivityLogger_h

#include "../platform/WebCommon.h"
#include "../platform/WebString.h"
#include "../platform/WebURL.h"
#include <v8.h>

namespace blink {

class WebDOMActivityLogger {
public:
    virtual ~WebDOMActivityLogger() { }
    virtual void log(const WebString& apiName, int argc, const v8::Handle<v8::Value>* argv, const WebString& extraInfo, const WebURL& url, const WebString& title) { }
};

// Checks if a logger already exists for the world identified
// by worldId (worldId may be 0 identifying the main world).
BLINK_EXPORT bool hasDOMActivityLogger(int worldId);

// Checks if the provided logger is non-null and if so associates it
// with the world identified by worldId (worldId may be 0 identifying the main world).
BLINK_EXPORT void setDOMActivityLogger(int worldId, WebDOMActivityLogger*);

} // namespace blink

#endif
