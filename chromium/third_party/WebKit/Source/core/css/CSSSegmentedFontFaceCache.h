/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSSegmentedFontFaceCache_h
#define CSSSegmentedFontFaceCache_h

#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

class CSSFontFace;
class CSSFontSelector;
class CSSSegmentedFontFace;
class FontDescription;
class StyleRuleFontFace;

class CSSSegmentedFontFaceCache {
public:
    CSSSegmentedFontFaceCache();

    // FIXME: Remove CSSFontSelector as argument. Passing CSSFontSelector here is
    // a result of egregious spaghettification in CSSFontFace/FontFaceSet.
    void add(CSSFontSelector*, const StyleRuleFontFace*, PassRefPtr<CSSFontFace>);
    void remove(const StyleRuleFontFace*);
    // FIXME: It's sort of weird that add/remove uses StyleRuleFontFace* as key,
    // but this function uses FontDescription/family pair.
    CSSSegmentedFontFace* get(const FontDescription&, const AtomicString& family);

    unsigned version() const { return m_version; }

private:
    typedef HashMap<unsigned, RefPtr<CSSSegmentedFontFace> > TraitsMap;
    typedef HashMap<String, OwnPtr<TraitsMap>, CaseFoldingHash> FamilyToTraitsMap;
    typedef HashMap<const StyleRuleFontFace*, RefPtr<CSSFontFace> > StyleRuleToFontFace;
    FamilyToTraitsMap m_fontFaces;
    FamilyToTraitsMap m_fonts;
    StyleRuleToFontFace m_styleRuleToFontFace;

    // FIXME: See if this could be ditched
    // Used to compare Font instances, and the usage seems suspect.
    unsigned m_version;
};

}

#endif
