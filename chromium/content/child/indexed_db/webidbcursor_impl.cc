// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/webidbcursor_impl.h"

#include <vector>

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_messages.h"

using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBKey;

namespace content {

WebIDBCursorImpl::WebIDBCursorImpl(int32 ipc_cursor_id,
                                   ThreadSafeSender* thread_safe_sender)
    : ipc_cursor_id_(ipc_cursor_id),
      continue_count_(0),
      used_prefetches_(0),
      pending_onsuccess_callbacks_(0),
      prefetch_amount_(kMinPrefetchAmount),
      thread_safe_sender_(thread_safe_sender) {}

WebIDBCursorImpl::~WebIDBCursorImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.

  if (ipc_cursor_id_ != kInvalidCursorId) {
    // Invalid ID used in tests to avoid really sending this message.
    thread_safe_sender_->Send(
        new IndexedDBHostMsg_CursorDestroyed(ipc_cursor_id_));
  }
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->CursorDestroyed(ipc_cursor_id_);
}

void WebIDBCursorImpl::advance(unsigned long count,
                               WebIDBCallbacks* callbacks_ptr) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  ResetPrefetchCache();
  dispatcher->RequestIDBCursorAdvance(
      count, callbacks.release(), ipc_cursor_id_);
}

void WebIDBCursorImpl::continueFunction(const WebIDBKey& key,
                                        WebIDBCallbacks* callbacks_ptr) {
  continueFunction(key, WebIDBKey::createNull(), callbacks_ptr);
}

void WebIDBCursorImpl::continueFunction(const WebIDBKey& key,
                                        const WebIDBKey& primary_key,
                                        WebIDBCallbacks* callbacks_ptr) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (key.keyType() == blink::WebIDBKeyTypeNull &&
      primary_key.keyType() == blink::WebIDBKeyTypeNull) {
    // No key(s), so this would qualify for a prefetch.
    ++continue_count_;

    if (!prefetch_keys_.empty()) {
      // We have a prefetch cache, so serve the result from that.
      CachedContinue(callbacks.get());
      return;
    }

    if (continue_count_ > kPrefetchContinueThreshold) {
      // Request pre-fetch.
      ++pending_onsuccess_callbacks_;
      dispatcher->RequestIDBCursorPrefetch(
          prefetch_amount_, callbacks.release(), ipc_cursor_id_);

      // Increase prefetch_amount_ exponentially.
      prefetch_amount_ *= 2;
      if (prefetch_amount_ > kMaxPrefetchAmount)
        prefetch_amount_ = kMaxPrefetchAmount;

      return;
    }
  } else {
    // Key argument supplied. We couldn't prefetch this.
    ResetPrefetchCache();
  }

  dispatcher->RequestIDBCursorContinue(IndexedDBKeyBuilder::Build(key),
                                       IndexedDBKeyBuilder::Build(primary_key),
                                       callbacks.release(),
                                       ipc_cursor_id_);
}

void WebIDBCursorImpl::postSuccessHandlerCallback() {
  pending_onsuccess_callbacks_--;

  // If the onsuccess callback called continue() on the cursor again,
  // and that continue was served by the prefetch cache, then
  // pending_onsuccess_callbacks_ would be incremented.
  // If not, it means the callback did something else, or nothing at all,
  // in which case we need to reset the cache.

  if (pending_onsuccess_callbacks_ == 0)
    ResetPrefetchCache();
}

void WebIDBCursorImpl::SetPrefetchData(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    const std::vector<WebData>& values) {
  prefetch_keys_.assign(keys.begin(), keys.end());
  prefetch_primary_keys_.assign(primary_keys.begin(), primary_keys.end());
  prefetch_values_.assign(values.begin(), values.end());

  used_prefetches_ = 0;
  pending_onsuccess_callbacks_ = 0;
}

void WebIDBCursorImpl::CachedContinue(WebIDBCallbacks* callbacks) {
  DCHECK_GT(prefetch_keys_.size(), 0ul);
  DCHECK(prefetch_primary_keys_.size() == prefetch_keys_.size());
  DCHECK(prefetch_values_.size() == prefetch_keys_.size());

  IndexedDBKey key = prefetch_keys_.front();
  IndexedDBKey primary_key = prefetch_primary_keys_.front();
  // this could be a real problem.. we need 2 CachedContinues
  WebData value = prefetch_values_.front();

  prefetch_keys_.pop_front();
  prefetch_primary_keys_.pop_front();
  prefetch_values_.pop_front();
  used_prefetches_++;

  pending_onsuccess_callbacks_++;

  callbacks->onSuccess(WebIDBKeyBuilder::Build(key),
                       WebIDBKeyBuilder::Build(primary_key),
                       value);
}

void WebIDBCursorImpl::ResetPrefetchCache() {
  continue_count_ = 0;
  prefetch_amount_ = kMinPrefetchAmount;

  if (!prefetch_keys_.size()) {
    // No prefetch cache, so no need to reset the cursor in the back-end.
    return;
  }

  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBCursorPrefetchReset(
      used_prefetches_, prefetch_keys_.size(), ipc_cursor_id_);
  prefetch_keys_.clear();
  prefetch_primary_keys_.clear();
  prefetch_values_.clear();

  pending_onsuccess_callbacks_ = 0;
}

}  // namespace content
