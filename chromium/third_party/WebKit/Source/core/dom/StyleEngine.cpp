/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "core/dom/StyleEngine.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleInvalidationAnalysis.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ShadowTreeStyleSheetCollection.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLImport.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/frame/Settings.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/URLPatternMatcher.h"

namespace WebCore {

using namespace HTMLNames;

StyleEngine::StyleEngine(Document& document)
    : m_document(document)
    , m_isMaster(HTMLImport::isMaster(&document))
    , m_pendingStylesheets(0)
    , m_injectedStyleSheetCacheValid(false)
    , m_needsUpdateActiveStylesheetsOnStyleRecalc(false)
    , m_documentStyleSheetCollection(document)
    , m_documentScopeDirty(true)
    , m_usesSiblingRules(false)
    , m_usesSiblingRulesOverride(false)
    , m_usesFirstLineRules(false)
    , m_usesFirstLetterRules(false)
    , m_usesRemUnits(false)
    , m_maxDirectAdjacentSelectors(0)
    , m_ignorePendingStylesheets(false)
    , m_didCalculateResolver(false)
    , m_lastResolverAccessCount(0)
    , m_resolverThrowawayTimer(this, &StyleEngine::resolverThrowawayTimerFired)
    // We don't need to create CSSFontSelector for imported document or
    // HTMLTemplateElement's document, because those documents have no frame.
    , m_fontSelector(document.frame() ? CSSFontSelector::create(&document) : 0)
{
}

StyleEngine::~StyleEngine()
{
    for (unsigned i = 0; i < m_injectedAuthorStyleSheets.size(); ++i)
        m_injectedAuthorStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_authorStyleSheets.size(); ++i)
        m_authorStyleSheets[i]->clearOwnerNode();

    if (m_fontSelector) {
        m_fontSelector->clearDocument();
        if (m_resolver)
            m_fontSelector->unregisterForInvalidationCallbacks(m_resolver.get());
    }
}

inline Document* StyleEngine::master()
{
    if (isMaster())
        return &m_document;
    HTMLImport* import = m_document.import();
    if (!import) // Document::import() can return null while executing its destructor.
        return 0;
    return import->master();
}

void StyleEngine::insertTreeScopeInDocumentOrder(TreeScopeSet& treeScopes, TreeScope* treeScope)
{
    if (treeScopes.isEmpty()) {
        treeScopes.add(treeScope);
        return;
    }
    if (treeScopes.contains(treeScope))
        return;

    TreeScopeSet::iterator begin = treeScopes.begin();
    TreeScopeSet::iterator end = treeScopes.end();
    TreeScopeSet::iterator it = end;
    TreeScope* followingTreeScope = 0;
    do {
        --it;
        TreeScope* n = *it;
        unsigned short position = n->comparePosition(*treeScope);
        if (position & Node::DOCUMENT_POSITION_FOLLOWING) {
            treeScopes.insertBefore(followingTreeScope, treeScope);
            return;
        }
        followingTreeScope = n;
    } while (it != begin);

    treeScopes.insertBefore(followingTreeScope, treeScope);
}

StyleSheetCollection* StyleEngine::ensureStyleSheetCollectionFor(TreeScope& treeScope)
{
    if (treeScope == m_document)
        return &m_documentStyleSheetCollection;

    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::AddResult result = m_styleSheetCollectionMap.add(&treeScope, nullptr);
    if (result.isNewEntry)
        result.iterator->value = adoptPtr(new ShadowTreeStyleSheetCollection(toShadowRoot(treeScope)));
    return result.iterator->value.get();
}

StyleSheetCollection* StyleEngine::styleSheetCollectionFor(TreeScope& treeScope)
{
    if (treeScope == m_document)
        return &m_documentStyleSheetCollection;

    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::iterator it = m_styleSheetCollectionMap.find(&treeScope);
    if (it == m_styleSheetCollectionMap.end())
        return 0;
    return it->value.get();
}

const Vector<RefPtr<StyleSheet> >& StyleEngine::styleSheetsForStyleSheetList(TreeScope& treeScope)
{
    if (treeScope == m_document)
        return m_documentStyleSheetCollection.styleSheetsForStyleSheetList();

    return ensureStyleSheetCollectionFor(treeScope)->styleSheetsForStyleSheetList();
}

