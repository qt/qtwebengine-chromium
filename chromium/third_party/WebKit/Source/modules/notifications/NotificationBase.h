/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef NotificationBase_h
#define NotificationBase_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/notifications/NotificationClient.h"
#include "platform/text/TextDirection.h"
#include "platform/weborigin/KURL.h"

namespace WebCore {

class ExecutionContext;
class NotificationClient;

class NotificationBase : public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData {
public:
    virtual ~NotificationBase();

    // Calling show() may start asynchronous operation. If this object has
    // a V8 wrapper, hasPendingActivity() prevents the wrapper from being
    // collected while m_state is Showing, and so this instance stays alive
    // until the operation completes. Otherwise, you need to hold a ref on this
    // instance until the operation completes.
    void show();

    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(show);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

    void dispatchShowEvent();
    void dispatchClickEvent();
    void dispatchErrorEvent();
    void dispatchCloseEvent();

    String title() const { return m_title; }
    String dir() const { return m_dir; }
    String lang() const { return m_lang; }
    String body() const { return m_body; }
    String tag() const { return m_tag; }
    String icon() const { return m_iconUrl; }

    TextDirection direction() const;
    KURL iconURL() const { return m_iconUrl; }

    // FIXME: This should be made protected once legacy notifications have been removed.
    void setDir(const String& dir) { m_dir = dir; }

    static const String& permissionString(NotificationClient::Permission);

    // EventTarget interface
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }
    virtual bool dispatchEvent(PassRefPtr<Event>) OVERRIDE;

    // ActiveDOMObject interface
    virtual void stop() OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE;

protected:
    NotificationBase(const String& title, ExecutionContext*, NotificationClient*);

    void setLang(const String& lang) { m_lang = lang; }
    void setBody(const String& body) { m_body = body; }
    void setIconUrl(KURL iconUrl) { m_iconUrl = iconUrl; }
    void setTag(const String& tag) { m_tag = tag; }

private:
    String m_title;
    String m_dir;
    String m_lang;
    String m_body;
    String m_tag;

    KURL m_iconUrl;

    enum NotificationState {
        Idle = 0,
        Showing = 1,
        Closed = 2,
    };

    NotificationState m_state;

    NotificationClient* m_client;
};

} // namespace WebCore

#endif // NotificationBase_h
