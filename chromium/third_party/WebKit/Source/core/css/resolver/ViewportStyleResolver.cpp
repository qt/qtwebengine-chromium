/*
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/resolver/ViewportStyleResolver.h"

#include "CSSValueKeywords.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/dom/Document.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/ViewportDescription.h"

namespace WebCore {

ViewportStyleResolver::ViewportStyleResolver(Document* document)
    : m_document(document),
    m_hasAuthorStyle(false)
{
    ASSERT(m_document);
}

ViewportStyleResolver::~ViewportStyleResolver()
{
}

void ViewportStyleResolver::collectViewportRules(RuleSet* rules, Origin origin)
{
    rules->compactRulesIfNeeded();

    const Vector<StyleRuleViewport*>& viewportRules = rules->viewportRules();
    for (size_t i = 0; i < viewportRules.size(); ++i)
        addViewportRule(viewportRules[i], origin);
}

void ViewportStyleResolver::addViewportRule(StyleRuleViewport* viewportRule, Origin origin)
{
    StylePropertySet* propertySet = viewportRule->mutableProperties();

    unsigned propertyCount = propertySet->propertyCount();
    if (!propertyCount)
        return;

    if (origin == AuthorOrigin)
        m_hasAuthorStyle = true;

    if (!m_propertySet) {
        m_propertySet = propertySet->mutableCopy();
        return;
    }

    // We cannot use mergeAndOverrideOnConflict() here because it doesn't
    // respect the !important declaration (but addParsedProperty() does).
    for (unsigned i = 0; i < propertyCount; ++i)
        m_propertySet->addParsedProperty(propertySet->propertyAt(i).toCSSProperty());
}

void ViewportStyleResolver::clearDocument()
{
    m_document = 0;
}

void ViewportStyleResolver::resolve()
{
    if (!m_document)
        return;

    if (!m_propertySet || (!m_hasAuthorStyle && m_document->hasLegacyViewportTag())) {
        ASSERT(!m_hasAuthorStyle);
        m_propertySet = 0;
        m_document->setViewportDescription(ViewportDescription());
        return;
    }

    ViewportDescription description(m_hasAuthorStyle ? ViewportDescription::AuthorStyleSheet : ViewportDescription::UserAgentStyleSheet);

    description.userZoom = viewportArgumentValue(CSSPropertyUserZoom);
    description.zoom = viewportArgumentValue(CSSPropertyZoom);
    description.minZoom = viewportArgumentValue(CSSPropertyMinZoom);
    description.maxZoom = viewportArgumentValue(CSSPropertyMaxZoom);
    description.minWidth = viewportLengthValue(CSSPropertyMinWidth);
    description.maxWidth = viewportLengthValue(CSSPropertyMaxWidth);
    description.minHeight = viewportLengthValue(CSSPropertyMinHeight);
    description.maxHeight = viewportLengthValue(CSSPropertyMaxHeight);
    description.orientation = viewportArgumentValue(CSSPropertyOrientation);

    m_document->setViewportDescription(description);

    m_propertySet = 0;
    m_hasAuthorStyle = false;
}

float ViewportStyleResolver::viewportArgumentValue(CSSPropertyID id) const
{
    float defaultValue = ViewportDescription::ValueAuto;

    // UserZoom default value is CSSValueZoom, which maps to true, meaning that
    // yes, it is user scalable. When the value is set to CSSValueFixed, we
    // return false.
    if (id == CSSPropertyUserZoom)
        defaultValue = 1;

    RefPtr<CSSValue> value = m_propertySet->getPropertyCSSValue(id);
    if (!value || !value->isPrimitiveValue())
        return defaultValue;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value.get());

    if (primitiveValue->isNumber() || primitiveValue->isPx())
        return primitiveValue->getFloatValue();

    if (primitiveValue->isFontRelativeLength())
        return primitiveValue->getFloatValue() * m_document->renderStyle()->fontDescription().computedSize();

    if (primitiveValue->isPercentage()) {
        float percentValue = primitiveValue->getFloatValue() / 100.0f;
        switch (id) {
        case CSSPropertyMaxZoom:
        case CSSPropertyMinZoom:
        case CSSPropertyZoom:
            return percentValue;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    switch (primitiveValue->getValueID()) {
    case CSSValueAuto:
        return defaultValue;
    case CSSValueLandscape:
        return ViewportDescription::ValueLandscape;
    case CSSValuePortrait:
        return ViewportDescription::ValuePortrait;
    case CSSValueZoom:
        return defaultValue;
    case CSSValueInternalExtendToZoom:
        return ViewportDescription::ValueExtendToZoom;
    case CSSValueFixed:
        return 0;
    default:
        return defaultValue;
    }
}

Length ViewportStyleResolver::viewportLengthValue(CSSPropertyID id) const
{
    ASSERT(id == CSSPropertyMaxHeight
        || id == CSSPropertyMinHeight
        || id == CSSPropertyMaxWidth
        || id == CSSPropertyMinWidth);

    RefPtr<CSSValue> value = m_propertySet->getPropertyCSSValue(id);
    if (!value || !value->isPrimitiveValue())
        return Length(); // auto

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value.get());

    if (primitiveValue->isLength())
        return primitiveValue->computeLength<Length>(CSSToLengthConversionData(m_document->renderStyle(), m_document->renderStyle(), 1.0f));

    if (primitiveValue->isViewportPercentageLength())
        return primitiveValue->viewportPercentageLength();

    if (primitiveValue->isPercentage())
        return Length(primitiveValue->getFloatValue(), Percent);

    switch (primitiveValue->getValueID()) {
    case CSSValueInternalExtendToZoom:
        return Length(ExtendToZoom);
    case CSSValueAuto:
        return Length();
    default:
        // Unrecognized keyword.
        ASSERT_NOT_REACHED();
        return Length(0, Fixed);
    }
}

} // namespace WebCore
