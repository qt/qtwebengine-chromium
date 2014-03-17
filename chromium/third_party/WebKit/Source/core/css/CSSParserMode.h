/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef CSSParserMode_h
#define CSSParserMode_h

#include "platform/weborigin/KURL.h"

namespace WebCore {

class Document;

// Must not grow beyond 3 bytes, due to packing in StylePropertySet.
enum CSSParserMode {
    HTMLStandardMode,
    HTMLQuirksMode,
    // HTML attributes are parsed in quirks mode but also allows internal properties and values.
    HTMLAttributeMode,
    // SVG attributes are parsed in quirks mode but rules differ slightly.
    SVGAttributeMode,
    // @viewport rules are parsed in standards mode but CSSOM modifications (via StylePropertySet)
    // must call parseViewportProperties so needs a special mode.
    CSSViewportRuleMode,
    // User agent stylesheets are parsed in standards mode but also allows internal properties and values.
    UASheetMode
};

inline bool isQuirksModeBehavior(CSSParserMode mode)
{
    return mode == HTMLQuirksMode; // || mode == HTMLAttributeMode;
}

inline bool isUASheetBehavior(CSSParserMode mode)
{
    return mode == UASheetMode;
}

inline bool isInternalPropertyAndValueParsingEnabledForMode(CSSParserMode mode)
{
    return mode == HTMLAttributeMode || mode == UASheetMode;
}

inline bool isUnitLessLengthParsingEnabledForMode(CSSParserMode mode)
{
    return mode == HTMLQuirksMode || mode == HTMLAttributeMode || mode == SVGAttributeMode;
}

inline bool isCSSViewportParsingEnabledForMode(CSSParserMode mode)
{
    return mode == CSSViewportRuleMode;
}

inline bool isSVGNumberParsingEnabledForMode(CSSParserMode mode)
{
    return mode == SVGAttributeMode;
}

inline bool isUseCounterEnabledForMode(CSSParserMode mode)
{
    // We don't count the UA style sheet in our statistics.
    return mode != UASheetMode;
}

class CSSParserContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSParserContext(CSSParserMode);
    CSSParserContext(const Document&, const KURL& baseURL = KURL(), const String& charset = emptyString());

    bool operator==(const CSSParserContext&) const;
    bool operator!=(const CSSParserContext& other) const { return !(*this == other); }

    CSSParserMode mode() const { return m_mode; }
    const KURL& baseURL() const { return m_baseURL; }
    const String& charset() const { return m_charset; }
    bool isHTMLDocument() const { return m_isHTMLDocument; }

    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/277157.
    bool useLegacyBackgroundSizeShorthandBehavior() const { return m_useLegacyBackgroundSizeShorthandBehavior; }

    // FIXME: These setters shouldn't exist, however the current lifetime of CSSParserContext
    // is not well understood and thus we sometimes need to override these fields.
    void setMode(CSSParserMode mode) { m_mode = mode; }
    void setBaseURL(const KURL& baseURL) { m_baseURL = baseURL; }
    void setCharset(const String& charset) { m_charset = charset; }

private:
    KURL m_baseURL;
    String m_charset;
    CSSParserMode m_mode;
    bool m_isHTMLDocument;
    bool m_useLegacyBackgroundSizeShorthandBehavior;
};

const CSSParserContext& strictCSSParserContext();

};

#endif // CSSParserMode_h
