// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"

namespace content {

const int64 kInactivityTimeoutPeriodSeconds = 60;

IndexedDBTransaction::TaskQueue::TaskQueue() {}
IndexedDBTransaction::TaskQueue::~TaskQueue() { clear(); }

void IndexedDBTransaction::TaskQueue::clear() {
  while (!queue_.empty())
    queue_.pop();
}

IndexedDBTransaction::Operation IndexedDBTransaction::TaskQueue::pop() {
  DCHECK(!queue_.empty());
  Operation task(queue_.front());
  queue_.pop();
  return task;
}

IndexedDBTransaction::TaskStack::TaskStack() {}
IndexedDBTransaction::TaskStack::~TaskStack() { clear(); }

void IndexedDBTransaction::TaskStack::clear() {
  while (!stack_.empty())
    stack_.pop();
}

IndexedDBTransaction::Operation IndexedDBTransaction::TaskStack::pop() {
  DCHECK(!stack_.empty());
  Operation task(stack_.top());
  stack_.pop();
  return task;
}

IndexedDBTransaction::IndexedDBTransaction(
    int64 id,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
    const std::set<int64>& object_store_ids,
    indexed_db::TransactionMode mode,
    IndexedDBDatabase* database,
    IndexedDBBackingStore::Transaction* backing_store_transaction)
    : id_(id),
      object_store_ids_(object_store_ids),
      mode_(mode),
      used_(false),
      state_(CREATED),
      commit_pending_(false),
      callbacks_(callbacks),
      database_(database),
      transaction_(backing_store_transaction),
      backing_store_transaction_begun_(false),
      should_process_queue_(false),
      pending_preemptive_events_(0) {
  database_->transaction_coordinator().DidCreateTransaction(this);

  diagnostics_.tasks_scheduled = 0;
  diagnostics_.tasks_completed = 0;
  diagnostics_.creation_time = base::Time::Now();
}

IndexedDBTransaction::~IndexedDBTransaction() {
  // It shouldn't be possible for this object to get deleted until it's either
  // complete or aborted.
  DCHECK_EQ(state_, FINISHED);
  DCHECK(preemptive_task_queue_.empty());
  DCHECK(task_queue_.empty());
  DCHECK(abort_task_stack_.empty());
}

void IndexedDBTransaction::ScheduleTask(Operation task, Operation abort_task) {
  if (state_ == FINISHED)
    return;

  timeout_timer_.Stop();
  used_ = true;
  task_queue_.push(task);
  ++diagnostics_.tasks_scheduled;
  abort_task_stack_.push(abort_task);
  RunTasksIfStarted();
}

void IndexedDBTransaction::ScheduleTask(IndexedDBDatabase::TaskType type,
                                        Operation task) {
  if (state_ == FINISHED)
    return;

  timeout_timer_.Stop();
  used_ = true;
  if (type == IndexedDBDatabase::NORMAL_TASK) {
    task_queue_.push(task);
    ++diagnostics_.tasks_scheduled;
  } else {
    preemptive_task_queue_.push(task);
  }
  RunTasksIfStarted();
}

void IndexedDBTransaction::RunTasksIfStarted() {
  DCHECK(used_);

  // Not started by the coordinator yet.
  if (state_ != STARTED)
    return;

  // A task is already posted.
  if (should_process_queue_)
    return;

  should_process_queue_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&IndexedDBTransaction::ProcessTaskQueue, this));
}

void IndexedDBTransaction::Abort() {
  Abort(IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error (unknown cause)"));
}

void IndexedDBTransaction::Abort(const IndexedDBDatabaseError& error) {
  IDB_TRACE("IndexedDBTransaction::Abort");
  if (state_ == FINISHED)
    return;

  // The last reference to this object may be released while performing the
  // abort steps below. We therefore take a self reference to keep ourselves
  // alive while executing this method.
  scoped_refptr<IndexedDBTransaction> protect(this);

  timeout_timer_.Stop();

  state_ = FINISHED;
  should_process_queue_ = false;

  if (backing_store_transaction_begun_)
    transaction_->Rollback();

  // Run the abort tasks, if any.
  while (!abort_task_stack_.empty())
    abort_task_stack_.pop().Run(0);

  preemptive_task_queue_.clear();
  task_queue_.clear();

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);
#ifndef NDEBUG
  DCHECK(!database_->transaction_coordinator().IsActive(this));
#endif
  database_->TransactionFinished(this);

  if (callbacks_.get())
    callbacks_->OnAbort(id_, error);

  database_->TransactionFinishedAndAbortFired(this);

  database_ = NULL;
}

