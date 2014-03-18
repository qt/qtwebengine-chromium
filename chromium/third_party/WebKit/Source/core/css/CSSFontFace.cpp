/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSFontFace.h"

#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceSet.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "platform/fonts/SimpleFontData.h"

namespace WebCore {

CSSFontFace::~CSSFontFace()
{
    m_fontFace->cssFontFaceDestroyed();
}

PassRefPtr<CSSFontFace> CSSFontFace::createFromStyleRule(Document* document, const StyleRuleFontFace* fontFaceRule)
{
    RefPtr<FontFace> fontFace = FontFace::create(fontFaceRule);
    if (!fontFace || fontFace->family().isEmpty())
        return 0;

    unsigned traitsMask = fontFace->traitsMask();
    if (!traitsMask)
        return 0;

    // FIXME: Plumbing back into createCSSFontFace seems odd.
    // Maybe move FontFace::createCSSFontFace logic here?
    RefPtr<CSSFontFace> cssFontFace = fontFace->createCSSFontFace(document);
    if (!cssFontFace || !cssFontFace->isValid())
        return 0;

    return cssFontFace;
}

bool CSSFontFace::isLoaded() const
{
    size_t size = m_sources.size();
    for (size_t i = 0; i < size; i++) {
        if (!m_sources[i]->isLoaded())
            return false;
    }
    return true;
}

bool CSSFontFace::isValid() const
{
    size_t size = m_sources.size();
    for (size_t i = 0; i < size; i++) {
        if (m_sources[i]->isValid())
            return true;
    }
    return false;
}

void CSSFontFace::addSource(PassOwnPtr<CSSFontFaceSource> source)
{
    source->setFontFace(this);
    m_sources.append(source);
}

void CSSFontFace::setSegmentedFontFace(CSSSegmentedFontFace* segmentedFontFace)
{
    ASSERT(!m_segmentedFontFace);
    m_segmentedFontFace = segmentedFontFace;
}

void CSSFontFace::beginLoadIfNeeded(CSSFontFaceSource* source)
{
    if (!m_segmentedFontFace)
        return;

    if (source->resource() && source->resource()->stillNeedsLoad()) {
        CSSFontSelector* fontSelector = m_segmentedFontFace->fontSelector();
        fontSelector->beginLoadingFontSoon(source->resource());
    }

    if (loadStatus() == FontFace::Unloaded)
        setLoadStatus(FontFace::Loading);
}

void CSSFontFace::fontLoaded(CSSFontFaceSource* source)
{
    if (source != m_activeSource)
        return;
    m_activeSource = 0;

    // FIXME: Can we assert that m_segmentedFontFace is non-null? That may
    // require stopping in-progress font loading when the last
    // CSSSegmentedFontFace is removed.
    if (!m_segmentedFontFace)
        return;

    CSSFontSelector* fontSelector = m_segmentedFontFace->fontSelector();
    fontSelector->fontLoaded();

    if (fontSelector->document() && loadStatus() == FontFace::Loading) {
        if (source->ensureFontData()) {
            setLoadStatus(FontFace::Loaded);
            if (source->isSVGFontFaceSource())
                UseCounter::count(*fontSelector->document(), UseCounter::SVGFontInCSS);
        }
        else if (!isValid())
            setLoadStatus(FontFace::Error);
    }

    m_segmentedFontFace->fontLoaded(this);
}

PassRefPtr<SimpleFontData> CSSFontFace::getFontData(const FontDescription& fontDescription)
{
    m_activeSource = 0;
    if (!isValid())
        return 0;

    size_t size = m_sources.size();
    for (size_t i = 0; i < size; ++i) {
        if (RefPtr<SimpleFontData> result = m_sources[i]->getFontData(fontDescription)) {
            m_activeSource = m_sources[i].get();
            if (loadStatus() == FontFace::Unloaded && (m_sources[i]->isLoading() || m_sources[i]->isLoaded()))
                setLoadStatus(FontFace::Loading);
            if (loadStatus() == FontFace::Loading && m_sources[i]->isLoaded())
                setLoadStatus(FontFace::Loaded);
            return result.release();
        }
    }

    if (loadStatus() == FontFace::Unloaded)
        setLoadStatus(FontFace::Loading);
    if (loadStatus() == FontFace::Loading)
        setLoadStatus(FontFace::Error);
    return 0;
}

void CSSFontFace::willUseFontData(const FontDescription& fontDescription)
{
    if (loadStatus() != FontFace::Unloaded || m_activeSource)
        return;

    // Kicks off font load here only if the @font-face has no unicode-range.
    // @font-faces with unicode-range will be loaded when a GlyphPage for the
    // font is created.
    // FIXME: Pass around the text to render from RenderText, and kick download
    // if m_ranges intersects with the text. Make sure this does not cause
    // performance regression.
    if (!m_ranges.isEntireRange())
        return;

    ASSERT(m_segmentedFontFace);

    size_t size = m_sources.size();
    for (size_t i = 0; i < size; ++i) {
        if (!m_sources[i]->isValid() || (m_sources[i]->isLocal() && !m_sources[i]->isLocalFontAvailable(fontDescription)))
            continue;
        if (!m_sources[i]->isLocal() && !m_sources[i]->isLoaded()) {
            m_activeSource = m_sources[i].get();
            beginLoadIfNeeded(m_activeSource);
        }
        break;
    }
}

void CSSFontFace::setLoadStatus(FontFace::LoadStatus newStatus)
{
    ASSERT(m_fontFace);
    m_fontFace->setLoadStatus(newStatus);

    if (!m_segmentedFontFace)
        return;
    Document* document = m_segmentedFontFace->fontSelector()->document();
    if (!document)
        return;

    switch (newStatus) {
    case FontFace::Loading:
        FontFaceSet::from(document)->beginFontLoading(m_fontFace.get());
        break;
    case FontFace::Loaded:
        FontFaceSet::from(document)->fontLoaded(m_fontFace.get());
        break;
    case FontFace::Error:
        FontFaceSet::from(document)->loadError(m_fontFace.get());
        break;
    default:
        break;
    }
}

bool CSSFontFace::UnicodeRangeSet::intersectsWith(const String& text) const
{
    if (text.isEmpty())
        return false;
    if (isEntireRange())
        return true;

    // FIXME: This takes O(text.length() * m_ranges.size()) time. It would be
    // better to make m_ranges sorted and use binary search.
    unsigned index = 0;
    while (index < text.length()) {
        UChar32 c = text.characterStartingAt(index);
        index += U16_LENGTH(c);
        for (unsigned i = 0; i < m_ranges.size(); i++) {
            if (m_ranges[i].contains(c))
                return true;
        }
    }
    return false;
}

}
