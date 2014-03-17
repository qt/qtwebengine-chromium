/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/svg/SVGGraphicsElement.h"

#include "SVGNames.h"
#include "core/rendering/svg/RenderSVGPath.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/SVGPathData.h"
#include "core/svg/SVGElementInstance.h"
#include "platform/transforms/AffineTransform.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_TRANSFORM_LIST(SVGGraphicsElement, SVGNames::transformAttr, Transform, transform)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGGraphicsElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(transform)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tagName, Document& document, ConstructionType constructionType)
    : SVGElement(tagName, document, constructionType)
{
    registerAnimatedPropertiesForSVGGraphicsElement();
}

SVGGraphicsElement::~SVGGraphicsElement()
{
}

AffineTransform SVGGraphicsElement::getTransformToElement(SVGElement* target, ExceptionState& exceptionState)
{
    AffineTransform ctm = getCTM(AllowStyleUpdate);

    if (target && target->isSVGGraphicsElement()) {
        AffineTransform targetCTM = toSVGGraphicsElement(target)->getCTM(AllowStyleUpdate);
        if (!targetCTM.isInvertible()) {
            exceptionState.throwUninformativeAndGenericDOMException(InvalidStateError);
            return ctm;
        }
        ctm = targetCTM.inverse() * ctm;
    }

    return ctm;
}

static AffineTransform computeCTM(SVGGraphicsElement* element, SVGElement::CTMScope mode, SVGGraphicsElement::StyleUpdateStrategy styleUpdateStrategy)
{
    ASSERT(element);
    if (styleUpdateStrategy == SVGGraphicsElement::AllowStyleUpdate)
        element->document().updateLayoutIgnorePendingStylesheets();

    AffineTransform ctm;

    SVGElement* stopAtElement = mode == SVGGraphicsElement::NearestViewportScope ? element->nearestViewportElement() : 0;
    for (Element* currentElement = element; currentElement; currentElement = currentElement->parentOrShadowHostElement()) {
        if (!currentElement->isSVGElement())
            break;

        ctm = toSVGElement(currentElement)->localCoordinateSpaceTransform(mode).multiply(ctm);

        // For getCTM() computation, stop at the nearest viewport element
        if (currentElement == stopAtElement)
            break;
    }

    return ctm;
}

AffineTransform SVGGraphicsElement::getCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeCTM(this, NearestViewportScope, styleUpdateStrategy);
}

AffineTransform SVGGraphicsElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeCTM(this, ScreenScope, styleUpdateStrategy);
}

AffineTransform SVGGraphicsElement::animatedLocalTransform() const
{
    AffineTransform matrix;
    RenderStyle* style = renderer() ? renderer()->style() : 0;

    // If CSS property was set, use that, otherwise fallback to attribute (if set).
    if (style && style->hasTransform()) {
        // Note: objectBoundingBox is an emptyRect for elements like pattern or clipPath.
        // See the "Object bounding box units" section of http://dev.w3.org/csswg/css3-transforms/
        TransformationMatrix transform;
        style->applyTransform(transform, renderer()->objectBoundingBox());

        // Flatten any 3D transform.
        matrix = transform.toAffineTransform();

        // CSS bakes the zoom factor into lengths, including translation components.
        // In order to align CSS & SVG transforms, we need to invert this operation.
        float zoom = style->effectiveZoom();
        if (zoom != 1) {
            matrix.setE(matrix.e() / zoom);
            matrix.setF(matrix.f() / zoom);
        }
    } else {
        transformCurrentValue().concatenate(matrix);
    }

    if (m_supplementalTransform)
        return *m_supplementalTransform * matrix;
    return matrix;
}

AffineTransform* SVGGraphicsElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform = adoptPtr(new AffineTransform);
    return m_supplementalTransform.get();
}

bool SVGGraphicsElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::transformAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGGraphicsElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::transformAttr) {
        SVGTransformList newList;
        newList.parse(value);
        detachAnimatedTransformListWrappers(newList.size());
        setTransformBaseValue(newList);
        return;
    } else if (SVGTests::parseAttribute(name, value)) {
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGGraphicsElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    // Reattach so the isValid() check will be run again during renderer creation.
    if (SVGTests::isKnownAttribute(attrName)) {
        lazyReattachIfAttached();
        return;
    }

    RenderObject* object = renderer();
    if (!object)
        return;

    if (attrName == SVGNames::transformAttr) {
        object->setNeedsTransformUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(object);
        return;
    }

    ASSERT_NOT_REACHED();
}

static bool isViewportElement(Node* node)
{
    return (node->hasTagName(SVGNames::svgTag)
        || node->hasTagName(SVGNames::symbolTag)
        || node->hasTagName(SVGNames::foreignObjectTag)
        || node->hasTagName(SVGNames::imageTag));
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const
{
    for (Element* current = parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        if (isViewportElement(current))
            return toSVGElement(current);
    }

    return 0;
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const
{
    SVGElement* farthest = 0;
    for (Element* current = parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        if (isViewportElement(current))
            farthest = toSVGElement(current);
    }
    return farthest;
}

SVGRect SVGGraphicsElement::getBBox()
{
    document().updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support getBBox for detached elements.
    if (!renderer())
        return SVGRect();

    return renderer()->objectBoundingBox();
}

SVGRect SVGGraphicsElement::getStrokeBBox()
{
    document().updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support getStrokeBBox for detached elements.
    if (!renderer())
        return SVGRect();

    return renderer()->strokeBoundingBox();
}

RenderObject* SVGGraphicsElement::createRenderer(RenderStyle*)
{
    // By default, any subclass is expected to do path-based drawing
    return new RenderSVGPath(this);
}

void SVGGraphicsElement::toClipPath(Path& path)
{
    updatePathFromGraphicsElement(this, path);
    // FIXME: How do we know the element has done a layout?
    path.transform(animatedLocalTransform());
}

}
