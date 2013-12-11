// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_cursor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"

namespace content {

IndexedDBCursor::IndexedDBCursor(
    scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
    indexed_db::CursorType cursor_type,
    IndexedDBDatabase::TaskType task_type,
    IndexedDBTransaction* transaction)
    : task_type_(task_type),
      cursor_type_(cursor_type),
      transaction_(transaction),
      cursor_(cursor.Pass()),
      closed_(false) {
  transaction_->RegisterOpenCursor(this);
}

IndexedDBCursor::~IndexedDBCursor() {
  transaction_->UnregisterOpenCursor(this);
}

void IndexedDBCursor::Continue(scoped_ptr<IndexedDBKey> key,
                               scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::Continue");

  transaction_->ScheduleTask(
      task_type_,
      base::Bind(&IndexedDBCursor::CursorIterationOperation,
                 this,
                 base::Passed(&key),
                 callbacks));
}

void IndexedDBCursor::Advance(uint32 count,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::Advance");

  transaction_->ScheduleTask(
      task_type_,
      base::Bind(
          &IndexedDBCursor::CursorAdvanceOperation, this, count, callbacks));
}

void IndexedDBCursor::CursorAdvanceOperation(
    uint32 count,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("IndexedDBCursor::CursorAdvanceOperation");
  if (!cursor_ || !cursor_->Advance(count)) {
    cursor_.reset();
    callbacks->OnSuccess(static_cast<std::string*>(NULL));
    return;
  }

  callbacks->OnSuccess(key(), primary_key(), Value());
}

void IndexedDBCursor::CursorIterationOperation(
    scoped_ptr<IndexedDBKey> key,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("IndexedDBCursor::CursorIterationOperation");
  if (!cursor_ ||
      !cursor_->Continue(key.get(), IndexedDBBackingStore::Cursor::SEEK)) {
    cursor_.reset();
    callbacks->OnSuccess(static_cast<std::string*>(NULL));
    return;
  }

  callbacks->OnSuccess(this->key(), primary_key(), Value());
}

void IndexedDBCursor::PrefetchContinue(
    int number_to_fetch,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::PrefetchContinue");

  transaction_->ScheduleTask(
      task_type_,
      base::Bind(&IndexedDBCursor::CursorPrefetchIterationOperation,
                 this,
                 number_to_fetch,
                 callbacks));
}

void IndexedDBCursor::CursorPrefetchIterationOperation(
    int number_to_fetch,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("IndexedDBCursor::CursorPrefetchIterationOperation");

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBKey> found_primary_keys;
  std::vector<std::string> found_values;

  if (cursor_)
    saved_cursor_.reset(cursor_->Clone());
  const size_t max_size_estimate = 10 * 1024 * 1024;
  size_t size_estimate = 0;

  for (int i = 0; i < number_to_fetch; ++i) {
    if (!cursor_ || !cursor_->Continue()) {
      cursor_.reset();
      break;
    }

    found_keys.push_back(cursor_->key());
    found_primary_keys.push_back(cursor_->primary_key());

    switch (cursor_type_) {
      case indexed_db::CURSOR_KEY_ONLY:
        found_values.push_back(std::string());
        break;
      case indexed_db::CURSOR_KEY_AND_VALUE: {
        std::string value;
        value.swap(*cursor_->Value());
        size_estimate += value.size();
        found_values.push_back(value);
        break;
      }
      default:
        NOTREACHED();
    }
    size_estimate += cursor_->key().size_estimate();
    size_estimate += cursor_->primary_key().size_estimate();

    if (size_estimate > max_size_estimate)
      break;
  }

  if (!found_keys.size()) {
    callbacks->OnSuccess(static_cast<std::string*>(NULL));
    return;
  }

  callbacks->OnSuccessWithPrefetch(
      found_keys, found_primary_keys, found_values);
}

void IndexedDBCursor::PrefetchReset(int used_prefetches, int) {
  IDB_TRACE("IndexedDBCursor::PrefetchReset");
  cursor_.swap(saved_cursor_);
  saved_cursor_.reset();

  if (closed_)
    return;
  if (cursor_) {
    for (int i = 0; i < used_prefetches; ++i) {
      bool ok = cursor_->Continue();
      DCHECK(ok);
    }
  }
}

void IndexedDBCursor::Close() {
  IDB_TRACE("IndexedDBCursor::Close");
  closed_ = true;
  cursor_.reset();
  saved_cursor_.reset();
}

}  // namespace content
