/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "core/dom/CustomElement.h"

#include "HTMLNames.h"
#include "MathMLNames.h"
#include "SVGNames.h"
#include "core/dom/CustomElementCallbackScheduler.h"
#include "core/dom/CustomElementUpgradeCandidateMap.h"
#include "core/dom/Element.h"

namespace WebCore {

Vector<AtomicString>& CustomElement::allowedCustomTagNames()
{
    DEFINE_STATIC_LOCAL(Vector<AtomicString>, tagNames, ());
    return tagNames;
}

void CustomElement::allowTagName(const AtomicString& localName)
{
    AtomicString lower = localName.lower();
    if (isCustomTagName(lower))
        return;
    allowedCustomTagNames().append(lower);
}

bool CustomElement::isValidTypeName(const AtomicString& name)
{
    if (notFound != allowedCustomTagNames().find(name))
        return true;

    if (allowedCustomTagNames().size() > 0)
        return false;

    if (notFound == name.find('-'))
        return false;

    DEFINE_STATIC_LOCAL(Vector<AtomicString>, reservedNames, ());
    if (reservedNames.isEmpty()) {
        reservedNames.append(MathMLNames::annotation_xmlTag.localName());
        reservedNames.append(SVGNames::color_profileTag.localName());
        reservedNames.append(SVGNames::font_faceTag.localName());
        reservedNames.append(SVGNames::font_face_srcTag.localName());
        reservedNames.append(SVGNames::font_face_uriTag.localName());
        reservedNames.append(SVGNames::font_face_formatTag.localName());
        reservedNames.append(SVGNames::font_face_nameTag.localName());
        reservedNames.append(SVGNames::missing_glyphTag.localName());
    }

    if (notFound != reservedNames.find(name))
        return false;

    return Document::isValidName(name.string());
}

bool CustomElement::isCustomTagName(const AtomicString& localName)
{
    return isValidTypeName(localName);
}

void CustomElement::define(Element* element, PassRefPtr<CustomElementDefinition> passDefinition)
{
    RefPtr<CustomElementDefinition> definition(passDefinition);
    element->setCustomElementState(Element::Defined);
    definitions().add(element, definition);
    CustomElementCallbackScheduler::scheduleCreatedCallback(definition->callbacks(), element);
}

CustomElementDefinition* CustomElement::definitionFor(Element* element)
{
    return definitions().get(element);
}

void CustomElement::attributeDidChange(Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    ASSERT(element->customElementState() == Element::Upgraded);
    CustomElementCallbackScheduler::scheduleAttributeChangedCallback(definitions().get(element)->callbacks(), element, name, oldValue, newValue);
}

void CustomElement::didEnterDocument(Element* element, Document* document)
{
    ASSERT(element->customElementState() == Element::Upgraded);
    if (!document->defaultView())
        return;
    CustomElementCallbackScheduler::scheduleEnteredDocumentCallback(definitions().get(element)->callbacks(), element);
}

void CustomElement::didLeaveDocument(Element* element, Document* document)
{
    ASSERT(element->customElementState() == Element::Upgraded);
    if (!document->defaultView())
        return;
    CustomElementCallbackScheduler::scheduleLeftDocumentCallback(definitions().get(element)->callbacks(), element);
}

void CustomElement::wasDestroyed(Element* element)
{
    switch (element->customElementState()) {
    case Element::NotCustomElement:
        ASSERT_NOT_REACHED();
        break;

    case Element::UpgradeCandidate:
        CustomElementUpgradeCandidateMap::elementWasDestroyed(element);
        break;

    case Element::Defined:
    case Element::Upgraded:
        definitions().remove(element);
        break;
    }
}

void CustomElement::DefinitionMap::add(Element* element, PassRefPtr<CustomElementDefinition> definition)
{
    ASSERT(definition.get());
    DefinitionMap::ElementDefinitionHashMap::AddResult result = m_definitions.add(element, definition);
    ASSERT(result.isNewEntry);
}

void CustomElement::DefinitionMap::remove(Element* element)
{
    m_definitions.remove(element);
}

CustomElementDefinition* CustomElement::DefinitionMap::get(Element* element)
{
    DefinitionMap::ElementDefinitionHashMap::const_iterator it = m_definitions.find(element);
    ASSERT(it != m_definitions.end());
    return it->value.get();
}

CustomElement::DefinitionMap& CustomElement::definitions()
{
    DEFINE_STATIC_LOCAL(DefinitionMap, map, ());
    return map;
}

} // namespace WebCore
