/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/css/FontFaceSet.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/ScriptState.h"
#include "core/css/CSSFontFaceLoadEvent.h"
#include "core/css/CSSFontFaceSource.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "public/platform/Platform.h"

namespace WebCore {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";

class LoadFontPromiseResolver : public CSSSegmentedFontFace::LoadFontCallback {
public:
    static PassRefPtr<LoadFontPromiseResolver> create(const FontFamily& family, ScriptPromise promise, ExecutionContext* context)
    {
        int numFamilies = 0;
        for (const FontFamily* f = &family; f; f = f->next())
            numFamilies++;
        return adoptRef<LoadFontPromiseResolver>(new LoadFontPromiseResolver(numFamilies, promise, context));
    }

    virtual void notifyLoaded(CSSSegmentedFontFace*) OVERRIDE;
    virtual void notifyError(CSSSegmentedFontFace*) OVERRIDE;
    void loaded(Document*);
    void error(Document*);

private:
    LoadFontPromiseResolver(int numLoading, ScriptPromise promise, ExecutionContext* context)
        : m_numLoading(numLoading)
        , m_errorOccured(false)
        , m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(promise, context))
    { }

    int m_numLoading;
    bool m_errorOccured;
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

void LoadFontPromiseResolver::loaded(Document* document)
{
    m_numLoading--;
    if (m_numLoading || !document)
        return;

    ScriptScope scope(m_scriptState);
    if (m_errorOccured)
        m_resolver->reject(ScriptValue::createNull());
    else
        m_resolver->resolve(ScriptValue::createNull());
}

void LoadFontPromiseResolver::error(Document* document)
{
    m_errorOccured = true;
    loaded(document);
}

void LoadFontPromiseResolver::notifyLoaded(CSSSegmentedFontFace* face)
{
    loaded(face->fontSelector()->document());
}

void LoadFontPromiseResolver::notifyError(CSSSegmentedFontFace* face)
{
    error(face->fontSelector()->document());
}

class FontsReadyPromiseResolver {
public:
    static PassOwnPtr<FontsReadyPromiseResolver> create(ScriptPromise promise, ExecutionContext* context)
    {
        return adoptPtr(new FontsReadyPromiseResolver(promise, context));
    }

    void resolve(PassRefPtr<FontFaceSet> fontFaceSet)
    {
        ScriptScope scope(m_scriptState);
        m_resolver->resolve(fontFaceSet);
    }

private:
    FontsReadyPromiseResolver(ScriptPromise promise, ExecutionContext* context)
        : m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(promise, context))
    { }
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

FontFaceSet::FontFaceSet(Document* document)
    : ActiveDOMObject(document)
    , m_loadingCount(0)
    , m_shouldFireLoadingEvent(false)
    , m_asyncRunner(this, &FontFaceSet::handlePendingEventsAndPromises)
{
    suspendIfNeeded();
}

FontFaceSet::~FontFaceSet()
{
}

Document* FontFaceSet::document() const
{
    return toDocument(executionContext());
}

const AtomicString& FontFaceSet::interfaceName() const
{
    return EventTargetNames::FontFaceSet;
}

ExecutionContext* FontFaceSet::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

AtomicString FontFaceSet::status() const
{
    DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded", AtomicString::ConstructFromLiteral));
    return (m_loadingCount > 0 || hasLoadedFonts()) ? loading : loaded;
}

void FontFaceSet::handlePendingEventsAndPromisesSoon()
{
    // setPendingActivity() is unnecessary because m_asyncRunner will be
    // automatically stopped on destruction.
    m_asyncRunner.runAsync();
}

void FontFaceSet::didLayout()
{
    if (document()->frame()->isMainFrame())
        m_histogram.record();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    if (m_loadingCount || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;
    handlePendingEventsAndPromisesSoon();
}

void FontFaceSet::handlePendingEventsAndPromises()
{
    fireLoadingEvent();
    fireDoneEventIfPossible();
}

void FontFaceSet::fireLoadingEvent()
{
    if (m_shouldFireLoadingEvent) {
        m_shouldFireLoadingEvent = false;
        dispatchEvent(CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loading));
    }
}

void FontFaceSet::suspend()
{
    m_asyncRunner.suspend();
}

void FontFaceSet::resume()
{
    m_asyncRunner.resume();
}

void FontFaceSet::stop()
{
    m_asyncRunner.stop();
}

void FontFaceSet::beginFontLoading(FontFace* fontFace)
{
    m_histogram.incrementCount();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;

    if (!m_loadingCount && !hasLoadedFonts()) {
        ASSERT(!m_shouldFireLoadingEvent);
        m_shouldFireLoadingEvent = true;
        handlePendingEventsAndPromisesSoon();
    }
    ++m_loadingCount;
}

void FontFaceSet::fontLoaded(FontFace* fontFace)
{
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    m_loadedFonts.append(fontFace);
    queueDoneEvent(fontFace);
}

void FontFaceSet::loadError(FontFace* fontFace)
{
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    m_failedFonts.append(fontFace);
    queueDoneEvent(fontFace);
}

void FontFaceSet::queueDoneEvent(FontFace* fontFace)
{
    ASSERT(m_loadingCount > 0);
    --m_loadingCount;
    if (!m_loadingCount)
        handlePendingEventsAndPromisesSoon();
}

ScriptPromise FontFaceSet::ready()
{
    ScriptPromise promise = ScriptPromise::createPending(executionContext());
    OwnPtr<FontsReadyPromiseResolver> resolver = FontsReadyPromiseResolver::create(promise, executionContext());
    m_readyResolvers.append(resolver.release());
    handlePendingEventsAndPromisesSoon();
    return promise;
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (m_shouldFireLoadingEvent)
        return;
    if (m_loadingCount || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (hasLoadedFonts()) {
        RefPtr<CSSFontFaceLoadEvent> doneEvent;
        RefPtr<CSSFontFaceLoadEvent> errorEvent;
        doneEvent = CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loadingdone, m_loadedFonts);
        m_loadedFonts.clear();
        if (!m_failedFonts.isEmpty()) {
            errorEvent = CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loadingerror, m_failedFonts);
            m_failedFonts.clear();
        }
        dispatchEvent(doneEvent);
        if (errorEvent)
            dispatchEvent(errorEvent);
    }

