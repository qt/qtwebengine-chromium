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

#ifndef NotificationPresenter_h
#define NotificationPresenter_h

#include "public/platform/WebNonCopyable.h"
#include "public/web/WebNotification.h"
#include "public/web/WebNotificationPresenter.h"
#include <map>
#include <set>
#include <string>

namespace WebTestRunner {

class WebTestDelegate;

// A class that implements WebNotificationPresenter for the TestRunner library.
class NotificationPresenter : public blink::WebNotificationPresenter, public blink::WebNonCopyable {
public:
    NotificationPresenter();
    virtual ~NotificationPresenter();

    void setDelegate(WebTestDelegate* delegate) { m_delegate = delegate; }

    // Called by the TestRunner to simulate a user granting permission.
    void grantPermission(const blink::WebString& origin);

    // Called by the TestRunner to simulate a user clicking on a notification.
    bool simulateClick(const blink::WebString& notificationIdentifier);

    // Called by the TestRunner to cancel all active notications.
    void cancelAllActiveNotifications();

    // blink::WebNotificationPresenter interface
    virtual bool show(const blink::WebNotification&);
    virtual void cancel(const blink::WebNotification&);
    virtual void objectDestroyed(const blink::WebNotification&);
    virtual Permission checkPermission(const blink::WebSecurityOrigin&);
    virtual void requestPermission(const blink::WebSecurityOrigin&, blink::WebNotificationPermissionCallback*);

    void reset() { m_allowedOrigins.clear(); }

private:
    WebTestDelegate* m_delegate;

    // Set of allowed origins.
    std::set<std::string> m_allowedOrigins;

    // Map of active notifications.
    std::map<std::string, blink::WebNotification> m_activeNotifications;

    // Map of active replacement IDs to the titles of those notifications
    std::map<std::string, std::string> m_replacements;
};

}

#endif // NotificationPresenter_h
