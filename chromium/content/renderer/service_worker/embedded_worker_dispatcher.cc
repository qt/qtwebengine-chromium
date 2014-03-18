// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_dispatcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorkerStartData.h"
#include "webkit/child/worker_task_runner.h"
#include "webkit/common/user_agent/user_agent.h"

namespace content {

// A thin wrapper of WebEmbeddedWorker which also adds and releases process
// references automatically.
class EmbeddedWorkerDispatcher::WorkerWrapper {
 public:
  explicit WorkerWrapper(blink::WebEmbeddedWorker* worker) : worker_(worker) {}
  ~WorkerWrapper() {}

  blink::WebEmbeddedWorker* worker() { return worker_.get(); }

 private:
  ScopedChildProcessReference process_ref_;
  scoped_ptr<blink::WebEmbeddedWorker> worker_;
};

EmbeddedWorkerDispatcher::EmbeddedWorkerDispatcher() : weak_factory_(this) {}

EmbeddedWorkerDispatcher::~EmbeddedWorkerDispatcher() {}

bool EmbeddedWorkerDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDispatcher, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_StartWorker, OnStartWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_TerminateWorker, OnTerminateWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerDispatcher::WorkerContextDestroyed(
    int embedded_worker_id) {
  workers_.Remove(embedded_worker_id);
}

void EmbeddedWorkerDispatcher::OnStartWorker(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& script_url) {
  DCHECK(!workers_.Lookup(embedded_worker_id));
  scoped_ptr<WorkerWrapper> wrapper(new WorkerWrapper(
      blink::WebEmbeddedWorker::create(
          new ServiceWorkerContextClient(
              embedded_worker_id,
              service_worker_version_id,
              script_url),
          NULL)));

  blink::WebEmbeddedWorkerStartData start_data;
  start_data.scriptURL = script_url;
  start_data.userAgent =
      base::UTF8ToUTF16(webkit_glue::GetUserAgent(script_url));

  wrapper->worker()->startWorkerContext(start_data);
  workers_.AddWithID(wrapper.release(), embedded_worker_id);
}

void EmbeddedWorkerDispatcher::OnTerminateWorker(int embedded_worker_id) {
  WorkerWrapper* wrapper = workers_.Lookup(embedded_worker_id);
  if (!wrapper) {
    LOG(WARNING) << "Got OnTerminateWorker for nonexistent worker";
    return;
  }

  // This should eventually call WorkerContextDestroyed. (We may need to post
  // a delayed task to forcibly abort the worker context if we find it
  // necessary)
  wrapper->worker()->terminateWorkerContext();
}

}  // namespace content
