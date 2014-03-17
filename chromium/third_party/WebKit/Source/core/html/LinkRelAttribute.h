/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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
 *
 */

#ifndef LinkRelAttribute_h
#define LinkRelAttribute_h

#include "core/dom/IconURL.h"

namespace WTF {
class String;
}

namespace WebCore {

class LinkRelAttribute {
public:
    LinkRelAttribute();
    explicit LinkRelAttribute(const String&);

    bool isStyleSheet() const { return m_isStyleSheet; }
    IconType iconType() const { return m_iconType; }
    bool isAlternate() const { return m_isAlternate; }
    bool isDNSPrefetch() const { return m_isDNSPrefetch; }
    bool isLinkPrefetch() const { return m_isLinkPrefetch; }
    bool isLinkSubresource() const { return m_isLinkSubresource; }
    bool isLinkPrerender() const { return m_isLinkPrerender; }
    bool isImport() const { return m_isImport; }

private:
    IconType m_iconType;
    bool m_isStyleSheet : 1;
    bool m_isAlternate : 1;
    bool m_isDNSPrefetch : 1;
    bool m_isLinkPrefetch : 1;
    bool m_isLinkSubresource : 1;
    bool m_isLinkPrerender : 1;
    bool m_isImport : 1;
};

}

#endif

