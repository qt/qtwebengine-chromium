/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/inspector/InspectorPageAgent.h"

#include "HTMLNames.h"
#include "UserAgentStyleSheets.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptRegexp.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ScriptResource.h"
#include "core/fetch/TextResourceDecoder.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ContentSearchUtils.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/CookieJar.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PageConsole.h"
#include "core/frame/Settings.h"
#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "modules/geolocation/GeolocationController.h"
#include "platform/Cookie.h"
#include "platform/JSONValues.h"
#include "platform/UserGestureIndicator.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/Base64.h"
#include "wtf/text/TextEncoding.h"

namespace WebCore {

namespace PageAgentState {
static const char pageAgentEnabled[] = "pageAgentEnabled";
static const char pageAgentScriptExecutionDisabled[] = "pageAgentScriptExecutionDisabled";
static const char pageAgentScriptsToEvaluateOnLoad[] = "pageAgentScriptsToEvaluateOnLoad";
static const char pageAgentScreenWidthOverride[] = "pageAgentScreenWidthOverride";
static const char pageAgentScreenHeightOverride[] = "pageAgentScreenHeightOverride";
static const char pageAgentDeviceScaleFactorOverride[] = "pageAgentDeviceScaleFactorOverride";
static const char pageAgentEmulateViewport[] = "pageAgentEmulateViewport";
static const char pageAgentFitWindow[] = "pageAgentFitWindow";
static const char fontScaleFactor[] = "fontScaleFactor";
static const char pageAgentShowFPSCounter[] = "pageAgentShowFPSCounter";
static const char pageAgentTextAutosizingOverride[] = "pageAgentTextAutosizingOverride";
static const char pageAgentContinuousPaintingEnabled[] = "pageAgentContinuousPaintingEnabled";
static const char pageAgentShowPaintRects[] = "pageAgentShowPaintRects";
static const char pageAgentShowDebugBorders[] = "pageAgentShowDebugBorders";
static const char pageAgentShowScrollBottleneckRects[] = "pageAgentShowScrollBottleneckRects";
static const char touchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char pageAgentEmulatedMedia[] = "pageAgentEmulatedMedia";
static const char showSizeOnResize[] = "showSizeOnResize";
static const char showGridOnResize[] = "showGridOnResize";
}

namespace {

KURL urlWithoutFragment(const KURL& url)
{
    KURL result = url;
    result.removeFragmentIdentifier();
    return result;
}

}

static bool decodeBuffer(const char* buffer, unsigned size, const String& textEncodingName, String* result)
{
    if (buffer) {
        WTF::TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer, size);
        return true;
    }
    return false;
}

static bool prepareResourceBuffer(Resource* cachedResource, bool* hasZeroSize)
{
    *hasZeroSize = false;
    if (!cachedResource)
        return false;

    if (cachedResource->dataBufferingPolicy() == DoNotBufferData)
        return false;

    // Zero-sized resources don't have data at all -- so fake the empty buffer, instead of indicating error by returning 0.
    if (!cachedResource->encodedSize()) {
        *hasZeroSize = true;
        return true;
    }

    if (cachedResource->isPurgeable()) {
        // If the resource is purgeable then make it unpurgeable to get
        // get its data. This might fail, in which case we return an
        // empty String.
        // FIXME: should we do something else in the case of a purged
        // resource that informs the user why there is no data in the
        // inspector?
        if (!cachedResource->makePurgeable(false))
            return false;
    }

    return true;
}

static bool hasTextContent(Resource* cachedResource)
{
    InspectorPageAgent::ResourceType type = InspectorPageAgent::cachedResourceType(*cachedResource);
    return type == InspectorPageAgent::DocumentResource || type == InspectorPageAgent::StylesheetResource || type == InspectorPageAgent::ScriptResource || type == InspectorPageAgent::XHRResource;
}

static PassOwnPtr<TextResourceDecoder> createXHRTextDecoder(const String& mimeType, const String& textEncodingName)
{
    if (!textEncodingName.isEmpty())
        return TextResourceDecoder::create("text/plain", textEncodingName);
    if (DOMImplementation::isXMLMIMEType(mimeType.lower())) {
        OwnPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("application/xml");
        decoder->useLenientXMLDecoding();
        return decoder.release();
    }
    if (equalIgnoringCase(mimeType, "text/html"))
        return TextResourceDecoder::create("text/html", "UTF-8");
    return TextResourceDecoder::create("text/plain", "UTF-8");
}

bool InspectorPageAgent::cachedResourceContent(Resource* cachedResource, String* result, bool* base64Encoded)
{
    bool hasZeroSize;
    bool prepared = prepareResourceBuffer(cachedResource, &hasZeroSize);
    if (!prepared)
        return false;

    *base64Encoded = !hasTextContent(cachedResource);
    if (*base64Encoded) {
        RefPtr<SharedBuffer> buffer = hasZeroSize ? SharedBuffer::create() : cachedResource->resourceBuffer();

        if (!buffer)
            return false;

        *result = base64Encode(buffer->data(), buffer->size());
        return true;
    }

    if (hasZeroSize) {
        *result = "";
        return true;
    }

    if (cachedResource) {
        switch (cachedResource->type()) {
        case Resource::CSSStyleSheet:
            *result = toCSSStyleSheetResource(cachedResource)->sheetText(false);
            return true;
        case Resource::Script:
            *result = toScriptResource(cachedResource)->script();
            return true;
        case Resource::MainResource:
            return false;
        case Resource::Raw: {
            SharedBuffer* buffer = cachedResource->resourceBuffer();
            if (!buffer)
                return false;
            OwnPtr<TextResourceDecoder> decoder = createXHRTextDecoder(cachedResource->response().mimeType(), cachedResource->response().textEncodingName());
            String content = decoder->decode(buffer->data(), buffer->size());
            *result = content + decoder->flush();
            return true;
        }
        default:
            SharedBuffer* buffer = cachedResource->resourceBuffer();
            return decodeBuffer(buffer ? buffer->data() : 0, buffer ? buffer->size() : 0, cachedResource->response().textEncodingName(), result);
        }
    }
    return false;
}

