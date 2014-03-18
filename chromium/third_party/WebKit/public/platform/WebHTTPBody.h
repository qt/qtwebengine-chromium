/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebHTTPBody_h
#define WebHTTPBody_h

#include "WebBlobData.h"
#include "WebData.h"
#include "WebNonCopyable.h"
#include "WebString.h"
#include "WebURL.h"

#if INSIDE_BLINK
namespace WebCore { class FormData; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace blink {

class WebHTTPBodyPrivate;

class WebHTTPBody {
public:
    struct Element {
        enum Type { TypeData, TypeFile, TypeBlob, TypeFileSystemURL } type;
        WebData data;
        WebString filePath;
        long long fileStart;
        long long fileLength; // -1 means to the end of the file.
        double modificationTime;
        WebURL fileSystemURL;
        WebString blobUUID;
    };

    ~WebHTTPBody() { reset(); }

    WebHTTPBody() : m_private(0) { }
    WebHTTPBody(const WebHTTPBody& b) : m_private(0) { assign(b); }
    WebHTTPBody& operator=(const WebHTTPBody& b)
    {
        assign(b);
        return *this;
    }

    BLINK_PLATFORM_EXPORT void initialize();
    BLINK_PLATFORM_EXPORT void reset();
    BLINK_PLATFORM_EXPORT void assign(const WebHTTPBody&);

    bool isNull() const { return !m_private; }

    // Returns the number of elements comprising the http body.
    BLINK_PLATFORM_EXPORT size_t elementCount() const;

    // Sets the values of the element at the given index. Returns false if
    // index is out of bounds.
    BLINK_PLATFORM_EXPORT bool elementAt(size_t index, Element&) const;

    // Append to the list of elements.
    BLINK_PLATFORM_EXPORT void appendData(const WebData&);
    BLINK_PLATFORM_EXPORT void appendFile(const WebString&);
    // Passing -1 to fileLength means to the end of the file.
    BLINK_PLATFORM_EXPORT void appendFileRange(const WebString&, long long fileStart, long long fileLength, double modificationTime);
    BLINK_PLATFORM_EXPORT void appendBlob(const WebString& uuid);

    // Append a resource which is identified by the FileSystem URL.
    BLINK_PLATFORM_EXPORT void appendFileSystemURLRange(const WebURL&, long long start, long long length, double modificationTime);

    // Identifies a particular form submission instance. A value of 0 is
    // used to indicate an unspecified identifier.
    BLINK_PLATFORM_EXPORT long long identifier() const;
    BLINK_PLATFORM_EXPORT void setIdentifier(long long);

    BLINK_PLATFORM_EXPORT bool containsPasswordData() const;
    BLINK_PLATFORM_EXPORT void setContainsPasswordData(bool);

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT WebHTTPBody(const WTF::PassRefPtr<WebCore::FormData>&);
    BLINK_PLATFORM_EXPORT WebHTTPBody& operator=(const WTF::PassRefPtr<WebCore::FormData>&);
    BLINK_PLATFORM_EXPORT operator WTF::PassRefPtr<WebCore::FormData>() const;
#endif

private:
    BLINK_PLATFORM_EXPORT void assign(WebHTTPBodyPrivate*);
    BLINK_PLATFORM_EXPORT void ensureMutable();

    WebHTTPBodyPrivate* m_private;
};

} // namespace blink

#endif
