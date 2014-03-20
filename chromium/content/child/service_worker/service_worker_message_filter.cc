// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_message_filter.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_task_runner.h"
#include "ipc/ipc_message_macros.h"

namespace content {

ServiceWorkerMessageFilter::ServiceWorkerMessageFilter(ThreadSafeSender* sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(sender) {}

ServiceWorkerMessageFilter::~ServiceWorkerMessageFilter() {}

base::TaskRunner* ServiceWorkerMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != ServiceWorkerMsgStart)
    return NULL;
  int ipc_thread_id = 0;
  const bool success = PickleIterator(msg).ReadInt(&ipc_thread_id);
  DCHECK(success);
  if (!ipc_thread_id)
    return main_thread_loop_proxy_.get();
  return new WorkerThreadTaskRunner(ipc_thread_id);
}

bool ServiceWorkerMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != ServiceWorkerMsgStart)
    return false;
  ServiceWorkerDispatcher::ThreadSpecificInstance(thread_safe_sender_.get())
      ->OnMessageReceived(msg);
  return true;
}

}  // namespace content