const Vector<RefPtr<CSSStyleSheet> >& StyleEngine::activeAuthorStyleSheets() const
{
    return m_documentStyleSheetCollection.activeAuthorStyleSheets();
}

void StyleEngine::getActiveAuthorStyleSheets(Vector<const Vector<RefPtr<CSSStyleSheet> >*>& activeAuthorStyleSheets) const
{
    activeAuthorStyleSheets.append(&m_documentStyleSheetCollection.activeAuthorStyleSheets());

    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::const_iterator::Values begin = m_styleSheetCollectionMap.values().begin();
    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::const_iterator::Values end = m_styleSheetCollectionMap.values().end();
    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::const_iterator::Values it = begin;
    for (; it != end; ++it) {
        const StyleSheetCollection* collection = it->get();
        activeAuthorStyleSheets.append(&collection->activeAuthorStyleSheets());
    }
}

void StyleEngine::combineCSSFeatureFlags(const RuleFeatureSet& features)
{
    // Delay resetting the flags until after next style recalc since unapplying the style may not work without these set (this is true at least with before/after).
    m_usesSiblingRules = m_usesSiblingRules || features.usesSiblingRules();
    m_usesFirstLineRules = m_usesFirstLineRules || features.usesFirstLineRules();
    m_maxDirectAdjacentSelectors = max(m_maxDirectAdjacentSelectors, features.maxDirectAdjacentSelectors());
}

void StyleEngine::resetCSSFeatureFlags(const RuleFeatureSet& features)
{
    m_usesSiblingRules = features.usesSiblingRules();
    m_usesFirstLineRules = features.usesFirstLineRules();
    m_maxDirectAdjacentSelectors = features.maxDirectAdjacentSelectors();
}

const Vector<RefPtr<CSSStyleSheet> >& StyleEngine::injectedAuthorStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedAuthorStyleSheets;
}

void StyleEngine::updateInjectedStyleSheetCache() const
{
    if (m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = true;
    m_injectedAuthorStyleSheets.clear();

    Page* owningPage = m_document.page();
    if (!owningPage)
        return;

    const PageGroup& pageGroup = owningPage->group();
    const InjectedStyleSheetVector& sheets = pageGroup.injectedStyleSheets();
    for (unsigned i = 0; i < sheets.size(); ++i) {
        const InjectedStyleSheet* sheet = sheets[i].get();
        if (sheet->injectedFrames() == InjectStyleInTopFrameOnly && m_document.ownerElement())
            continue;
        if (!URLPatternMatcher::matchesPatterns(m_document.url(), sheet->whitelist()))
            continue;
        RefPtr<CSSStyleSheet> groupSheet = CSSStyleSheet::createInline(const_cast<Document*>(&m_document), KURL());
        m_injectedAuthorStyleSheets.append(groupSheet);
        groupSheet->contents()->parseString(sheet->source());
    }
}

void StyleEngine::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    markDocumentDirty();
    // FIXME: updateInjectedStyleSheetCache is called inside StyleSheetCollection::updateActiveStyleSheets
    // and batch updates lots of sheets so we can't call addedStyleSheet() or removedStyleSheet().
    m_document.styleResolverChanged(RecalcStyleDeferred);
}

void StyleEngine::addAuthorSheet(PassRefPtr<StyleSheetContents> authorSheet)
{
    m_authorStyleSheets.append(CSSStyleSheet::create(authorSheet, &m_document));
    m_document.addedStyleSheet(m_authorStyleSheets.last().get(), RecalcStyleImmediately);
    markDocumentDirty();
}