// static
bool InspectorPageAgent::sharedBufferContent(PassRefPtr<SharedBuffer> buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->data() : 0, buffer ? buffer->size() : 0, textEncodingName, withBase64Encode, result);
}

bool InspectorPageAgent::dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64Encode(data, size);
        return true;
    }

    return decodeBuffer(data, size, textEncodingName, result);
}

PassOwnPtr<InspectorPageAgent> InspectorPageAgent::create(InstrumentingAgents* instrumentingAgents, Page* page, InspectorCompositeState* state, InjectedScriptManager* injectedScriptManager, InspectorClient* client, InspectorOverlay* overlay)
{
    return adoptPtr(new InspectorPageAgent(instrumentingAgents, page, state, injectedScriptManager, client, overlay));
}

// static
void InspectorPageAgent::resourceContent(ErrorString* errorString, Frame* frame, const KURL& url, String* result, bool* base64Encoded)
{
    DocumentLoader* loader = assertDocumentLoader(errorString, frame);
    if (!loader)
        return;
    if (!cachedResourceContent(cachedResource(frame, url), result, base64Encoded))
        *errorString = "No resource with given URL found";
}

Resource* InspectorPageAgent::cachedResource(Frame* frame, const KURL& url)
{
    Resource* cachedResource = frame->document()->fetcher()->cachedResource(url);
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url);
    return cachedResource;
}

TypeBuilder::Page::ResourceType::Enum InspectorPageAgent::resourceTypeJson(InspectorPageAgent::ResourceType resourceType)
{
    switch (resourceType) {
    case DocumentResource:
        return TypeBuilder::Page::ResourceType::Document;
    case ImageResource:
        return TypeBuilder::Page::ResourceType::Image;
    case Font:
        return TypeBuilder::Page::ResourceType::Font;
    case StylesheetResource:
        return TypeBuilder::Page::ResourceType::Stylesheet;
    case ScriptResource:
        return TypeBuilder::Page::ResourceType::Script;
    case XHRResource:
        return TypeBuilder::Page::ResourceType::XHR;
    case WebSocketResource:
        return TypeBuilder::Page::ResourceType::WebSocket;
    case OtherResource:
        return TypeBuilder::Page::ResourceType::Other;
    }
    return TypeBuilder::Page::ResourceType::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::cachedResourceType(const Resource& cachedResource)
{
    switch (cachedResource.type()) {
    case Resource::Image:
        return InspectorPageAgent::ImageResource;
    case Resource::Font:
        return InspectorPageAgent::Font;
    case Resource::CSSStyleSheet:
        // Fall through.
    case Resource::XSLStyleSheet:
        return InspectorPageAgent::StylesheetResource;
    case Resource::Script:
        return InspectorPageAgent::ScriptResource;
    case Resource::Raw:
        return InspectorPageAgent::XHRResource;
    case Resource::MainResource:
        return InspectorPageAgent::DocumentResource;
    default:
        break;
    }
    return InspectorPageAgent::OtherResource;
}

TypeBuilder::Page::ResourceType::Enum InspectorPageAgent::cachedResourceTypeJson(const Resource& cachedResource)
{
    return resourceTypeJson(cachedResourceType(cachedResource));
}

InspectorPageAgent::InspectorPageAgent(InstrumentingAgents* instrumentingAgents, Page* page, InspectorCompositeState* inspectorState, InjectedScriptManager* injectedScriptManager, InspectorClient* client, InspectorOverlay* overlay)
    : InspectorBaseAgent<InspectorPageAgent>("Page", instrumentingAgents, inspectorState)
    , m_page(page)
    , m_injectedScriptManager(injectedScriptManager)
    , m_client(client)
    , m_frontend(0)
    , m_overlay(overlay)
    , m_lastScriptIdentifier(0)
    , m_enabled(false)
    , m_geolocationOverridden(false)
    , m_ignoreScriptsEnabledNotification(false)
    , m_deviceMetricsOverridden(false)
    , m_emulateViewportEnabled(false)
{
}

void InspectorPageAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->page();
}

void InspectorPageAgent::clearFrontend()
{
    ErrorString error;
    disable(&error);
    updateTouchEventEmulationInPage(false);
    m_frontend = 0;
}

void InspectorPageAgent::restore()
{
    if (m_state->getBoolean(PageAgentState::pageAgentEnabled)) {
        ErrorString error;
        enable(&error);
        bool scriptExecutionDisabled = m_state->getBoolean(PageAgentState::pageAgentScriptExecutionDisabled);
        setScriptExecutionDisabled(0, scriptExecutionDisabled);
        bool showPaintRects = m_state->getBoolean(PageAgentState::pageAgentShowPaintRects);
        setShowPaintRects(0, showPaintRects);
        bool showDebugBorders = m_state->getBoolean(PageAgentState::pageAgentShowDebugBorders);
        setShowDebugBorders(0, showDebugBorders);
        bool showFPSCounter = m_state->getBoolean(PageAgentState::pageAgentShowFPSCounter);
        setShowFPSCounter(0, showFPSCounter);
        String emulatedMedia = m_state->getString(PageAgentState::pageAgentEmulatedMedia);
        setEmulatedMedia(0, emulatedMedia);
        bool continuousPaintingEnabled = m_state->getBoolean(PageAgentState::pageAgentContinuousPaintingEnabled);
        setContinuousPaintingEnabled(0, continuousPaintingEnabled);
        bool showScrollBottleneckRects = m_state->getBoolean(PageAgentState::pageAgentShowScrollBottleneckRects);
        setShowScrollBottleneckRects(0, showScrollBottleneckRects);

        int currentWidth = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenWidthOverride));
        int currentHeight = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenHeightOverride));
        double currentDeviceScaleFactor = m_state->getDouble(PageAgentState::pageAgentDeviceScaleFactorOverride);
        bool currentEmulateViewport = m_state->getBoolean(PageAgentState::pageAgentEmulateViewport);
        bool currentFitWindow = m_state->getBoolean(PageAgentState::pageAgentFitWindow);
        updateViewMetrics(currentWidth, currentHeight, currentDeviceScaleFactor, currentEmulateViewport, currentFitWindow);
        updateTouchEventEmulationInPage(m_state->getBoolean(PageAgentState::touchEventEmulationEnabled));
    }
}

