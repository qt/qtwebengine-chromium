// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/child/web_discardable_memory_impl.h"

namespace webkit_glue {

WebDiscardableMemoryImpl::~WebDiscardableMemoryImpl() {}

// static
scoped_ptr<WebDiscardableMemoryImpl>
WebDiscardableMemoryImpl::CreateLockedMemory(size_t size) {
  scoped_ptr<base::DiscardableMemory> memory(
      base::DiscardableMemory::CreateLockedMemory(size));
  if (!memory)
    return scoped_ptr<WebDiscardableMemoryImpl>();
  return make_scoped_ptr(new WebDiscardableMemoryImpl(memory.Pass()));
}

bool WebDiscardableMemoryImpl::lock() {
  base::LockDiscardableMemoryStatus status = discardable_->Lock();
  switch (status) {
    case base::DISCARDABLE_MEMORY_SUCCESS:
      return true;
    case base::DISCARDABLE_MEMORY_PURGED:
      discardable_->Unlock();
      return false;
    default:
      discardable_.reset();
      return false;
  }
}

void WebDiscardableMemoryImpl::unlock() {
  discardable_->Unlock();
}

void* WebDiscardableMemoryImpl::data() {
  return discardable_->Memory();
}

WebDiscardableMemoryImpl::WebDiscardableMemoryImpl(
    scoped_ptr<base::DiscardableMemory> memory)
    : discardable_(memory.Pass()) {
}

}  // namespace webkit_glue
