/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2010, 2011 Brent Fulgham <bfulgham@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

// FIXME: This is temporary until all ports switch to using this file.
#if OS(WIN)
#include "platform/fonts/win/FontPlatformDataWin.h"
#elif !OS(MACOSX)
#include "platform/fonts/harfbuzz/FontPlatformDataHarfBuzz.h"

#else

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/FontWidthVariant.h"

#if OS(MACOSX)
OBJC_CLASS NSFont;

typedef struct CGFont* CGFontRef;
typedef const struct __CTFont* CTFontRef;

#include <CoreFoundation/CFBase.h>
#include <objc/objc-auto.h>
#endif

#include "wtf/Forward.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RetainPtr.h"
#include "wtf/text/StringImpl.h"

#if OS(MACOSX)
#include "platform/fonts/mac/MemoryActivatedFont.h"
#endif

#if OS(MACOSX)
typedef struct CGFont* CGFontRef;
typedef const struct __CTFont* CTFontRef;
typedef UInt32 FMFont;
typedef FMFont ATSUFontID;
typedef UInt32 ATSFontRef;
#endif

namespace WebCore {

class FontDescription;
class SharedBuffer;

#if OS(MACOSX)
class HarfBuzzFace;
#endif

#if OS(MACOSX)
inline CTFontRef toCTFontRef(NSFont *nsFont) { return reinterpret_cast<CTFontRef>(nsFont); }
#endif

class PLATFORM_EXPORT FontPlatformData {
public:
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
    FontPlatformData(const FontPlatformData&);
    FontPlatformData(const FontDescription&, const AtomicString& family);
    FontPlatformData(float size, bool syntheticBold, bool syntheticOblique, FontOrientation = Horizontal, FontWidthVariant = RegularWidth);

#if OS(MACOSX)
    FontPlatformData(NSFont*, float size, bool isPrinterFont = false, bool syntheticBold = false, bool syntheticOblique = false,
                     FontOrientation = Horizontal, FontWidthVariant = RegularWidth);
    FontPlatformData(CGFontRef, float size, bool syntheticBold, bool syntheticOblique, FontOrientation, FontWidthVariant);
#endif

    ~FontPlatformData();

#if OS(MACOSX)
    NSFont* font() const { return m_font; }
    void setFont(NSFont*);
#endif

#if OS(MACOSX)
    CGFontRef cgFont() const { return m_cgFont.get(); }
    CTFontRef ctFont() const;

    bool roundsGlyphAdvances() const;
    bool allowsLigatures() const;
#endif

    String fontFamilyName() const;
    bool isFixedPitch() const;
    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }
    bool isColorBitmapFont() const { return m_isColorBitmapFont; }
    bool isCompositeFontReference() const { return m_isCompositeFontReference; }
#if OS(MACOSX)
    bool isPrinterFont() const { return m_isPrinterFont; }
#endif
    FontOrientation orientation() const { return m_orientation; }
    FontWidthVariant widthVariant() const { return m_widthVariant; }

    void setOrientation(FontOrientation orientation) { m_orientation = orientation; }

#if OS(MACOSX)
    HarfBuzzFace* harfBuzzFace();
#endif

    unsigned hash() const
    {
#if OS(MACOSX)
        ASSERT(m_font || !m_cgFont);
        uintptr_t hashCodes[3] = { (uintptr_t)m_font, m_widthVariant, static_cast<uintptr_t>(m_isPrinterFont << 3 | m_orientation << 2 | m_syntheticBold << 1 | m_syntheticOblique) };
        return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
#endif
    }

    const FontPlatformData& operator=(const FontPlatformData&);

    bool operator==(const FontPlatformData& other) const
    {
        return platformIsEqual(other)
            && m_size == other.m_size
            && m_syntheticBold == other.m_syntheticBold
            && m_syntheticOblique == other.m_syntheticOblique
            && m_isColorBitmapFont == other.m_isColorBitmapFont
            && m_isCompositeFontReference == other.m_isCompositeFontReference
#if OS(MACOSX)
            && m_isPrinterFont == other.m_isPrinterFont
#endif
            && m_orientation == other.m_orientation
            && m_widthVariant == other.m_widthVariant;
    }

    bool isHashTableDeletedValue() const
    {
#if OS(MACOSX)
        return m_font == hashTableDeletedFontValue();
#endif
    }

#ifndef NDEBUG
    String description() const;
#endif

private:
    bool platformIsEqual(const FontPlatformData&) const;
    void platformDataInit(const FontPlatformData&);
    const FontPlatformData& platformDataAssign(const FontPlatformData&);
#if OS(MACOSX)
    // Load various data about the font specified by |nsFont| with the size fontSize into the following output paramters:
    // Note: Callers should always take into account that for the Chromium port, |outNSFont| isn't necessarily the same
    // font as |nsFont|. This because the sandbox may block loading of the original font.
    // * outNSFont - The font that was actually loaded, for the Chromium port this may be different than nsFont.
    // The caller is responsible for calling CFRelease() on this parameter when done with it.
    // * cgFont - CGFontRef representing the input font at the specified point size.
    void loadFont(NSFont*, float fontSize, NSFont*& outNSFont, CGFontRef&);
    static NSFont* hashTableDeletedFontValue() { return reinterpret_cast<NSFont *>(-1); }
#endif

public:
    bool m_syntheticBold;
    bool m_syntheticOblique;
    FontOrientation m_orientation;
    float m_size;
    FontWidthVariant m_widthVariant;

private:
#if OS(MACOSX)
    NSFont* m_font;
#endif

#if OS(MACOSX)
    RetainPtr<CGFontRef> m_cgFont;
    mutable RetainPtr<CTFontRef> m_CTFont;

    RefPtr<MemoryActivatedFont> m_inMemoryFont;
    RefPtr<HarfBuzzFace> m_harfBuzzFace;
#endif

    bool m_isColorBitmapFont;
    bool m_isCompositeFontReference;
#if OS(MACOSX)
    bool m_isPrinterFont;
#endif
};

} // namespace WebCore

#endif // FontPlatformData_h

#endif