void InspectorPageAgent::webViewResized(const IntSize& size)
{
    int currentWidth = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenWidthOverride));
    m_overlay->resize(currentWidth ? size : IntSize());
}

void InspectorPageAgent::enable(ErrorString*)
{
    m_enabled = true;
    m_state->setBoolean(PageAgentState::pageAgentEnabled, true);
    m_instrumentingAgents->setInspectorPageAgent(this);
}

void InspectorPageAgent::disable(ErrorString*)
{
    m_enabled = false;
    m_state->setBoolean(PageAgentState::pageAgentEnabled, false);
    m_state->remove(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    m_overlay->hide();
    m_instrumentingAgents->setInspectorPageAgent(0);
    m_deviceMetricsOverridden = false;

    setShowPaintRects(0, false);
    setShowDebugBorders(0, false);
    setShowFPSCounter(0, false);
    setEmulatedMedia(0, String());
    setContinuousPaintingEnabled(0, false);
    setShowScrollBottleneckRects(0, false);
    setShowViewportSizeOnResize(0, false, 0);

    if (!deviceMetricsChanged(0, 0, 1, false, false, 1, false))
        return;

    // When disabling the agent, reset the override values if necessary.
    updateViewMetrics(0, 0, 1, false, false);
    m_state->setLong(PageAgentState::pageAgentScreenWidthOverride, 0);
    m_state->setLong(PageAgentState::pageAgentScreenHeightOverride, 0);
    m_state->setDouble(PageAgentState::pageAgentDeviceScaleFactorOverride, 1);
    m_state->setBoolean(PageAgentState::pageAgentEmulateViewport, false);
    m_state->setBoolean(PageAgentState::pageAgentFitWindow, false);
    m_state->setDouble(PageAgentState::fontScaleFactor, 1);
    m_state->setBoolean(PageAgentState::pageAgentTextAutosizingOverride, false);
}

void InspectorPageAgent::addScriptToEvaluateOnLoad(ErrorString*, const String& source, String* identifier)
{
    RefPtr<JSONObject> scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (!scripts) {
        scripts = JSONObject::create();
        m_state->setObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad, scripts);
    }
    // Assure we don't override existing ids -- m_lastScriptIdentifier could get out of sync WRT actual
    // scripts once we restored the scripts from the cookie during navigation.
    do {
        *identifier = String::number(++m_lastScriptIdentifier);
    } while (scripts->find(*identifier) != scripts->end());
    scripts->setString(*identifier, source);

    // Force cookie serialization.
    m_state->setObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad, scripts);
}

void InspectorPageAgent::removeScriptToEvaluateOnLoad(ErrorString* error, const String& identifier)
{
    RefPtr<JSONObject> scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (!scripts || scripts->find(identifier) == scripts->end()) {
        *error = "Script not found";
        return;
    }
    scripts->remove(identifier);
}

void InspectorPageAgent::reload(ErrorString*, const bool* const optionalIgnoreCache, const String* optionalScriptToEvaluateOnLoad, const String* optionalScriptPreprocessor)
{
    m_pendingScriptToEvaluateOnLoadOnce = optionalScriptToEvaluateOnLoad ? *optionalScriptToEvaluateOnLoad : "";
    m_pendingScriptPreprocessor = optionalScriptPreprocessor ? *optionalScriptPreprocessor : "";
    m_page->mainFrame()->loader().reload(optionalIgnoreCache && *optionalIgnoreCache ? EndToEndReload : NormalReload);
}

void InspectorPageAgent::navigate(ErrorString*, const String& url)
{
    UserGestureIndicator indicator(DefinitelyProcessingNewUserGesture);
    Frame* frame = m_page->mainFrame();
    FrameLoadRequest request(frame->document(), ResourceRequest(frame->document()->completeURL(url)));
    frame->loader().load(request);
}

void InspectorPageAgent::getNavigationHistory(ErrorString*, int*, RefPtr<TypeBuilder::Array<TypeBuilder::Page::NavigationEntry> >&)
{ }

void InspectorPageAgent::navigateToHistoryEntry(ErrorString*, int)
{ }

static PassRefPtr<TypeBuilder::Page::Cookie> buildObjectForCookie(const Cookie& cookie)
{
    return TypeBuilder::Page::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires)
        .setSize((cookie.name.length() + cookie.value.length()))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSession(cookie.session)
        .release();
}

static PassRefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> > buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> > cookies = TypeBuilder::Array<TypeBuilder::Page::Cookie>::create();

    ListHashSet<Cookie>::iterator end = cookiesList.end();
    ListHashSet<Cookie>::iterator it = cookiesList.begin();
    for (int i = 0; it != end; ++it, i++)
        cookies->addItem(buildObjectForCookie(*it));

    return cookies;
}

