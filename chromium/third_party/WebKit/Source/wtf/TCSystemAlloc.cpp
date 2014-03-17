// Copyright (c) 2005, 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Sanjay Ghemawat

#include "config.h"
#if !USE(SYSTEM_MALLOC)
#include "TCSystemAlloc.h"

#include "Assertions.h"
#include "CheckedArithmetic.h"
#include "TCSpinLock.h"
#include "VMTags.h"
#include <algorithm>
#include <stdint.h>

#if OS(WIN)
#include "windows.h"
#else
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

using namespace std;

// Structure for discovering alignment
union MemoryAligner {
  void*  p;
  double d;
  size_t s;
};

static SpinLock spinlock = SPINLOCK_INITIALIZER;

// Page size is initialized on demand
static size_t pagesize = 0;

// Configuration parameters.

#if HAVE(MMAP)
static bool use_mmap = true;
#endif

// Flags to keep us from retrying allocators that failed.
static bool devmem_failure = false;
static bool sbrk_failure = false;
static bool mmap_failure = false;

#if HAVE(MMAP)

static void* TryMmap(size_t size, size_t *actual_size, size_t alignment) {
  // Enforce page alignment
  if (pagesize == 0) pagesize = getpagesize();
  if (alignment < pagesize) alignment = pagesize;
  size = ((size + alignment - 1) / alignment) * alignment;

  // could theoretically return the "extra" bytes here, but this
  // is simple and correct.
  if (actual_size)
    *actual_size = size;

  // Ask for extra memory if alignment > pagesize
  size_t extra = 0;
  if (alignment > pagesize) {
    extra = alignment - pagesize;
  }
  Checked<size_t> mapSize = Checked<size_t>(size) + extra + 2 * pagesize;
  void* result = mmap(NULL, mapSize.unsafeGet(),
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS,
                      VM_TAG_FOR_TCMALLOC_MEMORY, 0);
  if (result == reinterpret_cast<void*>(MAP_FAILED)) {
    mmap_failure = true;
    return NULL;
  }
  mmap(result, pagesize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, VM_TAG_FOR_TCMALLOC_MEMORY, 0);
  mmap(static_cast<char*>(result) + (mapSize - pagesize).unsafeGet(), pagesize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, VM_TAG_FOR_TCMALLOC_MEMORY, 0);
  result = static_cast<char*>(result) + pagesize;
  // Adjust the return memory so it is aligned
  uintptr_t ptr = reinterpret_cast<uintptr_t>(result);
  size_t adjust = 0;
  if ((ptr & (alignment - 1)) != 0) {
    adjust = alignment - (ptr & (alignment - 1));
  }

  // Return the unused memory to the system
  if (adjust > 0) {
    munmap(reinterpret_cast<void*>(ptr), adjust);
  }
  if (adjust < extra) {
    munmap(reinterpret_cast<void*>(ptr + adjust + size), extra - adjust);
  }

  ptr += adjust;
  return reinterpret_cast<void*>(ptr);
}

#endif /* HAVE(MMAP) */

void* TCMalloc_SystemAlloc(size_t size, size_t *actual_size, size_t alignment) {
  // Discard requests that overflow
  if (size + alignment < size) return NULL;

  SpinLockHolder lock_holder(&spinlock);

  // Enforce minimum alignment
  if (alignment < sizeof(MemoryAligner)) alignment = sizeof(MemoryAligner);

  // Try twice, once avoiding allocators that failed before, and once
  // more trying all allocators even if they failed before.
  for (int i = 0; i < 2; i++) {

#if HAVE(MMAP)
    if (use_mmap && !mmap_failure) {
      void* result = TryMmap(size, actual_size, alignment);
      if (result != NULL) return result;
    }
#endif

    // nothing worked - reset failure flags and try again
    devmem_failure = false;
    sbrk_failure = false;
    mmap_failure = false;
  }
  return NULL;
}

#if HAVE(MADV_FREE_REUSE)

void TCMalloc_SystemRelease(void* start, size_t length)
{
    int madviseResult;

    while ((madviseResult = madvise(start, length, MADV_FREE_REUSABLE)) == -1 && errno == EAGAIN) { }

    // Although really advisory, if madvise fail, we want to know about it.
    ASSERT_UNUSED(madviseResult, madviseResult != -1);
}

#elif HAVE(MADV_FREE) || HAVE(MADV_DONTNEED)

void TCMalloc_SystemRelease(void* start, size_t length)
{
    // MADV_FREE clears the modified bit on pages, which allows
    // them to be discarded immediately.
#if HAVE(MADV_FREE)
    const int advice = MADV_FREE;
#else
    const int advice = MADV_DONTNEED;
#endif
  if (pagesize == 0) pagesize = getpagesize();
  const size_t pagemask = pagesize - 1;

  size_t new_start = reinterpret_cast<size_t>(start);
  size_t end = new_start + length;
  size_t new_end = end;

  // Round up the starting address and round down the ending address
  // to be page aligned:
  new_start = (new_start + pagesize - 1) & ~pagemask;
  new_end = new_end & ~pagemask;

  ASSERT((new_start & pagemask) == 0);
  ASSERT((new_end & pagemask) == 0);
  ASSERT(new_start >= reinterpret_cast<size_t>(start));
  ASSERT(new_end <= end);

  if (new_end > new_start) {
    // Note -- ignoring most return codes, because if this fails it
    // doesn't matter...
    while (madvise(reinterpret_cast<char*>(new_start), new_end - new_start,
                   advice) == -1 &&
           errno == EAGAIN) {
      // NOP
    }
  }
}

#elif HAVE(MMAP)

void TCMalloc_SystemRelease(void* start, size_t length)
{
  void* newAddress = mmap(start, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  // If the mmap failed then that's ok, we just won't return the memory to the system.
  ASSERT_UNUSED(newAddress, newAddress == start || newAddress == reinterpret_cast<void*>(MAP_FAILED));
}

#else

// Platforms that don't support returning memory use an empty inline version of TCMalloc_SystemRelease
// declared in TCSystemAlloc.h

#endif

#if HAVE(MADV_FREE_REUSE)

void TCMalloc_SystemCommit(void* start, size_t length)
{
    while (madvise(start, length, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
}

#else

// Platforms that don't need to explicitly commit memory use an empty inline version of TCMalloc_SystemCommit
// declared in TCSystemAlloc.h

#endif

#endif // #if !USE(SYSTEM_MALLOC)

