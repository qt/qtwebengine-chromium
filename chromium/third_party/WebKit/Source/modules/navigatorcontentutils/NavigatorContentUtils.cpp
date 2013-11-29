/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "modules/navigatorcontentutils/NavigatorContentUtils.h"

#if ENABLE(NAVIGATOR_CONTENT_UTILS)

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/Frame.h"
#include "core/page/Navigator.h"
#include "core/page/Page.h"
#include "wtf/HashSet.h"

namespace WebCore {

static HashSet<String>* protocolWhitelist;

static void initProtocolHandlerWhitelist()
{
    protocolWhitelist = new HashSet<String>;
    static const char* protocols[] = {
        "bitcoin",
        "geo",
        "im",
        "irc",
        "ircs",
        "magnet",
        "mailto",
        "mms",
        "news",
        "nntp",
        "sip",
        "sms",
        "smsto",
        "ssh",
        "tel",
        "urn",
        "webcal",
        "webtai",
        "xmpp",
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(protocols); ++i)
        protocolWhitelist->add(protocols[i]);
}

static bool verifyCustomHandlerURL(const String& baseURL, const String& url, ExceptionState& es)
{
    // The specification requires that it is a SyntaxError if the "%s" token is
    // not present.
    static const char token[] = "%s";
    int index = url.find(token);
    if (-1 == index) {
        es.throwDOMException(SyntaxError);
        return false;
    }

    // It is also a SyntaxError if the custom handler URL, as created by removing
    // the "%s" token and prepending the base url, does not resolve.
    String newURL = url;
    newURL.remove(index, WTF_ARRAY_LENGTH(token) - 1);

    KURL base(ParsedURLString, baseURL);
    KURL kurl(base, newURL);

    if (kurl.isEmpty() || !kurl.isValid()) {
        es.throwDOMException(SyntaxError);
        return false;
    }

    return true;
}

static bool isProtocolWhitelisted(const String& scheme)
{
    if (!protocolWhitelist)
        initProtocolHandlerWhitelist();
    return protocolWhitelist->contains(scheme);
}

static bool verifyProtocolHandlerScheme(const String& scheme, ExceptionState& es)
{
    if (scheme.startsWith("web+")) {
        if (isValidProtocol(scheme))
            return true;
        es.throwDOMException(SecurityError);
        return false;
    }

    if (isProtocolWhitelisted(scheme))
        return true;
    es.throwDOMException(SecurityError);
    return false;
}

NavigatorContentUtils* NavigatorContentUtils::from(Page* page)
{
    return static_cast<NavigatorContentUtils*>(RefCountedSupplement<Page, NavigatorContentUtils>::from(page, NavigatorContentUtils::supplementName()));
}

NavigatorContentUtils::~NavigatorContentUtils()
{
}

PassRefPtr<NavigatorContentUtils> NavigatorContentUtils::create(NavigatorContentUtilsClient* client)
{
    return adoptRef(new NavigatorContentUtils(client));
}

void NavigatorContentUtils::registerProtocolHandler(Navigator* navigator, const String& scheme, const String& url, const String& title, ExceptionState& es)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    if (!document)
        return;

    String baseURL = document->baseURL().baseAsString();

    if (!verifyCustomHandlerURL(baseURL, url, es))
        return;

    if (!verifyProtocolHandlerScheme(scheme, es))
        return;

    NavigatorContentUtils::from(navigator->frame()->page())->client()->registerProtocolHandler(scheme, baseURL, url, navigator->frame()->displayStringModifiedByEncoding(title));
}

#if ENABLE(CUSTOM_SCHEME_HANDLER)
static String customHandlersStateString(const NavigatorContentUtilsClient::CustomHandlersState state)
{
    DEFINE_STATIC_LOCAL(const String, newHandler, ("new"));
    DEFINE_STATIC_LOCAL(const String, registeredHandler, ("registered"));
    DEFINE_STATIC_LOCAL(const String, declinedHandler, ("declined"));

    switch (state) {
    case NavigatorContentUtilsClient::CustomHandlersNew:
        return newHandler;
    case NavigatorContentUtilsClient::CustomHandlersRegistered:
        return registeredHandler;
    case NavigatorContentUtilsClient::CustomHandlersDeclined:
        return declinedHandler;
    }

    ASSERT_NOT_REACHED();
    return String();
}

String NavigatorContentUtils::isProtocolHandlerRegistered(Navigator* navigator, const String& scheme, const String& url, ExceptionState& es)
{
    DEFINE_STATIC_LOCAL(const String, declined, ("declined"));

    if (!navigator->frame())
        return declined;

    Document* document = navigator->frame()->document();
    String baseURL = document->baseURL().baseAsString();

    if (!verifyCustomHandlerURL(baseURL, url, es))
        return declined;

    if (!verifyProtocolHandlerScheme(scheme, es))
        return declined;

    return customHandlersStateString(NavigatorContentUtils::from(navigator->frame()->page())->client()->isProtocolHandlerRegistered(scheme, baseURL, url));
}

void NavigatorContentUtils::unregisterProtocolHandler(Navigator* navigator, const String& scheme, const String& url, ExceptionState& es)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    String baseURL = document->baseURL().baseAsString();

    if (!verifyCustomHandlerURL(baseURL, url, es))
        return;

    if (!verifyProtocolHandlerScheme(scheme, es))
        return;

    NavigatorContentUtils::from(navigator->frame()->page())->client()->unregisterProtocolHandler(scheme, baseURL, url);
}
#endif

const char* NavigatorContentUtils::supplementName()
{
    return "NavigatorContentUtils";
}

void provideNavigatorContentUtilsTo(Page* page, NavigatorContentUtilsClient* client)
{
    RefCountedSupplement<Page, NavigatorContentUtils>::provideTo(page, NavigatorContentUtils::supplementName(), NavigatorContentUtils::create(client));
}

} // namespace WebCore

#endif // ENABLE(NAVIGATOR_CONTENT_UTILS)