void StyleEngine::addPendingSheet()
{
    master()->styleEngine()->notifyPendingStyleSheetAdded();
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::removePendingSheet(Node* styleSheetCandidateNode, RemovePendingSheetNotificationType notification)
{
    TreeScope* treeScope = isHTMLStyleElement(styleSheetCandidateNode) ? &styleSheetCandidateNode->treeScope() : &m_document;
    markTreeScopeDirty(*treeScope);
    master()->styleEngine()->notifyPendingStyleSheetRemoved(notification);
}

void StyleEngine::notifyPendingStyleSheetAdded()
{
    ASSERT(isMaster());
    m_pendingStylesheets++;
}

void StyleEngine::notifyPendingStyleSheetRemoved(RemovePendingSheetNotificationType notification)
{
    ASSERT(isMaster());
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStylesheets > 0);

    m_pendingStylesheets--;
    if (m_pendingStylesheets)
        return;

    if (notification == RemovePendingSheetNotifyLater) {
        m_document.setNeedsNotifyRemoveAllPendingStylesheet();
        return;
    }

    // FIXME: We can't call addedStyleSheet or removedStyleSheet here because we don't know
    // what's new. We should track that to tell the style system what changed.
    m_document.didRemoveAllPendingStylesheet();
}

void StyleEngine::modifiedStyleSheet(StyleSheet* sheet)
{
    if (!sheet)
        return;

    Node* node = sheet->ownerNode();
    if (!node || !node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);

    markTreeScopeDirty(treeScope);
}

void StyleEngine::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    if (!node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);

    StyleSheetCollection* collection = ensureStyleSheetCollectionFor(treeScope);
    ASSERT(collection);
    collection->addStyleSheetCandidateNode(node, createdByParser);

    markTreeScopeDirty(treeScope);
    if (treeScope != m_document)
        insertTreeScopeInDocumentOrder(m_activeTreeScopes, &treeScope);
}

void StyleEngine::removeStyleSheetCandidateNode(Node* node, ContainerNode* scopingNode)
{
    TreeScope& treeScope = scopingNode ? scopingNode->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);

    StyleSheetCollection* collection = styleSheetCollectionFor(treeScope);
    ASSERT(collection);
    collection->removeStyleSheetCandidateNode(node, scopingNode);

    markTreeScopeDirty(treeScope);
    m_activeTreeScopes.remove(&treeScope);
}

void StyleEngine::modifiedStyleSheetCandidateNode(Node* node)
{
    if (!node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);
    markTreeScopeDirty(treeScope);
}

bool StyleEngine::shouldUpdateShadowTreeStyleSheetCollection(StyleResolverUpdateMode updateMode)
{
    return !m_dirtyTreeScopes.isEmpty() || updateMode == FullStyleUpdate;
}

void StyleEngine::clearMediaQueryRuleSetOnTreeScopeStyleSheets(TreeScopeSet treeScopes)
{
    for (TreeScopeSet::iterator it = treeScopes.begin(); it != treeScopes.end(); ++it) {
        TreeScope& treeScope = **it;
        ASSERT(treeScope != m_document);
        ShadowTreeStyleSheetCollection* collection = static_cast<ShadowTreeStyleSheetCollection*>(styleSheetCollectionFor(treeScope));
        ASSERT(collection);
        collection->clearMediaQueryRuleSetStyleSheets();
    }
}

void StyleEngine::clearMediaQueryRuleSetStyleSheets()
{
    m_documentStyleSheetCollection.clearMediaQueryRuleSetStyleSheets();
    clearMediaQueryRuleSetOnTreeScopeStyleSheets(m_activeTreeScopes);
    clearMediaQueryRuleSetOnTreeScopeStyleSheets(m_dirtyTreeScopes);
}

void StyleEngine::collectDocumentActiveStyleSheets(StyleSheetCollectionBase& collection)
{
    ASSERT(isMaster());

    if (HTMLImport* rootImport = m_document.import()) {
        for (HTMLImport* import = traverseFirstPostOrder(rootImport); import; import = traverseNextPostOrder(import)) {
            Document* document = import->document();
            if (!document)
                continue;
            StyleEngine* engine = document->styleEngine();
            DocumentStyleSheetCollection::CollectFor collectFor = document == &m_document ?
                DocumentStyleSheetCollection::CollectForList : DocumentStyleSheetCollection::DontCollectForList;
            engine->m_documentStyleSheetCollection.collectStyleSheets(engine, collection, collectFor);
        }
    } else {
        m_documentStyleSheetCollection.collectStyleSheets(this, collection, DocumentStyleSheetCollection::CollectForList);
    }
}

