/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#ifndef ContentSecurityPolicy_h
#define ContentSecurityPolicy_h

#include "bindings/v8/ScriptState.h"
#include "platform/network/HTTPParsers.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"

namespace WTF {
class OrdinalNumber;
}

namespace WebCore {

class ContentSecurityPolicyResponseHeaders;
class CSPDirectiveList;
class DOMStringList;
class JSONObject;
class KURL;
class ExecutionContextClient;
class SecurityOrigin;

typedef int SandboxFlags;
typedef Vector<OwnPtr<CSPDirectiveList> > CSPDirectiveListVector;

class ContentSecurityPolicy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ContentSecurityPolicy> create(ExecutionContextClient* client)
    {
        return adoptPtr(new ContentSecurityPolicy(client));
    }
    ~ContentSecurityPolicy();

    void copyStateFrom(const ContentSecurityPolicy*);

    enum HeaderType {
        Report,
        Enforce,
    };

    enum ReportingStatus {
        SendReport,
        SuppressReport
    };

    enum HashAlgorithms {
        HashAlgorithmsNone   = 0,
        HashAlgorithmsSha1   = 1 << 1,
        HashAlgorithmsSha256 = 1 << 2
    };

    void didReceiveHeaders(const ContentSecurityPolicyResponseHeaders&);
    void didReceiveHeader(const String&, HeaderType);

    // These functions are wrong because they assume that there is only one header.
    // FIXME: Replace them with functions that return vectors.
    const String& deprecatedHeader() const;
    HeaderType deprecatedHeaderType() const;

    bool allowJavaScriptURLs(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus = SendReport) const;
    bool allowInlineEventHandlers(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus = SendReport) const;
    bool allowInlineScript(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus = SendReport) const;
    bool allowInlineStyle(const String& contextURL, const WTF::OrdinalNumber& contextLine, ReportingStatus = SendReport) const;
    bool allowEval(ScriptState* = 0, ReportingStatus = SendReport) const;
    bool allowPluginType(const String& type, const String& typeAttribute, const KURL&, ReportingStatus = SendReport) const;

    bool allowScriptFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowObjectFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowChildFrameFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowImageFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowStyleFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowFontFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowMediaFromSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowConnectToSource(const KURL&, ReportingStatus = SendReport) const;
    bool allowFormAction(const KURL&, ReportingStatus = SendReport) const;
    bool allowBaseURI(const KURL&, ReportingStatus = SendReport) const;
    // The nonce and hash allow functions are guaranteed to not have any side
    // effects, including reporting.
    bool allowScriptNonce(const String& nonce) const;
    bool allowStyleNonce(const String& nonce) const;
    bool allowScriptHash(const String& source) const;

    void usesScriptHashAlgorithms(uint8_t HashAlgorithms);

    ReflectedXSSDisposition reflectedXSSDisposition() const;

    void setOverrideAllowInlineStyle(bool);

    bool isActive() const;
    void gatherReportURIs(DOMStringList&) const;

    void reportDirectiveAsSourceExpression(const String& directiveName, const String& sourceExpression) const;
    void reportDuplicateDirective(const String&) const;
    void reportInvalidDirectiveValueCharacter(const String& directiveName, const String& value) const;
    void reportInvalidPathCharacter(const String& directiveName, const String& value, const char) const;
    void reportInvalidPluginTypes(const String&) const;
    void reportInvalidSandboxFlags(const String&) const;
    void reportInvalidSourceExpression(const String& directiveName, const String& source) const;
    void reportInvalidReflectedXSS(const String&) const;
    void reportMissingReportURI(const String&) const;
    void reportUnsupportedDirective(const String&) const;
    void reportViolation(const String& directiveText, const String& effectiveDirective, const String& consoleMessage, const KURL& blockedURL, const Vector<KURL>& reportURIs, const String& header);

    void reportBlockedScriptExecutionToInspector(const String& directiveText) const;

    const KURL url() const;
    KURL completeURL(const String&) const;
    SecurityOrigin* securityOrigin() const;
    void enforceSandboxFlags(SandboxFlags) const;
    String evalDisabledErrorMessage() const;

    bool experimentalFeaturesEnabled() const;

    static bool shouldBypassMainWorld(ExecutionContext*);

    ExecutionContextClient* client() { return m_client; }

private:
    explicit ContentSecurityPolicy(ExecutionContextClient*);

    void logToConsole(const String& message) const;
    void addPolicyFromHeaderValue(const String&, HeaderType);

    bool shouldSendViolationReport(const String&) const;
    void didSendViolationReport(const String&);

    ExecutionContextClient* m_client;
    bool m_overrideInlineStyleAllowed;
    CSPDirectiveListVector m_policies;

    HashSet<unsigned, AlreadyHashed> m_violationReportsSent;

    // We put the hash functions used on the policy object so that we only need
    // to calculate a script hash once and then distribute it to all of the
    // directives for validation.
    uint8_t m_sourceHashAlgorithmsUsed;
};

}

#endif