static Vector<Resource*> cachedResourcesForFrame(Frame* frame)
{
    Vector<Resource*> result;

    const ResourceFetcher::DocumentResourceMap& allResources = frame->document()->fetcher()->allResources();
    ResourceFetcher::DocumentResourceMap::const_iterator end = allResources.end();
    for (ResourceFetcher::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
        Resource* cachedResource = it->value.get();

        switch (cachedResource->type()) {
        case Resource::Image:
            // Skip images that were not auto loaded (images disabled in the user agent).
            if (toImageResource(cachedResource)->stillNeedsLoad())
                continue;
            break;
        case Resource::Font:
            // Skip fonts that were referenced in CSS but never used/downloaded.
            if (toFontResource(cachedResource)->stillNeedsLoad())
                continue;
            break;
        default:
            // All other Resource types download immediately.
            break;
        }

        result.append(cachedResource);
    }

    return result;
}

static Vector<KURL> allResourcesURLsForFrame(Frame* frame)
{
    Vector<KURL> result;

    result.append(urlWithoutFragment(frame->loader().documentLoader()->url()));

    Vector<Resource*> allResources = cachedResourcesForFrame(frame);
    for (Vector<Resource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it)
        result.append(urlWithoutFragment((*it)->url()));

    return result;
}

void InspectorPageAgent::getCookies(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> >& cookies)
{
    ListHashSet<Cookie> rawCookiesList;

    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext(mainFrame())) {
        Document* document = frame->document();
        Vector<KURL> allURLs = allResourcesURLsForFrame(frame);
        for (Vector<KURL>::const_iterator it = allURLs.begin(); it != allURLs.end(); ++it) {
            Vector<Cookie> docCookiesList;
            getRawCookies(document, *it, docCookiesList);
            int cookiesSize = docCookiesList.size();
            for (int i = 0; i < cookiesSize; i++) {
                if (!rawCookiesList.contains(docCookiesList[i]))
                    rawCookiesList.add(docCookiesList[i]);
            }
        }
    }

    cookies = buildArrayForCookies(rawCookiesList);
}

void InspectorPageAgent::deleteCookie(ErrorString*, const String& cookieName, const String& url)
{
    KURL parsedURL(ParsedURLString, url);
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext(m_page->mainFrame()))
        WebCore::deleteCookie(frame->document(), parsedURL, cookieName);
}

void InspectorPageAgent::getResourceTree(ErrorString*, RefPtr<TypeBuilder::Page::FrameResourceTree>& object)
{
    object = buildObjectForFrameTree(m_page->mainFrame());
}

void InspectorPageAgent::getResourceContent(ErrorString* errorString, const String& frameId, const String& url, String* content, bool* base64Encoded)
{
    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;
    resourceContent(errorString, frame, KURL(ParsedURLString, url), content, base64Encoded);
}

static bool textContentForResource(Resource* cachedResource, String* result)
{
    if (hasTextContent(cachedResource)) {
        String content;
        bool base64Encoded;
        if (InspectorPageAgent::cachedResourceContent(cachedResource, result, &base64Encoded)) {
            ASSERT(!base64Encoded);
            return true;
        }
    }
    return false;
}

void InspectorPageAgent::searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchMatch> >& results)
{
    results = TypeBuilder::Array<TypeBuilder::Page::SearchMatch>::create();

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    Frame* frame = frameForId(frameId);
    KURL kurl(ParsedURLString, url);

    FrameLoader* frameLoader = frame ? &frame->loader() : 0;
    DocumentLoader* loader = frameLoader ? frameLoader->documentLoader() : 0;
    if (!loader)
        return;

    String content;
    bool success = false;
    Resource* resource = cachedResource(frame, kurl);
    if (resource)
        success = textContentForResource(resource, &content);

    if (!success)
        return;

    results = ContentSearchUtils::searchInTextByLines(content, query, caseSensitive, isRegex);
}

void InspectorPageAgent::setDocumentContent(ErrorString* errorString, const String& frameId, const String& html)
{
    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;

    Document* document = frame->document();
    if (!document) {
        *errorString = "No Document instance to set HTML for";
        return;
    }
    DOMPatchSupport::patchDocument(*document, html);
}

void InspectorPageAgent::setDeviceMetricsOverride(ErrorString* errorString, int width, int height, double deviceScaleFactor, bool emulateViewport, bool fitWindow, const bool* optionalTextAutosizing, const double* optionalFontScaleFactor)
{
    const static long maxDimension = 10000000;

    bool textAutosizing = optionalTextAutosizing ? *optionalTextAutosizing : false;
    double fontScaleFactor = optionalFontScaleFactor ? *optionalFontScaleFactor : 1;

    if (width < 0 || height < 0 || width > maxDimension || height > maxDimension) {
        *errorString = "Width and height values must be positive, not greater than " + String::number(maxDimension);
        return;
    }

    if (!width ^ !height) {
        *errorString = "Both width and height must be either zero or non-zero at once";
        return;
    }

    if (deviceScaleFactor <= 0) {
        *errorString = "deviceScaleFactor must be positive";
        return;
    }

    if (fontScaleFactor <= 0) {
        *errorString = "fontScaleFactor must be positive";
        return;
    }

    Settings& settings = m_page->settings();
    if (width && height && !settings.acceleratedCompositingEnabled()) {
        if (errorString)
            *errorString = "Compositing mode is not supported";
        return;
    }

    if (!deviceMetricsChanged(width, height, deviceScaleFactor, emulateViewport, fitWindow, fontScaleFactor, textAutosizing))
        return;


    m_state->setLong(PageAgentState::pageAgentScreenWidthOverride, width);
    m_state->setLong(PageAgentState::pageAgentScreenHeightOverride, height);
    m_state->setDouble(PageAgentState::pageAgentDeviceScaleFactorOverride, deviceScaleFactor);
    m_state->setBoolean(PageAgentState::pageAgentEmulateViewport, emulateViewport);
    m_state->setBoolean(PageAgentState::pageAgentFitWindow, fitWindow);
    m_state->setDouble(PageAgentState::fontScaleFactor, fontScaleFactor);
    m_state->setBoolean(PageAgentState::pageAgentTextAutosizingOverride, textAutosizing);

    updateViewMetrics(width, height, deviceScaleFactor, emulateViewport, fitWindow);
}

