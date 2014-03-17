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
#include "core/css/RuleSet.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SelectorFilter.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/html/track/TextTrackCue.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

using namespace HTMLNames;

// -----------------------------------------------------------------

static inline bool isSelectorMatchingHTMLBasedOnRuleHash(const CSSSelector* selector)
{
    ASSERT(selector);
    if (selector->m_match == CSSSelector::Tag) {
        const AtomicString& selectorNamespace = selector->tagQName().namespaceURI();
        if (selectorNamespace != starAtom && selectorNamespace != xhtmlNamespaceURI)
            return false;
        if (selector->relation() == CSSSelector::SubSelector)
            return isSelectorMatchingHTMLBasedOnRuleHash(selector->tagHistory());
        return true;
    }
    if (SelectorChecker::isCommonPseudoClassSelector(selector))
        return true;
    return selector->m_match == CSSSelector::Id || selector->m_match == CSSSelector::Class;
}

static inline bool selectorListContainsUncommonAttributeSelector(const CSSSelector* selector)
{
    const CSSSelectorList* selectorList = selector->selectorList();
    if (!selectorList)
        return false;
    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(selector)) {
        for (const CSSSelector* component = selector; component; component = component->tagHistory()) {
            if (component->isAttributeSelector())
                return true;
        }
    }
    return false;
}

static inline bool isCommonAttributeSelectorAttribute(const QualifiedName& attribute)
{
    // These are explicitly tested for equality in canShareStyleWithElement.
    return attribute == typeAttr || attribute == readonlyAttr;
}

static inline bool containsUncommonAttributeSelector(const CSSSelector* selector)
{
    for (; selector; selector = selector->tagHistory()) {
        // Allow certain common attributes (used in the default style) in the selectors that match the current element.
        if (selector->isAttributeSelector() && !isCommonAttributeSelectorAttribute(selector->attribute()))
            return true;
        if (selectorListContainsUncommonAttributeSelector(selector))
            return true;
        if (selector->relation() != CSSSelector::SubSelector) {
            selector = selector->tagHistory();
            break;
        }
    }

    for (; selector; selector = selector->tagHistory()) {
        if (selector->isAttributeSelector())
            return true;
        if (selectorListContainsUncommonAttributeSelector(selector))
            return true;
    }
    return false;
}

static inline PropertyWhitelistType determinePropertyWhitelistType(const AddRuleFlags addRuleFlags, const CSSSelector* selector)
{
    if (addRuleFlags & RuleIsInRegionRule)
        return PropertyWhitelistRegion;
    for (const CSSSelector* component = selector; component; component = component->tagHistory()) {
        if (component->pseudoType() == CSSSelector::PseudoCue || (component->m_match == CSSSelector::PseudoElement && component->value() == TextTrackCue::cueShadowPseudoId()))
            return PropertyWhitelistCue;
    }
    return PropertyWhitelistNone;
}

namespace {

// FIXME: Should we move this class to WTF?
template<typename T>
class TerminatedArrayBuilder {
public:
    explicit TerminatedArrayBuilder(PassOwnPtr<T> array)
        : m_array(array)
        , m_count(0)
        , m_capacity(0)
    {
        if (!m_array)
            return;
        for (T* item = m_array.get(); !item->isLastInArray(); ++item)
            ++m_count;
        ++m_count; // To count the last item itself.
        m_capacity = m_count;
    }

    void grow(size_t count)
    {
        ASSERT(count);
        if (!m_array) {
            ASSERT(!m_count);
            ASSERT(!m_capacity);
            m_capacity = count;
            m_array = adoptPtr(static_cast<T*>(fastMalloc(m_capacity * sizeof(T))));
            return;
        }
        m_capacity += count;
        m_array = adoptPtr(static_cast<T*>(fastRealloc(m_array.leakPtr(), m_capacity * sizeof(T))));
        m_array.get()[m_count - 1].setLastInArray(false);
    }

    void append(const T& item)
    {
        RELEASE_ASSERT(m_count < m_capacity);
        ASSERT(!item.isLastInArray());
        m_array.get()[m_count++] = item;
    }

