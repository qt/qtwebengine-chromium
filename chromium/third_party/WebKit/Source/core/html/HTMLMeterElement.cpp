/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "core/html/HTMLMeterElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/MeterShadowElement.h"
#include "core/rendering/RenderMeter.h"
#include "core/rendering/RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

HTMLMeterElement::HTMLMeterElement(Document& document)
    : LabelableElement(meterTag, document)
{
    ScriptWrappable::init(this);
}

HTMLMeterElement::~HTMLMeterElement()
{
}

PassRefPtr<HTMLMeterElement> HTMLMeterElement::create(Document& document)
{
    RefPtr<HTMLMeterElement> meter = adoptRef(new HTMLMeterElement(document));
    meter->ensureUserAgentShadowRoot();
    return meter;
}

RenderObject* HTMLMeterElement::createRenderer(RenderStyle* style)
{
    if (hasAuthorShadowRoot() || !RenderTheme::theme().supportsMeter(style->appearance()))
        return RenderObject::createObject(this, style);

    return new RenderMeter(this);
}

void HTMLMeterElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == valueAttr || name == minAttr || name == maxAttr || name == lowAttr || name == highAttr || name == optimumAttr)
        didElementStateChange();
    else
        LabelableElement::parseAttribute(name, value);
}

double HTMLMeterElement::min() const
{
    return getFloatingPointAttribute(minAttr, 0);
}

void HTMLMeterElement::setMin(double min, ExceptionState& exceptionState)
{
    if (!std::isfinite(min)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(min));
        return;
    }
    setFloatingPointAttribute(minAttr, min);
}

double HTMLMeterElement::max() const
{
    return std::max(getFloatingPointAttribute(maxAttr, std::max(1.0, min())), min());
}

void HTMLMeterElement::setMax(double max, ExceptionState& exceptionState)
{
    if (!std::isfinite(max)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(max));
        return;
    }
    setFloatingPointAttribute(maxAttr, max);
}

double HTMLMeterElement::value() const
{
    double value = getFloatingPointAttribute(valueAttr, 0);
    return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value, ExceptionState& exceptionState)
{
    if (!std::isfinite(value)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(value));
        return;
    }
    setFloatingPointAttribute(valueAttr, value);
}

double HTMLMeterElement::low() const
{
    double low = getFloatingPointAttribute(lowAttr, min());
    return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low, ExceptionState& exceptionState)
{
    if (!std::isfinite(low)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(low));
        return;
    }
    setFloatingPointAttribute(lowAttr, low);
}

double HTMLMeterElement::high() const
{
    double high = getFloatingPointAttribute(highAttr, max());
    return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high, ExceptionState& exceptionState)
{
    if (!std::isfinite(high)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(high));
        return;
    }
    setFloatingPointAttribute(highAttr, high);
}

double HTMLMeterElement::optimum() const
{
    double optimum = getFloatingPointAttribute(optimumAttr, (max() + min()) / 2);
    return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum, ExceptionState& exceptionState)
{
    if (!std::isfinite(optimum)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(optimum));
        return;
    }
    setFloatingPointAttribute(optimumAttr, optimum);
}

HTMLMeterElement::GaugeRegion HTMLMeterElement::gaugeRegion() const
{
    double lowValue = low();
    double highValue = high();
    double theValue = value();
    double optimumValue = optimum();

    if (optimumValue < lowValue) {
        // The optimum range stays under low
        if (theValue <= lowValue)
            return GaugeRegionOptimum;
        if (theValue <= highValue)
            return GaugeRegionSuboptimal;
        return GaugeRegionEvenLessGood;
    }

    if (highValue < optimumValue) {
        // The optimum range stays over high
        if (highValue <= theValue)
            return GaugeRegionOptimum;
        if (lowValue <= theValue)
            return GaugeRegionSuboptimal;
        return GaugeRegionEvenLessGood;
    }

    // The optimum range stays between high and low.
    // According to the standard, <meter> never show GaugeRegionEvenLessGood in this case
    // because the value is never less or greater than min or max.
    if (lowValue <= theValue && theValue <= highValue)
        return GaugeRegionOptimum;
    return GaugeRegionSuboptimal;
}

double HTMLMeterElement::valueRatio() const
{
    double min = this->min();
    double max = this->max();
    double value = this->value();

    if (max <= min)
        return 0;
    return (value - min) / (max - min);
}

void HTMLMeterElement::didElementStateChange()
{
    m_value->setWidthPercentage(valueRatio()*100);
    m_value->updatePseudo();
    if (RenderMeter* render = renderMeter())
        render->updateFromElement();
}

RenderMeter* HTMLMeterElement::renderMeter() const
{
    if (renderer() && renderer()->isMeter())
        return toRenderMeter(renderer());

    RenderObject* renderObject = userAgentShadowRoot()->firstChild()->renderer();
    return toRenderMeter(renderObject);
}

void HTMLMeterElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    ASSERT(!m_value);

    RefPtr<MeterInnerElement> inner = MeterInnerElement::create(document());
    root.appendChild(inner);

    RefPtr<MeterBarElement> bar = MeterBarElement::create(document());
    m_value = MeterValueElement::create(document());
    m_value->setWidthPercentage(0);
    m_value->updatePseudo();
    bar->appendChild(m_value);

    inner->appendChild(bar);
}

} // namespace
