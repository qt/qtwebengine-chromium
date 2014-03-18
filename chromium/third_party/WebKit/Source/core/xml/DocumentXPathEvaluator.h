/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef DocumentXPathEvaluator_h
#define DocumentXPathEvaluator_h

#include "core/dom/DocumentSupplementable.h"
#include "core/xml/XPathEvaluator.h"
#include "core/xml/XPathNSResolver.h"

namespace WebCore {

class ExceptionState;
class XPathExpression;
class XPathResult;

class DocumentXPathEvaluator : public DocumentSupplement {
public:
    virtual ~DocumentXPathEvaluator();

    static DocumentXPathEvaluator* from(DocumentSupplementable*);

    static PassRefPtr<XPathExpression> createExpression(DocumentSupplementable*,
        const String& expression, PassRefPtr<XPathNSResolver>, ExceptionState&);
    static PassRefPtr<XPathNSResolver> createNSResolver(DocumentSupplementable*, Node* nodeResolver);
    static PassRefPtr<XPathResult> evaluate(DocumentSupplementable*,
        const String& expression, Node* contextNode, PassRefPtr<XPathNSResolver>,
        unsigned short type, XPathResult*, ExceptionState&);

private:
    DocumentXPathEvaluator();

    static const char* supplementName() { return "DocumentXPathEvaluator"; }

    RefPtr<XPathEvaluator> m_xpathEvaluator;
};

} // namespace WebCore

#endif // DocumentXPathEvaluator_h
