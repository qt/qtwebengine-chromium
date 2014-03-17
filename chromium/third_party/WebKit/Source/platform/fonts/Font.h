/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#ifndef Font_h
#define Font_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/TypesettingFeatures.h"
#include "platform/text/TextDirection.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/MathExtras.h"
#include "wtf/unicode/CharacterNames.h"

// "X11/X.h" defines Complex to 0 and conflicts
// with Complex value in CodePath enum.
#ifdef Complex
#undef Complex
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class FontData;
class FontMetrics;
class FontPlatformData;
class FontSelector;
class GlyphBuffer;
class GraphicsContext;
class TextLayout;
class TextRun;
struct TextRunPaintInfo;

struct GlyphData;

struct GlyphOverflow {
    GlyphOverflow()
        : left(0)
        , right(0)
        , top(0)
        , bottom(0)
        , computeBounds(false)
    {
    }

    int left;
    int right;
    int top;
    int bottom;
    bool computeBounds;
};


class PLATFORM_EXPORT Font {
public:
    Font();
    Font(const FontDescription&, float letterSpacing, float wordSpacing);
    // This constructor is only used if the platform wants to start with a native font.
    Font(const FontPlatformData&, bool isPrinting, FontSmoothingMode = AutoSmoothing);
    ~Font();

    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const;
    bool operator!=(const Font& other) const { return !(*this == other); }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }

    void update(PassRefPtr<FontSelector>) const;

    enum CustomFontNotReadyAction { DoNotPaintIfFontNotReady, UseFallbackIfFontNotReady };
    void drawText(GraphicsContext*, const TextRunPaintInfo&, const FloatPoint&, CustomFontNotReadyAction = DoNotPaintIfFontNotReady) const;
    void drawEmphasisMarks(GraphicsContext*, const TextRunPaintInfo&, const AtomicString& mark, const FloatPoint&) const;

    float width(const TextRun&, HashSet<const SimpleFontData*>* fallbackFonts = 0, GlyphOverflow* = 0) const;
    float width(const TextRun&, int& charsConsumed, String& glyphName) const;

    PassOwnPtr<TextLayout> createLayoutForMacComplexText(const TextRun&, unsigned textLength, float xPos, bool collapseWhiteSpace) const;
    static void deleteLayout(TextLayout*);
    static float width(TextLayout&, unsigned from, unsigned len, HashSet<const SimpleFontData*>* fallbackFonts = 0);

    int offsetForPosition(const TextRun&, float position, bool includePartialGlyphs) const;
    FloatRect selectionRectForText(const TextRun&, const FloatPoint&, int h, int from = 0, int to = -1) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    float wordSpacing() const { return m_wordSpacing; }
    float letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(float s) { m_wordSpacing = s; }
    void setLetterSpacing(float s) { m_letterSpacing = s; }
    bool isFixedPitch() const;
    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }

    TypesettingFeatures typesettingFeatures() const { return static_cast<TypesettingFeatures>(m_typesettingFeatures); }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    FontItalic italic() const { return m_fontDescription.italic(); }
    FontWeight weight() const { return m_fontDescription.weight(); }
    FontWidthVariant widthVariant() const { return m_fontDescription.widthVariant(); }

    bool isPlatformFont() const { return m_isPlatformFont; }

    // Metrics that we query the FontFallbackList for.
    const FontMetrics& fontMetrics() const { return primaryFont()->fontMetrics(); }
    float spaceWidth() const { return primaryFont()->spaceWidth() + m_letterSpacing; }
    float tabWidth(const SimpleFontData&, unsigned tabSize, float position) const;
    float tabWidth(unsigned tabSize, float position) const { return tabWidth(*primaryFont(), tabSize, position); }

    int emphasisMarkAscent(const AtomicString&) const;
    int emphasisMarkDescent(const AtomicString&) const;
    int emphasisMarkHeight(const AtomicString&) const;

    const SimpleFontData* primaryFont() const;
    const FontData* fontDataAt(unsigned) const;
    inline GlyphData glyphDataForCharacter(UChar32 c, bool mirror, FontDataVariant variant = AutoVariant) const
    {
        return glyphDataAndPageForCharacter(c, mirror, variant).first;
    }
#if OS(MACOSX)
    const SimpleFontData* fontDataForCombiningCharacterSequence(const UChar*, size_t length, FontDataVariant) const;
#endif
    std::pair<GlyphData, GlyphPage*> glyphDataAndPageForCharacter(UChar32, bool mirror, FontDataVariant = AutoVariant) const;
    bool primaryFontHasGlyphForCharacter(UChar32) const;

    static bool isCJKIdeograph(UChar32);
    static bool isCJKIdeographOrSymbol(UChar32);

    static unsigned expansionOpportunityCount(const LChar*, size_t length, TextDirection, bool& isAfterExpansion);
    static unsigned expansionOpportunityCount(const UChar*, size_t length, TextDirection, bool& isAfterExpansion);

    static void setShouldUseSmoothing(bool);
    static bool shouldUseSmoothing();

    enum CodePath { Auto, Simple, Complex, SimpleWithGlyphOverflow };
    CodePath codePath(const TextRun&) const;
    static CodePath characterRangeCodePath(const LChar*, unsigned) { return Simple; }
    static CodePath characterRangeCodePath(const UChar*, unsigned len);

private:
    enum ForTextEmphasisOrNot { NotForTextEmphasis, ForTextEmphasis };

    // Returns the initial in-stream advance.
    float getGlyphsAndAdvancesForSimpleText(const TextRun&, int from, int to, GlyphBuffer&, ForTextEmphasisOrNot = NotForTextEmphasis) const;
    void drawSimpleText(GraphicsContext*, const TextRunPaintInfo&, const FloatPoint&) const;
    void drawEmphasisMarksForSimpleText(GraphicsContext*, const TextRunPaintInfo&, const AtomicString& mark, const FloatPoint&) const;
    void drawGlyphs(GraphicsContext*, const SimpleFontData*, const GlyphBuffer&, unsigned from, unsigned numGlyphs, const FloatPoint&, const FloatRect& textRect) const;
    void drawGlyphBuffer(GraphicsContext*, const TextRunPaintInfo&, const GlyphBuffer&, const FloatPoint&) const;
    void drawEmphasisMarks(GraphicsContext*, const TextRunPaintInfo&, const GlyphBuffer&, const AtomicString&, const FloatPoint&) const;
    float floatWidthForSimpleText(const TextRun&, HashSet<const SimpleFontData*>* fallbackFonts = 0, GlyphOverflow* = 0) const;
    int offsetForPositionForSimpleText(const TextRun&, float position, bool includePartialGlyphs) const;
    FloatRect selectionRectForSimpleText(const TextRun&, const FloatPoint&, int h, int from, int to) const;

    bool getEmphasisMarkGlyphData(const AtomicString&, GlyphData&) const;

    static bool canReturnFallbackFontsForComplexText();
    static bool canExpandAroundIdeographsInComplexText();

    // Returns the initial in-stream advance.
    float getGlyphsAndAdvancesForComplexText(const TextRun&, int from, int to, GlyphBuffer&, ForTextEmphasisOrNot = NotForTextEmphasis) const;
    void drawComplexText(GraphicsContext*, const TextRunPaintInfo&, const FloatPoint&) const;
    void drawEmphasisMarksForComplexText(GraphicsContext*, const TextRunPaintInfo&, const AtomicString& mark, const FloatPoint&) const;
    float floatWidthForComplexText(const TextRun&, HashSet<const SimpleFontData*>* fallbackFonts = 0, GlyphOverflow* = 0) const;
    int offsetForPositionForComplexText(const TextRun&, float position, bool includePartialGlyphs) const;
    FloatRect selectionRectForComplexText(const TextRun&, const FloatPoint&, int h, int from, int to) const;

    friend struct WidthIterator;
    friend class SVGTextRunRenderingContext;

