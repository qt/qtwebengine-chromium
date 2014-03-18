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

#include "core/html/shadow/MeterShadowElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "core/html/HTMLMeterElement.h"
#include "core/rendering/RenderMeter.h"
#include "core/rendering/RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

inline MeterShadowElement::MeterShadowElement(Document& document)
    : HTMLDivElement(document)
{
}

HTMLMeterElement* MeterShadowElement::meterElement() const
{
    return toHTMLMeterElement(shadowHost());
}

bool MeterShadowElement::rendererIsNeeded(const RenderStyle& style)
{
    RenderObject* renderer = meterElement()->renderer();
    return renderer && !RenderTheme::theme().supportsMeter(renderer->style()->appearance()) && HTMLDivElement::rendererIsNeeded(style);
}

inline MeterInnerElement::MeterInnerElement(Document& document)
    : MeterShadowElement(document)
{
}

PassRefPtr<MeterInnerElement> MeterInnerElement::create(Document& document)
{
    RefPtr<MeterInnerElement> element = adoptRef(new MeterInnerElement(document));
    element->setPseudo(AtomicString("-webkit-meter-inner-element", AtomicString::ConstructFromLiteral));
    return element.release();
}

bool MeterInnerElement::rendererIsNeeded(const RenderStyle& style)
{
    if (meterElement()->hasAuthorShadowRoot())
        return HTMLDivElement::rendererIsNeeded(style);

    RenderObject* renderer = meterElement()->renderer();
    return renderer && !RenderTheme::theme().supportsMeter(renderer->style()->appearance()) && HTMLDivElement::rendererIsNeeded(style);
}

RenderObject* MeterInnerElement::createRenderer(RenderStyle*)
{
    return new RenderMeter(this);
}

inline MeterBarElement::MeterBarElement(Document& document)
    : MeterShadowElement(document)
{
}

PassRefPtr<MeterBarElement> MeterBarElement::create(Document& document)
{
    RefPtr<MeterBarElement> element = adoptRef(new MeterBarElement(document));
    element->setPseudo(AtomicString("-webkit-meter-bar", AtomicString::ConstructFromLiteral));
    return element.release();
}

inline MeterValueElement::MeterValueElement(Document& document)
    : MeterShadowElement(document)
{
}

PassRefPtr<MeterValueElement> MeterValueElement::create(Document& document)
{
    RefPtr<MeterValueElement> element = adoptRef(new MeterValueElement(document));
    element->updatePseudo();
    return element.release();
}

const AtomicString& MeterValueElement::valuePseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, optimumPseudoId, ("-webkit-meter-optimum-value", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, suboptimumPseudoId, ("-webkit-meter-suboptimum-value", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, evenLessGoodPseudoId, ("-webkit-meter-even-less-good-value", AtomicString::ConstructFromLiteral));

    HTMLMeterElement* meter = meterElement();
    if (!meter)
        return optimumPseudoId;

    switch (meter->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        return optimumPseudoId;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        return suboptimumPseudoId;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        return evenLessGoodPseudoId;
    default:
        ASSERT_NOT_REACHED();
        return optimumPseudoId;
    }
}

void MeterValueElement::setWidthPercentage(double width)
{
    setInlineStyleProperty(CSSPropertyWidth, width, CSSPrimitiveValue::CSS_PERCENTAGE);
}

}
