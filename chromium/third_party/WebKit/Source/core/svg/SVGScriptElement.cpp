/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGScriptElement.h"

#include "HTMLNames.h"
#include "XLinkNames.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/properties/SVGAnimatedStaticPropertyTearOff.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGScriptElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGScriptElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGScriptElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGScriptElement::SVGScriptElement(Document& document, bool wasInsertedByParser, bool alreadyStarted)
    : SVGElement(SVGNames::scriptTag, document)
    , m_svgLoadEventTimer(this, &SVGElement::svgLoadEventTimerFired)
    , m_loader(ScriptLoader::create(this, wasInsertedByParser, alreadyStarted))
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGScriptElement();
}

PassRefPtr<SVGScriptElement> SVGScriptElement::create(Document& document, bool insertedByParser)
{
    return adoptRef(new SVGScriptElement(document, insertedByParser, false));
}

bool SVGScriptElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::typeAttr);
        supportedAttributes.add(HTMLNames::onerrorAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGScriptElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::typeAttr) {
        setType(value);
        return;
    }

    if (name == HTMLNames::onerrorAttr) {
        setAttributeEventListener(EventTypeNames::error, createAttributeEventListener(this, name, value));
        return;
    }

    if (SVGURIReference::parseAttribute(name, value))
        return;
    if (SVGExternalResourcesRequired::parseAttribute(name, value))
        return;

    ASSERT_NOT_REACHED();
}

void SVGScriptElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::typeAttr || attrName == HTMLNames::onerrorAttr)
        return;

    if (SVGURIReference::isKnownAttribute(attrName)) {
        m_loader->handleSourceAttribute(hrefCurrentValue());
        return;
    }

    if (SVGExternalResourcesRequired::handleAttributeChange(this, attrName))
        return;

    ASSERT_NOT_REACHED();
}

Node::InsertionNotificationRequest SVGScriptElement::insertedInto(ContainerNode* rootParent)
{
    SVGElement::insertedInto(rootParent);
    return InsertionShouldCallDidNotifySubtreeInsertions;
}

void SVGScriptElement::didNotifySubtreeInsertionsToDocument()
{
    m_loader->didNotifySubtreeInsertionsToDocument();
    SVGExternalResourcesRequired::insertedIntoDocument(this);
}

void SVGScriptElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    m_loader->childrenChanged();
}

bool SVGScriptElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == sourceAttributeValue();
}

void SVGScriptElement::finishParsingChildren()
{
    SVGElement::finishParsingChildren();
    SVGExternalResourcesRequired::finishParsingChildren();
}

String SVGScriptElement::type() const
{
    return m_type;
}

void SVGScriptElement::setType(const String& type)
{
    m_type = type;
}

void SVGScriptElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(hrefCurrentValue()));
}

String SVGScriptElement::sourceAttributeValue() const
{
    return hrefCurrentValue();
}

String SVGScriptElement::charsetAttributeValue() const
{
    return String();
}

String SVGScriptElement::typeAttributeValue() const
{
    return type();
}

String SVGScriptElement::languageAttributeValue() const
{
    return String();
}

String SVGScriptElement::forAttributeValue() const
{
    return String();
}

String SVGScriptElement::eventAttributeValue() const
{
    return String();
}

bool SVGScriptElement::asyncAttributeValue() const
{
    return false;
}

bool SVGScriptElement::deferAttributeValue() const
{
    return false;
}

bool SVGScriptElement::hasSourceAttribute() const
{
    return hasAttribute(XLinkNames::hrefAttr);
}

PassRefPtr<Element> SVGScriptElement::cloneElementWithoutAttributesAndChildren()
{
    return adoptRef(new SVGScriptElement(document(), false, m_loader->alreadyStarted()));
}

void SVGScriptElement::setHaveFiredLoadEvent(bool haveFiredLoadEvent)
{
    m_loader->setHaveFiredLoadEvent(haveFiredLoadEvent);
}

bool SVGScriptElement::isParserInserted() const
{
    return m_loader->isParserInserted();
}

bool SVGScriptElement::haveFiredLoadEvent() const
{
    return m_loader->haveFiredLoadEvent();
}

Timer<SVGElement>* SVGScriptElement::svgLoadEventTimer()
{
    return &m_svgLoadEventTimer;
}

}