    if (!m_readyResolvers.isEmpty()) {
        Vector<OwnPtr<FontsReadyPromiseResolver> > resolvers;
        m_readyResolvers.swap(resolvers);
        for (size_t index = 0; index < resolvers.size(); ++index)
            resolvers[index]->resolve(this);
    }
}

static const String& nullToSpace(const String& s)
{
    DEFINE_STATIC_LOCAL(String, space, (" "));
    return s.isNull() ? space : s;
}

Vector<RefPtr<FontFace> > FontFaceSet::match(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    Vector<RefPtr<FontFace> > matchedFonts;

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwUninformativeAndGenericDOMException(SyntaxError);
        return matchedFonts;
    }

    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = document()->styleEngine()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (face)
            matchedFonts.append(face->fontFaces(nullToSpace(text)));
    }
    return matchedFonts;
}

ScriptPromise FontFaceSet::load(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwUninformativeAndGenericDOMException(SyntaxError);
        return ScriptPromise();
    }

    Document* d = document();
    ScriptPromise promise = ScriptPromise::createPending(executionContext());
    RefPtr<LoadFontPromiseResolver> resolver = LoadFontPromiseResolver::create(font.family(), promise, executionContext());
    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = d->styleEngine()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (!face) {
            resolver->error(d);
            continue;
        }
        face->loadFont(font.fontDescription(), nullToSpace(text), resolver);
    }
    return promise;
}

bool FontFaceSet::check(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwUninformativeAndGenericDOMException(SyntaxError);
        return false;
    }

    for (const FontFamily* f = &font.family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = document()->styleEngine()->fontSelector()->getFontFace(font.fontDescription(), f->family());
        if (!face || !face->checkFont(nullToSpace(text)))
            return false;
    }
    return true;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    if (fontString.isEmpty())
        return false;

    // Interpret fontString in the same way as the 'font' attribute of CanvasRenderingContext2D.
    RefPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    CSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, fontString, true, HTMLStandardMode, 0);
    if (parsedStyle->isEmpty())
        return false;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);
    if (fontValue == "inherit" || fontValue == "initial")
        return false;

    RefPtr<RenderStyle> style = RenderStyle::create();

    FontFamily fontFamily;
    fontFamily.setFamily(defaultFontFamily);

    FontDescription defaultFontDescription;
    defaultFontDescription.setFamily(fontFamily);
    defaultFontDescription.setSpecifiedSize(defaultFontSize);
    defaultFontDescription.setComputedSize(defaultFontSize);

    style->setFontDescription(defaultFontDescription);

    style->font().update(style->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };
    StyleResolver& styleResolver = document()->ensureStyleResolver();
    styleResolver.applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), style.get());

    font = style->font();
    font.update(document()->styleEngine()->fontSelector());
    return true;
}

void FontFaceSet::FontLoadHistogram::record()
{
    if (m_recorded)
        return;
    m_recorded = true;
    blink::Platform::current()->histogramCustomCounts("WebFont.WebFontsInPage", m_count, 1, 100, 50);
}

static const char* supplementName()
{
    return "FontFaceSet";
}

PassRefPtr<FontFaceSet> FontFaceSet::from(Document* document)
{
    RefPtr<FontFaceSet> fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName()));
    if (!fonts) {
        fonts = FontFaceSet::create(document);
        SupplementType::provideTo(document, supplementName(), fonts);
    }

    return fonts.release();
}

void FontFaceSet::didLayout(Document* document)
{
    if (FontFaceSet* fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName())))
        fonts->didLayout();
}


} // namespace WebCore