bool IndexedDBTransaction::IsTaskQueueEmpty() const {
  return preemptive_task_queue_.empty() && task_queue_.empty();
}

bool IndexedDBTransaction::HasPendingTasks() const {
  return pending_preemptive_events_ || !IsTaskQueueEmpty();
}

void IndexedDBTransaction::RegisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.insert(cursor);
}

void IndexedDBTransaction::UnregisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.erase(cursor);
}

void IndexedDBTransaction::Start() {
  // TransactionCoordinator has started this transaction.
  DCHECK_EQ(CREATED, state_);
  state_ = STARTED;
  database_->TransactionStarted(this);
  diagnostics_.start_time = base::Time::Now();

  if (!used_)
    return;

  RunTasksIfStarted();
}

void IndexedDBTransaction::Commit() {
  IDB_TRACE("IndexedDBTransaction::Commit");

  // In multiprocess ports, front-end may have requested a commit but
  // an abort has already been initiated asynchronously by the
  // back-end.
  if (state_ == FINISHED)
    return;

  DCHECK(!used_ || state_ == STARTED);
  commit_pending_ = true;

  // Front-end has requested a commit, but there may be tasks like
  // create_index which are considered synchronous by the front-end
  // but are processed asynchronously.
  if (HasPendingTasks())
    return;

  // The last reference to this object may be released while performing the
  // commit steps below. We therefore take a self reference to keep ourselves
  // alive while executing this method.
  scoped_refptr<IndexedDBTransaction> protect(this);

  timeout_timer_.Stop();

  state_ = FINISHED;

  bool committed = !used_ || transaction_->Commit();

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  database_->transaction_coordinator().DidFinishTransaction(this);
  database_->TransactionFinished(this);

  if (committed) {
    abort_task_stack_.clear();
    callbacks_->OnComplete(id_);
    database_->TransactionFinishedAndCompleteFired(this);
  } else {
    while (!abort_task_stack_.empty())
      abort_task_stack_.pop().Run(0);

    callbacks_->OnAbort(
        id_,
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error committing transaction."));
    database_->TransactionFinishedAndAbortFired(this);
    database_->TransactionCommitFailed();
  }

  database_ = NULL;
}

void IndexedDBTransaction::ProcessTaskQueue() {
  IDB_TRACE("IndexedDBTransaction::ProcessTaskQueue");

  // May have been aborted.
  if (!should_process_queue_)
    return;

  DCHECK(!IsTaskQueueEmpty());
  should_process_queue_ = false;

  if (!backing_store_transaction_begun_) {
    transaction_->Begin();
    backing_store_transaction_begun_ = true;
  }

  // The last reference to this object may be released while performing the
  // tasks. Take take a self reference to keep this object alive so that
  // the loop termination conditions can be checked.
  scoped_refptr<IndexedDBTransaction> protect(this);

  TaskQueue* task_queue =
      pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  while (!task_queue->empty() && state_ != FINISHED) {
    DCHECK_EQ(STARTED, state_);
    Operation task(task_queue->pop());
    task.Run(this);
    if (!pending_preemptive_events_) {
      DCHECK(diagnostics_.tasks_completed < diagnostics_.tasks_scheduled);
      ++diagnostics_.tasks_completed;
    }

    // Event itself may change which queue should be processed next.
    task_queue =
        pending_preemptive_events_ ? &preemptive_task_queue_ : &task_queue_;
  }

  // If there are no pending tasks, we haven't already committed/aborted,
  // and the front-end requested a commit, it is now safe to do so.
  if (!HasPendingTasks() && state_ != FINISHED && commit_pending_) {
    Commit();
    return;
  }

  // The transaction may have been aborted while processing tasks.
  if (state_ == FINISHED)
    return;

  // Otherwise, start a timer in case the front-end gets wedged and
  // never requests further activity.
  timeout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kInactivityTimeoutPeriodSeconds),
      base::Bind(&IndexedDBTransaction::Timeout, this));
}

void IndexedDBTransaction::Timeout() {
  Abort(IndexedDBDatabaseError(
      blink::WebIDBDatabaseExceptionTimeoutError,
      ASCIIToUTF16("Transaction timed out due to inactivity.")));
}

void IndexedDBTransaction::CloseOpenCursors() {
  for (std::set<IndexedDBCursor*>::iterator i = open_cursors_.begin();
       i != open_cursors_.end();
       ++i)
    (*i)->Close();
  open_cursors_.clear();
}

}  // namespace content
