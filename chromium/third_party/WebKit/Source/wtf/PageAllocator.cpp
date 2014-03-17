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

#include "config.h"
#include "wtf/PageAllocator.h"

#include "wtf/Assertions.h"
#include "wtf/ProcessID.h"
#include "wtf/SpinLock.h"

#include <limits.h>

#if OS(POSIX)

#include <sys/mman.h>

#ifndef MADV_FREE
#define MADV_FREE MADV_DONTNEED
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#elif OS(WIN)

#include <windows.h>

#else
#error Unknown OS
#endif // OS(POSIX)

namespace WTF {

// This simple internal function wraps the OS-specific page allocation call so
// that it behaves consistently: the address is a hint and if it cannot be used,
// the allocation will be placed elsewhere.
static void* systemAllocPages(void* addr, size_t len)
{
    ASSERT(!(len & kPageAllocationGranularityOffsetMask));
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kPageAllocationGranularityOffsetMask));
    void* ret;
#if OS(WIN)
    ret = VirtualAlloc(addr, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ret)
        ret = VirtualAlloc(0, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    ret = mmap(addr, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    RELEASE_ASSERT(ret != MAP_FAILED);
#endif
    RELEASE_ASSERT(ret);
    return ret;
}

static bool trimMapping(void* baseAddr, size_t baseLen, void* trimAddr, size_t trimLen)
{
#if OS(WIN)
    return false;
#else
    char* basePtr = static_cast<char*>(baseAddr);
    char* trimPtr = static_cast<char*>(trimAddr);
    ASSERT(trimPtr >= basePtr);
    ASSERT(trimPtr + trimLen <= basePtr + baseLen);
    size_t preLen = trimPtr - basePtr;
    if (preLen) {
        int ret = munmap(basePtr, preLen);
        RELEASE_ASSERT(!ret);
    }
    size_t postLen = (basePtr + baseLen) - (trimPtr + trimLen);
    if (postLen) {
        int ret = munmap(trimPtr + trimLen, postLen);
        RELEASE_ASSERT(!ret);
    }
    return true;
#endif
}

// This is the same PRNG as used by tcmalloc for mapping address randomness;
// see http://burtleburtle.net/bob/rand/smallprng.html
struct ranctx {
    int lock;
    bool initialized;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
};

#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

uint32_t ranvalInternal(ranctx* x)
{
    uint32_t e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

#undef rot

uint32_t ranval(ranctx* x)
{
    spinLockLock(&x->lock);
    if (UNLIKELY(!x->initialized)) {
        x->initialized = true;
        char c;
        uint32_t seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&c));
        seed ^= static_cast<uint32_t>(getCurrentProcessID());
        x->a = 0xf1ea5eed;
        x->b = x->c = x->d = seed;
        for (int i = 0; i < 20; ++i) {
            (void) ranvalInternal(x);
        }
    }
    uint32_t ret = ranvalInternal(x);
    spinLockUnlock(&x->lock);
    return ret;
}

static struct ranctx s_ranctx;

// This internal function calculates a random preferred mapping address.
// It is used when the client of allocPages() passes null as the address.
// In calculating an address, we balance good ASLR against not fragmenting the
// address space too badly.
static void* getRandomPageBase()
{
    uintptr_t random;
    random = static_cast<uintptr_t>(ranval(&s_ranctx));
#if CPU(X86_64)
    random <<= 32UL;
    random |= static_cast<uintptr_t>(ranval(&s_ranctx));
    // This address mask gives a low liklihood of address space collisions.
    // We handle the situation gracefully if there is a collision.
#if OS(WIN)
    // 64-bit Windows has a bizarrely small 8TB user address space.
    // Allocates in the 1-5TB region.
    random &= 0x3ffffffffffUL;
    random += 0x10000000000UL;
#else
    // Linux and OS X support the full 47-bit user space of x64 processors.
    random &= 0x3fffffffffffUL;
#endif
#else // !CPU(X86_64)
    // This is a good range on Windows, Linux and Mac.
    // Allocates in the 0.5-1.5GB region.
    random &= 0x3fffffff;
    random += 0x20000000;
#endif // CPU(X86_64)
    random &= kPageAllocationGranularityBaseMask;
    return reinterpret_cast<void*>(random);
}

void* allocPages(void* addr, size_t len, size_t align)
{
    RELEASE_ASSERT(len < INT_MAX - align);
    ASSERT(len >= kPageAllocationGranularity);
    ASSERT(!(len & kPageAllocationGranularityOffsetMask));
    ASSERT(align >= kPageAllocationGranularity);
    ASSERT(!(align & kPageAllocationGranularityOffsetMask));
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kPageAllocationGranularityOffsetMask));
    size_t alignOffsetMask = align - 1;
    size_t alignBaseMask = ~alignOffsetMask;
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & alignOffsetMask));
    // If the client passed null as the address, choose a good one.
    if (!addr) {
        addr = getRandomPageBase();
        addr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & alignBaseMask);
    }

    // The common case, which is also the least work we can do, is that the
    // address and length are suitable. Just try it.
    void* ret = systemAllocPages(addr, len);
    // If the alignment is to our liking, we're done.
    if (!(reinterpret_cast<uintptr_t>(ret) & alignOffsetMask))
        return ret;

    // Annoying. Unmap and map a larger range to be sure to succeed on the
    // second, slower attempt.
    freePages(ret, len);

    size_t tryLen = len + (align - kPageAllocationGranularity);

    // We loop to cater for the unlikely case where another thread maps on top
    // of the aligned location we choose.
    int count = 0;
    while (count++ < 100) {
        ret = systemAllocPages(addr, tryLen);
        // We can now try and trim out a subset of the mapping.
        addr = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(ret) + alignOffsetMask) & alignBaseMask);

        // On POSIX systems, we can trim the oversized mapping to fit exactly.
        // This will always work on POSIX systems.
        if (trimMapping(ret, tryLen, addr, len))
            return addr;

        // On Windows, you can't trim an existing mapping so we unmap and remap
        // a subset. We used to do for all platforms, but OSX 10.8 has a
        // broken mmap() that ignores address hints for valid, unused addresses.
        freePages(ret, tryLen);
        ret = systemAllocPages(addr, len);
        if (ret == addr)
            return ret;

        // Unlikely race / collision. Do the simple thing and just start again.
        freePages(ret, len);
        addr = getRandomPageBase();
        addr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & alignBaseMask);
    }
    IMMEDIATE_CRASH();
    return 0;
}

void freePages(void* addr, size_t len)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(addr) & kPageAllocationGranularityOffsetMask));
    ASSERT(!(len & kPageAllocationGranularityOffsetMask));
#if OS(POSIX)
    int ret = munmap(addr, len);
    RELEASE_ASSERT(!ret);
#else
    BOOL ret = VirtualFree(addr, 0, MEM_RELEASE);
    RELEASE_ASSERT(ret);
#endif
}

void setSystemPagesInaccessible(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    int ret = mprotect(addr, len, PROT_NONE);
    RELEASE_ASSERT(!ret);
#else
    BOOL ret = VirtualFree(addr, len, MEM_DECOMMIT);
    RELEASE_ASSERT(ret);
#endif
}

void decommitSystemPages(void* addr, size_t len)
{
    ASSERT(!(len & kSystemPageOffsetMask));
#if OS(POSIX)
    int ret = madvise(addr, len, MADV_FREE);
    RELEASE_ASSERT(!ret);
#else
    void* ret = VirtualAlloc(addr, len, MEM_RESET, PAGE_READWRITE);
    RELEASE_ASSERT(ret);
#endif
}

} // namespace WTF

