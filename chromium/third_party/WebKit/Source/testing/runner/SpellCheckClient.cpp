/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#include "SpellCheckClient.h"

#include "MockGrammarCheck.h"
#include "public/testing/WebTestDelegate.h"
#include "public/testing/WebTestProxy.h"
#include "public/web/WebTextCheckingCompletion.h"
#include "public/web/WebTextCheckingResult.h"

using namespace blink;
using namespace std;

namespace WebTestRunner {

namespace {

class HostMethodTask : public WebMethodTask<SpellCheckClient> {
public:
    typedef void (SpellCheckClient::*CallbackMethodType)();
    HostMethodTask(SpellCheckClient* object, CallbackMethodType callback)
        : WebMethodTask<SpellCheckClient>(object)
        , m_callback(callback)
    { }

    virtual void runIfValid() { (m_object->*m_callback)(); }

private:
    CallbackMethodType m_callback;
};

}

SpellCheckClient::SpellCheckClient(WebTestProxyBase* webTestProxy)
    : m_lastRequestedTextCheckingCompletion(0)
    , m_webTestProxy(webTestProxy)
{
}

SpellCheckClient::~SpellCheckClient()
{
}

void SpellCheckClient::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
}

// blink::WebSpellCheckClient
void SpellCheckClient::spellCheck(const WebString& text, int& misspelledOffset, int& misspelledLength, WebVector<WebString>* optionalSuggestions)
{
    // Check the spelling of the given text.
    m_spellcheck.spellCheckWord(text, &misspelledOffset, &misspelledLength);
}

void SpellCheckClient::checkTextOfParagraph(const WebString& text, WebTextCheckingTypeMask mask, WebVector<WebTextCheckingResult>* webResults)
{
    vector<WebTextCheckingResult> results;
    if (mask & WebTextCheckingTypeSpelling) {
        size_t offset = 0;
        string16 data = text;
        while (offset < data.length()) {
            int misspelledPosition = 0;
            int misspelledLength = 0;
            m_spellcheck.spellCheckWord(data.substr(offset), &misspelledPosition, &misspelledLength);
            if (!misspelledLength)
                break;
            WebTextCheckingResult result;
            result.decoration = WebTextDecorationTypeSpelling;
            result.location = offset + misspelledPosition;
            result.length = misspelledLength;
            results.push_back(result);
            offset += misspelledPosition + misspelledLength;
        }
    }
    if (mask & WebTextCheckingTypeGrammar)
        MockGrammarCheck::checkGrammarOfString(text, &results);
    webResults->assign(results);
}

void SpellCheckClient::requestCheckingOfText(
        const WebString& text,
        const WebVector<uint32_t>& markers,
        const WebVector<unsigned>& markerOffsets,
        WebTextCheckingCompletion* completion)
{
    if (text.isEmpty()) {
        if (completion)
            completion->didCancelCheckingText();
        return;
    }

    if (m_lastRequestedTextCheckingCompletion)
        m_lastRequestedTextCheckingCompletion->didCancelCheckingText();

    m_lastRequestedTextCheckingCompletion = completion;
    m_lastRequestedTextCheckString = text;
    if (m_spellcheck.hasInCache(text))
        finishLastTextCheck();
    else
        m_delegate->postDelayedTask(new HostMethodTask(this, &SpellCheckClient::finishLastTextCheck), 0);
}

void SpellCheckClient::finishLastTextCheck()
{
    if (!m_lastRequestedTextCheckingCompletion)
        return;
    vector<WebTextCheckingResult> results;
    int offset = 0;
    string16 text = m_lastRequestedTextCheckString;
    if (!m_spellcheck.isMultiWordMisspelling(WebString(text), &results)) {
        while (text.length()) {
            int misspelledPosition = 0;
            int misspelledLength = 0;
            m_spellcheck.spellCheckWord(WebString(text), &misspelledPosition, &misspelledLength);
            if (!misspelledLength)
                break;
            WebVector<WebString> suggestions;
            m_spellcheck.fillSuggestionList(WebString(text.substr(misspelledPosition, misspelledLength)), &suggestions);
            results.push_back(WebTextCheckingResult(WebTextDecorationTypeSpelling, offset + misspelledPosition, misspelledLength, suggestions.isEmpty() ? WebString() : suggestions[0]));
            text = text.substr(misspelledPosition + misspelledLength);
            offset += misspelledPosition + misspelledLength;
        }
        MockGrammarCheck::checkGrammarOfString(m_lastRequestedTextCheckString, &results);
    }
    m_lastRequestedTextCheckingCompletion->didFinishCheckingText(results);
    m_lastRequestedTextCheckingCompletion = 0;

    m_webTestProxy->postSpellCheckEvent(WebString("finishLastTextCheck"));
}

WebString SpellCheckClient::autoCorrectWord(const WebString&)
{
    // Returns an empty string as Mac WebKit ('WebKitSupport/WebEditorClient.mm')
    // does. (If this function returns a non-empty string, WebKit replaces the
    // given misspelled string with the result one. This process executes some
    // editor commands and causes layout-test failures.)
    return WebString();
}

}
