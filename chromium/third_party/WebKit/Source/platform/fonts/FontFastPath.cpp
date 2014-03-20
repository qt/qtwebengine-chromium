/**
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "config.h"
#include "platform/fonts/Font.h"

#include "platform/LayoutUnit.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/GlyphPageTreeNode.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/WidthIterator.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/unicode/CharacterNames.h"
#include "wtf/unicode/Unicode.h"

using namespace WTF;
using namespace Unicode;
using namespace std;

namespace WebCore {

static inline bool isInRange(UChar32 character, UChar32 lowerBound, UChar32 upperBound)
{
    return character >= lowerBound && character <= upperBound;
}

static bool shouldIgnoreRotation(UChar32 character)
{
    if (character == 0x000A7 || character == 0x000A9 || character == 0x000AE)
        return true;

    if (character == 0x000B6 || character == 0x000BC || character == 0x000BD || character == 0x000BE)
        return true;

    if (isInRange(character, 0x002E5, 0x002EB))
        return true;

    if (isInRange(character, 0x01100, 0x011FF) || isInRange(character, 0x01401, 0x0167F) || isInRange(character, 0x01800, 0x018FF))
        return true;

    if (character == 0x02016 || character == 0x02018 || character == 0x02019 || character == 0x02020 || character == 0x02021
        || character == 0x2030 || character == 0x02031)
        return true;

    if (isInRange(character, 0x0203B, 0x0203D) || character == 0x02042 || character == 0x02044 || character == 0x02047
        || character == 0x02048 || character == 0x02049 || character == 0x2051)
        return true;

    if (isInRange(character, 0x02065, 0x02069) || isInRange(character, 0x020DD, 0x020E0)
        || isInRange(character, 0x020E2, 0x020E4) || isInRange(character, 0x02100, 0x02117)
        || isInRange(character, 0x02119, 0x02131) || isInRange(character, 0x02133, 0x0213F))
        return true;

    if (isInRange(character, 0x02145, 0x0214A) || character == 0x0214C || character == 0x0214D
        || isInRange(character, 0x0214F, 0x0218F))
        return true;

    if (isInRange(character, 0x02300, 0x02307) || isInRange(character, 0x0230C, 0x0231F)
        || isInRange(character, 0x02322, 0x0232B) || isInRange(character, 0x0237D, 0x0239A)
        || isInRange(character, 0x023B4, 0x023B6) || isInRange(character, 0x023BA, 0x023CF)
        || isInRange(character, 0x023D1, 0x023DB) || isInRange(character, 0x023E2, 0x024FF))
        return true;

    if (isInRange(character, 0x025A0, 0x02619) || isInRange(character, 0x02620, 0x02767)
        || isInRange(character, 0x02776, 0x02793) || isInRange(character, 0x02B12, 0x02B2F)
        || isInRange(character, 0x02B4D, 0x02BFF) || isInRange(character, 0x02E80, 0x03007))
        return true;

    if (character == 0x03012 || character == 0x03013 || isInRange(character, 0x03020, 0x0302F)
        || isInRange(character, 0x03031, 0x0309F) || isInRange(character, 0x030A1, 0x030FB)
        || isInRange(character, 0x030FD, 0x0A4CF))
        return true;

    if (isInRange(character, 0x0A840, 0x0A87F) || isInRange(character, 0x0A960, 0x0A97F)
        || isInRange(character, 0x0AC00, 0x0D7FF) || isInRange(character, 0x0E000, 0x0FAFF))
        return true;

    if (isInRange(character, 0x0FE10, 0x0FE1F) || isInRange(character, 0x0FE30, 0x0FE48)
        || isInRange(character, 0x0FE50, 0x0FE57) || isInRange(character, 0x0FE5F, 0x0FE62)
        || isInRange(character, 0x0FE67, 0x0FE6F))
        return true;

    if (isInRange(character, 0x0FF01, 0x0FF07) || isInRange(character, 0x0FF0A, 0x0FF0C)
        || isInRange(character, 0x0FF0E, 0x0FF19) ||isInRange (character, 0x0FF1F, 0x0FF3A))
        return true;

    if (character == 0x0FF3C || character == 0x0FF3E)
        return true;

    if (isInRange(character, 0x0FF40, 0x0FF5A) || isInRange(character, 0x0FFE0, 0x0FFE2)
        || isInRange(character, 0x0FFE4, 0x0FFE7) || isInRange(character, 0x0FFF0, 0x0FFF8)
        || character == 0x0FFFD)
        return true;

    if (isInRange(character, 0x13000, 0x1342F) || isInRange(character, 0x1B000, 0x1B0FF)
        || isInRange(character, 0x1D000, 0x1D1FF) || isInRange(character, 0x1D300, 0x1D37F)
        || isInRange(character, 0x1F000, 0x1F64F) || isInRange(character, 0x1F680, 0x1F77F))
        return true;

    if (isInRange(character, 0x20000, 0x2FFFD) || isInRange(character, 0x30000, 0x3FFFD))
        return true;

    return false;
}

static inline std::pair<GlyphData, GlyphPage*> glyphDataAndPageForNonCJKCharacterWithGlyphOrientation(UChar32 character, NonCJKGlyphOrientation orientation, GlyphData& data, GlyphPage* page, unsigned pageNumber)
{
    if (orientation == NonCJKGlyphOrientationUpright || shouldIgnoreRotation(character)) {
        RefPtr<SimpleFontData> uprightFontData = data.fontData->uprightOrientationFontData();
        GlyphPageTreeNode* uprightNode = GlyphPageTreeNode::getRootChild(uprightFontData.get(), pageNumber);
        GlyphPage* uprightPage = uprightNode->page();
        if (uprightPage) {
            GlyphData uprightData = uprightPage->glyphDataForCharacter(character);
            // If the glyphs are the same, then we know we can just use the horizontal glyph rotated vertically to be upright.
            if (data.glyph == uprightData.glyph)
                return make_pair(data, page);
            // The glyphs are distinct, meaning that the font has a vertical-right glyph baked into it. We can't use that
            // glyph, so we fall back to the upright data and use the horizontal glyph.
            if (uprightData.fontData)
                return make_pair(uprightData, uprightPage);
        }
    } else if (orientation == NonCJKGlyphOrientationVerticalRight) {
        RefPtr<SimpleFontData> verticalRightFontData = data.fontData->verticalRightOrientationFontData();
        GlyphPageTreeNode* verticalRightNode = GlyphPageTreeNode::getRootChild(verticalRightFontData.get(), pageNumber);
        GlyphPage* verticalRightPage = verticalRightNode->page();
        if (verticalRightPage) {
            GlyphData verticalRightData = verticalRightPage->glyphDataForCharacter(character);
            // If the glyphs are distinct, we will make the assumption that the font has a vertical-right glyph baked
            // into it.
            if (data.glyph != verticalRightData.glyph)
                return make_pair(data, page);
            // The glyphs are identical, meaning that we should just use the horizontal glyph.
            if (verticalRightData.fontData)
                return make_pair(verticalRightData, verticalRightPage);
        }
    }
    return make_pair(data, page);
}

std::pair<GlyphData, GlyphPage*> Font::glyphDataAndPageForCharacter(UChar32 c, bool mirror, FontDataVariant variant) const
{
    ASSERT(isMainThread());

    if (variant == AutoVariant) {
        if (m_fontDescription.smallCaps() && !primaryFont()->isSVGFont()) {
            UChar32 upperC = toUpper(c);
            if (upperC != c) {
                c = upperC;
                variant = SmallCapsVariant;
            } else
                variant = NormalVariant;
        } else
            variant = NormalVariant;
    }

    if (mirror)
        c = mirroredChar(c);

    unsigned pageNumber = (c / GlyphPage::size);

    GlyphPageTreeNode* node = pageNumber ? m_fontFallbackList->m_pages.get(pageNumber) : m_fontFallbackList->m_pageZero;
    if (!node) {
        node = GlyphPageTreeNode::getRootChild(fontDataAt(0), pageNumber);
        if (pageNumber)
            m_fontFallbackList->m_pages.set(pageNumber, node);
        else
            m_fontFallbackList->m_pageZero = node;
    }

    GlyphPage* page = 0;
    if (variant == NormalVariant) {
        // Fastest loop, for the common case (normal variant).
        while (true) {
            page = node->page();
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData && (data.fontData->platformData().orientation() == Horizontal || data.fontData->isTextOrientationFallback()))
                    return make_pair(data, page);

                if (data.fontData) {
                    if (isCJKIdeographOrSymbol(c)) {
                        if (!data.fontData->hasVerticalGlyphs()) {
                            // Use the broken ideograph font data. The broken ideograph font will use the horizontal width of glyphs
                            // to make sure you get a square (even for broken glyphs like symbols used for punctuation).
                            variant = BrokenIdeographVariant;
                            break;
                        }
                    } else
                        return glyphDataAndPageForNonCJKCharacterWithGlyphOrientation(c, m_fontDescription.nonCJKGlyphOrientation(), data, page, pageNumber);

                    return make_pair(data, page);
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = node->getChild(fontDataAt(node->level()), pageNumber);
            if (pageNumber)
                m_fontFallbackList->m_pages.set(pageNumber, node);
            else
                m_fontFallbackList->m_pageZero = node;
        }
    }
    if (variant != NormalVariant) {
        while (true) {
            page = node->page();
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData) {
                    // The variantFontData function should not normally return 0.
                    // But if it does, we will just render the capital letter big.
                    RefPtr<SimpleFontData> variantFontData = data.fontData->variantFontData(m_fontDescription, variant);
                    if (!variantFontData)
                        return make_pair(data, page);

                    GlyphPageTreeNode* variantNode = GlyphPageTreeNode::getRootChild(variantFontData.get(), pageNumber);
                    GlyphPage* variantPage = variantNode->page();
                    if (variantPage) {
                        GlyphData data = variantPage->glyphDataForCharacter(c);
                        if (data.fontData)
                            return make_pair(data, variantPage);
                    }

                    // Do not attempt system fallback off the variantFontData. This is the very unlikely case that
                    // a font has the lowercase character but the small caps font does not have its uppercase version.
                    return make_pair(variantFontData->missingGlyphData(), page);
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = node->getChild(fontDataAt(node->level()), pageNumber);
            if (pageNumber)
                m_fontFallbackList->m_pages.set(pageNumber, node);
            else
                m_fontFallbackList->m_pageZero = node;
        }
    }

    ASSERT(page);
    ASSERT(node->isSystemFallback());

    // System fallback is character-dependent. When we get here, we
    // know that the character in question isn't in the system fallback
    // font's glyph page. Try to lazily create it here.

    // FIXME: Unclear if this should normalizeSpaces above 0xFFFF.
    // Doing so changes fast/text/international/plane2-diffs.html
    UChar32 characterToRender = c;
    if (characterToRender <=  0xFFFF)
        characterToRender = Font::normalizeSpaces(characterToRender);
    const SimpleFontData* fontDataToSubstitute = fontDataAt(0)->fontDataForCharacter(characterToRender);
    RefPtr<SimpleFontData> characterFontData = FontCache::fontCache()->platformFallbackForCharacter(m_fontDescription, characterToRender, fontDataToSubstitute, isPlatformFont());
    if (characterFontData) {
        if (characterFontData->platformData().orientation() == Vertical && !characterFontData->hasVerticalGlyphs() && isCJKIdeographOrSymbol(c))
            variant = BrokenIdeographVariant;
        if (variant != NormalVariant)
            characterFontData = characterFontData->variantFontData(m_fontDescription, variant);
    }
    if (characterFontData) {
        // Got the fallback glyph and font.
        GlyphPage* fallbackPage = GlyphPageTreeNode::getRootChild(characterFontData.get(), pageNumber)->page();
        GlyphData data = fallbackPage && fallbackPage->fontDataForCharacter(c) ? fallbackPage->glyphDataForCharacter(c) : characterFontData->missingGlyphData();
        // Cache it so we don't have to do system fallback again next time.
        if (variant == NormalVariant) {
            page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
            data.fontData->setMaxGlyphPageTreeLevel(max(data.fontData->maxGlyphPageTreeLevel(), node->level()));
            if (!isCJKIdeographOrSymbol(c) && data.fontData->platformData().orientation() != Horizontal && !data.fontData->isTextOrientationFallback())
                return glyphDataAndPageForNonCJKCharacterWithGlyphOrientation(c, m_fontDescription.nonCJKGlyphOrientation(), data, fallbackPage, pageNumber);
        }
        return make_pair(data, page);
    }

    // Even system fallback can fail; use the missing glyph in that case.
    // FIXME: It would be nicer to use the missing glyph from the last resort font instead.
    GlyphData data = primaryFont()->missingGlyphData();
    if (variant == NormalVariant) {
        page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
        data.fontData->setMaxGlyphPageTreeLevel(max(data.fontData->maxGlyphPageTreeLevel(), node->level()));
    }
    return make_pair(data, page);
}

bool Font::primaryFontHasGlyphForCharacter(UChar32 character) const
{
    unsigned pageNumber = (character / GlyphPage::size);

    GlyphPageTreeNode* node = GlyphPageTreeNode::getRootChild(primaryFont(), pageNumber);
    GlyphPage* page = node->page();

    return page && page->fontDataForCharacter(character);
}

// FIXME: This function may not work if the emphasis mark uses a complex script, but none of the
// standard emphasis marks do so.
bool Font::getEmphasisMarkGlyphData(const AtomicString& mark, GlyphData& glyphData) const
{
    if (mark.isEmpty())
        return false;

    UChar32 character = mark[0];

    if (U16_IS_SURROGATE(character)) {
        if (!U16_IS_SURROGATE_LEAD(character))
            return false;

        if (mark.length() < 2)
            return false;

        UChar low = mark[1];
        if (!U16_IS_TRAIL(low))
            return false;

        character = U16_GET_SUPPLEMENTARY(character, low);
    }

    glyphData = glyphDataForCharacter(character, false, EmphasisMarkVariant);
    return true;
}

int Font::emphasisMarkAscent(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().ascent();
}

int Font::emphasisMarkDescent(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().descent();
}

int Font::emphasisMarkHeight(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().height();
}

float Font::getGlyphsAndAdvancesForSimpleText(const TextRun& run, int from, int to, GlyphBuffer& glyphBuffer, ForTextEmphasisOrNot forTextEmphasis) const
{
    float initialAdvance;

    WidthIterator it(this, run, 0, false, forTextEmphasis);
    // FIXME: Using separate glyph buffers for the prefix and the suffix is incorrect when kerning or
    // ligatures are enabled.
    GlyphBuffer localGlyphBuffer;
    it.advance(from, &localGlyphBuffer);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to, &glyphBuffer);

    if (glyphBuffer.isEmpty())
        return 0;

    float afterWidth = it.m_runWidthSoFar;

    if (run.rtl()) {
        float finalRoundingWidth = it.m_finalRoundingWidth;
        it.advance(run.length(), &localGlyphBuffer);
        initialAdvance = finalRoundingWidth + it.m_runWidthSoFar - afterWidth;
    } else
        initialAdvance = beforeWidth;

    if (run.rtl())
        glyphBuffer.reverse(0, glyphBuffer.size());

    return initialAdvance;
}

void Font::drawSimpleText(GraphicsContext* context, const TextRunPaintInfo& runInfo, const FloatPoint& point) const
{
    // This glyph buffer holds our glyphs+advances+font data for each glyph.
    GlyphBuffer glyphBuffer;

    float startX = point.x() + getGlyphsAndAdvancesForSimpleText(runInfo.run, runInfo.from, runInfo.to, glyphBuffer);

    if (glyphBuffer.isEmpty())
        return;

    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, runInfo, glyphBuffer, startPoint);
}

void Font::drawEmphasisMarksForSimpleText(GraphicsContext* context, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point) const
{
    GlyphBuffer glyphBuffer;
    float initialAdvance = getGlyphsAndAdvancesForSimpleText(runInfo.run, runInfo.from, runInfo.to, glyphBuffer, ForTextEmphasis);

    if (glyphBuffer.isEmpty())
        return;

    drawEmphasisMarks(context, runInfo, glyphBuffer, mark, FloatPoint(point.x() + initialAdvance, point.y()));
}

void Font::drawGlyphBuffer(GraphicsContext* context, const TextRunPaintInfo& runInfo, const GlyphBuffer& glyphBuffer, const FloatPoint& point) const
{
    // Draw each contiguous run of glyphs that use the same font data.
    const SimpleFontData* fontData = glyphBuffer.fontDataAt(0);
    FloatPoint startPoint(point);
    float nextX = startPoint.x() + glyphBuffer.advanceAt(0);
    unsigned lastFrom = 0;
    unsigned nextGlyph = 1;
#if ENABLE(SVG_FONTS)
    TextRun::RenderingContext* renderingContext = runInfo.run.renderingContext();
#endif
    while (nextGlyph < glyphBuffer.size()) {
        const SimpleFontData* nextFontData = glyphBuffer.fontDataAt(nextGlyph);

        if (nextFontData != fontData) {
#if ENABLE(SVG_FONTS)
            if (renderingContext && fontData->isSVGFont())
                renderingContext->drawSVGGlyphs(context, runInfo.run, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint);
            else
#endif
                drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint, runInfo.bounds);

            lastFrom = nextGlyph;
            fontData = nextFontData;
            startPoint.setX(nextX);
        }
        nextX += glyphBuffer.advanceAt(nextGlyph);
        nextGlyph++;
    }

#if ENABLE(SVG_FONTS)
    if (renderingContext && fontData->isSVGFont())
        renderingContext->drawSVGGlyphs(context, runInfo.run, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint);
    else
#endif
        drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint, runInfo.bounds);
}

inline static float offsetToMiddleOfGlyph(const SimpleFontData* fontData, Glyph glyph)
{
    if (fontData->platformData().orientation() == Horizontal) {
        FloatRect bounds = fontData->boundsForGlyph(glyph);
        return bounds.x() + bounds.width() / 2;
    }
    // FIXME: Use glyph bounds once they make sense for vertical fonts.
    return fontData->widthForGlyph(glyph) / 2;
}

inline static float offsetToMiddleOfGlyphAtIndex(const GlyphBuffer& glyphBuffer, size_t i)
{
    return offsetToMiddleOfGlyph(glyphBuffer.fontDataAt(i), glyphBuffer.glyphAt(i));
}

void Font::drawEmphasisMarks(GraphicsContext* context, const TextRunPaintInfo& runInfo, const GlyphBuffer& glyphBuffer, const AtomicString& mark, const FloatPoint& point) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return;

    Glyph markGlyph = markGlyphData.glyph;
    Glyph spaceGlyph = markFontData->spaceGlyph();

    float middleOfLastGlyph = offsetToMiddleOfGlyphAtIndex(glyphBuffer, 0);
    FloatPoint startPoint(point.x() + middleOfLastGlyph - offsetToMiddleOfGlyph(markFontData, markGlyph), point.y());

    GlyphBuffer markBuffer;
    for (unsigned i = 0; i + 1 < glyphBuffer.size(); ++i) {
        float middleOfNextGlyph = offsetToMiddleOfGlyphAtIndex(glyphBuffer, i + 1);
        float advance = glyphBuffer.advanceAt(i) - middleOfLastGlyph + middleOfNextGlyph;
        markBuffer.add(glyphBuffer.glyphAt(i) ? markGlyph : spaceGlyph, markFontData, advance);
        middleOfLastGlyph = middleOfNextGlyph;
    }
    markBuffer.add(glyphBuffer.glyphAt(glyphBuffer.size() - 1) ? markGlyph : spaceGlyph, markFontData, 0);

    drawGlyphBuffer(context, runInfo, markBuffer, startPoint);
}

float Font::floatWidthForSimpleText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    WidthIterator it(this, run, fallbackFonts, glyphOverflow);
    GlyphBuffer glyphBuffer;
    it.advance(run.length(), (typesettingFeatures() & (Kerning | Ligatures)) ? &glyphBuffer : 0);

    if (glyphOverflow) {
        glyphOverflow->top = max<int>(glyphOverflow->top, ceilf(-it.minGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().ascent()));
        glyphOverflow->bottom = max<int>(glyphOverflow->bottom, ceilf(it.maxGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().descent()));
        glyphOverflow->left = ceilf(it.firstGlyphOverflow());
        glyphOverflow->right = ceilf(it.lastGlyphOverflow());
    }

    return it.m_runWidthSoFar;
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const FloatPoint& point, int h, int from, int to) const
{
    GlyphBuffer glyphBuffer;
    WidthIterator it(this, run);
    it.advance(from, &glyphBuffer);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to, &glyphBuffer);
    float afterWidth = it.m_runWidthSoFar;

    // Using roundf() rather than ceilf() for the right edge as a compromise to
    // ensure correct caret positioning.
    // Use LayoutUnit::epsilon() to ensure that values that cannot be stored as
    // an integer are floored to n and not n-1 due to floating point imprecision.
    if (run.rtl()) {
        it.advance(run.length(), &glyphBuffer);
        float totalWidth = it.m_runWidthSoFar;
        float pixelAlignedX = floorf(point.x() + totalWidth - afterWidth + LayoutUnit::epsilon());
        return FloatRect(pixelAlignedX, point.y(),
            roundf(point.x() + totalWidth - beforeWidth) - pixelAlignedX, h);
    }

    float pixelAlignedX = floorf(point.x() + beforeWidth + LayoutUnit::epsilon());
    return FloatRect(pixelAlignedX, point.y(),
        roundf(point.x() + afterWidth) - pixelAlignedX, h);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    float delta = x;

    WidthIterator it(this, run);
    GlyphBuffer localGlyphBuffer;
    unsigned offset;
    if (run.rtl()) {
        delta -= floatWidthForSimpleText(run);
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, localGlyphBuffer))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, localGlyphBuffer))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset;
}

}
