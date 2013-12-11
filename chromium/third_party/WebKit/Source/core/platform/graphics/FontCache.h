/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontCache_h
#define FontCache_h

#include <limits.h>
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include "wtf/unicode/Unicode.h"

#if OS(WIN)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

#if OS(WIN) && !ENABLE(GDI_FONTS_ON_WINDOWS)
#include "SkFontMgr.h"
#endif

class SkTypeface;

namespace WebCore {

class Font;
class FontPlatformData;
class FontData;
class FontDescription;
class FontSelector;
class OpenTypeVerticalData;
class SimpleFontData;

class FontCache {
    friend class FontCachePurgePreventer;

    WTF_MAKE_NONCOPYABLE(FontCache); WTF_MAKE_FAST_ALLOCATED;
public:
    friend FontCache* fontCache();

    enum ShouldRetain { Retain, DoNotRetain };

    PassRefPtr<FontData> getFontData(const Font&, int& familyIndex, FontSelector*);
    void releaseFontData(const SimpleFontData*);

    // This method is implemented by the plaform and used by
    // FontFastPath to lookup the font for a given character.
    PassRefPtr<SimpleFontData> getFontDataForCharacter(const Font&, UChar32);

    // Also implemented by the platform.
    void platformInit();

    void getTraitsInFamily(const AtomicString&, Vector<unsigned>&);

    PassRefPtr<SimpleFontData> getFontResourceData(const FontDescription&, const AtomicString&, bool checkingAlternateName = false, ShouldRetain = Retain);
    PassRefPtr<SimpleFontData> getLastResortFallbackFont(const FontDescription&, ShouldRetain = Retain);
    SimpleFontData* getNonRetainedLastResortFallbackFont(const FontDescription&);
    bool isPlatformFontAvailable(const FontDescription&, const AtomicString&, bool checkingAlternateName = false);

    void addClient(FontSelector*);
    void removeClient(FontSelector*);

    unsigned short generation();
    void invalidate();

    size_t fontDataCount();
    size_t inactiveFontDataCount();
    void purgeInactiveFontData(int count = INT_MAX);

#if OS(WIN)
    PassRefPtr<SimpleFontData> fontDataFromDescriptionAndLogFont(const FontDescription&, ShouldRetain, const LOGFONT&, wchar_t* outFontFamilyName);
#endif

#if ENABLE(OPENTYPE_VERTICAL)
    typedef uint32_t FontFileKey;
    PassRefPtr<OpenTypeVerticalData> getVerticalData(const FontFileKey&, const FontPlatformData&);
#endif

    struct SimpleFontFamily {
        String name;
        bool isBold;
        bool isItalic;
    };
    static void getFontFamilyForCharacter(UChar32, const char* preferredLocale, SimpleFontFamily*);

private:
    FontCache();
    ~FontCache();

    void disablePurging() { m_purgePreventCount++; }
    void enablePurging()
    {
        ASSERT(m_purgePreventCount);
        if (!--m_purgePreventCount)
            purgeInactiveFontDataIfNeeded();
    }

    void purgeInactiveFontDataIfNeeded();

    // FIXME: This method should eventually be removed.
    FontPlatformData* getFontResourcePlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
    PassRefPtr<SimpleFontData> getSimilarFontPlatformData(const Font&);
    FontPlatformData* createFontPlatformData(const FontDescription&, const AtomicString& family);

    // Implemented on skia platforms.
    SkTypeface* createTypeface(const FontDescription&, const AtomicString& family, CString& name);

    PassRefPtr<SimpleFontData> getFontResourceData(const FontPlatformData*, ShouldRetain = Retain);
    const FontPlatformData* getFallbackFontData(const FontDescription&);

    // Don't purge if this count is > 0;
    int m_purgePreventCount;

#if OS(WIN) && !ENABLE(GDI_FONTS_ON_WINDOWS)
    OwnPtr<SkFontMgr> m_fontManager;
#endif

#if OS(MACOSX) || OS(ANDROID)
    friend class ComplexTextController;
#endif
    friend class SimpleFontData; // For getFontResourceData(const FontPlatformData*)
    friend class FontFallbackList;
};

// Get the global fontCache.
FontCache* fontCache();

class FontCachePurgePreventer {
public:
    FontCachePurgePreventer() { fontCache()->disablePurging(); }
    ~FontCachePurgePreventer() { fontCache()->enablePurging(); }
};

}

#endif
