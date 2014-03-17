/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIDBKeyPath_h
#define WebIDBKeyPath_h

#include "WebCommon.h"
#include "WebIDBTypes.h"
#include "WebPrivateOwnPtr.h"
#include "WebString.h"
#include "WebVector.h"

namespace WebCore { class IDBKeyPath; }

namespace blink {

class WebIDBKeyPath {
public:
    BLINK_EXPORT static WebIDBKeyPath create(const WebString&);
    BLINK_EXPORT static WebIDBKeyPath create(const WebVector<WebString>&);
    BLINK_EXPORT static WebIDBKeyPath createNull();

    WebIDBKeyPath(const WebIDBKeyPath& keyPath) { assign(keyPath); }
    virtual ~WebIDBKeyPath() { reset(); }
    WebIDBKeyPath& operator=(const WebIDBKeyPath& keyPath)
    {
        assign(keyPath);
        return *this;
    }

    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebIDBKeyPath&);

    BLINK_EXPORT bool isValid() const;
    BLINK_EXPORT WebIDBKeyPathType keyPathType() const;
    BLINK_EXPORT WebVector<WebString> array() const; // Only valid for ArrayType.
    BLINK_EXPORT WebString string() const; // Only valid for StringType.

#if BLINK_IMPLEMENTATION
    WebIDBKeyPath(const WebCore::IDBKeyPath&);
    WebIDBKeyPath& operator=(const WebCore::IDBKeyPath&);
    operator const WebCore::IDBKeyPath&() const;
#endif

private:
    WebPrivateOwnPtr<WebCore::IDBKeyPath> m_private;
};

} // namespace blink

#endif // WebIDBKeyPath_h