bool StyleEngine::updateActiveStyleSheets(StyleResolverUpdateMode updateMode)
{
    ASSERT(isMaster());

    if (m_document.inStyleRecalc()) {
        // SVG <use> element may manage to invalidate style selector in the middle of a style recalc.
        // https://bugs.webkit.org/show_bug.cgi?id=54344
        // FIXME: This should be fixed in SVG and the call site replaced by ASSERT(!m_inStyleRecalc).
        m_needsUpdateActiveStylesheetsOnStyleRecalc = true;
        return false;

    }
    if (!m_document.isActive())
        return false;

    bool requiresFullStyleRecalc = false;
    if (m_documentScopeDirty || updateMode == FullStyleUpdate)
        requiresFullStyleRecalc = m_documentStyleSheetCollection.updateActiveStyleSheets(this, updateMode);

    if (shouldUpdateShadowTreeStyleSheetCollection(updateMode)) {
        TreeScopeSet treeScopes = updateMode == FullStyleUpdate ? m_activeTreeScopes : m_dirtyTreeScopes;
        HashSet<TreeScope*> treeScopesRemoved;

        for (TreeScopeSet::iterator it = treeScopes.begin(); it != treeScopes.end(); ++it) {
            TreeScope* treeScope = *it;
            ASSERT(treeScope != m_document);
            ShadowTreeStyleSheetCollection* collection = static_cast<ShadowTreeStyleSheetCollection*>(styleSheetCollectionFor(*treeScope));
            ASSERT(collection);
            collection->updateActiveStyleSheets(this, updateMode);
            if (!collection->hasStyleSheetCandidateNodes())
                treeScopesRemoved.add(treeScope);
        }
        if (!treeScopesRemoved.isEmpty())
            for (HashSet<TreeScope*>::iterator it = treeScopesRemoved.begin(); it != treeScopesRemoved.end(); ++it)
                m_activeTreeScopes.remove(*it);
    }
    m_needsUpdateActiveStylesheetsOnStyleRecalc = false;
    activeStyleSheetsUpdatedForInspector();
    m_usesRemUnits = m_documentStyleSheetCollection.usesRemUnits();

    if (m_documentScopeDirty || updateMode == FullStyleUpdate)
        m_document.notifySeamlessChildDocumentsOfStylesheetUpdate();

    m_dirtyTreeScopes.clear();
    m_documentScopeDirty = false;

    return requiresFullStyleRecalc;
}

void StyleEngine::activeStyleSheetsUpdatedForInspector()
{
    if (m_activeTreeScopes.isEmpty()) {
        InspectorInstrumentation::activeStyleSheetsUpdated(&m_document, m_documentStyleSheetCollection.styleSheetsForStyleSheetList());
        return;
    }
    Vector<RefPtr<StyleSheet> > activeStyleSheets;

    activeStyleSheets.append(m_documentStyleSheetCollection.styleSheetsForStyleSheetList());

    TreeScopeSet::iterator begin = m_activeTreeScopes.begin();
    TreeScopeSet::iterator end = m_activeTreeScopes.end();
    for (TreeScopeSet::iterator it = begin; it != end; ++it) {
        if (StyleSheetCollection* collection = m_styleSheetCollectionMap.get(*it))
            activeStyleSheets.append(collection->styleSheetsForStyleSheetList());
    }

    // FIXME: Inspector needs a vector which has all active stylesheets.
    // However, creating such a large vector might cause performance regression.
    // Need to implement some smarter solution.
    InspectorInstrumentation::activeStyleSheetsUpdated(&m_document, activeStyleSheets);
}

void StyleEngine::didRemoveShadowRoot(ShadowRoot* shadowRoot)
{
    m_styleSheetCollectionMap.remove(shadowRoot);
}

