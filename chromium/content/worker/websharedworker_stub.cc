// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/websharedworker_stub.h"

#include "base/compiler_specific.h"
#include "content/child/child_process.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "content/worker/shared_worker_devtools_agent.h"
#include "content/worker/worker_thread.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

WebSharedWorkerStub::WebSharedWorkerStub(
    const base::string16& name,
    int route_id,
    const WorkerAppCacheInitInfo& appcache_init_info)
    : route_id_(route_id),
      appcache_init_info_(appcache_init_info),
      client_(route_id, this),
      name_(name),
      started_(false) {

  WorkerThread* worker_thread = WorkerThread::current();
  DCHECK(worker_thread);
  worker_thread->AddWorkerStub(this);
  // Start processing incoming IPCs for this worker.
  worker_thread->AddRoute(route_id_, this);

  // TODO(atwilson): Add support for NaCl when they support MessagePorts.
  impl_ = blink::WebSharedWorker::create(client());
  worker_devtools_agent_.reset(new SharedWorkerDevToolsAgent(route_id, impl_));
  client()->set_devtools_agent(worker_devtools_agent_.get());
}

WebSharedWorkerStub::~WebSharedWorkerStub() {
  impl_->clientDestroyed();
  WorkerThread* worker_thread = WorkerThread::current();
  DCHECK(worker_thread);
  worker_thread->RemoveWorkerStub(this);
  worker_thread->RemoveRoute(route_id_);
}

void WebSharedWorkerStub::Shutdown() {
  // The worker has exited - free ourselves and the client.
  delete this;
}

void WebSharedWorkerStub::EnsureWorkerContextTerminates() {
  client_.EnsureWorkerContextTerminates();
}

bool WebSharedWorkerStub::OnMessageReceived(const IPC::Message& message) {
  if (worker_devtools_agent_->OnMessageReceived(message))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerStub, message)
    IPC_MESSAGE_HANDLER(WorkerMsg_StartWorkerContext, OnStartWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSharedWorkerStub::OnChannelError() {
    OnTerminateWorkerContext();
}

const GURL& WebSharedWorkerStub::url() {
  return url_;
}

void WebSharedWorkerStub::OnStartWorkerContext(
    const GURL& url, const base::string16& user_agent,
    const base::string16& source_code,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType policy_type) {
  // Ignore multiple attempts to start this worker (can happen if two pages
  // try to start it simultaneously).
  if (started_)
    return;

  impl_->startWorkerContext(url, name_, user_agent, source_code,
                            content_security_policy, policy_type, 0);
  started_ = true;
  url_ = url;

  // Process any pending connections.
  for (PendingConnectInfoList::const_iterator iter = pending_connects_.begin();
       iter != pending_connects_.end();
       ++iter) {
    OnConnect(iter->first, iter->second);
  }
  pending_connects_.clear();
}

void WebSharedWorkerStub::OnConnect(int sent_message_port_id, int routing_id) {
  if (started_) {
    blink::WebMessagePortChannel* channel =
        new WebMessagePortChannelImpl(routing_id,
                                      sent_message_port_id,
                                      base::MessageLoopProxy::current().get());
    impl_->connect(channel);
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // WorkerMsg_Connect for one of the documents can come in before the
    // worker is started. Just queue up the connect and deliver it once the
    // worker starts.
    PendingConnectInfo pending_connect(sent_message_port_id, routing_id);
    pending_connects_.push_back(pending_connect);
  }
}

void WebSharedWorkerStub::OnTerminateWorkerContext() {
  impl_->terminateWorkerContext();

  // Call the client to make sure context exits.
  EnsureWorkerContextTerminates();
  started_ = false;
}

}  // namespace content
