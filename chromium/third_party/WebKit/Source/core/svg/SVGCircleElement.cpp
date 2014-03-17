/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGCircleElement.h"

#include "core/rendering/svg/RenderSVGEllipse.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGLength.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGCircleElement, SVGNames::cxAttr, Cx, cx)
DEFINE_ANIMATED_LENGTH(SVGCircleElement, SVGNames::cyAttr, Cy, cy)
DEFINE_ANIMATED_LENGTH(SVGCircleElement, SVGNames::rAttr, R, r)
DEFINE_ANIMATED_BOOLEAN(SVGCircleElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGCircleElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(cy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(r)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGCircleElement::SVGCircleElement(Document& document)
    : SVGGeometryElement(SVGNames::circleTag, document)
    , m_cx(LengthModeWidth)
    , m_cy(LengthModeHeight)
    , m_r(LengthModeOther)
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGCircleElement();
}

PassRefPtr<SVGCircleElement> SVGCircleElement::create(Document& document)
{
    return adoptRef(new SVGCircleElement(document));
}

bool SVGCircleElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::cxAttr);
        supportedAttributes.add(SVGNames::cyAttr);
        supportedAttributes.add(SVGNames::rAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGCircleElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGeometryElement::parseAttribute(name, value);
    else if (name == SVGNames::cxAttr)
        setCxBaseValue(SVGLength::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::cyAttr)
        setCyBaseValue(SVGLength::construct(LengthModeHeight, value, parseError));
    else if (name == SVGNames::rAttr)
        setRBaseValue(SVGLength::construct(LengthModeOther, value, parseError, ForbidNegativeLengths));
    else if (SVGExternalResourcesRequired::parseAttribute(name, value)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGCircleElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGeometryElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::cxAttr
                          || attrName == SVGNames::cyAttr
                          || attrName == SVGNames::rAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    RenderSVGShape* renderer = toRenderSVGShape(this->renderer());
    if (!renderer)
        return;

    if (isLengthAttribute) {
        renderer->setNeedsShapeUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool SVGCircleElement::selfHasRelativeLengths() const
{
    return cxCurrentValue().isRelative()
        || cyCurrentValue().isRelative()
        || rCurrentValue().isRelative();
}

RenderObject* SVGCircleElement::createRenderer(RenderStyle*)
{
    return new RenderSVGEllipse(this);
}

}
