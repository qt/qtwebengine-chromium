/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScopedStyleResolver_h
#define ScopedStyleResolver_h

#include "core/css/ElementRuleCollector.h"
#include "core/css/RuleSet.h"
#include "core/dom/ContainerNode.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class MediaQueryEvaluator;
class PageRuleCollector;
class ShadowRoot;
class StyleSheetContents;

// This class selects a RenderStyle for a given element based on a collection of stylesheets.
class ScopedStyleResolver {
    WTF_MAKE_NONCOPYABLE(ScopedStyleResolver); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ScopedStyleResolver> create(ContainerNode& scopingNode) { return adoptPtr(new ScopedStyleResolver(scopingNode)); }

    static ContainerNode* scopingNodeFor(Document&, const CSSStyleSheet*);

    const ContainerNode& scopingNode() const { return m_scopingNode; }
    const TreeScope& treeScope() const { return m_scopingNode.treeScope(); }
    void setParent(ScopedStyleResolver* newParent) { m_parent = newParent; }
    ScopedStyleResolver* parent() { return m_parent; }

public:
    bool checkRegionStyle(Element*);
    const StyleRuleKeyframes* keyframeStylesForAnimation(const StringImpl* animationName);
    void addKeyframeStyle(PassRefPtr<StyleRuleKeyframes>);

    void collectMatchingAuthorRules(ElementRuleCollector&, bool includeEmptyRules, bool applyAuthorStyles, CascadeScope, CascadeOrder = ignoreCascadeOrder);
    void matchPageRules(PageRuleCollector&);
    void addRulesFromSheet(StyleSheetContents*, const MediaQueryEvaluator&, StyleResolver*);
    void collectFeaturesTo(RuleFeatureSet&);
    void resetAuthorStyle();
    void collectViewportRulesTo(StyleResolver*) const;

private:
    explicit ScopedStyleResolver(ContainerNode& scopingNode) : m_scopingNode(scopingNode), m_parent(0) { }

    RuleSet* ensureAuthorStyle();

    ContainerNode& m_scopingNode;
    ScopedStyleResolver* m_parent;

    Vector<StyleSheetContents*> m_authorStyleSheets;

    typedef HashMap<const StringImpl*, RefPtr<StyleRuleKeyframes> > KeyframesRuleMap;
    KeyframesRuleMap m_keyframesRuleMap;
};

} // namespace WebCore

#endif // ScopedStyleResolver_h
