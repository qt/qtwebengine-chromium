/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "core/html/track/TextTrackCueGeneric.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/track/TextTrackCue.h"

namespace WebCore {

class TextTrackCueGenericBoxElement FINAL : public TextTrackCueBox {
public:
    static PassRefPtr<TextTrackCueGenericBoxElement> create(Document& document, TextTrackCueGeneric* cue)
    {
        return adoptRef(new TextTrackCueGenericBoxElement(document, cue));
    }

    virtual void applyCSSProperties(const IntSize&) OVERRIDE;

private:
    TextTrackCueGenericBoxElement(Document&, TextTrackCue*);
};

TextTrackCueGenericBoxElement::TextTrackCueGenericBoxElement(Document& document, TextTrackCue* cue)
    : TextTrackCueBox(document, cue)
{
}

void TextTrackCueGenericBoxElement::applyCSSProperties(const IntSize& videoSize)
{
    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueWebkitPlaintext);

    TextTrackCueGeneric* cue = static_cast<TextTrackCueGeneric*>(getCue());

    float size = static_cast<float>(cue->getCSSSize());
    if (cue->useDefaultPosition()) {
        setInlineStyleProperty(CSSPropertyBottom, 0.0, CSSPrimitiveValue::CSS_PX);
        setInlineStyleProperty(CSSPropertyMarginBottom, 1.0, CSSPrimitiveValue::CSS_PERCENTAGE);
    } else {
        setInlineStyleProperty(CSSPropertyLeft, static_cast<float>(cue->position()), CSSPrimitiveValue::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyTop, static_cast<float>(cue->line()), CSSPrimitiveValue::CSS_PERCENTAGE);

        if (cue->getWritingDirection() == TextTrackCue::Horizontal)
            setInlineStyleProperty(CSSPropertyWidth, size, CSSPrimitiveValue::CSS_PERCENTAGE);
        else
            setInlineStyleProperty(CSSPropertyHeight, size,  CSSPrimitiveValue::CSS_PERCENTAGE);
    }

    if (cue->foregroundColor().isValid())
        setInlineStyleProperty(CSSPropertyColor, cue->foregroundColor().serialized());

    if (cue->backgroundColor().isValid())
        cue->element()->setInlineStyleProperty(CSSPropertyBackgroundColor, cue->backgroundColor().serialized());

    if (cue->getWritingDirection() == TextTrackCue::Horizontal)
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    else
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);

    if (cue->baseFontSizeRelativeToVideoHeight()) {
        double fontSize = videoSize.height() * cue->baseFontSizeRelativeToVideoHeight() / 100;
        if (cue->fontSizeMultiplier())
            fontSize *= cue->fontSizeMultiplier() / 100;
        setInlineStyleProperty(CSSPropertyFontSize, fontSize, CSSPrimitiveValue::CSS_PX);
    }

    if (cue->getAlignment() == TextTrackCue::Middle)
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueCenter);
    else if (cue->getAlignment() == TextTrackCue::End)
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueEnd);
    else
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueStart);

    setInlineStyleProperty(CSSPropertyWebkitWritingMode, cue->getCSSWritingMode(), false);
    setInlineStyleProperty(CSSPropertyWhiteSpace, CSSValuePreWrap);
    setInlineStyleProperty(CSSPropertyWordBreak, CSSValueNormal);
}

TextTrackCueGeneric::TextTrackCueGeneric(ScriptExecutionContext* context, double start, double end, const String& content)
    : TextTrackCue(context, start, end, content)
    , m_baseFontSizeRelativeToVideoHeight(0)
    , m_fontSizeMultiplier(0)
    , m_defaultPosition(true)
{
}

PassRefPtr<TextTrackCueBox> TextTrackCueGeneric::createDisplayTree()
{
    ASSERT(ownerDocument());
    return TextTrackCueGenericBoxElement::create(*ownerDocument(), this);
}

void TextTrackCueGeneric::setLine(int line, ExceptionState& es)
{
    m_defaultPosition = false;
    TextTrackCue::setLine(line, es);
}

void TextTrackCueGeneric::setPosition(int position, ExceptionState& es)
{
    m_defaultPosition = false;
    TextTrackCue::setPosition(position, es);
}

void TextTrackCueGeneric::videoSizeDidChange(const IntSize& videoSize)
{
    if (!hasDisplayTree())
        return;

    if (baseFontSizeRelativeToVideoHeight()) {
        double fontSize = videoSize.height() * baseFontSizeRelativeToVideoHeight() / 100;
        if (fontSizeMultiplier())
            fontSize *= fontSizeMultiplier() / 100;
        displayTreeInternal()->setInlineStyleProperty(CSSPropertyFontSize, fontSize, CSSPrimitiveValue::CSS_PX);
    }

}

bool TextTrackCueGeneric::operator==(const TextTrackCue& cue) const
{
    if (cue.cueType() != TextTrackCue::Generic)
        return false;

    const TextTrackCueGeneric* other = static_cast<const TextTrackCueGeneric*>(&cue);

    if (m_baseFontSizeRelativeToVideoHeight != other->baseFontSizeRelativeToVideoHeight())
        return false;
    if (m_fontSizeMultiplier != other->fontSizeMultiplier())
        return false;
    if (m_fontName != other->fontName())
        return false;
    if (m_foregroundColor != other->foregroundColor())
        return false;
    if (m_backgroundColor != other->backgroundColor())
        return false;

    return TextTrackCue::operator==(cue);
}

} // namespace WebCore

