/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/css/CSSSVGDocumentValue.h"

#include "FetchInitiatorTypeNames.h"
#include "core/css/CSSParser.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"

namespace WebCore {

CSSSVGDocumentValue::CSSSVGDocumentValue(const String& url)
    : CSSValue(CSSSVGDocumentClass)
    , m_url(url)
    , m_loadRequested(false)
{
}

CSSSVGDocumentValue::~CSSSVGDocumentValue()
{
}

DocumentResource* CSSSVGDocumentValue::load(ResourceFetcher* loader)
{
    ASSERT(loader);

    if (!m_loadRequested) {
        m_loadRequested = true;

        FetchRequest request(ResourceRequest(loader->document()->completeURL(m_url)), FetchInitiatorTypeNames::css);
        m_document = loader->fetchSVGDocument(request);
    }

    return m_document.get();
}

String CSSSVGDocumentValue::customCSSText() const
{
    return quoteCSSStringIfNeeded(m_url);
}

bool CSSSVGDocumentValue::equals(const CSSSVGDocumentValue& other) const
{
    return m_url == other.m_url;
}

} // namespace WebCore
