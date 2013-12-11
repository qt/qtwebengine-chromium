/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, 2012 Google Inc. All rights reserved.
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
#include "core/platform/graphics/FontCache.h"

#include "SkFontMgr.h"
#include "SkTypeface_win.h"
#include "core/platform/NotImplemented.h"
#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/SimpleFontData.h"
#include "core/platform/graphics/chromium/FontPlatformDataChromiumWin.h"
#include "core/platform/graphics/chromium/FontUtilsChromiumWin.h"

namespace WebCore {

FontCache::FontCache()
    : m_purgePreventCount(0)
{
    m_fontManager = adoptPtr(SkFontMgr_New_GDI());
}


static bool fontContainsCharacter(const FontPlatformData* fontData, const wchar_t* family, UChar32 character)
{
    SkPaint paint;
    fontData->setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF32_TextEncoding);

    uint16_t glyph;
    paint.textToGlyphs(&character, sizeof(character), &glyph);
    return glyph != 0;
}

// Given the desired base font, this will create a SimpleFontData for a specific
// font that can be used to render the given range of characters.
PassRefPtr<SimpleFontData> FontCache::getFontDataForCharacter(const Font& font, UChar32 inputC)
{
    // FIXME: We should fix getFallbackFamily to take a UChar32
    // and remove this split-to-UChar16 code.
    UChar codeUnits[2];
    int codeUnitsLength;
    if (inputC <= 0xFFFF) {
        codeUnits[0] = inputC;
        codeUnitsLength = 1;
    } else {
        codeUnits[0] = U16_LEAD(inputC);
        codeUnits[1] = U16_TRAIL(inputC);
        codeUnitsLength = 2;
    }

    // FIXME: Consider passing fontDescription.dominantScript()
    // to GetFallbackFamily here.
    FontDescription fontDescription = font.fontDescription();
    UChar32 c;
    UScriptCode script;
    const wchar_t* family = getFallbackFamily(codeUnits, codeUnitsLength, fontDescription.genericFamily(), &c, &script);
    FontPlatformData* data = 0;
    if (family)
        data = getFontResourcePlatformData(font.fontDescription(),  AtomicString(family, wcslen(family)), false);

    // Last resort font list : PanUnicode. CJK fonts have a pretty
    // large repertoire. Eventually, we need to scan all the fonts
    // on the system to have a Firefox-like coverage.
    // Make sure that all of them are lowercased.
    const static wchar_t* const cjkFonts[] = {
        L"arial unicode ms",
        L"ms pgothic",
        L"simsun",
        L"gulim",
        L"pmingliu",
        L"wenquanyi zen hei", // Partial CJK Ext. A coverage but more widely known to Chinese users.
        L"ar pl shanheisun uni",
        L"ar pl zenkai uni",
        L"han nom a", // Complete CJK Ext. A coverage.
        L"code2000" // Complete CJK Ext. A coverage.
        // CJK Ext. B fonts are not listed here because it's of no use
        // with our current non-BMP character handling because we use
        // Uniscribe for it and that code path does not go through here.
    };

    const static wchar_t* const commonFonts[] = {
        L"tahoma",
        L"arial unicode ms",
        L"lucida sans unicode",
        L"microsoft sans serif",
        L"palatino linotype",
        // Six fonts below (and code2000 at the end) are not from MS, but
        // once installed, cover a very wide range of characters.
        L"dejavu serif",
        L"dejavu sasns",
        L"freeserif",
        L"freesans",
        L"gentium",
        L"gentiumalt",
        L"ms pgothic",
        L"simsun",
        L"gulim",
        L"pmingliu",
        L"code2000"
    };

    const wchar_t* const* panUniFonts = 0;
    int numFonts = 0;
    if (script == USCRIPT_HAN) {
        panUniFonts = cjkFonts;
        numFonts = WTF_ARRAY_LENGTH(cjkFonts);
    } else {
        panUniFonts = commonFonts;
        numFonts = WTF_ARRAY_LENGTH(commonFonts);
    }
    // Font returned from GetFallbackFamily may not cover |characters|
    // because it's based on script to font mapping. This problem is
    // critical enough for non-Latin scripts (especially Han) to
    // warrant an additional (real coverage) check with fontCotainsCharacter.
    int i;
    for (i = 0; (!data || !fontContainsCharacter(data, family, c)) && i < numFonts; ++i) {
        family = panUniFonts[i];
        data = getFontResourcePlatformData(font.fontDescription(), AtomicString(family, wcslen(family)));
    }
    // When i-th font (0-base) in |panUniFonts| contains a character and
    // we get out of the loop, |i| will be |i + 1|. That is, if only the
    // last font in the array covers the character, |i| will be numFonts.
    // So, we have to use '<=" rather than '<' to see if we found a font
    // covering the character.
    if (i <= numFonts)
        return getFontResourceData(data, DoNotRetain);

    return 0;
}

static inline bool equalIgnoringCase(const AtomicString& a, const SkString& b)
{
    return equalIgnoringCase(a, AtomicString::fromUTF8(b.c_str()));
}

static bool typefacesMatchesFamily(const SkTypeface* tf, const AtomicString& family)
{
    SkTypeface::LocalizedStrings* actualFamilies = tf->createFamilyNameIterator();
    bool matchesRequestedFamily = false;
    SkTypeface::LocalizedString actualFamily;

    while (actualFamilies->next(&actualFamily)) {
        if (equalIgnoringCase(family, actualFamily.fString)) {
            matchesRequestedFamily = true;
            break;
        }
    }
    actualFamilies->unref();

    // getFamilyName may return a name not returned by the createFamilyNameIterator.
    // Specifically in cases where Windows substitutes the font based on the
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\FontSubstitutes registry entries.
    if (!matchesRequestedFamily) {
        SkString familyName;
        tf->getFamilyName(&familyName);
        if (equalIgnoringCase(family, familyName))
            matchesRequestedFamily = true;
    }

    return matchesRequestedFamily;
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    CString name;
    SkTypeface* tf = createTypeface(fontDescription, family, name);
    if (!tf)
        return 0;

    // Windows will always give us a valid pointer here, even if the face name
    // is non-existent. We have to double-check and see if the family name was
    // really used.
    // FIXME: Do we need to use predefined fonts "guaranteed" to exist
    // when we're running in layout-test mode?
    if (!typefacesMatchesFamily(tf, family)) {
        tf->unref();
        return 0;
    }

    FontPlatformData* result = new FontPlatformData(tf,
        name.data(),
        fontDescription.computedSize(),
        fontDescription.weight() >= FontWeightBold && !tf->isBold(),
        fontDescription.italic() && !tf->isItalic(),
        fontDescription.orientation());
    tf->unref();
    return result;
}

}
