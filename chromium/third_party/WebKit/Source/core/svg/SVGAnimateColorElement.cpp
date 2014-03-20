/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 */

#include "config.h"

#include "SVGNames.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGAnimateColorElement.h"

namespace WebCore {

inline SVGAnimateColorElement::SVGAnimateColorElement(Document& document)
    : SVGAnimateElement(SVGNames::animateColorTag, document)
{
    ScriptWrappable::init(this);

    UseCounter::count(document, UseCounter::SVGAnimateColorElement);
}

PassRefPtr<SVGAnimateColorElement> SVGAnimateColorElement::create(Document& document)
{
    return adoptRef(new SVGAnimateColorElement(document));
}

static bool attributeValueIsCurrentColor(const String& value)
{
    DEFINE_STATIC_LOCAL(const AtomicString, currentColor, ("currentColor", AtomicString::ConstructFromLiteral));
    return value == currentColor;
}

void SVGAnimateColorElement::determinePropertyValueTypes(const String& from, const String& to)
{
    SVGAnimateElement::determinePropertyValueTypes(from, to);
    if (attributeValueIsCurrentColor(from))
        m_fromPropertyValueType = CurrentColorValue;
    if (attributeValueIsCurrentColor(to))
        m_toPropertyValueType = CurrentColorValue;
}

}
