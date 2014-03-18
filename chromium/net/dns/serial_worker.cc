// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/worker_pool.h"

namespace net {

SerialWorker::SerialWorker()
  : message_loop_(base::MessageLoopProxy::current()),
    state_(IDLE) {}

SerialWorker::~SerialWorker() {}

void SerialWorker::WorkNow() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  switch (state_) {
    case IDLE:
      if (!base::WorkerPool::PostTask(FROM_HERE, base::Bind(
          &SerialWorker::DoWorkJob, this), false)) {
#if defined(OS_POSIX)
        // See worker_pool_posix.cc.
        NOTREACHED() << "WorkerPool::PostTask is not expected to fail on posix";
#else
        LOG(WARNING) << "Failed to WorkerPool::PostTask, will retry later";
        const int kWorkerPoolRetryDelayMs = 100;
        message_loop_->PostDelayedTask(
            FROM_HERE,
            base::Bind(&SerialWorker::RetryWork, this),
            base::TimeDelta::FromMilliseconds(kWorkerPoolRetryDelayMs));
        state_ = WAITING;
        return;
#endif
      }
      state_ = WORKING;
      return;
    case WORKING:
      // Remember to re-read after |DoRead| finishes.
      state_ = PENDING;
      return;
    case CANCELLED:
    case PENDING:
    case WAITING:
      return;
    default:
      NOTREACHED() << "Unexpected state " << state_;
  }
}

void SerialWorker::Cancel() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  state_ = CANCELLED;
}

void SerialWorker::DoWorkJob() {
  this->DoWork();
  // If this fails, the loop is gone, so there is no point retrying.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &SerialWorker::OnWorkJobFinished, this));
}

void SerialWorker::OnWorkJobFinished() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  switch (state_) {
    case CANCELLED:
      return;
    case WORKING:
      state_ = IDLE;
      this->OnWorkFinished();
      return;
    case PENDING:
      state_ = IDLE;
      WorkNow();
      return;
    default:
      NOTREACHED() << "Unexpected state " << state_;
  }
}

void SerialWorker::RetryWork() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  switch (state_) {
    case CANCELLED:
      return;
    case WAITING:
      state_ = IDLE;
      WorkNow();
      return;
    default:
      NOTREACHED() << "Unexpected state " << state_;
  }
}

}  // namespace net

