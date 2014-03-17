/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef AccessibilityController_h
#define AccessibilityController_h

#include "CppBoundClass.h"
#include "WebAXObjectProxy.h"

namespace blink {
class WebAXObject;
class WebFrame;
class WebView;
}

namespace WebTestRunner {

class WebTestDelegate;

class AccessibilityController : public CppBoundClass {
public:
    AccessibilityController();

    // Shadow to include accessibility initialization.
    void bindToJavascript(blink::WebFrame*, const blink::WebString& classname);
    void reset();

    void setFocusedElement(const blink::WebAXObject&);
    WebAXObjectProxy* getFocusedElement();
    WebAXObjectProxy* getRootElement();
    WebAXObjectProxy* getAccessibleElementById(const std::string& id);

    bool shouldLogAccessibilityEvents();

    void notificationReceived(const blink::WebAXObject& target, const char* notificationName);

    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }
    void setWebView(blink::WebView* webView) { m_webView = webView; }

private:
    // If true, will log all accessibility notifications.
    bool m_logAccessibilityEvents;

    // Bound methods and properties
    void logAccessibilityEventsCallback(const CppArgumentList&, CppVariant*);
    void fallbackCallback(const CppArgumentList&, CppVariant*);
    void addNotificationListenerCallback(const CppArgumentList&, CppVariant*);
    void removeNotificationListenerCallback(const CppArgumentList&, CppVariant*);

    void focusedElementGetterCallback(CppVariant*);
    void rootElementGetterCallback(CppVariant*);
    void accessibleElementByIdGetterCallback(const CppArgumentList&, CppVariant*);

    WebAXObjectProxy* findAccessibleElementByIdRecursive(const blink::WebAXObject&, const blink::WebString& id);

    blink::WebAXObject m_focusedElement;
    blink::WebAXObject m_rootElement;

    WebAXObjectProxyList m_elements;

    std::vector<CppVariant> m_notificationCallbacks;

    WebTestDelegate* m_delegate;
    blink::WebView* m_webView;
};

}

#endif // AccessibilityController_h
