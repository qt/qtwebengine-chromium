/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebIDBDatabaseCallbacksImpl_h
#define WebIDBDatabaseCallbacksImpl_h

#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "public/platform/WebIDBDatabaseCallbacks.h"
#include "public/platform/WebIDBDatabaseError.h"
#include "public/platform/WebString.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class WebIDBDatabaseCallbacksImpl : public blink::WebIDBDatabaseCallbacks {
public:
    static PassOwnPtr<WebIDBDatabaseCallbacksImpl> create(PassRefPtr<IDBDatabaseCallbacks>);

    virtual ~WebIDBDatabaseCallbacksImpl();

    virtual void onForcedClose();
    virtual void onVersionChange(long long oldVersion, long long newVersion);
    virtual void onAbort(long long transactionId, const blink::WebIDBDatabaseError&);
    virtual void onComplete(long long transactionId);

private:
    explicit WebIDBDatabaseCallbacksImpl(PassRefPtr<IDBDatabaseCallbacks>);

    RefPtr<IDBDatabaseCallbacks> m_callbacks;
};

} // namespace WebCore

#endif // WebIDBDatabaseCallbacksImpl_h