public:
    // Useful for debugging the different font rendering code paths.
    static void setCodePath(CodePath);
    static CodePath codePath();
    static CodePath s_codePath;

    static void setDefaultTypesettingFeatures(TypesettingFeatures);
    static TypesettingFeatures defaultTypesettingFeatures();

    static const uint8_t s_roundingHackCharacterTable[256];
    static bool isRoundingHackCharacter(UChar32 c)
    {
        return !(c & ~0xFF) && s_roundingHackCharacterTable[c];
    }

    FontSelector* fontSelector() const;
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == noBreakSpace; }
    static bool treatAsZeroWidthSpace(UChar c) { return treatAsZeroWidthSpaceInComplexScript(c) || c == 0x200c || c == 0x200d; }
    static bool treatAsZeroWidthSpaceInComplexScript(UChar c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == softHyphen || c == zeroWidthSpace || (c >= 0x200e && c <= 0x200f) || (c >= 0x202a && c <= 0x202e) || c == zeroWidthNoBreakSpace || c == objectReplacementCharacter; }
    static bool canReceiveTextEmphasis(UChar32 c);

    static inline UChar normalizeSpaces(UChar character)
    {
        if (treatAsSpace(character))
            return space;

        if (treatAsZeroWidthSpace(character))
            return zeroWidthSpace;

        return character;
    }

    static String normalizeSpaces(const LChar*, unsigned length);
    static String normalizeSpaces(const UChar*, unsigned length);

    FontFallbackList* fontList() const { return m_fontFallbackList.get(); }

    void willUseFontData() const;

private:
    bool loadingCustomFonts() const
    {
        return m_fontFallbackList && m_fontFallbackList->loadingCustomFonts();
    }

    TypesettingFeatures computeTypesettingFeatures() const
    {
        TextRenderingMode textRenderingMode = m_fontDescription.textRenderingMode();
        TypesettingFeatures features = s_defaultTypesettingFeatures;

        switch (textRenderingMode) {
        case AutoTextRendering:
            break;
        case OptimizeSpeed:
            features &= ~(Kerning | Ligatures);
            break;
        case GeometricPrecision:
        case OptimizeLegibility:
            features |= Kerning | Ligatures;
            break;
        }

        switch (m_fontDescription.kerning()) {
        case FontDescription::NoneKerning:
            features &= ~Kerning;
            break;
        case FontDescription::NormalKerning:
            features |= Kerning;
            break;
        case FontDescription::AutoKerning:
            break;
        }

        switch (m_fontDescription.commonLigaturesState()) {
        case FontDescription::DisabledLigaturesState:
            features &= ~Ligatures;
            break;
        case FontDescription::EnabledLigaturesState:
            features |= Ligatures;
            break;
        case FontDescription::NormalLigaturesState:
            break;
        }

        return features;
    }

    static TypesettingFeatures s_defaultTypesettingFeatures;

    FontDescription m_fontDescription;
    mutable RefPtr<FontFallbackList> m_fontFallbackList;
    float m_letterSpacing;
    float m_wordSpacing;
    bool m_isPlatformFont;
    mutable unsigned m_typesettingFeatures : 2; // (TypesettingFeatures) Caches values computed from m_fontDescription.
};

inline Font::~Font()
{
}

inline const SimpleFontData* Font::primaryFont() const
{
    ASSERT(m_fontFallbackList);
    return m_fontFallbackList->primarySimpleFontData(m_fontDescription);
}

inline const FontData* Font::fontDataAt(unsigned index) const
{
    ASSERT(m_fontFallbackList);
    return m_fontFallbackList->fontDataAt(m_fontDescription, index);
}

inline bool Font::isFixedPitch() const
{
    ASSERT(m_fontFallbackList);
    return m_fontFallbackList->isFixedPitch(m_fontDescription);
}

inline FontSelector* Font::fontSelector() const
{
    return m_fontFallbackList ? m_fontFallbackList->fontSelector() : 0;
}

inline float Font::tabWidth(const SimpleFontData& fontData, unsigned tabSize, float position) const
{
    if (!tabSize)
        return letterSpacing();
    float tabWidth = tabSize * fontData.spaceWidth() + letterSpacing();
    return tabWidth - fmodf(position, tabWidth);
}

}

namespace WTF {

template <> struct PLATFORM_EXPORT OwnedPtrDeleter<WebCore::TextLayout> {
    static void deletePtr(WebCore::TextLayout*);
};

}

#endif
