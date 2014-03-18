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

#ifndef WebTestInterfaces_h
#define WebTestInterfaces_h

#include "WebScopedPtr.h"
#include "WebTestCommon.h"

namespace blink {
class WebAudioDevice;
class WebFrame;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace WebTestRunner {

class TestInterfaces;
class WebTestDelegate;
class WebTestProxyBase;
class WebTestRunner;

class WEBTESTRUNNER_EXPORT WebTestInterfaces {
public:
    WebTestInterfaces();
    ~WebTestInterfaces();

    void setWebView(blink::WebView*, WebTestProxyBase*);
    void setDelegate(WebTestDelegate*);
    void bindTo(blink::WebFrame*);
    void resetAll();
    void setTestIsRunning(bool);
    void configureForTestWithURL(const blink::WebURL&, bool generatePixels);

    WebTestRunner* testRunner();
    blink::WebThemeEngine* themeEngine();

    blink::WebMediaStreamCenter* createMediaStreamCenter(blink::WebMediaStreamCenterClient*);
    blink::WebRTCPeerConnectionHandler* createWebRTCPeerConnectionHandler(blink::WebRTCPeerConnectionHandlerClient*);

    blink::WebMIDIAccessor* createMIDIAccessor(blink::WebMIDIAccessorClient*);

    blink::WebAudioDevice* createAudioDevice(double sampleRate);

#if WEBTESTRUNNER_IMPLEMENTATION
    TestInterfaces* testInterfaces();
#endif

private:
    WebScopedPtr<TestInterfaces> m_interfaces;
};

}

#endif // WebTestInterfaces_h
