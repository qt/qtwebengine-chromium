/*
 *  Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
 *  Copyright (C) 2007, 2008 Julien Chaffraix <jchaffraix@webkit.org>
 *  Copyright (C) 2008, 2011 Google Inc. All rights reserved.
 *  Copyright (C) 2012 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "core/xml/XMLHttpRequest.h"

#include "FetchInitiatorTypeNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Event.h"
#include "core/dom/EventListener.h"
#include "core/dom/EventNames.h"
#include "core/dom/ExceptionCode.h"
#include "core/editing/markup.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/html/DOMFormData.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/CrossOriginAccessControl.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/loader/ThreadableLoader.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/Settings.h"
#include "core/platform/HistogramSupport.h"
#include "core/platform/SharedBuffer.h"
#include "core/platform/network/BlobData.h"
#include "core/platform/network/HTTPParsers.h"
#include "core/platform/network/ParsedContentType.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/xml/XMLHttpRequestProgressEvent.h"
#include "core/xml/XMLHttpRequestUpload.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/StdLibExtras.h"
#include "wtf/UnusedParam.h"
#include "wtf/text/CString.h"

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, xmlHttpRequestCounter, ("XMLHttpRequest"));

// Histogram enum to see when we can deprecate xhr.send(ArrayBuffer).
enum XMLHttpRequestSendArrayBufferOrView {
    XMLHttpRequestSendArrayBuffer,
    XMLHttpRequestSendArrayBufferView,
    XMLHttpRequestSendArrayBufferOrViewMax,
};

struct XMLHttpRequestStaticData {
    WTF_MAKE_NONCOPYABLE(XMLHttpRequestStaticData); WTF_MAKE_FAST_ALLOCATED;
public:
    XMLHttpRequestStaticData();
    String m_proxyHeaderPrefix;
    String m_secHeaderPrefix;
    HashSet<String, CaseFoldingHash> m_forbiddenRequestHeaders;
};

XMLHttpRequestStaticData::XMLHttpRequestStaticData()
    : m_proxyHeaderPrefix("proxy-")
    , m_secHeaderPrefix("sec-")
{
    m_forbiddenRequestHeaders.add("accept-charset");
    m_forbiddenRequestHeaders.add("accept-encoding");
    m_forbiddenRequestHeaders.add("access-control-request-headers");
    m_forbiddenRequestHeaders.add("access-control-request-method");
    m_forbiddenRequestHeaders.add("connection");
    m_forbiddenRequestHeaders.add("content-length");
    m_forbiddenRequestHeaders.add("content-transfer-encoding");
    m_forbiddenRequestHeaders.add("cookie");
    m_forbiddenRequestHeaders.add("cookie2");
    m_forbiddenRequestHeaders.add("date");
    m_forbiddenRequestHeaders.add("expect");
    m_forbiddenRequestHeaders.add("host");
    m_forbiddenRequestHeaders.add("keep-alive");
    m_forbiddenRequestHeaders.add("origin");
    m_forbiddenRequestHeaders.add("referer");
    m_forbiddenRequestHeaders.add("te");
    m_forbiddenRequestHeaders.add("trailer");
    m_forbiddenRequestHeaders.add("transfer-encoding");
    m_forbiddenRequestHeaders.add("upgrade");
    m_forbiddenRequestHeaders.add("user-agent");
    m_forbiddenRequestHeaders.add("via");
}

static bool isSetCookieHeader(const AtomicString& name)
{
    return equalIgnoringCase(name, "set-cookie") || equalIgnoringCase(name, "set-cookie2");
}

static void replaceCharsetInMediaType(String& mediaType, const String& charsetValue)
{
    unsigned int pos = 0, len = 0;

    findCharsetInMediaType(mediaType, pos, len);

    if (!len) {
        // When no charset found, do nothing.
        return;
    }

    // Found at least one existing charset, replace all occurrences with new charset.
    while (len) {
        mediaType.replace(pos, len, charsetValue);
        unsigned int start = pos + charsetValue.length();
        findCharsetInMediaType(mediaType, pos, len, start);
    }
}

static const XMLHttpRequestStaticData* staticData = 0;

static const XMLHttpRequestStaticData* createXMLHttpRequestStaticData()
{
    staticData = new XMLHttpRequestStaticData;
    return staticData;
}

static const XMLHttpRequestStaticData* initializeXMLHttpRequestStaticData()
{
    // Uses dummy to avoid warnings about an unused variable.
    AtomicallyInitializedStatic(const XMLHttpRequestStaticData*, dummy = createXMLHttpRequestStaticData());
    return dummy;
}

static void logConsoleError(ScriptExecutionContext* context, const String& message)
{
    if (!context)
        return;
    // FIXME: It's not good to report the bad usage without indicating what source line it came from.
    // We should pass additional parameters so we can tell the console where the mistake occurred.
    context->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message);
}

PassRefPtr<XMLHttpRequest> XMLHttpRequest::create(ScriptExecutionContext* context, PassRefPtr<SecurityOrigin> securityOrigin)
{
    RefPtr<XMLHttpRequest> xmlHttpRequest(adoptRef(new XMLHttpRequest(context, securityOrigin)));
    xmlHttpRequest->suspendIfNeeded();

    return xmlHttpRequest.release();
}

XMLHttpRequest::XMLHttpRequest(ScriptExecutionContext* context, PassRefPtr<SecurityOrigin> securityOrigin)
    : ActiveDOMObject(context)
    , m_async(true)
    , m_includeCredentials(false)
    , m_timeoutMilliseconds(0)
    , m_state(UNSENT)
    , m_createdDocument(false)
    , m_error(false)
    , m_uploadEventsAllowed(true)
    , m_uploadComplete(false)
    , m_sameOriginRequest(true)
    , m_allowCrossOriginRequests(false)
    , m_receivedLength(0)
    , m_lastSendLineNumber(0)
    , m_exceptionCode(0)
    , m_progressEventThrottle(this)
    , m_responseTypeCode(ResponseTypeDefault)
    , m_protectionTimer(this, &XMLHttpRequest::dropProtection)
    , m_securityOrigin(securityOrigin)
{
    initializeXMLHttpRequestStaticData();
#ifndef NDEBUG
    xmlHttpRequestCounter.increment();
#endif
    ScriptWrappable::init(this);
}

XMLHttpRequest::~XMLHttpRequest()
{
#ifndef NDEBUG
    xmlHttpRequestCounter.decrement();
#endif
}

Document* XMLHttpRequest::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return toDocument(scriptExecutionContext());
}

SecurityOrigin* XMLHttpRequest::securityOrigin() const
{
    return m_securityOrigin ? m_securityOrigin.get() : scriptExecutionContext()->securityOrigin();
}

XMLHttpRequest::State XMLHttpRequest::readyState() const
{
    return m_state;
}

ScriptString XMLHttpRequest::responseText(ExceptionState& es)
{
    if (m_responseTypeCode != ResponseTypeDefault && m_responseTypeCode != ResponseTypeText) {
        es.throwDOMException(InvalidStateError);
        return ScriptString();
    }
    if (m_error || (m_state != LOADING && m_state != DONE))
        return ScriptString();
    return m_responseText;
}

Document* XMLHttpRequest::responseXML(ExceptionState& es)
{
    if (m_responseTypeCode != ResponseTypeDefault && m_responseTypeCode != ResponseTypeDocument) {
        es.throwDOMException(InvalidStateError);
        return 0;
    }

    if (m_error || m_state != DONE)
        return 0;

    if (!m_createdDocument) {
        bool isHTML = equalIgnoringCase(responseMIMEType(), "text/html");

        // The W3C spec requires the final MIME type to be some valid XML type, or text/html.
        // If it is text/html, then the responseType of "document" must have been supplied explicitly.
        if ((m_response.isHTTP() && !responseIsXML() && !isHTML)
            || (isHTML && m_responseTypeCode == ResponseTypeDefault)
            || scriptExecutionContext()->isWorkerGlobalScope()) {
            m_responseDocument = 0;
        } else {
            if (isHTML)
                m_responseDocument = HTMLDocument::create(DocumentInit(m_url));
            else
                m_responseDocument = Document::create(DocumentInit(m_url));
            // FIXME: Set Last-Modified.
            m_responseDocument->setContent(m_responseText.flattenToString());
            m_responseDocument->setSecurityOrigin(securityOrigin());
            m_responseDocument->setContextFeatures(document()->contextFeatures());
            if (!m_responseDocument->wellFormed())
                m_responseDocument = 0;
        }
        m_createdDocument = true;
    }

    return m_responseDocument.get();
}

Blob* XMLHttpRequest::responseBlob(ExceptionState& es)
{
    if (m_responseTypeCode != ResponseTypeBlob) {
        es.throwDOMException(InvalidStateError);
        return 0;
    }
    // We always return null before DONE.
    if (m_error || m_state != DONE)
        return 0;

    if (!m_responseBlob) {
        // FIXME: This causes two (or more) unnecessary copies of the data.
        // Chromium stores blob data in the browser process, so we're pulling the data
        // from the network only to copy it into the renderer to copy it back to the browser.
        // Ideally we'd get the blob/file-handle from the ResourceResponse directly
        // instead of copying the bytes. Embedders who store blob data in the
        // same process as WebCore would at least to teach BlobData to take
        // a SharedBuffer, even if they don't get the Blob from the network layer directly.
        OwnPtr<BlobData> blobData = BlobData::create();
        // If we errored out or got no data, we still return a blob, just an empty one.
        size_t size = 0;
        if (m_binaryResponseBuilder) {
            RefPtr<RawData> rawData = RawData::create();
            size = m_binaryResponseBuilder->size();
            rawData->mutableData()->append(m_binaryResponseBuilder->data(), size);
            blobData->appendData(rawData, 0, BlobDataItem::toEndOfFile);
            blobData->setContentType(responseMIMEType()); // responseMIMEType defaults to text/xml which may be incorrect.
            m_binaryResponseBuilder.clear();
        }
        m_responseBlob = Blob::create(blobData.release(), size);
    }

    return m_responseBlob.get();
}

ArrayBuffer* XMLHttpRequest::responseArrayBuffer(ExceptionState& es)
{
    if (m_responseTypeCode != ResponseTypeArrayBuffer) {
        es.throwDOMException(InvalidStateError);
        return 0;
    }

    if (m_error || m_state != DONE)
        return 0;

    if (!m_responseArrayBuffer.get() && m_binaryResponseBuilder.get() && m_binaryResponseBuilder->size() > 0) {
        m_responseArrayBuffer = m_binaryResponseBuilder->getAsArrayBuffer();
        m_binaryResponseBuilder.clear();
    }

    return m_responseArrayBuffer.get();
}

void XMLHttpRequest::setTimeout(unsigned long timeout, ExceptionState& es)
{
    // FIXME: Need to trigger or update the timeout Timer here, if needed. http://webkit.org/b/98156
    // XHR2 spec, 4.7.3. "This implies that the timeout attribute can be set while fetching is in progress. If that occurs it will still be measured relative to the start of fetching."
    if (scriptExecutionContext()->isDocument() && !m_async) {
        logConsoleError(scriptExecutionContext(), "XMLHttpRequest.timeout cannot be set for synchronous HTTP(S) requests made from the window context.");
        es.throwDOMException(InvalidAccessError);
        return;
    }
    m_timeoutMilliseconds = timeout;
}

void XMLHttpRequest::setResponseType(const String& responseType, ExceptionState& es)
{
    if (m_state >= LOADING) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // Newer functionality is not available to synchronous requests in window contexts, as a spec-mandated
    // attempt to discourage synchronous XHR use. responseType is one such piece of functionality.
    // We'll only disable this functionality for HTTP(S) requests since sync requests for local protocols
    // such as file: and data: still make sense to allow.
    if (!m_async && scriptExecutionContext()->isDocument() && m_url.protocolIsInHTTPFamily()) {
        logConsoleError(scriptExecutionContext(), "XMLHttpRequest.responseType cannot be changed for synchronous HTTP(S) requests made from the window context.");
        es.throwDOMException(InvalidAccessError);
        return;
    }

    if (responseType == "")
        m_responseTypeCode = ResponseTypeDefault;
    else if (responseType == "text")
        m_responseTypeCode = ResponseTypeText;
    else if (responseType == "document")
        m_responseTypeCode = ResponseTypeDocument;
    else if (responseType == "blob")
        m_responseTypeCode = ResponseTypeBlob;
    else if (responseType == "arraybuffer")
        m_responseTypeCode = ResponseTypeArrayBuffer;
    else
        ASSERT_NOT_REACHED();
}

String XMLHttpRequest::responseType()
{
    switch (m_responseTypeCode) {
    case ResponseTypeDefault:
        return "";
    case ResponseTypeText:
        return "text";
    case ResponseTypeDocument:
        return "document";
    case ResponseTypeBlob:
        return "blob";
    case ResponseTypeArrayBuffer:
        return "arraybuffer";
    }
    return "";
}

XMLHttpRequestUpload* XMLHttpRequest::upload()
{
    if (!m_upload)
        m_upload = XMLHttpRequestUpload::create(this);
    return m_upload.get();
}

void XMLHttpRequest::changeState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        callReadyStateChangeListener();
    }
}

void XMLHttpRequest::callReadyStateChangeListener()
{
    if (!scriptExecutionContext())
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchXHRReadyStateChangeEvent(scriptExecutionContext(), this);

    if (m_async || (m_state <= OPENED || m_state == DONE))
        m_progressEventThrottle.dispatchReadyStateChangeEvent(XMLHttpRequestProgressEvent::create(eventNames().readystatechangeEvent), m_state == DONE ? FlushProgressEvent : DoNotFlushProgressEvent);

    InspectorInstrumentation::didDispatchXHRReadyStateChangeEvent(cookie);
    if (m_state == DONE && !m_error) {
        InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchXHRLoadEvent(scriptExecutionContext(), this);
        m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadEvent));
        InspectorInstrumentation::didDispatchXHRLoadEvent(cookie);
        m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadendEvent));
    }
}

void XMLHttpRequest::setWithCredentials(bool value, ExceptionState& es)
{
    if (m_state > OPENED || m_loader) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    m_includeCredentials = value;
}

bool XMLHttpRequest::isAllowedHTTPMethod(const String& method)
{
    return !equalIgnoringCase(method, "TRACE")
        && !equalIgnoringCase(method, "TRACK")
        && !equalIgnoringCase(method, "CONNECT");
}

String XMLHttpRequest::uppercaseKnownHTTPMethod(const String& method)
{
    if (equalIgnoringCase(method, "COPY") || equalIgnoringCase(method, "DELETE") || equalIgnoringCase(method, "GET")
        || equalIgnoringCase(method, "HEAD") || equalIgnoringCase(method, "INDEX") || equalIgnoringCase(method, "LOCK")
        || equalIgnoringCase(method, "M-POST") || equalIgnoringCase(method, "MKCOL") || equalIgnoringCase(method, "MOVE")
        || equalIgnoringCase(method, "OPTIONS") || equalIgnoringCase(method, "POST") || equalIgnoringCase(method, "PROPFIND")
        || equalIgnoringCase(method, "PROPPATCH") || equalIgnoringCase(method, "PUT") || equalIgnoringCase(method, "UNLOCK")) {
        return method.upper();
    }
    return method;
}

bool XMLHttpRequest::isAllowedHTTPHeader(const String& name)
{
    initializeXMLHttpRequestStaticData();
    return !staticData->m_forbiddenRequestHeaders.contains(name) && !name.startsWith(staticData->m_proxyHeaderPrefix, false)
        && !name.startsWith(staticData->m_secHeaderPrefix, false);
}

void XMLHttpRequest::open(const String& method, const KURL& url, ExceptionState& es)
{
    open(method, url, true, es);
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, ExceptionState& es)
{
    internalAbort();
    State previousState = m_state;
    m_state = UNSENT;
    m_error = false;
    m_uploadComplete = false;

    // clear stuff from possible previous load
    clearResponse();
    clearRequest();

    ASSERT(m_state == UNSENT);

    if (!isValidHTTPToken(method)) {
        es.throwDOMException(SyntaxError);
        return;
    }

    if (!isAllowedHTTPMethod(method)) {
        es.throwDOMException(SecurityError, "'XMLHttpRequest.open' does not support the '" + method + "' method.");
        return;
    }

    if (!ContentSecurityPolicy::shouldBypassMainWorld(scriptExecutionContext()) && !scriptExecutionContext()->contentSecurityPolicy()->allowConnectToSource(url)) {
        es.throwDOMException(SecurityError, "Refused to connect to '" + url.elidedString() + "' because it violates the document's Content Security Policy.");
        return;
    }

    if (!async && scriptExecutionContext()->isDocument()) {
        if (document()->settings() && !document()->settings()->syncXHRInDocumentsEnabled()) {
            logConsoleError(scriptExecutionContext(), "Synchronous XMLHttpRequests are disabled for this page.");
            es.throwDOMException(InvalidAccessError);
            return;
        }

        // Newer functionality is not available to synchronous requests in window contexts, as a spec-mandated
        // attempt to discourage synchronous XHR use. responseType is one such piece of functionality.
        // We'll only disable this functionality for HTTP(S) requests since sync requests for local protocols
        // such as file: and data: still make sense to allow.
        if (url.protocolIsInHTTPFamily() && m_responseTypeCode != ResponseTypeDefault) {
            logConsoleError(scriptExecutionContext(), "Synchronous HTTP(S) requests made from the window context cannot have XMLHttpRequest.responseType set.");
            es.throwDOMException(InvalidAccessError);
            return;
        }

        // Similarly, timeouts are disabled for synchronous requests as well.
        if (m_timeoutMilliseconds > 0) {
            logConsoleError(scriptExecutionContext(), "Synchronous XMLHttpRequests must not have a timeout value set.");
            es.throwDOMException(InvalidAccessError);
            return;
        }
    }

    m_method = uppercaseKnownHTTPMethod(method);

    m_url = url;

    m_async = async;

    ASSERT(!m_loader);

    // Check previous state to avoid dispatching readyState event
    // when calling open several times in a row.
    if (previousState != OPENED)
        changeState(OPENED);
    else
        m_state = OPENED;
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, ExceptionState& es)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);

    open(method, urlWithCredentials, async, es);
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, const String& password, ExceptionState& es)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);
    urlWithCredentials.setPass(password);

    open(method, urlWithCredentials, async, es);
}

bool XMLHttpRequest::initSend(ExceptionState& es)
{
    if (!scriptExecutionContext())
        return false;

    if (m_state != OPENED || m_loader) {
        es.throwDOMException(InvalidStateError);
        return false;
    }

    m_error = false;
    return true;
}

void XMLHttpRequest::send(ExceptionState& es)
{
    send(String(), es);
}

bool XMLHttpRequest::areMethodAndURLValidForSend()
{
    return m_method != "GET" && m_method != "HEAD" && m_url.protocolIsInHTTPFamily();
}

void XMLHttpRequest::send(Document* document, ExceptionState& es)
{
    ASSERT(document);

    if (!initSend(es))
        return;

    if (areMethodAndURLValidForSend()) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
            // FIXME: this should include the charset used for encoding.
            setRequestHeaderInternal("Content-Type", "application/xml");
        }

        // FIXME: According to XMLHttpRequest Level 2, this should use the Document.innerHTML algorithm
        // from the HTML5 specification to serialize the document.
        String body = createMarkup(document);

        // FIXME: This should use value of document.inputEncoding to determine the encoding to use.
        m_requestEntityBody = FormData::create(UTF8Encoding().normalizeAndEncode(body, WTF::EntitiesForUnencodables));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(es);
}

void XMLHttpRequest::send(const String& body, ExceptionState& es)
{
    if (!initSend(es))
        return;

    if (!body.isNull() && areMethodAndURLValidForSend()) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
            setRequestHeaderInternal("Content-Type", "text/plain;charset=UTF-8");
        } else {
            replaceCharsetInMediaType(contentType, "UTF-8");
            m_requestHeaders.set("Content-Type", contentType);
        }

        m_requestEntityBody = FormData::create(UTF8Encoding().normalizeAndEncode(body, WTF::EntitiesForUnencodables));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(es);
}

void XMLHttpRequest::send(Blob* body, ExceptionState& es)
{
    if (!initSend(es))
        return;

    if (areMethodAndURLValidForSend()) {
        const String& contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
            const String& blobType = body->type();
            if (!blobType.isEmpty() && isValidContentType(blobType))
                setRequestHeaderInternal("Content-Type", blobType);
            else {
                // From FileAPI spec, whenever media type cannot be determined, empty string must be returned.
                setRequestHeaderInternal("Content-Type", "");
            }
        }

        // FIXME: add support for uploading bundles.
        m_requestEntityBody = FormData::create();
        if (body->isFile())
            m_requestEntityBody->appendFile(toFile(body)->path());
        else
            m_requestEntityBody->appendBlob(body->url());
    }

    createRequest(es);
}

void XMLHttpRequest::send(DOMFormData* body, ExceptionState& es)
{
    if (!initSend(es))
        return;

    if (areMethodAndURLValidForSend()) {
        m_requestEntityBody = FormData::createMultiPart(*(static_cast<FormDataList*>(body)), body->encoding(), document());

        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
            contentType = String("multipart/form-data; boundary=") + m_requestEntityBody->boundary().data();
            setRequestHeaderInternal("Content-Type", contentType);
        }
    }

    createRequest(es);
}

void XMLHttpRequest::send(ArrayBuffer* body, ExceptionState& es)
{
    String consoleMessage("ArrayBuffer is deprecated in XMLHttpRequest.send(). Use ArrayBufferView instead.");
    scriptExecutionContext()->addConsoleMessage(JSMessageSource, WarningMessageLevel, consoleMessage);

    HistogramSupport::histogramEnumeration("WebCore.XHR.send.ArrayBufferOrView", XMLHttpRequestSendArrayBuffer, XMLHttpRequestSendArrayBufferOrViewMax);

    sendBytesData(body->data(), body->byteLength(), es);
}

void XMLHttpRequest::send(ArrayBufferView* body, ExceptionState& es)
{
    HistogramSupport::histogramEnumeration("WebCore.XHR.send.ArrayBufferOrView", XMLHttpRequestSendArrayBufferView, XMLHttpRequestSendArrayBufferOrViewMax);

    sendBytesData(body->baseAddress(), body->byteLength(), es);
}

void XMLHttpRequest::sendBytesData(const void* data, size_t length, ExceptionState& es)
{
    if (!initSend(es))
        return;

    if (areMethodAndURLValidForSend()) {
        m_requestEntityBody = FormData::create(data, length);
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(es);
}

void XMLHttpRequest::sendForInspectorXHRReplay(PassRefPtr<FormData> formData, ExceptionState& es)
{
    m_requestEntityBody = formData ? formData->deepCopy() : 0;
    createRequest(es);
    m_exceptionCode = es.code();
}

void XMLHttpRequest::createRequest(ExceptionState& es)
{
    // Only GET request is supported for blob URL.
    if (m_url.protocolIs("blob") && m_method != "GET") {
        es.throwDOMException(NetworkError);
        return;
    }

    // The presence of upload event listeners forces us to use preflighting because POSTing to an URL that does not
    // permit cross origin requests should look exactly like POSTing to an URL that does not respond at all.
    // Also, only async requests support upload progress events.
    bool uploadEvents = false;
    if (m_async) {
        m_progressEventThrottle.dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadstartEvent));
        if (m_requestEntityBody && m_upload) {
            uploadEvents = m_upload->hasEventListeners();
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().loadstartEvent));
        }
    }

    m_sameOriginRequest = securityOrigin()->canRequest(m_url);

    // We also remember whether upload events should be allowed for this request in case the upload listeners are
    // added after the request is started.
    m_uploadEventsAllowed = m_sameOriginRequest || uploadEvents || !isSimpleCrossOriginAccessRequest(m_method, m_requestHeaders);

    ResourceRequest request(m_url);
    request.setHTTPMethod(m_method);
    request.setTargetType(ResourceRequest::TargetIsXHR);

    InspectorInstrumentation::willLoadXHR(scriptExecutionContext(), this, m_method, m_url, m_async, m_requestEntityBody ? m_requestEntityBody->deepCopy() : 0, m_requestHeaders, m_includeCredentials);

    if (m_requestEntityBody) {
        ASSERT(m_method != "GET");
        ASSERT(m_method != "HEAD");
        request.setHTTPBody(m_requestEntityBody.release());
    }

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbacks;
    options.sniffContent = DoNotSniffContent;
    options.preflightPolicy = uploadEvents ? ForcePreflight : ConsiderPreflight;
    options.allowCredentials = (m_sameOriginRequest || m_includeCredentials) ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    options.credentialsRequested = m_includeCredentials ? ClientRequestedCredentials : ClientDidNotRequestCredentials;
    options.crossOriginRequestPolicy = m_allowCrossOriginRequests ? AllowCrossOriginRequests : UseAccessControl;
    options.securityOrigin = securityOrigin();
    options.initiator = FetchInitiatorTypeNames::xmlhttprequest;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicy::shouldBypassMainWorld(scriptExecutionContext()) ? DoNotEnforceContentSecurityPolicy : EnforceConnectSrcDirective;
    options.timeoutMilliseconds = m_timeoutMilliseconds;

    m_exceptionCode = 0;
    m_error = false;

    if (m_async) {
        if (m_upload)
            request.setReportUploadProgress(true);

        // ThreadableLoader::create can return null here, for example if we're no longer attached to a page.
        // This is true while running onunload handlers.
        // FIXME: Maybe we need to be able to send XMLHttpRequests from onunload, <http://bugs.webkit.org/show_bug.cgi?id=10904>.
        // FIXME: Maybe create() can return null for other reasons too?
        m_loader = ThreadableLoader::create(scriptExecutionContext(), this, request, options);
        if (m_loader) {
            // Neither this object nor the JavaScript wrapper should be deleted while
            // a request is in progress because we need to keep the listeners alive,
            // and they are referenced by the JavaScript wrapper.
            setPendingActivity(this);
        }
    } else {
        request.setPriority(ResourceLoadPriorityVeryHigh);
        InspectorInstrumentation::willLoadXHRSynchronously(scriptExecutionContext());
        ThreadableLoader::loadResourceSynchronously(scriptExecutionContext(), request, *this, options);
        InspectorInstrumentation::didLoadXHRSynchronously(scriptExecutionContext());
    }

    if (!m_exceptionCode && m_error)
        m_exceptionCode = NetworkError;
    if (m_exceptionCode)
        es.throwDOMException(m_exceptionCode);
}

void XMLHttpRequest::abort()
{
    // internalAbort() calls dropProtection(), which may release the last reference.
    RefPtr<XMLHttpRequest> protect(this);

    bool sendFlag = m_loader;

    internalAbort();

    clearResponseBuffers();

    // Clear headers as required by the spec
    m_requestHeaders.clear();

    if ((m_state <= OPENED && !sendFlag) || m_state == DONE)
        m_state = UNSENT;
    else {
        ASSERT(!m_loader);
        changeState(DONE);
        m_state = UNSENT;
    }

    m_progressEventThrottle.dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    }
}

void XMLHttpRequest::internalAbort()
{
    bool hadLoader = m_loader;

    m_error = true;

    // FIXME: when we add the support for multi-part XHR, we will have to think be careful with this initialization.
    m_receivedLength = 0;

    if (hadLoader) {
        m_loader->cancel();
        m_loader = 0;
    }

    m_decoder = 0;

    InspectorInstrumentation::didFailXHRLoading(scriptExecutionContext(), this);

    if (hadLoader)
        dropProtectionSoon();
}

void XMLHttpRequest::clearResponse()
{
    m_response = ResourceResponse();
    clearResponseBuffers();
}

void XMLHttpRequest::clearResponseBuffers()
{
    m_responseText.clear();
    m_responseEncoding = String();
    m_createdDocument = false;
    m_responseDocument = 0;
    m_responseBlob = 0;
    m_binaryResponseBuilder.clear();
    m_responseArrayBuffer.clear();
}

void XMLHttpRequest::clearRequest()
{
    m_requestHeaders.clear();
    m_requestEntityBody = 0;
}

void XMLHttpRequest::genericError()
{
    clearResponse();
    clearRequest();
    m_error = true;

    changeState(DONE);
}

void XMLHttpRequest::networkError()
{
    genericError();
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().errorEvent));
    }
    m_progressEventThrottle.dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().errorEvent));
    internalAbort();
}

void XMLHttpRequest::abortError()
{
    genericError();
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
    }
    m_progressEventThrottle.dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().abortEvent));
}

void XMLHttpRequest::dropProtectionSoon()
{
    if (m_protectionTimer.isActive())
        return;
    m_protectionTimer.startOneShot(0);
}

void XMLHttpRequest::dropProtection(Timer<XMLHttpRequest>*)
{
    unsetPendingActivity(this);
}

void XMLHttpRequest::overrideMimeType(const String& override)
{
    m_mimeTypeOverride = override;
}

void XMLHttpRequest::setRequestHeader(const AtomicString& name, const String& value, ExceptionState& es)
{
    if (m_state != OPENED || m_loader) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    if (!isValidHTTPToken(name) || !isValidHTTPHeaderValue(value)) {
        es.throwDOMException(SyntaxError);
        return;
    }

    // No script (privileged or not) can set unsafe headers.
    if (!isAllowedHTTPHeader(name)) {
        logConsoleError(scriptExecutionContext(), "Refused to set unsafe header \"" + name + "\"");
        return;
    }

    setRequestHeaderInternal(name, value);
}

void XMLHttpRequest::setRequestHeaderInternal(const AtomicString& name, const String& value)
{
    HTTPHeaderMap::AddResult result = m_requestHeaders.add(name, value);
    if (!result.isNewEntry)
        result.iterator->value = result.iterator->value + ", " + value;
}

String XMLHttpRequest::getRequestHeader(const AtomicString& name) const
{
    return m_requestHeaders.get(name);
}

String XMLHttpRequest::getAllResponseHeaders(ExceptionState& es) const
{
    if (m_state < HEADERS_RECEIVED) {
        es.throwDOMException(InvalidStateError);
        return "";
    }

    StringBuilder stringBuilder;

    HTTPHeaderSet accessControlExposeHeaderSet;
    parseAccessControlExposeHeadersAllowList(m_response.httpHeaderField("Access-Control-Expose-Headers"), accessControlExposeHeaderSet);
    HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin(); it!= end; ++it) {
        // Hide Set-Cookie header fields from the XMLHttpRequest client for these reasons:
        //     1) If the client did have access to the fields, then it could read HTTP-only
        //        cookies; those cookies are supposed to be hidden from scripts.
        //     2) There's no known harm in hiding Set-Cookie header fields entirely; we don't
        //        know any widely used technique that requires access to them.
        //     3) Firefox has implemented this policy.
        if (isSetCookieHeader(it->key) && !securityOrigin()->canLoadLocalResources())
            continue;

        if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(it->key) && !accessControlExposeHeaderSet.contains(it->key))
            continue;

        stringBuilder.append(it->key);
        stringBuilder.append(':');
        stringBuilder.append(' ');
        stringBuilder.append(it->value);
        stringBuilder.append('\r');
        stringBuilder.append('\n');
    }

    return stringBuilder.toString();
}

String XMLHttpRequest::getResponseHeader(const AtomicString& name, ExceptionState& es) const
{
    if (m_state < HEADERS_RECEIVED) {
        es.throwDOMException(InvalidStateError);
        return String();
    }

    // See comment in getAllResponseHeaders above.
    if (isSetCookieHeader(name) && !securityOrigin()->canLoadLocalResources()) {
        logConsoleError(scriptExecutionContext(), "Refused to get unsafe header \"" + name + "\"");
        return String();
    }

    HTTPHeaderSet accessControlExposeHeaderSet;
    parseAccessControlExposeHeadersAllowList(m_response.httpHeaderField("Access-Control-Expose-Headers"), accessControlExposeHeaderSet);

    if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(name) && !accessControlExposeHeaderSet.contains(name)) {
        logConsoleError(scriptExecutionContext(), "Refused to get unsafe header \"" + name + "\"");
        return String();
    }
    return m_response.httpHeaderField(name);
}

String XMLHttpRequest::responseMIMEType() const
{
    String mimeType = extractMIMETypeFromMediaType(m_mimeTypeOverride);
    if (mimeType.isEmpty()) {
        if (m_response.isHTTP())
            mimeType = extractMIMETypeFromMediaType(m_response.httpHeaderField("Content-Type"));
        else
            mimeType = m_response.mimeType();
    }
    if (mimeType.isEmpty())
        mimeType = "text/xml";

    return mimeType;
}

bool XMLHttpRequest::responseIsXML() const
{
    // FIXME: Remove the lower() call when DOMImplementation.isXMLMIMEType() is modified
    //        to do case insensitive MIME type matching.
    return DOMImplementation::isXMLMIMEType(responseMIMEType().lower());
}

int XMLHttpRequest::status(ExceptionState& es) const
{
    if (m_response.httpStatusCode())
        return m_response.httpStatusCode();

    if (m_state == OPENED) {
        // Firefox only raises an exception in this state; we match it.
        // Note the case of local file requests, where we have no HTTP response code! Firefox never raises an exception for those, but we match HTTP case for consistency.
        es.throwDOMException(InvalidStateError);
    }

    return 0;
}

String XMLHttpRequest::statusText(ExceptionState& es) const
{
    if (!m_response.httpStatusText().isNull())
        return m_response.httpStatusText();

    if (m_state == OPENED) {
        // See comments in status() above.
        es.throwDOMException(InvalidStateError);
    }

    return String();
}

void XMLHttpRequest::didFail(const ResourceError& error)
{

    // If we are already in an error state, for instance we called abort(), bail out early.
    if (m_error)
        return;

    if (error.isCancellation()) {
        m_exceptionCode = AbortError;
        abortError();
        return;
    }

    if (error.isTimeout()) {
        didTimeout();
        return;
    }

    // Network failures are already reported to Web Inspector by ResourceLoader.
    if (error.domain() == errorDomainWebKitInternal)
        logConsoleError(scriptExecutionContext(), "XMLHttpRequest cannot load " + error.failingURL() + ". " + error.localizedDescription());

    m_exceptionCode = NetworkError;
    networkError();
}

void XMLHttpRequest::didFailRedirectCheck()
{
    networkError();
}

void XMLHttpRequest::didFinishLoading(unsigned long identifier, double)
{
    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (m_decoder)
        m_responseText = m_responseText.concatenateWith(m_decoder->flush());

    InspectorInstrumentation::didFinishXHRLoading(scriptExecutionContext(), this, identifier, m_responseText, m_url, m_lastSendURL, m_lastSendLineNumber);

    bool hadLoader = m_loader;
    m_loader = 0;

    changeState(DONE);
    m_responseEncoding = String();
    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!m_upload)
        return;

    if (m_uploadEventsAllowed)
        m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(eventNames().progressEvent, true, bytesSent, totalBytesToBeSent));

    if (bytesSent == totalBytesToBeSent && !m_uploadComplete) {
        m_uploadComplete = true;
        if (m_uploadEventsAllowed)
            m_upload->dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().loadEvent));
    }
}

void XMLHttpRequest::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    InspectorInstrumentation::didReceiveXHRResponse(scriptExecutionContext(), identifier);

    m_response = response;
    if (!m_mimeTypeOverride.isEmpty()) {
        m_response.setHTTPHeaderField("Content-Type", m_mimeTypeOverride);
        m_responseEncoding = extractCharsetFromMediaType(m_mimeTypeOverride);
    }

    if (m_responseEncoding.isEmpty())
        m_responseEncoding = response.textEncodingName();
}

void XMLHttpRequest::didReceiveData(const char* data, int len)
{
    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    bool useDecoder = m_responseTypeCode == ResponseTypeDefault || m_responseTypeCode == ResponseTypeText || m_responseTypeCode == ResponseTypeDocument;

    if (useDecoder && !m_decoder) {
        if (!m_responseEncoding.isEmpty())
            m_decoder = TextResourceDecoder::create("text/plain", m_responseEncoding);
        // allow TextResourceDecoder to look inside the m_response if it's XML or HTML
        else if (responseIsXML()) {
            m_decoder = TextResourceDecoder::create("application/xml");
            // Don't stop on encoding errors, unlike it is done for other kinds of XML resources. This matches the behavior of previous WebKit versions, Firefox and Opera.
            m_decoder->useLenientXMLDecoding();
        } else if (equalIgnoringCase(responseMIMEType(), "text/html"))
            m_decoder = TextResourceDecoder::create("text/html", "UTF-8");
        else
            m_decoder = TextResourceDecoder::create("text/plain", "UTF-8");
    }

    if (!len)
        return;

    if (len == -1)
        len = strlen(data);

    if (useDecoder)
        m_responseText = m_responseText.concatenateWith(m_decoder->decode(data, len));
    else if (m_responseTypeCode == ResponseTypeArrayBuffer || m_responseTypeCode == ResponseTypeBlob) {
        // Buffer binary data.
        if (!m_binaryResponseBuilder)
            m_binaryResponseBuilder = SharedBuffer::create();
        m_binaryResponseBuilder->append(data, len);
    }

    if (!m_error) {
        long long expectedLength = m_response.expectedContentLength();
        m_receivedLength += len;

        if (m_async) {
            bool lengthComputable = expectedLength > 0 && m_receivedLength <= expectedLength;
            unsigned long long total = lengthComputable ? expectedLength : 0;
            m_progressEventThrottle.dispatchProgressEvent(lengthComputable, m_receivedLength, total);
        }

        if (m_state != LOADING)
            changeState(LOADING);
        else
            // Firefox calls readyStateChanged every time it receives data, 4449442
            callReadyStateChangeListener();
    }
}

void XMLHttpRequest::didTimeout()
{
    // internalAbort() calls dropProtection(), which may release the last reference.
    RefPtr<XMLHttpRequest> protect(this);
    internalAbort();

    clearResponse();
    clearRequest();

    m_error = true;
    m_exceptionCode = TimeoutError;

    if (!m_async) {
        m_state = DONE;
        m_exceptionCode = TimeoutError;
        return;
    }

    changeState(DONE);

    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().timeoutEvent));
    }
    m_progressEventThrottle.dispatchEventAndLoadEnd(XMLHttpRequestProgressEvent::create(eventNames().timeoutEvent));
}

bool XMLHttpRequest::canSuspend() const
{
    return !m_loader;
}

void XMLHttpRequest::suspend(ReasonForSuspension)
{
    m_progressEventThrottle.suspend();
}

void XMLHttpRequest::resume()
{
    m_progressEventThrottle.resume();
}

void XMLHttpRequest::stop()
{
    internalAbort();
}

void XMLHttpRequest::contextDestroyed()
{
    ASSERT(!m_loader);
    ActiveDOMObject::contextDestroyed();
}

const AtomicString& XMLHttpRequest::interfaceName() const
{
    return eventNames().interfaceForXMLHttpRequest;
}

ScriptExecutionContext* XMLHttpRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetData* XMLHttpRequest::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* XMLHttpRequest::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore
