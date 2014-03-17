/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#ifndef DiscardablePixelRef_h
#define DiscardablePixelRef_h

#include "SkBitmap.h"
#include "SkPixelRef.h"

#include "platform/PlatformExport.h"
#include "public/platform/WebDiscardableMemory.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

// Class for allocating the DiscardablePixelRef object.
class PLATFORM_EXPORT DiscardablePixelRefAllocator : public SkBitmap::Allocator {
    // SkBitmap::Allocator implementation. The discardable memory allocated
    // after this call is locked and will not be purged until next
    // onUnlockPixels().
    virtual bool allocPixelRef(SkBitmap*, SkColorTable*);
};

// PixelRef object whose memory can be discarded when pixels are unlocked.
class PLATFORM_EXPORT DiscardablePixelRef : public SkPixelRef {
public:
    DiscardablePixelRef(const SkImageInfo&, PassOwnPtr<SkMutex>);
    ~DiscardablePixelRef();

    static bool isDiscardable(SkPixelRef*);
    bool allocAndLockDiscardableMemory(size_t);

    SK_DECLARE_UNFLATTENABLE_OBJECT()

protected:
    // SkPixelRef implementation.
    virtual void* onLockPixels(SkColorTable**);
    virtual void onUnlockPixels();

private:
    void* m_lockedMemory;
    OwnPtr<blink::WebDiscardableMemory> m_discardable;
    OwnPtr<SkMutex> m_mutex;
};

} // namespace WebCore

#endif
