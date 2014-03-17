/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StorageAreaProxy_h
#define StorageAreaProxy_h

#include "core/storage/StorageArea.h"

namespace blink {
class WebStorageArea;
class WebStorageNamespace;
}

namespace WebCore {

class ExceptionState;
class Frame;
class KURL;
class Page;
class SecurityOrigin;
class Storage;

class StorageAreaProxy : public StorageArea {
public:
    StorageAreaProxy(PassOwnPtr<blink::WebStorageArea>, StorageType);
    virtual ~StorageAreaProxy();

    // The HTML5 DOM Storage API
    virtual unsigned length(ExceptionState&, Frame* sourceFrame);
    virtual String key(unsigned index, ExceptionState&, Frame* sourceFrame);
    virtual String getItem(const String& key, ExceptionState&, Frame* sourceFrame);
    virtual void setItem(const String& key, const String& value, ExceptionState&, Frame* sourceFrame);
    virtual void removeItem(const String& key, ExceptionState&, Frame* sourceFrame);
    virtual void clear(ExceptionState&, Frame* sourceFrame);
    virtual bool contains(const String& key, ExceptionState&, Frame* sourceFrame);

    virtual bool canAccessStorage(Frame*);

    virtual size_t memoryBytesUsedByCache();

    static void dispatchLocalStorageEvent(
            const String& key, const String& oldValue, const String& newValue,
            SecurityOrigin*, const KURL& pageURL, blink::WebStorageArea* sourceAreaInstance, bool originatedInProcess);
    static void dispatchSessionStorageEvent(
            const String& key, const String& oldValue, const String& newValue,
            SecurityOrigin*, const KURL& pageURL, const blink::WebStorageNamespace&,
            blink::WebStorageArea* sourceAreaInstance, bool originatedInProcess);

private:
    static bool isEventSource(Storage*, blink::WebStorageArea* sourceAreaInstance);

    OwnPtr<blink::WebStorageArea> m_storageArea;
    StorageType m_storageType;
    mutable bool m_canAccessStorageCachedResult;
    mutable Frame* m_canAccessStorageCachedFrame;
};

} // namespace WebCore

#endif // StorageAreaProxy_h
