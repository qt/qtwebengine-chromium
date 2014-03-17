/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSStyleSheet_h
#define CSSStyleSheet_h

#include "core/css/CSSRule.h"
#include "core/css/StyleSheet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

class CSSCharsetRule;
class CSSImportRule;
class CSSParser;
class CSSRule;
class CSSRuleList;
class CSSStyleSheet;
class CSSStyleSheetResource;
class Document;
class ExceptionState;
class MediaQuerySet;
class SecurityOrigin;
class StyleSheetContents;

enum StyleSheetUpdateType {
    PartialRuleUpdate,
    EntireStyleSheetUpdate
};

class CSSStyleSheet : public StyleSheet {
public:
    static PassRefPtr<CSSStyleSheet> create(PassRefPtr<StyleSheetContents>, CSSImportRule* ownerRule = 0);
    static PassRefPtr<CSSStyleSheet> create(PassRefPtr<StyleSheetContents>, Node* ownerNode);
    static PassRefPtr<CSSStyleSheet> createInline(Node*, const KURL&, const TextPosition& startPosition = TextPosition::minimumPosition(), const String& encoding = String());

    virtual ~CSSStyleSheet();

    virtual CSSStyleSheet* parentStyleSheet() const OVERRIDE;
    virtual Node* ownerNode() const OVERRIDE { return m_ownerNode; }
    virtual MediaList* media() const OVERRIDE;
    virtual String href() const OVERRIDE;
    virtual String title() const OVERRIDE { return m_title; }
    virtual bool disabled() const OVERRIDE { return m_isDisabled; }
    virtual void setDisabled(bool) OVERRIDE;

    PassRefPtr<CSSRuleList> cssRules();
    unsigned insertRule(const String& rule, unsigned index, ExceptionState&);
    unsigned insertRule(const String& rule, ExceptionState&); // Deprecated.
    void deleteRule(unsigned index, ExceptionState&);

    // IE Extensions
    PassRefPtr<CSSRuleList> rules();
    int addRule(const String& selector, const String& style, int index, ExceptionState&);
    int addRule(const String& selector, const String& style, ExceptionState&);
    void removeRule(unsigned index, ExceptionState& exceptionState) { deleteRule(index, exceptionState); }

    // For CSSRuleList.
    unsigned length() const;
    CSSRule* item(unsigned index);

    virtual void clearOwnerNode() OVERRIDE { didMutate(EntireStyleSheetUpdate); m_ownerNode = 0; }
    virtual CSSRule* ownerRule() const OVERRIDE { return m_ownerRule; }
    virtual KURL baseURL() const OVERRIDE;
    virtual bool isLoading() const OVERRIDE;

    void clearOwnerRule() { m_ownerRule = 0; }
    Document* ownerDocument() const;
    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }
    void setMediaQueries(PassRefPtr<MediaQuerySet>);
    void setTitle(const String& title) { m_title = title; }

    class RuleMutationScope {
        WTF_MAKE_NONCOPYABLE(RuleMutationScope);
    public:
        RuleMutationScope(CSSStyleSheet*);
        RuleMutationScope(CSSRule*);
        ~RuleMutationScope();

    private:
        CSSStyleSheet* m_styleSheet;
    };

    void willMutateRules();
    void didMutateRules();
    void didMutate(StyleSheetUpdateType = PartialRuleUpdate);

    void clearChildRuleCSSOMWrappers();

    void registerExtraChildRuleCSSOMWrapper(PassRefPtr<CSSRule>);

    StyleSheetContents* contents() const { return m_contents.get(); }

    bool isInline() const { return m_isInlineStylesheet; }
    TextPosition startPositionInSource() const { return m_startPosition; }

private:
    CSSStyleSheet(PassRefPtr<StyleSheetContents>, CSSImportRule* ownerRule);
    CSSStyleSheet(PassRefPtr<StyleSheetContents>, Node* ownerNode, bool isInlineStylesheet, const TextPosition& startPosition);

    virtual bool isCSSStyleSheet() const { return true; }
    virtual String type() const { return "text/css"; }

    void extraCSSOMWrapperIndices(Vector<unsigned>& indices);
    void reattachChildRuleCSSOMWrappers(const Vector<unsigned>& extraCSSOMWrapperIndices);

    bool canAccessRules() const;

    RefPtr<StyleSheetContents> m_contents;
    bool m_isInlineStylesheet;
    bool m_isDisabled;
    String m_title;
    RefPtr<MediaQuerySet> m_mediaQueries;

    Node* m_ownerNode;
    CSSRule* m_ownerRule;

    TextPosition m_startPosition;

    mutable RefPtr<MediaList> m_mediaCSSOMWrapper;
    mutable Vector<RefPtr<CSSRule> > m_childRuleCSSOMWrappers;
    // These are CSSOMWrappers that come from getMatchedCSSRules and thus don't map 1-1 to
    // the StyleRules in the StyleSheetContents.
    mutable Vector<RefPtr<CSSRule> > m_extraChildRuleCSSOMWrappers;
    mutable OwnPtr<CSSRuleList> m_ruleListCSSOMWrapper;
};

inline CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSStyleSheet* sheet)
    : m_styleSheet(sheet)
{
    if (m_styleSheet)
        m_styleSheet->willMutateRules();
}

inline CSSStyleSheet::RuleMutationScope::RuleMutationScope(CSSRule* rule)
    : m_styleSheet(rule ? rule->parentStyleSheet() : 0)
{
    if (m_styleSheet)
        m_styleSheet->willMutateRules();
}

inline CSSStyleSheet::RuleMutationScope::~RuleMutationScope()
{
    if (m_styleSheet)
        m_styleSheet->didMutateRules();
}

DEFINE_TYPE_CASTS(CSSStyleSheet, StyleSheet, sheet, sheet->isCSSStyleSheet(), sheet.isCSSStyleSheet());

} // namespace

#endif
