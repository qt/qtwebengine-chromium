/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "core/fetch/CrossOriginAccessControl.h"

#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

bool isOnAccessControlSimpleRequestMethodWhitelist(const String& method)
{
    return method == "GET" || method == "HEAD" || method == "POST";
}

bool isOnAccessControlSimpleRequestHeaderWhitelist(const AtomicString& name, const AtomicString& value)
{
    if (equalIgnoringCase(name, "accept")
        || equalIgnoringCase(name, "accept-language")
        || equalIgnoringCase(name, "content-language")
        || equalIgnoringCase(name, "origin")
        || equalIgnoringCase(name, "referer"))
        return true;

    // Preflight is required for MIME types that can not be sent via form submission.
    if (equalIgnoringCase(name, "content-type")) {
        AtomicString mimeType = extractMIMETypeFromMediaType(value);
        return equalIgnoringCase(mimeType, "application/x-www-form-urlencoded")
            || equalIgnoringCase(mimeType, "multipart/form-data")
            || equalIgnoringCase(mimeType, "text/plain");
    }

    return false;
}

bool isSimpleCrossOriginAccessRequest(const String& method, const HTTPHeaderMap& headerMap)
{
    if (!isOnAccessControlSimpleRequestMethodWhitelist(method))
        return false;

    HTTPHeaderMap::const_iterator end = headerMap.end();
    for (HTTPHeaderMap::const_iterator it = headerMap.begin(); it != end; ++it) {
        if (!isOnAccessControlSimpleRequestHeaderWhitelist(it->key, it->value))
            return false;
    }

    return true;
}

static PassOwnPtr<HTTPHeaderSet> createAllowedCrossOriginResponseHeadersSet()
{
    OwnPtr<HTTPHeaderSet> headerSet = adoptPtr(new HashSet<String, CaseFoldingHash>);

    headerSet->add("cache-control");
    headerSet->add("content-language");
    headerSet->add("content-type");
    headerSet->add("expires");
    headerSet->add("last-modified");
    headerSet->add("pragma");

    return headerSet.release();
}

bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    AtomicallyInitializedStatic(HTTPHeaderSet*, allowedCrossOriginResponseHeaders = createAllowedCrossOriginResponseHeadersSet().leakPtr());

    return allowedCrossOriginResponseHeaders->contains(name);
}

void updateRequestForAccessControl(ResourceRequest& request, SecurityOrigin* securityOrigin, StoredCredentials allowCredentials)
{
    request.removeCredentials();
    request.setAllowCookies(allowCredentials == AllowStoredCredentials);

    if (securityOrigin)
        request.setHTTPOrigin(securityOrigin->toString());
}

ResourceRequest createAccessControlPreflightRequest(const ResourceRequest& request, SecurityOrigin* securityOrigin)
{
    ResourceRequest preflightRequest(request.url());
    updateRequestForAccessControl(preflightRequest, securityOrigin, DoNotAllowStoredCredentials);
    preflightRequest.setHTTPMethod("OPTIONS");
    preflightRequest.setHTTPHeaderField("Access-Control-Request-Method", request.httpMethod());
    preflightRequest.setPriority(request.priority());

    const HTTPHeaderMap& requestHeaderFields = request.httpHeaderFields();

    if (requestHeaderFields.size() > 0) {
        StringBuilder headerBuffer;
        HTTPHeaderMap::const_iterator it = requestHeaderFields.begin();
        headerBuffer.append(it->key);
        ++it;

        HTTPHeaderMap::const_iterator end = requestHeaderFields.end();
        for (; it != end; ++it) {
            headerBuffer.appendLiteral(", ");
            headerBuffer.append(it->key);
        }

        preflightRequest.setHTTPHeaderField("Access-Control-Request-Headers", headerBuffer.toString().lower());
    }

    return preflightRequest;
}

static bool isOriginSeparator(UChar ch)
{
    return isASCIISpace(ch) || ch == ',';
}

bool passesAccessControlCheck(const ResourceResponse& response, StoredCredentials includeCredentials, SecurityOrigin* securityOrigin, String& errorDescription)
{
    AtomicallyInitializedStatic(AtomicString&, accessControlAllowOrigin = *new AtomicString("access-control-allow-origin", AtomicString::ConstructFromLiteral));
    AtomicallyInitializedStatic(AtomicString&, accessControlAllowCredentials = *new AtomicString("access-control-allow-credentials", AtomicString::ConstructFromLiteral));

    // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
    // even with Access-Control-Allow-Credentials set to true.
    const AtomicString& accessControlOriginString = response.httpHeaderField(accessControlAllowOrigin);
    if (accessControlOriginString == starAtom && includeCredentials == DoNotAllowStoredCredentials)
        return true;

    if (accessControlOriginString != securityOrigin->toString()) {
        if (accessControlOriginString == starAtom) {
            errorDescription = "A wildcard '*' cannot be used in the 'Access-Control-Allow-Origin' header when the credentials flag is true. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        } else if (accessControlOriginString.isEmpty()) {
            errorDescription = "No 'Access-Control-Allow-Origin' header is present on the requested resource. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        } else if (accessControlOriginString.string().find(isOriginSeparator, 0) != kNotFound) {
            errorDescription = "The 'Access-Control-Allow-Origin' header contains multiple values '" + accessControlOriginString + "', but only one is allowed. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        } else {
            KURL headerOrigin(KURL(), accessControlOriginString);
            if (!headerOrigin.isValid())
                errorDescription = "The 'Access-Control-Allow-Origin' header contains the invalid value '" + accessControlOriginString + "'. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
            else
                errorDescription = "The 'Access-Control-Allow-Origin' header has a value '" + accessControlOriginString + "' that is not equal to the supplied origin. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        }
        return false;
    }

    if (includeCredentials == AllowStoredCredentials) {
        const AtomicString& accessControlCredentialsString = response.httpHeaderField(accessControlAllowCredentials);
        if (accessControlCredentialsString != "true") {
            errorDescription = "Credentials flag is 'true', but the 'Access-Control-Allow-Credentials' header is '" + accessControlCredentialsString + "'. It must be 'true' to allow credentials.";
            return false;
        }
    }

    return true;
}

bool passesPreflightStatusCheck(const ResourceResponse& response, String& errorDescription)
{
    if (response.httpStatusCode() < 200 || response.httpStatusCode() >= 400) {
        errorDescription = "Invalid HTTP status code " + String::number(response.httpStatusCode());
        return false;
    }

    return true;
}

void parseAccessControlExposeHeadersAllowList(const String& headerValue, HTTPHeaderSet& headerSet)
{
    Vector<String> headers;
    headerValue.split(',', false, headers);
    for (unsigned headerCount = 0; headerCount < headers.size(); headerCount++) {
        String strippedHeader = headers[headerCount].stripWhiteSpace();
        if (!strippedHeader.isEmpty())
            headerSet.add(strippedHeader);
    }
}

} // namespace WebCore
