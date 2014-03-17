/*
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

#include "core/svg/SVGMPathElement.h"

#include "XLinkNames.h"
#include "core/dom/Document.h"
#include "core/svg/SVGAnimateMotionElement.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGPathElement.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGMPathElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGMPathElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGMPathElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGMPathElement::SVGMPathElement(Document& document)
    : SVGElement(SVGNames::mpathTag, document)
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGMPathElement();
}

PassRefPtr<SVGMPathElement> SVGMPathElement::create(Document& document)
{
    return adoptRef(new SVGMPathElement(document));
}

SVGMPathElement::~SVGMPathElement()
{
    clearResourceReferences();
}

void SVGMPathElement::buildPendingResource()
{
    clearResourceReferences();
    if (!inDocument())
        return;

    String id;
    Element* target = SVGURIReference::targetElementFromIRIString(hrefCurrentValue(), document(), &id);
    if (!target) {
        // Do not register as pending if we are already pending this resource.
        if (document().accessSVGExtensions()->isElementPendingResource(this, id))
            return;

        if (!id.isEmpty()) {
            document().accessSVGExtensions()->addPendingResource(id, this);
            ASSERT(hasPendingResources());
        }
    } else if (target->isSVGElement()) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document().accessSVGExtensions()->addElementReferencingTarget(this, toSVGElement(target));
    }

    targetPathChanged();
}

void SVGMPathElement::clearResourceReferences()
{
    document().accessSVGExtensions()->removeAllTargetReferencesForElement(this);
}

Node::InsertionNotificationRequest SVGMPathElement::insertedInto(ContainerNode* rootParent)
{
    SVGElement::insertedInto(rootParent);
    if (rootParent->inDocument())
        buildPendingResource();
    return InsertionDone;
}

void SVGMPathElement::removedFrom(ContainerNode* rootParent)
{
    SVGElement::removedFrom(rootParent);
    notifyParentOfPathChange(rootParent);
    if (rootParent->inDocument())
        clearResourceReferences();
}

bool SVGMPathElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGMPathElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    if (SVGURIReference::parseAttribute(name, value))
        return;
    if (SVGExternalResourcesRequired::parseAttribute(name, value))
        return;

    ASSERT_NOT_REACHED();
}

void SVGMPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        return;

    ASSERT_NOT_REACHED();
}

SVGPathElement* SVGMPathElement::pathElement()
{
    Element* target = targetElementFromIRIString(hrefCurrentValue(), document());
    if (target && target->hasTagName(SVGNames::pathTag))
        return toSVGPathElement(target);
    return 0;
}

void SVGMPathElement::targetPathChanged()
{
    notifyParentOfPathChange(parentNode());
}

void SVGMPathElement::notifyParentOfPathChange(ContainerNode* parent)
{
    if (parent && parent->hasTagName(SVGNames::animateMotionTag))
        static_cast<SVGAnimateMotionElement*>(parent)->updateAnimationPath();
}

} // namespace WebCore