    PassOwnPtr<T> release()
    {
        RELEASE_ASSERT(m_count == m_capacity);
        if (m_array)
            m_array.get()[m_count - 1].setLastInArray(true);
        assertValid();
        return m_array.release();
    }

private:
#ifndef NDEBUG
    void assertValid()
    {
        for (size_t i = 0; i < m_count; ++i) {
            bool isLastInArray = (i + 1 == m_count);
            ASSERT(m_array.get()[i].isLastInArray() == isLastInArray);
        }
    }
#else
    void assertValid() { }
#endif

    OwnPtr<T> m_array;
    size_t m_count;
    size_t m_capacity;
};

}

RuleData::RuleData(StyleRule* rule, unsigned selectorIndex, unsigned position, AddRuleFlags addRuleFlags)
    : m_rule(rule)
    , m_selectorIndex(selectorIndex)
    , m_isLastInArray(false)
    , m_position(position)
    , m_hasFastCheckableSelector((addRuleFlags & RuleCanUseFastCheckSelector) && SelectorCheckerFastPath::canUse(selector()))
    , m_specificity(selector()->specificity())
    , m_hasMultipartSelector(!!selector()->tagHistory())
    , m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash(isSelectorMatchingHTMLBasedOnRuleHash(selector()))
    , m_containsUncommonAttributeSelector(WebCore::containsUncommonAttributeSelector(selector()))
    , m_linkMatchType(SelectorChecker::determineLinkMatchType(selector()))
    , m_hasDocumentSecurityOrigin(addRuleFlags & RuleHasDocumentSecurityOrigin)
    , m_propertyWhitelistType(determinePropertyWhitelistType(addRuleFlags, selector()))
{
    ASSERT(m_position == position);
    ASSERT(m_selectorIndex == selectorIndex);
    SelectorFilter::collectIdentifierHashes(selector(), m_descendantSelectorIdentifierHashes, maximumIdentifierCount);
}

static void collectFeaturesFromRuleData(RuleFeatureSet& features, const RuleData& ruleData)
{
    bool foundSiblingSelector = false;
    unsigned maxDirectAdjacentSelectors = 0;
    for (const CSSSelector* selector = ruleData.selector(); selector; selector = selector->tagHistory()) {
        features.collectFeaturesFromSelector(selector);

        if (const CSSSelectorList* selectorList = selector->selectorList()) {
            for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                // FIXME: Shouldn't this be checking subSelector->isSiblingSelector()?
                if (!foundSiblingSelector && selector->isSiblingSelector())
                    foundSiblingSelector = true;
                if (subSelector->isDirectAdjacentSelector())
                    maxDirectAdjacentSelectors++;
                features.collectFeaturesFromSelector(subSelector);
            }
        } else {
            if (!foundSiblingSelector && selector->isSiblingSelector())
                foundSiblingSelector = true;
            if (selector->isDirectAdjacentSelector())
                maxDirectAdjacentSelectors++;
        }
    }
    features.setMaxDirectAdjacentSelectors(maxDirectAdjacentSelectors);
    if (foundSiblingSelector)
        features.siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        features.uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
}

void RuleSet::addToRuleSet(StringImpl* key, PendingRuleMap& map, const RuleData& ruleData)
{
    if (!key)
        return;
    OwnPtr<LinkedStack<RuleData> >& rules = map.add(key, nullptr).iterator->value;
    if (!rules)
        rules = adoptPtr(new LinkedStack<RuleData>);
    rules->push(ruleData);
}

bool RuleSet::findBestRuleSetAndAdd(const CSSSelector* component, RuleData& ruleData)
{
    if (component->m_match == CSSSelector::Id) {
        addToRuleSet(component->value().impl(), ensurePendingRules()->idRules, ruleData);
        return true;
    }
    if (component->m_match == CSSSelector::Class) {
        addToRuleSet(component->value().impl(), ensurePendingRules()->classRules, ruleData);
        return true;
    }
    if (component->isCustomPseudoElement()) {
        addToRuleSet(component->value().impl(), ensurePendingRules()->shadowPseudoElementRules, ruleData);
        return true;
    }
    if (component->pseudoType() == CSSSelector::PseudoCue) {
        m_cuePseudoRules.append(ruleData);
        return true;
    }
    if (SelectorChecker::isCommonPseudoClassSelector(component)) {
        switch (component->pseudoType()) {
        case CSSSelector::PseudoLink:
        case CSSSelector::PseudoVisited:
        case CSSSelector::PseudoAnyLink:
            m_linkPseudoClassRules.append(ruleData);
            return true;
        case CSSSelector::PseudoFocus:
            m_focusPseudoClassRules.append(ruleData);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return true;
        }
    }

    if (component->m_match == CSSSelector::Tag) {
        if (component->tagQName().localName() != starAtom) {
            // If this is part of a subselector chain, recurse ahead to find a narrower set (ID/class.)
            if (component->relation() == CSSSelector::SubSelector
                && (component->tagHistory()->m_match == CSSSelector::Class || component->tagHistory()->m_match == CSSSelector::Id || SelectorChecker::isCommonPseudoClassSelector(component->tagHistory()))
                && findBestRuleSetAndAdd(component->tagHistory(), ruleData))
                return true;

            addToRuleSet(component->tagQName().localName().impl(), ensurePendingRules()->tagRules, ruleData);
            return true;
        }
    }
    return false;
}