bool InspectorPageAgent::deviceMetricsChanged(int width, int height, double deviceScaleFactor, bool emulateViewport, bool fitWindow, double fontScaleFactor, bool textAutosizing)
{
    // These two always fit an int.
    int currentWidth = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenWidthOverride));
    int currentHeight = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenHeightOverride));
    double currentDeviceScaleFactor = m_state->getDouble(PageAgentState::pageAgentDeviceScaleFactorOverride, 1);
    bool currentEmulateViewport = m_state->getBoolean(PageAgentState::pageAgentEmulateViewport);
    bool currentFitWindow = m_state->getBoolean(PageAgentState::pageAgentFitWindow);
    double currentFontScaleFactor = m_state->getDouble(PageAgentState::fontScaleFactor, 1);
    bool currentTextAutosizing = m_state->getBoolean(PageAgentState::pageAgentTextAutosizingOverride);

    return width != currentWidth || height != currentHeight || deviceScaleFactor != currentDeviceScaleFactor || emulateViewport != currentEmulateViewport || fitWindow != currentFitWindow || fontScaleFactor != currentFontScaleFactor || textAutosizing != currentTextAutosizing;
}

void InspectorPageAgent::setShowPaintRects(ErrorString*, bool show)
{
    m_state->setBoolean(PageAgentState::pageAgentShowPaintRects, show);
    m_client->setShowPaintRects(show);

    if (!show && mainFrame() && mainFrame()->view())
        mainFrame()->view()->invalidate();
}

void InspectorPageAgent::setShowDebugBorders(ErrorString* errorString, bool show)
{
    m_state->setBoolean(PageAgentState::pageAgentShowDebugBorders, show);
    if (show && !forceCompositingMode(errorString))
        return;
    m_client->setShowDebugBorders(show);
}

void InspectorPageAgent::setShowFPSCounter(ErrorString* errorString, bool show)
{
    // FIXME: allow metrics override, fps counter and continuous painting at the same time: crbug.com/299837.
    m_state->setBoolean(PageAgentState::pageAgentShowFPSCounter, show);
    if (show && !forceCompositingMode(errorString))
        return;
    m_client->setShowFPSCounter(show && !m_deviceMetricsOverridden);
}

void InspectorPageAgent::setContinuousPaintingEnabled(ErrorString* errorString, bool enabled)
{
    m_state->setBoolean(PageAgentState::pageAgentContinuousPaintingEnabled, enabled);
    if (enabled && !forceCompositingMode(errorString))
        return;
    m_client->setContinuousPaintingEnabled(enabled && !m_deviceMetricsOverridden);
}

void InspectorPageAgent::setShowScrollBottleneckRects(ErrorString* errorString, bool show)
{
    m_state->setBoolean(PageAgentState::pageAgentShowScrollBottleneckRects, show);
    if (show && !forceCompositingMode(errorString))
        return;
    m_client->setShowScrollBottleneckRects(show);
}

void InspectorPageAgent::getScriptExecutionStatus(ErrorString*, PageCommandHandler::Result::Enum* status)
{
    bool disabledByScriptController = false;
    bool disabledInSettings = false;
    Frame* frame = mainFrame();
    if (frame) {
        disabledByScriptController = !frame->script().canExecuteScripts(NotAboutToExecuteScript);
        if (frame->settings())
            disabledInSettings = !frame->settings()->isScriptEnabled();
    }

    if (!disabledByScriptController) {
        *status = PageCommandHandler::Result::Allowed;
        return;
    }

    if (disabledInSettings)
        *status = PageCommandHandler::Result::Disabled;
    else
        *status = PageCommandHandler::Result::Forbidden;
}

void InspectorPageAgent::setScriptExecutionDisabled(ErrorString*, bool value)
{
    m_state->setBoolean(PageAgentState::pageAgentScriptExecutionDisabled, value);
    if (!mainFrame())
        return;

    Settings* settings = mainFrame()->settings();
    if (settings) {
        m_ignoreScriptsEnabledNotification = true;
        settings->setScriptEnabled(!value);
        m_ignoreScriptsEnabledNotification = false;
    }
}

void InspectorPageAgent::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (frame == m_page->mainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    if (!m_frontend)
        return;

    RefPtr<JSONObject> scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (scripts) {
        JSONObject::const_iterator end = scripts->end();
        for (JSONObject::const_iterator it = scripts->begin(); it != end; ++it) {
            String scriptText;
            if (it->value->asString(&scriptText))
                frame->script().executeScriptInMainWorld(scriptText);
        }
    }
    if (!m_scriptToEvaluateOnLoadOnce.isEmpty())
        frame->script().executeScriptInMainWorld(m_scriptToEvaluateOnLoadOnce);
}

void InspectorPageAgent::domContentLoadedEventFired(Frame* frame)
{
    if (!frame->isMainFrame())
        return;
    m_frontend->domContentEventFired(currentTime());
}

void InspectorPageAgent::loadEventFired(Frame* frame)
{
    if (!frame->isMainFrame())
        return;
    m_frontend->loadEventFired(currentTime());
}

void InspectorPageAgent::didCommitLoad(Frame*, DocumentLoader* loader)
{
    // FIXME: If "frame" is always guarenteed to be in the same Page as loader->frame()
    // then all we need to check here is loader->frame()->isMainFrame()
    // and we don't need "frame" at all.
    if (loader->frame() == m_page->mainFrame()) {
        m_scriptToEvaluateOnLoadOnce = m_pendingScriptToEvaluateOnLoadOnce;
        m_scriptPreprocessorSource = m_pendingScriptPreprocessor;
        m_pendingScriptToEvaluateOnLoadOnce = String();
        m_pendingScriptPreprocessor = String();
    }
    m_frontend->frameNavigated(buildObjectForFrame(loader->frame()));
}

void InspectorPageAgent::frameAttachedToParent(Frame* frame)
{
    m_frontend->frameAttached(frameId(frame), frameId(frame->tree().parent()));
}

void InspectorPageAgent::frameDetachedFromParent(Frame* frame)
{
    HashMap<Frame*, String>::iterator iterator = m_frameToIdentifier.find(frame);
    if (iterator != m_frameToIdentifier.end()) {
        m_frontend->frameDetached(iterator->value);
        m_identifierToFrame.remove(iterator->value);
        m_frameToIdentifier.remove(iterator);
    }
}

Frame* InspectorPageAgent::mainFrame()
{
    return m_page->mainFrame();
}

Frame* InspectorPageAgent::frameForId(const String& frameId)
{
    return frameId.isEmpty() ? 0 : m_identifierToFrame.get(frameId);
}

String InspectorPageAgent::frameId(Frame* frame)
{
    if (!frame)
        return "";
    String identifier = m_frameToIdentifier.get(frame);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_frameToIdentifier.set(frame, identifier);
        m_identifierToFrame.set(identifier, frame);
    }
    return identifier;
}

bool InspectorPageAgent::hasIdForFrame(Frame* frame) const
{
    return frame && m_frameToIdentifier.contains(frame);
}

String InspectorPageAgent::loaderId(DocumentLoader* loader)
{
    if (!loader)
        return "";
    String identifier = m_loaderToIdentifier.get(loader);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_loaderToIdentifier.set(loader, identifier);
    }
    return identifier;
}

Frame* InspectorPageAgent::findFrameWithSecurityOrigin(const String& originRawString)
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr<SecurityOrigin> documentOrigin = frame->document()->securityOrigin();
        if (documentOrigin->toRawString() == originRawString)
            return frame;
    }
    return 0;
}

Frame* InspectorPageAgent::assertFrame(ErrorString* errorString, const String& frameId)
{
    Frame* frame = frameForId(frameId);
    if (!frame)
        *errorString = "No frame for given id found";
    return frame;
}

const AtomicString& InspectorPageAgent::resourceSourceMapURL(const String& url)
{
    DEFINE_STATIC_LOCAL(const AtomicString, sourceMapHttpHeader, ("SourceMap", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, deprecatedSourceMapHttpHeader, ("X-SourceMap", AtomicString::ConstructFromLiteral));
    if (url.isEmpty())
        return nullAtom;
    Frame* frame = mainFrame();
    if (!frame)
        return nullAtom;
    Resource* resource = cachedResource(frame, KURL(ParsedURLString, url));
    if (!resource)
        return nullAtom;
    const AtomicString& deprecatedHeaderSourceMapURL = resource->response().httpHeaderField(deprecatedSourceMapHttpHeader);
    if (!deprecatedHeaderSourceMapURL.isEmpty()) {
        // FIXME: add deprecated console message here.
        return deprecatedHeaderSourceMapURL;
    }
    return resource->response().httpHeaderField(sourceMapHttpHeader);
}

bool InspectorPageAgent::deviceMetricsOverrideEnabled()
{
    return m_enabled && m_deviceMetricsOverridden;
}

// static
DocumentLoader* InspectorPageAgent::assertDocumentLoader(ErrorString* errorString, Frame* frame)
{
    DocumentLoader* documentLoader = frame->loader().documentLoader();
    if (!documentLoader)
        *errorString = "No documentLoader for given frame found";
    return documentLoader;
}

void InspectorPageAgent::loaderDetachedFromFrame(DocumentLoader* loader)
{
    HashMap<DocumentLoader*, String>::iterator iterator = m_loaderToIdentifier.find(loader);
    if (iterator != m_loaderToIdentifier.end())
        m_loaderToIdentifier.remove(iterator);
}

void InspectorPageAgent::frameStartedLoading(Frame* frame)
{
    m_frontend->frameStartedLoading(frameId(frame));
}

void InspectorPageAgent::frameStoppedLoading(Frame* frame)
{
    m_frontend->frameStoppedLoading(frameId(frame));
}

void InspectorPageAgent::frameScheduledNavigation(Frame* frame, double delay)
{
    m_frontend->frameScheduledNavigation(frameId(frame), delay);
}

void InspectorPageAgent::frameClearedScheduledNavigation(Frame* frame)
{
    m_frontend->frameClearedScheduledNavigation(frameId(frame));
}

void InspectorPageAgent::willRunJavaScriptDialog(const String& message)
{
    m_frontend->javascriptDialogOpening(message);
}

void InspectorPageAgent::didRunJavaScriptDialog()
{
    m_frontend->javascriptDialogClosed();
}

void InspectorPageAgent::didPaint(RenderObject*, const GraphicsLayer*, GraphicsContext* context, const LayoutRect& rect)
{
    if (!m_enabled || m_client->overridesShowPaintRects() || !m_state->getBoolean(PageAgentState::pageAgentShowPaintRects))
        return;

    static int colorSelector = 0;
    const Color colors[] = {
        Color(0xFF, 0, 0, 0x3F),
        Color(0xFF, 0, 0xFF, 0x3F),
        Color(0, 0, 0xFF, 0x3F),
    };

    LayoutRect inflatedRect(rect);
    inflatedRect.inflate(-1);
    m_overlay->drawOutline(context, inflatedRect, colors[colorSelector++ % WTF_ARRAY_LENGTH(colors)]);
}

void InspectorPageAgent::didLayout(RenderObject*)
{
    if (!m_enabled)
        return;
    m_overlay->update();
}

void InspectorPageAgent::didScroll()
{
    if (m_enabled)
        m_overlay->update();
}

void InspectorPageAgent::didResizeMainFrame()
{
    if (m_enabled && m_state->getBoolean(PageAgentState::showSizeOnResize))
        m_overlay->showAndHideViewSize(m_state->getBoolean(PageAgentState::showGridOnResize));
    m_frontend->frameResized();
}

void InspectorPageAgent::didRecalculateStyle()
{
    if (m_enabled)
        m_overlay->update();
}

void InspectorPageAgent::scriptsEnabled(bool isEnabled)
{
    if (m_ignoreScriptsEnabledNotification)
        return;

    m_frontend->scriptsEnabled(isEnabled);
}

PassRefPtr<TypeBuilder::Page::Frame> InspectorPageAgent::buildObjectForFrame(Frame* frame)
{
    RefPtr<TypeBuilder::Page::Frame> frameObject = TypeBuilder::Page::Frame::create()
        .setId(frameId(frame))
        .setLoaderId(loaderId(frame->loader().documentLoader()))
        .setUrl(urlWithoutFragment(frame->document()->url()).string())
        .setMimeType(frame->loader().documentLoader()->responseMIMEType())
        .setSecurityOrigin(frame->document()->securityOrigin()->toRawString());
    if (frame->tree().parent())
        frameObject->setParentId(frameId(frame->tree().parent()));
    if (frame->ownerElement()) {
        AtomicString name = frame->ownerElement()->getNameAttribute();
        if (name.isEmpty())
            name = frame->ownerElement()->getAttribute(HTMLNames::idAttr);
        frameObject->setName(name);
    }

    return frameObject;
}

PassRefPtr<TypeBuilder::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(Frame* frame)
{
    RefPtr<TypeBuilder::Page::Frame> frameObject = buildObjectForFrame(frame);
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree::Resources> > subresources = TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree::Resources>::create();
    RefPtr<TypeBuilder::Page::FrameResourceTree> result = TypeBuilder::Page::FrameResourceTree::create()
         .setFrame(frameObject)
         .setResources(subresources);

    Vector<Resource*> allResources = cachedResourcesForFrame(frame);
    for (Vector<Resource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it) {
        Resource* cachedResource = *it;

        RefPtr<TypeBuilder::Page::FrameResourceTree::Resources> resourceObject = TypeBuilder::Page::FrameResourceTree::Resources::create()
            .setUrl(urlWithoutFragment(cachedResource->url()).string())
            .setType(cachedResourceTypeJson(*cachedResource))
            .setMimeType(cachedResource->response().mimeType());
        if (cachedResource->wasCanceled())
            resourceObject->setCanceled(true);
        else if (cachedResource->status() == Resource::LoadError)
            resourceObject->setFailed(true);
        subresources->addItem(resourceObject);
    }

    RefPtr<TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree> > childrenArray;
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!childrenArray) {
            childrenArray = TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree>::create();
            result->setChildFrames(childrenArray);
        }
        childrenArray->addItem(buildObjectForFrameTree(child));
    }
    return result;
}

void InspectorPageAgent::updateViewMetrics(int width, int height, double deviceScaleFactor, bool emulateViewport, bool fitWindow)
{
    if (width && height && !m_page->settings().acceleratedCompositingEnabled())
        return;

    m_deviceMetricsOverridden = width && height;
    m_emulateViewportEnabled = emulateViewport;
    m_client->overrideDeviceMetrics(width, height, static_cast<float>(deviceScaleFactor), emulateViewport, fitWindow);

    Document* document = mainFrame()->document();
    if (document) {
        document->styleResolverChanged(RecalcStyleImmediately);
        document->mediaQueryAffectingValueChanged();
    }
    InspectorInstrumentation::mediaQueryResultChanged(document);

    // FIXME: allow metrics override, fps counter and continuous painting at the same time: crbug.com/299837.
    m_client->setShowFPSCounter(m_state->getBoolean(PageAgentState::pageAgentShowFPSCounter) && !m_deviceMetricsOverridden);
    m_client->setContinuousPaintingEnabled(m_state->getBoolean(PageAgentState::pageAgentContinuousPaintingEnabled) && !m_deviceMetricsOverridden);
}

void InspectorPageAgent::updateTouchEventEmulationInPage(bool enabled)
{
    m_state->setBoolean(PageAgentState::touchEventEmulationEnabled, enabled);
    if (mainFrame() && mainFrame()->settings())
        mainFrame()->settings()->setTouchEventEmulationEnabled(enabled);
}

void InspectorPageAgent::setGeolocationOverride(ErrorString* error, const double* latitude, const double* longitude, const double* accuracy)
{
    GeolocationController* controller = GeolocationController::from(m_page);
    GeolocationPosition* position = 0;
    if (!controller) {
        *error = "Internal error: unable to override geolocation";
        return;
    }
    position = controller->lastPosition();
    if (!m_geolocationOverridden && position)
        m_platformGeolocationPosition = position;

    m_geolocationOverridden = true;
    if (latitude && longitude && accuracy)
        m_geolocationPosition = GeolocationPosition::create(currentTimeMS(), *latitude, *longitude, *accuracy);
    else
        m_geolocationPosition.clear();

    controller->positionChanged(0); // Kick location update.
}