void StyleEngine::appendActiveAuthorStyleSheets()
{
    ASSERT(isMaster());

    m_resolver->setBuildScopedStyleTreeInDocumentOrder(true);
    m_resolver->appendAuthorStyleSheets(0, m_documentStyleSheetCollection.activeAuthorStyleSheets());

    TreeScopeSet::iterator begin = m_activeTreeScopes.begin();
    TreeScopeSet::iterator end = m_activeTreeScopes.end();
    for (TreeScopeSet::iterator it = begin; it != end; ++it) {
        if (StyleSheetCollection* collection = m_styleSheetCollectionMap.get(*it)) {
            m_resolver->setBuildScopedStyleTreeInDocumentOrder(!collection->scopingNodesForStyleScoped());
            m_resolver->appendAuthorStyleSheets(0, collection->activeAuthorStyleSheets());
        }
    }
    m_resolver->finishAppendAuthorStyleSheets();
    m_resolver->setBuildScopedStyleTreeInDocumentOrder(false);
}

void StyleEngine::createResolver()
{
    // It is a programming error to attempt to resolve style on a Document
    // which is not in a frame. Code which hits this should have checked
    // Document::isActive() before calling into code which could get here.

    ASSERT(m_document.frame());
    ASSERT(m_fontSelector);

    m_resolver = adoptPtr(new StyleResolver(m_document));
    appendActiveAuthorStyleSheets();
    m_fontSelector->registerForInvalidationCallbacks(m_resolver.get());
    combineCSSFeatureFlags(m_resolver->ensureRuleFeatureSet());
}

void StyleEngine::clearResolver()
{
    ASSERT(!m_document.inStyleRecalc());
    ASSERT(isMaster() || !m_resolver);
    ASSERT(m_fontSelector || !m_resolver);
    if (m_resolver)
        m_fontSelector->unregisterForInvalidationCallbacks(m_resolver.get());
    m_resolver.clear();
}

void StyleEngine::clearMasterResolver()
{
    if (Document* master = this->master())
        master->styleEngine()->clearResolver();
}

unsigned StyleEngine::resolverAccessCount() const
{
    return m_resolver ? m_resolver->accessCount() : 0;
}

void StyleEngine::resolverThrowawayTimerFired(Timer<StyleEngine>*)
{
    if (resolverAccessCount() == m_lastResolverAccessCount)
        clearResolver();
    m_lastResolverAccessCount = resolverAccessCount();
}

void StyleEngine::didAttach()
{
    m_resolverThrowawayTimer.startRepeating(60);
}

void StyleEngine::didDetach()
{
    m_resolverThrowawayTimer.stop();
    clearResolver();
}

bool StyleEngine::shouldClearResolver() const
{
    return !m_didCalculateResolver && !haveStylesheetsLoaded();
}

StyleResolverChange StyleEngine::resolverChanged(RecalcStyleTime time, StyleResolverUpdateMode mode)
{
    StyleResolverChange change;

    if (!isMaster()) {
        if (Document* master = this->master())
            master->styleResolverChanged(time, mode);
        return change;
    }

    // Don't bother updating, since we haven't loaded all our style info yet
    // and haven't calculated the style selector for the first time.
    if (!m_document.isActive() || shouldClearResolver()) {
        clearResolver();
        return change;
    }

    m_didCalculateResolver = true;
    if (m_document.didLayoutWithPendingStylesheets() && !hasPendingSheets())
        change.setNeedsRepaint();

    if (updateActiveStyleSheets(mode))
        change.setNeedsStyleRecalc();

    return change;
}

void StyleEngine::resetFontSelector()
{
    if (!m_fontSelector)
        return;

    m_fontSelector->clearDocument();
    if (m_resolver) {
        m_fontSelector->unregisterForInvalidationCallbacks(m_resolver.get());
        m_resolver->invalidateMatchedPropertiesCache();
    }

    // If the document has been already detached, we don't need to recreate
    // CSSFontSelector.
    if (m_document.isActive()) {
        m_fontSelector = CSSFontSelector::create(&m_document);
        if (m_resolver)
            m_fontSelector->registerForInvalidationCallbacks(m_resolver.get());
    } else {
        m_fontSelector = 0;
    }
}

void StyleEngine::markTreeScopeDirty(TreeScope& scope)
{
    if (scope == m_document) {
        markDocumentDirty();
        return;
    }

    m_dirtyTreeScopes.add(&scope);
}

void StyleEngine::markDocumentDirty()
{
    m_documentScopeDirty = true;
    if (!HTMLImport::isMaster(&m_document))
        m_document.import()->master()->styleEngine()->markDocumentDirty();
}

}