void RuleSet::addRule(StyleRule* rule, unsigned selectorIndex, AddRuleFlags addRuleFlags)
{
    RuleData ruleData(rule, selectorIndex, m_ruleCount++, addRuleFlags);
    collectFeaturesFromRuleData(m_features, ruleData);

    if (!findBestRuleSetAndAdd(ruleData.selector(), ruleData)) {
        // If we didn't find a specialized map to stick it in, file under universal rules.
        m_universalRules.append(ruleData);
    }
}

void RuleSet::addPageRule(StyleRulePage* rule)
{
    ensurePendingRules(); // So that m_pageRules.shrinkToFit() gets called.
    m_pageRules.append(rule);
}

void RuleSet::addViewportRule(StyleRuleViewport* rule)
{
    ensurePendingRules(); // So that m_viewportRules.shrinkToFit() gets called.
    m_viewportRules.append(rule);
}

void RuleSet::addFontFaceRule(StyleRuleFontFace* rule)
{
    ensurePendingRules(); // So that m_fontFaceRules.shrinkToFit() gets called.
    m_fontFaceRules.append(rule);
}

void RuleSet::addKeyframesRule(StyleRuleKeyframes* rule)
{
    ensurePendingRules(); // So that m_keyframesRules.shrinkToFit() gets called.
    m_keyframesRules.append(rule);
}

void RuleSet::addRegionRule(StyleRuleRegion* regionRule, bool hasDocumentSecurityOrigin)
{
    ensurePendingRules(); // So that m_regionSelectorsAndRuleSets.shrinkToFit() gets called.
    OwnPtr<RuleSet> regionRuleSet = RuleSet::create();
    // The region rule set should take into account the position inside the parent rule set.
    // Otherwise, the rules inside region block might be incorrectly positioned before other similar rules from
    // the stylesheet that contains the region block.
    regionRuleSet->m_ruleCount = m_ruleCount;

    // Collect the region rules into a rule set
    // FIXME: Should this add other types of rules? (i.e. use addChildRules() directly?)
    const Vector<RefPtr<StyleRuleBase> >& childRules = regionRule->childRules();
    AddRuleFlags addRuleFlags = hasDocumentSecurityOrigin ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState;
    addRuleFlags = static_cast<AddRuleFlags>(addRuleFlags | RuleIsInRegionRule | RuleCanUseFastCheckSelector);
    for (unsigned i = 0; i < childRules.size(); ++i) {
        StyleRuleBase* regionStylingRule = childRules[i].get();
        if (regionStylingRule->isStyleRule())
            regionRuleSet->addStyleRule(toStyleRule(regionStylingRule), addRuleFlags);
    }
    // Update the "global" rule count so that proper order is maintained
    m_ruleCount = regionRuleSet->m_ruleCount;

    m_regionSelectorsAndRuleSets.append(RuleSetSelectorPair(regionRule->selectorList().first(), regionRuleSet.release()));
}

