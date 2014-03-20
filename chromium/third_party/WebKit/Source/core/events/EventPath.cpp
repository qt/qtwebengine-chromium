/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/events/EventPath.h"

#include "EventNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/shadow/HTMLShadowElement.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGUseElement.h"

namespace WebCore {

Node* EventPath::parent(Node* node)
{
    EventPath eventPath(node);
    return eventPath.size() > 1 ? eventPath[1].node() : 0;
}

EventTarget* EventPath::eventTargetRespectingTargetRules(Node* referenceNode)
{
    ASSERT(referenceNode);

    if (referenceNode->isPseudoElement())
        return referenceNode->parentNode();

    if (!referenceNode->isSVGElement() || !referenceNode->isInShadowTree())
        return referenceNode;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects.
    Node* rootNode = referenceNode->treeScope().rootNode();
    Element* shadowHostElement = rootNode->isShadowRoot() ? toShadowRoot(rootNode)->host() : 0;
    // At this time, SVG nodes are not supported in non-<use> shadow trees.
    if (!shadowHostElement || !shadowHostElement->hasTagName(SVGNames::useTag))
        return referenceNode;
    SVGUseElement* useElement = toSVGUseElement(shadowHostElement);
    if (SVGElementInstance* instance = useElement->instanceForShadowTreeElement(referenceNode))
        return instance;

    return referenceNode;
}

static inline bool inTheSameScope(ShadowRoot* shadowRoot, EventTarget* target)
{
    return target->toNode() && target->toNode()->treeScope().rootNode() == shadowRoot;
}

static inline EventDispatchBehavior determineDispatchBehavior(Event* event, ShadowRoot* shadowRoot, EventTarget* target)
{
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (Element* element = FullscreenElementStack::currentFullScreenElementFrom(&target->toNode()->document())) {
        // FIXME: We assume that if the full screen element is a media element that it's
        // the video-only full screen. Both here and elsewhere. But that is probably wrong.
        if (element->isMediaElement() && shadowRoot && shadowRoot->host() == element)
            return StayInsideShadowDOM;
    }

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    const AtomicString eventType = event->type();
    if (inTheSameScope(shadowRoot, target)
        && (eventType == EventTypeNames::abort
            || eventType == EventTypeNames::change
            || eventType == EventTypeNames::error
            || eventType == EventTypeNames::load
            || eventType == EventTypeNames::reset
            || eventType == EventTypeNames::resize
            || eventType == EventTypeNames::scroll
            || eventType == EventTypeNames::select
            || eventType == EventTypeNames::selectstart))
        return StayInsideShadowDOM;

    return RetargetEvent;
}

EventPath::EventPath(Event* event)
    : m_node(0)
    , m_event(event)
{
}

EventPath::EventPath(Node* node)
    : m_node(node)
    , m_event(0)
{
    resetWith(node);
}

void EventPath::resetWith(Node* node)
{
    ASSERT(node);
    m_node = node;
    m_eventContexts.clear();
    calculatePath();
    calculateAdjustedTargets();
    calculateAdjustedEventPathForEachNode();
}

void EventPath::addEventContext(Node* node)
{
    m_eventContexts.append(EventContext(node, eventTargetRespectingTargetRules(node)));
}

void EventPath::calculatePath()
{
    ASSERT(m_node);
    ASSERT(m_eventContexts.isEmpty());
    m_node->document().updateDistributionForNodeIfNeeded(const_cast<Node*>(m_node));

    Node* current = m_node;
    addEventContext(current);
    if (!m_node->inDocument())
        return;
    while (current) {
        if (current->isShadowRoot() && m_event && determineDispatchBehavior(m_event, toShadowRoot(current), m_node) == StayInsideShadowDOM)
            break;
        Vector<InsertionPoint*, 8> insertionPoints;
        collectDestinationInsertionPoints(*current, insertionPoints);
        if (!insertionPoints.isEmpty()) {
            for (size_t i = 0; i < insertionPoints.size(); ++i) {
                InsertionPoint* insertionPoint = insertionPoints[i];
                if (insertionPoint->isShadowInsertionPoint()) {
                    ShadowRoot* containingShadowRoot = insertionPoint->containingShadowRoot();
                    ASSERT(containingShadowRoot);
                    if (!containingShadowRoot->isOldest())
                        addEventContext(containingShadowRoot->olderShadowRoot());
                }
                addEventContext(insertionPoint);
            }
            current = insertionPoints.last();
            continue;
        }
        if (current->isShadowRoot()) {
            current = current->shadowHost();
            addEventContext(current);
        } else {
            current = current->parentNode();
            if (current)
                addEventContext(current);
        }
    }
}

void EventPath::calculateAdjustedEventPathForEachNode()
{
    if (!RuntimeEnabledFeatures::shadowDOMEnabled())
        return;
    TreeScope* lastScope = 0;
    for (size_t i = 0; i < size(); ++i) {
        TreeScope* currentScope = &at(i).node()->treeScope();
        if (currentScope == lastScope) {
            // Fast path.
            at(i).setEventPath(at(i - 1).eventPath());
            continue;
        }
        lastScope = currentScope;
        Vector<RefPtr<Node> > nodes;
        nodes.reserveInitialCapacity(size());
        for (size_t j = 0; j < size(); ++j) {
            if (at(j).node()->treeScope().isInclusiveAncestorOf(*currentScope))
                nodes.append(at(j).node());
        }
        at(i).adoptEventPath(nodes);
    }
}

#ifndef NDEBUG
static inline bool movedFromOlderToYounger(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    Node* rootNode = lastTreeScope.rootNode();
    return rootNode->isShadowRoot() && toShadowRoot(rootNode)->youngerShadowRoot() == currentTreeScope.rootNode();
}

static inline bool movedFromYoungerToOlder(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    Node* rootNode = lastTreeScope.rootNode();
    return rootNode->isShadowRoot() && toShadowRoot(rootNode)->olderShadowRoot() == currentTreeScope.rootNode();
}
#endif

static inline bool movedFromChildToParent(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    return lastTreeScope.parentTreeScope() == &currentTreeScope;
}

static inline bool movedFromParentToChild(const TreeScope& lastTreeScope, const TreeScope& currentTreeScope)
{
    return currentTreeScope.parentTreeScope() == &lastTreeScope;
}

void EventPath::calculateAdjustedTargets()
{
    Vector<Node*, 32> targetStack;
    const TreeScope* lastTreeScope = 0;
    bool isSVGElement = at(0).node()->isSVGElement();

    for (size_t i = 0; i < size(); ++i) {
        Node* current = at(i).node();
        const TreeScope& currentTreeScope = current->treeScope();
        if (targetStack.isEmpty()) {
            targetStack.append(current);
        } else if (*lastTreeScope != currentTreeScope && !isSVGElement) {
            if (movedFromParentToChild(*lastTreeScope, currentTreeScope)) {
                targetStack.append(targetStack.last());
            } else if (movedFromChildToParent(*lastTreeScope, currentTreeScope)) {
                ASSERT(!targetStack.isEmpty());
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(current);
            } else {
                ASSERT(movedFromYoungerToOlder(*lastTreeScope, currentTreeScope) || movedFromOlderToYounger(*lastTreeScope, currentTreeScope));
                ASSERT(!targetStack.isEmpty());
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(current);
                else
                    targetStack.append(targetStack.last());
            }
        }
        at(i).setTarget(eventTargetRespectingTargetRules(targetStack.last()));
        lastTreeScope = &currentTreeScope;
    }
}

} // namespace
