/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIDBCallbacksImpl_h
#define WebIDBCallbacksImpl_h

#include "public/platform/WebIDBCallbacks.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {
class WebData;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBIndex;
class WebIDBKey;
class WebIDBKeyPath;
struct WebIDBMetadata;
}

namespace WebCore {
class IDBRequest;

class WebIDBCallbacksImpl : public blink::WebIDBCallbacks {
public:
    static PassOwnPtr<WebIDBCallbacksImpl> create(PassRefPtr<IDBRequest>);

    virtual ~WebIDBCallbacksImpl();

    // Pointers transfer ownership.
    virtual void onError(const blink::WebIDBDatabaseError&);
    virtual void onSuccess(const blink::WebVector<blink::WebString>&);
    virtual void onSuccess(blink::WebIDBCursor*, const blink::WebIDBKey&, const blink::WebIDBKey& primaryKey, const blink::WebData&);
    virtual void onSuccess(blink::WebIDBDatabase*, const blink::WebIDBMetadata&);
    virtual void onSuccess(const blink::WebIDBKey&);
    virtual void onSuccess(const blink::WebData&);
    virtual void onSuccess(const blink::WebData&, const blink::WebIDBKey&, const blink::WebIDBKeyPath&);
    virtual void onSuccess(long long);
    virtual void onSuccess();
    virtual void onSuccess(const blink::WebIDBKey&, const blink::WebIDBKey& primaryKey, const blink::WebData&);
    virtual void onBlocked(long long oldVersion);
    virtual void onUpgradeNeeded(long long oldVersion, blink::WebIDBDatabase*, const blink::WebIDBMetadata&, unsigned short dataLoss, blink::WebString dataLossMessage);

private:
    explicit WebIDBCallbacksImpl(PassRefPtr<IDBRequest>);

    RefPtr<IDBRequest> m_request;
};

} // namespace WebCore

#endif // WebIDBCallbacksImpl_h