void RuleSet::addChildRules(const Vector<RefPtr<StyleRuleBase> >& rules, const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        StyleRuleBase* rule = rules[i].get();

        if (rule->isStyleRule()) {
            StyleRule* styleRule = toStyleRule(rule);

            const CSSSelectorList& selectorList = styleRule->selectorList();
            for (size_t selectorIndex = 0; selectorIndex != kNotFound; selectorIndex = selectorList.indexOfNextSelectorAfter(selectorIndex)) {
                if (selectorList.hasCombinatorCrossingTreeBoundaryAt(selectorIndex)) {
                    m_treeBoundaryCrossingRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else if (selectorList.hasShadowDistributedAt(selectorIndex)) {
                    m_shadowDistributedRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else {
                    addRule(styleRule, selectorIndex, addRuleFlags);
                }
            }
        } else if (rule->isPageRule()) {
            addPageRule(toStyleRulePage(rule));
        } else if (rule->isMediaRule()) {
            StyleRuleMedia* mediaRule = toStyleRuleMedia(rule);
            if ((!mediaRule->mediaQueries() || medium.eval(mediaRule->mediaQueries(), &m_viewportDependentMediaQueryResults)))
                addChildRules(mediaRule->childRules(), medium, addRuleFlags);
        } else if (rule->isFontFaceRule()) {
            addFontFaceRule(toStyleRuleFontFace(rule));
        } else if (rule->isKeyframesRule()) {
            addKeyframesRule(toStyleRuleKeyframes(rule));
        } else if (rule->isRegionRule()) {
            addRegionRule(toStyleRuleRegion(rule), addRuleFlags & RuleHasDocumentSecurityOrigin);
        } else if (rule->isViewportRule()) {
            addViewportRule(toStyleRuleViewport(rule));
        } else if (rule->isSupportsRule() && toStyleRuleSupports(rule)->conditionIsSupported()) {
            addChildRules(toStyleRuleSupports(rule)->childRules(), medium, addRuleFlags);
        }
    }
}

void RuleSet::addRulesFromSheet(StyleSheetContents* sheet, const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    ASSERT(sheet);

    addRuleFlags = static_cast<AddRuleFlags>(addRuleFlags | RuleCanUseFastCheckSelector);
    const Vector<RefPtr<StyleRuleImport> >& importRules = sheet->importRules();
    for (unsigned i = 0; i < importRules.size(); ++i) {
        StyleRuleImport* importRule = importRules[i].get();
        if (importRule->styleSheet() && (!importRule->mediaQueries() || medium.eval(importRule->mediaQueries(), &m_viewportDependentMediaQueryResults)))
            addRulesFromSheet(importRule->styleSheet(), medium, addRuleFlags);
    }

    addChildRules(sheet->childRules(), medium, addRuleFlags);
}

void RuleSet::addStyleRule(StyleRule* rule, AddRuleFlags addRuleFlags)
{
    for (size_t selectorIndex = 0; selectorIndex != kNotFound; selectorIndex = rule->selectorList().indexOfNextSelectorAfter(selectorIndex))
        addRule(rule, selectorIndex, addRuleFlags);
}

void RuleSet::compactPendingRules(PendingRuleMap& pendingMap, CompactRuleMap& compactMap)
{
    PendingRuleMap::iterator end = pendingMap.end();
    for (PendingRuleMap::iterator it = pendingMap.begin(); it != end; ++it) {
        OwnPtr<LinkedStack<RuleData> > pendingRules = it->value.release();
        CompactRuleMap::iterator compactRules = compactMap.add(it->key, nullptr).iterator;

        TerminatedArrayBuilder<RuleData> builder(compactRules->value.release());
        builder.grow(pendingRules->size());
        while (!pendingRules->isEmpty()) {
            builder.append(pendingRules->peek());
            pendingRules->pop();
        }

        compactRules->value = builder.release();
    }
}

void RuleSet::compactRules()
{
    ASSERT(m_pendingRules);
    OwnPtr<PendingRuleMaps> pendingRules = m_pendingRules.release();
    compactPendingRules(pendingRules->idRules, m_idRules);
    compactPendingRules(pendingRules->classRules, m_classRules);
    compactPendingRules(pendingRules->tagRules, m_tagRules);
    compactPendingRules(pendingRules->shadowPseudoElementRules, m_shadowPseudoElementRules);
    m_linkPseudoClassRules.shrinkToFit();
    m_cuePseudoRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();
    m_pageRules.shrinkToFit();
    m_viewportRules.shrinkToFit();
    m_fontFaceRules.shrinkToFit();
    m_keyframesRules.shrinkToFit();
    m_treeBoundaryCrossingRules.shrinkToFit();
    m_shadowDistributedRules.shrinkToFit();
}

} // namespace WebCore
