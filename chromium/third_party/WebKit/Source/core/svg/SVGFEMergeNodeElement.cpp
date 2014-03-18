/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGFEMergeNodeElement.h"

#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEMergeNodeElement, SVGNames::inAttr, In1, in1)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEMergeNodeElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEMergeNodeElement::SVGFEMergeNodeElement(Document& document)
    : SVGElement(SVGNames::feMergeNodeTag, document)
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGFEMergeNodeElement();
}

PassRefPtr<SVGFEMergeNodeElement> SVGFEMergeNodeElement::create(Document& document)
{
    return adoptRef(new SVGFEMergeNodeElement(document));
}

bool SVGFEMergeNodeElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        supportedAttributes.add(SVGNames::inAttr);
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEMergeNodeElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGFEMergeNodeElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::inAttr) {
        invalidateFilterPrimitiveParent(this);
        return;
    }

    ASSERT_NOT_REACHED();
}

}
