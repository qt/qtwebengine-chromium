/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "core/svg/SVGClipPathElement.h"

#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "core/svg/SVGElementInstance.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGClipPathElement, SVGNames::clipPathUnitsAttr, ClipPathUnits, clipPathUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_BOOLEAN(SVGClipPathElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGClipPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(clipPathUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGClipPathElement::SVGClipPathElement(Document& document)
    : SVGGraphicsElement(SVGNames::clipPathTag, document)
    , m_clipPathUnits(SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE)
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGClipPathElement();
}

PassRefPtr<SVGClipPathElement> SVGClipPathElement::create(Document& document)
{
    return adoptRef(new SVGClipPathElement(document));
}

bool SVGClipPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::clipPathUnitsAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGClipPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGGraphicsElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::clipPathUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setClipPathUnitsBaseValue(propertyValue);
        return;
    }

    if (SVGExternalResourcesRequired::parseAttribute(name, value))
        return;

    ASSERT_NOT_REACHED();
}

void SVGClipPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    RenderSVGResourceContainer* renderer = toRenderSVGResourceContainer(this->renderer());
    if (renderer)
        renderer->invalidateCacheAndMarkForLayout();
}

void SVGClipPathElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGGraphicsElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

RenderObject* SVGClipPathElement::createRenderer(RenderStyle*)
{
    return new RenderSVGResourceClipper(this);
}

}
