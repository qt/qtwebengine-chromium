/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
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
 *
 */

#include "config.h"
#include "core/dom/SandboxFlags.h"

#include "core/html/parser/HTMLParserIdioms.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

SandboxFlags parseSandboxPolicy(const String& policy, String& invalidTokensErrorMessage)
{
    // http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
    // Parse the unordered set of unique space-separated tokens.
    SandboxFlags flags = SandboxAll;
    unsigned length = policy.length();
    unsigned start = 0;
    unsigned numberOfTokenErrors = 0;
    StringBuilder tokenErrors;
    while (true) {
        while (start < length && isHTMLSpace<UChar>(policy[start]))
            ++start;
        if (start >= length)
            break;
        unsigned end = start + 1;
        while (end < length && !isHTMLSpace<UChar>(policy[end]))
            ++end;

        // Turn off the corresponding sandbox flag if it's set as "allowed".
        String sandboxToken = policy.substring(start, end - start);
        if (equalIgnoringCase(sandboxToken, "allow-same-origin")) {
            flags &= ~SandboxOrigin;
        } else if (equalIgnoringCase(sandboxToken, "allow-forms")) {
            flags &= ~SandboxForms;
        } else if (equalIgnoringCase(sandboxToken, "allow-scripts")) {
            flags &= ~SandboxScripts;
            flags &= ~SandboxAutomaticFeatures;
        } else if (equalIgnoringCase(sandboxToken, "allow-top-navigation")) {
            flags &= ~SandboxTopNavigation;
        } else if (equalIgnoringCase(sandboxToken, "allow-popups")) {
            flags &= ~SandboxPopups;
        } else if (equalIgnoringCase(sandboxToken, "allow-pointer-lock")) {
            flags &= ~SandboxPointerLock;
        } else {
            if (numberOfTokenErrors)
                tokenErrors.appendLiteral(", '");
            else
                tokenErrors.append('\'');
            tokenErrors.append(sandboxToken);
            tokenErrors.append('\'');
            numberOfTokenErrors++;
        }

        start = end + 1;
    }

    if (numberOfTokenErrors) {
        if (numberOfTokenErrors > 1)
            tokenErrors.appendLiteral(" are invalid sandbox flags.");
        else
            tokenErrors.appendLiteral(" is an invalid sandbox flag.");
        invalidTokensErrorMessage = tokenErrors.toString();
    }

    return flags;
}

}
