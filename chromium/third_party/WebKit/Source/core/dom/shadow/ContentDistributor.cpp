/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "core/dom/shadow/ContentDistributor.h"

#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/shadow/HTMLContentElement.h"
#include "core/html/shadow/HTMLShadowElement.h"


namespace WebCore {

void ContentDistribution::swap(ContentDistribution& other)
{
    m_nodes.swap(other.m_nodes);
    m_indices.swap(other.m_indices);
}

void ContentDistribution::append(PassRefPtr<Node> node)
{
    size_t size = m_nodes.size();
    m_indices.set(node.get(), size);
    m_nodes.append(node);
}

size_t ContentDistribution::find(const Node* node) const
{
    HashMap<const Node*, size_t>::const_iterator it = m_indices.find(node);
    if (it == m_indices.end())
        return notFound;

    return it.get()->value;
}

Node* ContentDistribution::nextTo(const Node* node) const
{
    size_t index = find(node);
    if (index == notFound || index + 1 == size())
        return 0;
    return at(index + 1).get();
}

Node* ContentDistribution::previousTo(const Node* node) const
{
    size_t index = find(node);
    if (index == notFound || !index)
        return 0;
    return at(index - 1).get();
}


ScopeContentDistribution::ScopeContentDistribution()
    : m_insertionPointAssignedTo(0)
    , m_numberOfShadowElementChildren(0)
    , m_numberOfContentElementChildren(0)
    , m_numberOfElementShadowChildren(0)
    , m_insertionPointListIsValid(false)
{
}

void ScopeContentDistribution::setInsertionPointAssignedTo(PassRefPtr<InsertionPoint> insertionPoint)
{
    m_insertionPointAssignedTo = insertionPoint;
}

void ScopeContentDistribution::invalidateInsertionPointList()
{
    m_insertionPointListIsValid = false;
    m_insertionPointList.clear();
}

const Vector<RefPtr<InsertionPoint> >& ScopeContentDistribution::ensureInsertionPointList(ShadowRoot* shadowRoot)
{
    if (m_insertionPointListIsValid)
        return m_insertionPointList;

    m_insertionPointListIsValid = true;
    ASSERT(m_insertionPointList.isEmpty());

    if (!shadowRoot->containsInsertionPoints())
        return m_insertionPointList;

    for (Element* element = ElementTraversal::firstWithin(shadowRoot); element; element = ElementTraversal::next(element, shadowRoot)) {
        if (element->isInsertionPoint())
            m_insertionPointList.append(toInsertionPoint(element));
    }

    return m_insertionPointList;
}

void ScopeContentDistribution::registerInsertionPoint(InsertionPoint* point)
{
    if (isHTMLShadowElement(point))
        ++m_numberOfShadowElementChildren;
    else if (isHTMLContentElement(point))
        ++m_numberOfContentElementChildren;
    else
        ASSERT_NOT_REACHED();

    invalidateInsertionPointList();
}

void ScopeContentDistribution::unregisterInsertionPoint(InsertionPoint* point)
{
    if (isHTMLShadowElement(point))
        --m_numberOfShadowElementChildren;
    else if (isHTMLContentElement(point))
        --m_numberOfContentElementChildren;
    else
        ASSERT_NOT_REACHED();

    ASSERT(m_numberOfContentElementChildren >= 0);
    ASSERT(m_numberOfShadowElementChildren >= 0);

    invalidateInsertionPointList();
}

ContentDistributor::ContentDistributor()
    : m_needsSelectFeatureSet(false)
{
}

ContentDistributor::~ContentDistributor()
{
}

InsertionPoint* ContentDistributor::findInsertionPointFor(const Node* key) const
{
    return m_nodeToInsertionPoint.get(key);
}

void ContentDistributor::populate(Node* node, Vector<Node*>& pool)
{
    node->lazyReattachIfAttached();

    if (!isActiveInsertionPoint(node)) {
        pool.append(node);
        return;
    }

    InsertionPoint* insertionPoint = toInsertionPoint(node);
    if (insertionPoint->hasDistribution()) {
        for (size_t i = 0; i < insertionPoint->size(); ++i)
            populate(insertionPoint->at(i), pool);
    } else {
        for (Node* fallbackNode = insertionPoint->firstChild(); fallbackNode; fallbackNode = fallbackNode->nextSibling())
            pool.append(fallbackNode);
    }
}

void ContentDistributor::distribute(Element* host)
{
    Vector<Node*> pool;
    for (Node* node = host->firstChild(); node; node = node->nextSibling())
        populate(node, pool);

    host->setNeedsStyleRecalc();

    Vector<bool> distributed(pool.size());
    distributed.fill(false);

    Vector<HTMLShadowElement*, 8> activeShadowInsertionPoints;
    for (ShadowRoot* root = host->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        HTMLShadowElement* firstActiveShadowInsertionPoint = 0;

        if (ScopeContentDistribution* scope = root->scopeDistribution()) {
            const Vector<RefPtr<InsertionPoint> >& insertionPoints = scope->ensureInsertionPointList(root);
            for (size_t i = 0; i < insertionPoints.size(); ++i) {
                InsertionPoint* point = insertionPoints[i].get();
                if (!point->isActive())
                    continue;

                if (isHTMLShadowElement(point)) {
                    if (!firstActiveShadowInsertionPoint)
                        firstActiveShadowInsertionPoint = toHTMLShadowElement(point);
                } else {
                    distributeSelectionsTo(point, pool, distributed);
                    if (ElementShadow* shadow = shadowOfParentForDistribution(point))
                        shadow->setNeedsDistributionRecalc();
                }
            }
        }

        if (firstActiveShadowInsertionPoint)
            activeShadowInsertionPoints.append(firstActiveShadowInsertionPoint);
    }

    for (size_t i = activeShadowInsertionPoints.size(); i > 0; --i) {
        HTMLShadowElement* shadowElement = activeShadowInsertionPoints[i - 1];
        ShadowRoot* root = shadowElement->containingShadowRoot();
        ASSERT(root);
        if (!shadowElement->shouldSelect()) {
            if (root->olderShadowRoot())
                root->olderShadowRoot()->ensureScopeDistribution()->setInsertionPointAssignedTo(shadowElement);
        } else if (root->olderShadowRoot()) {
            distributeNodeChildrenTo(shadowElement, root->olderShadowRoot());
            root->olderShadowRoot()->ensureScopeDistribution()->setInsertionPointAssignedTo(shadowElement);
        } else {
            distributeSelectionsTo(shadowElement, pool, distributed);
        }
        if (ElementShadow* shadow = shadowOfParentForDistribution(shadowElement))
            shadow->setNeedsDistributionRecalc();
    }
}

void ContentDistributor::distributeSelectionsTo(InsertionPoint* insertionPoint, const Vector<Node*>& pool, Vector<bool>& distributed)
{
    ContentDistribution distribution;

    for (size_t i = 0; i < pool.size(); ++i) {
        if (distributed[i])
            continue;

        if (isHTMLContentElement(insertionPoint) && !toHTMLContentElement(insertionPoint)->canSelectNode(pool, i))
            continue;

        Node* child = pool[i];
        distribution.append(child);
        m_nodeToInsertionPoint.add(child, insertionPoint);
        distributed[i] = true;
    }

    insertionPoint->lazyReattachIfAttached();
    insertionPoint->setDistribution(distribution);
}

void ContentDistributor::distributeNodeChildrenTo(InsertionPoint* insertionPoint, ContainerNode* containerNode)
{
    ContentDistribution distribution;
    for (Node* node = containerNode->firstChild(); node; node = node->nextSibling()) {
        node->lazyReattachIfAttached();
        if (isActiveInsertionPoint(node)) {
            InsertionPoint* innerInsertionPoint = toInsertionPoint(node);
            if (innerInsertionPoint->hasDistribution()) {
                for (size_t i = 0; i < innerInsertionPoint->size(); ++i) {
                    distribution.append(innerInsertionPoint->at(i));
                    m_nodeToInsertionPoint.add(innerInsertionPoint->at(i), insertionPoint);
                }
            } else {
                for (Node* child = innerInsertionPoint->firstChild(); child; child = child->nextSibling()) {
                    distribution.append(child);
                    m_nodeToInsertionPoint.add(child, insertionPoint);
                }
            }
        } else {
            distribution.append(node);
            m_nodeToInsertionPoint.add(node, insertionPoint);
        }
    }

    insertionPoint->lazyReattachIfAttached();
    insertionPoint->setDistribution(distribution);
}

const SelectRuleFeatureSet& ContentDistributor::ensureSelectFeatureSet(ElementShadow* shadow)
{
    if (!m_needsSelectFeatureSet)
        return m_selectFeatures;

    m_selectFeatures.clear();
    for (ShadowRoot* root = shadow->oldestShadowRoot(); root; root = root->youngerShadowRoot())
        collectSelectFeatureSetFrom(root);
    m_needsSelectFeatureSet = false;
    return m_selectFeatures;
}

void ContentDistributor::collectSelectFeatureSetFrom(ShadowRoot* root)
{
    if (!root->containsShadowRoots() && !root->containsContentElements())
        return;

    for (Element* element = ElementTraversal::firstWithin(root); element; element = ElementTraversal::next(element, root)) {
        if (ElementShadow* shadow = element->shadow())
            m_selectFeatures.add(shadow->ensureSelectFeatureSet());
        if (!isHTMLContentElement(element))
            continue;
        const CSSSelectorList& list = toHTMLContentElement(element)->selectorList();
        for (const CSSSelector* selector = list.first(); selector; selector = CSSSelectorList::next(selector)) {
            for (const CSSSelector* component = selector; component; component = component->tagHistory())
                m_selectFeatures.collectFeaturesFromSelector(component);
        }
    }
}

void ContentDistributor::didAffectSelector(Element* host, AffectedSelectorMask mask)
{
    if (ensureSelectFeatureSet(host->shadow()).hasSelectorFor(mask))
        host->shadow()->setNeedsDistributionRecalc();
}

void ContentDistributor::willAffectSelector(Element* host)
{
    for (ElementShadow* shadow = host->shadow(); shadow; shadow = shadow->containingShadow()) {
        if (shadow->distributor().needsSelectFeatureSet())
            break;
        shadow->distributor().setNeedsSelectFeatureSet();
    }
    host->shadow()->setNeedsDistributionRecalc();
}

void ContentDistributor::clearDistribution(Element* host)
{
    m_nodeToInsertionPoint.clear();

    for (ShadowRoot* root = host->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (ScopeContentDistribution* scope = root->scopeDistribution())
            scope->setInsertionPointAssignedTo(0);
    }
}

}
