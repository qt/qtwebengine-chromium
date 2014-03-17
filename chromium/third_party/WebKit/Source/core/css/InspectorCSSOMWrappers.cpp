/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/css/InspectorCSSOMWrappers.h"

#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSRegionRule.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSSupportsRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/StyleEngine.h"

namespace WebCore {

void InspectorCSSOMWrappers::collectFromStyleSheetIfNeeded(CSSStyleSheet* styleSheet)
{
    if (!m_styleRuleToCSSOMWrapperMap.isEmpty())
        collect(styleSheet);
}

void InspectorCSSOMWrappers::reset()
{
    m_styleRuleToCSSOMWrapperMap.clear();
    m_styleSheetCSSOMWrapperSet.clear();
}

template <class ListType>
void InspectorCSSOMWrappers::collect(ListType* listType)
{
    if (!listType)
        return;
    unsigned size = listType->length();
    for (unsigned i = 0; i < size; ++i) {
        CSSRule* cssRule = listType->item(i);
        switch (cssRule->type()) {
        case CSSRule::IMPORT_RULE:
            collect(toCSSImportRule(cssRule)->styleSheet());
            break;
        case CSSRule::MEDIA_RULE:
            collect(toCSSMediaRule(cssRule));
            break;
        case CSSRule::SUPPORTS_RULE:
            collect(toCSSSupportsRule(cssRule));
            break;
        case CSSRule::WEBKIT_REGION_RULE:
            collect(toCSSRegionRule(cssRule));
            break;
        case CSSRule::STYLE_RULE:
            m_styleRuleToCSSOMWrapperMap.add(toCSSStyleRule(cssRule)->styleRule(), toCSSStyleRule(cssRule));
            break;
        default:
            break;
        }
    }
}

void InspectorCSSOMWrappers::collectFromStyleSheetContents(HashSet<RefPtr<CSSStyleSheet> >& sheetWrapperSet, StyleSheetContents* styleSheet)
{
    if (!styleSheet)
        return;
    RefPtr<CSSStyleSheet> styleSheetWrapper = CSSStyleSheet::create(styleSheet);
    sheetWrapperSet.add(styleSheetWrapper);
    collect(styleSheetWrapper.get());
}

void InspectorCSSOMWrappers::collectFromStyleSheets(const Vector<RefPtr<CSSStyleSheet> >& sheets)
{
    for (unsigned i = 0; i < sheets.size(); ++i)
        collect(sheets[i].get());
}

void InspectorCSSOMWrappers::collectFromStyleEngine(StyleEngine* styleSheetCollection)
{
    Vector<const Vector<RefPtr<CSSStyleSheet> >*> activeAuthorStyleSheets;
    styleSheetCollection->getActiveAuthorStyleSheets(activeAuthorStyleSheets);
    for (size_t i = 0; i < activeAuthorStyleSheets.size(); ++i)
        collectFromStyleSheets(*activeAuthorStyleSheets[i]);
}

CSSStyleRule* InspectorCSSOMWrappers::getWrapperForRuleInSheets(StyleRule* rule, StyleEngine* styleSheetCollection)
{
    if (m_styleRuleToCSSOMWrapperMap.isEmpty()) {
        collectFromStyleSheetContents(m_styleSheetCSSOMWrapperSet, CSSDefaultStyleSheets::defaultStyleSheet);
        collectFromStyleSheetContents(m_styleSheetCSSOMWrapperSet, CSSDefaultStyleSheets::viewportStyleSheet);
        collectFromStyleSheetContents(m_styleSheetCSSOMWrapperSet, CSSDefaultStyleSheets::quirksStyleSheet);
        collectFromStyleSheetContents(m_styleSheetCSSOMWrapperSet, CSSDefaultStyleSheets::svgStyleSheet);
        collectFromStyleSheetContents(m_styleSheetCSSOMWrapperSet, CSSDefaultStyleSheets::mediaControlsStyleSheet);
        collectFromStyleSheetContents(m_styleSheetCSSOMWrapperSet, CSSDefaultStyleSheets::fullscreenStyleSheet);

        collectFromStyleEngine(styleSheetCollection);
    }
    return m_styleRuleToCSSOMWrapperMap.get(rule);
}

} // namespace WebCore