void InspectorPageAgent::clearGeolocationOverride(ErrorString*)
{
    if (!m_geolocationOverridden)
        return;
    m_geolocationOverridden = false;
    m_geolocationPosition.clear();

    GeolocationController* controller = GeolocationController::from(m_page);
    if (controller && m_platformGeolocationPosition.get())
        controller->positionChanged(m_platformGeolocationPosition.get());
}

GeolocationPosition* InspectorPageAgent::overrideGeolocationPosition(GeolocationPosition* position)
{
    if (m_geolocationOverridden) {
        if (position)
            m_platformGeolocationPosition = position;
        return m_geolocationPosition.get();
    }
    return position;
}

void InspectorPageAgent::setDeviceOrientationOverride(ErrorString* error, double alpha, double beta, double gamma)
{
    DeviceOrientationController* controller = DeviceOrientationController::from(mainFrame()->document());
    if (!controller) {
        *error = "Internal error: unable to override device orientation";
        return;
    }

    controller->didChangeDeviceOrientation(DeviceOrientationData::create(true, alpha, true, beta, true, gamma).get());
}

void InspectorPageAgent::clearDeviceOrientationOverride(ErrorString* error)
{
    setDeviceOrientationOverride(error, 0, 0, 0);
}

bool InspectorPageAgent::overrideTextAutosizing(bool textAutosizing)
{
    if (!m_deviceMetricsOverridden)
        return textAutosizing;
    return m_state->getBoolean(PageAgentState::pageAgentTextAutosizingOverride);
}

float InspectorPageAgent::overrideFontScaleFactor(float fontScaleFactor)
{
    if (!m_deviceMetricsOverridden)
        return fontScaleFactor;
    return static_cast<float>(m_state->getDouble(PageAgentState::fontScaleFactor));
}

void InspectorPageAgent::setTouchEmulationEnabled(ErrorString*, bool enabled)
{
    if (m_state->getBoolean(PageAgentState::touchEventEmulationEnabled) == enabled)
        return;
    updateTouchEventEmulationInPage(enabled);
}

void InspectorPageAgent::setEmulatedMedia(ErrorString*, const String& media)
{
    String currentMedia = m_state->getString(PageAgentState::pageAgentEmulatedMedia);
    if (media == currentMedia)
        return;

    m_state->setString(PageAgentState::pageAgentEmulatedMedia, media);
    Document* document = 0;
    if (m_page->mainFrame())
        document = m_page->mainFrame()->document();
    if (document) {
        document->mediaQueryAffectingValueChanged();
        document->styleResolverChanged(RecalcStyleImmediately);
        document->updateLayout();
    }
}

bool InspectorPageAgent::applyViewportStyleOverride(StyleResolver* resolver)
{
    if (!m_deviceMetricsOverridden || !m_emulateViewportEnabled)
        return false;

    RefPtr<StyleSheetContents> styleSheet = StyleSheetContents::create(CSSParserContext(UASheetMode));
    styleSheet->parseString(String(viewportAndroidUserAgentStyleSheet, sizeof(viewportAndroidUserAgentStyleSheet)));
    OwnPtr<RuleSet> ruleSet = RuleSet::create();
    ruleSet->addRulesFromSheet(styleSheet.get(), MediaQueryEvaluator("screen"));
    resolver->viewportStyleResolver()->collectViewportRules(ruleSet.get(), ViewportStyleResolver::UserAgentOrigin);
    return true;
}

void InspectorPageAgent::applyEmulatedMedia(String* media)
{
    String emulatedMedia = m_state->getString(PageAgentState::pageAgentEmulatedMedia);
    if (!emulatedMedia.isEmpty())
        *media = emulatedMedia;
}

bool InspectorPageAgent::forceCompositingMode(ErrorString* errorString)
{
    Settings& settings = m_page->settings();
    if (!settings.acceleratedCompositingEnabled()) {
        if (errorString)
            *errorString = "Compositing mode is not supported";
        return false;
    }
    if (settings.forceCompositingMode())
        return true;
    settings.setForceCompositingMode(true);
    Frame* mainFrame = m_page->mainFrame();
    if (mainFrame)
        mainFrame->view()->updateCompositingLayersAfterStyleChange();
    return true;
}

void InspectorPageAgent::captureScreenshot(ErrorString*, const String*, const int*, const int*, const int*, String*, RefPtr<TypeBuilder::Page::ScreencastFrameMetadata>&)
{
    // Handled on the browser level.
}

void InspectorPageAgent::canScreencast(ErrorString*, bool*)
{
    // Handled on the browser level.
}

void InspectorPageAgent::startScreencast(ErrorString*, const String*, const int*, const int*, const int*)
{
    // Handled on the browser level.
}

void InspectorPageAgent::stopScreencast(ErrorString*)
{
    // Handled on the browser level.
}

void InspectorPageAgent::handleJavaScriptDialog(ErrorString* errorString, bool accept, const String* promptText)
{
    // Handled on the browser level.
}

void InspectorPageAgent::queryUsageAndQuota(WebCore::ErrorString*, const WTF::String&, WTF::RefPtr<WebCore::TypeBuilder::Page::Quota>&, WTF::RefPtr<WebCore::TypeBuilder::Page::Usage>&)
{
    // Handled on the browser level.
}

void InspectorPageAgent::setShowViewportSizeOnResize(ErrorString*, bool show, const bool* showGrid)
{
    m_state->setBoolean(PageAgentState::showSizeOnResize, show);
    m_state->setBoolean(PageAgentState::showGridOnResize, showGrid && *showGrid);
}

} // namespace WebCore

