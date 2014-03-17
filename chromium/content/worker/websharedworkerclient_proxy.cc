// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/websharedworkerclient_proxy.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/worker/shared_worker_devtools_agent.h"
#include "content/worker/shared_worker_permission_client_proxy.h"
#include "content/worker/websharedworker_stub.h"
#include "content/worker/worker_thread.h"
#include "content/worker/worker_webapplicationcachehost_impl.h"
#include "ipc/ipc_logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

using blink::WebApplicationCacheHost;
using blink::WebFrame;
using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebWorker;
using blink::WebSharedWorkerClient;

namespace content {

// How long to wait for worker to finish after it's been told to terminate.
#define kMaxTimeForRunawayWorkerSeconds 3

WebSharedWorkerClientProxy::WebSharedWorkerClientProxy(
    int route_id, WebSharedWorkerStub* stub)
    : route_id_(route_id),
      appcache_host_id_(0),
      stub_(stub),
      weak_factory_(this),
      devtools_agent_(NULL) {
}

WebSharedWorkerClientProxy::~WebSharedWorkerClientProxy() {
}

void WebSharedWorkerClientProxy::workerContextClosed() {
  Send(new WorkerHostMsg_WorkerContextClosed(route_id_));
}

void WebSharedWorkerClientProxy::workerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
  // Tell the stub that the worker has shutdown - frees this object.
  if (stub_)
    stub_->Shutdown();
}

blink::WebNotificationPresenter*
WebSharedWorkerClientProxy::notificationPresenter() {
  // TODO(johnnyg): Notifications are not yet hooked up to workers.
  // Coming soon.
  NOTREACHED();
  return NULL;
}

WebApplicationCacheHost* WebSharedWorkerClientProxy::createApplicationCacheHost(
    blink::WebApplicationCacheHostClient* client) {
  WorkerWebApplicationCacheHostImpl* host =
      new WorkerWebApplicationCacheHostImpl(stub_->appcache_init_info(),
                                            client);
  // Remember the id of the instance we create so we have access to that
  // value when creating nested dedicated workers in createWorker.
  appcache_host_id_ = host->host_id();
  return host;
}

blink::WebWorkerPermissionClientProxy*
WebSharedWorkerClientProxy::createWorkerPermissionClientProxy(
    const blink::WebSecurityOrigin& origin) {
  return new SharedWorkerPermissionClientProxy(
      GURL(origin.toString()), origin.isUnique(), route_id_,
      ChildThread::current()->thread_safe_sender());
}

// TODO(kinuko): Deprecate these methods.
bool WebSharedWorkerClientProxy::allowDatabase(WebFrame* frame,
                                         const WebString& name,
                                         const WebString& display_name,
                                         unsigned long estimated_size) {
  return false;
}

bool WebSharedWorkerClientProxy::allowFileSystem() {
  return false;
}

bool WebSharedWorkerClientProxy::allowIndexedDB(const blink::WebString& name) {
  return false;
}

void WebSharedWorkerClientProxy::dispatchDevToolsMessage(
    const WebString& message) {
  if (devtools_agent_)
    devtools_agent_->SendDevToolsMessage(message);
}

void WebSharedWorkerClientProxy::saveDevToolsAgentState(
    const blink::WebString& state) {
  if (devtools_agent_)
    devtools_agent_->SaveDevToolsAgentState(state);
}

bool WebSharedWorkerClientProxy::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}

void WebSharedWorkerClientProxy::EnsureWorkerContextTerminates() {
  // This shuts down the process cleanly from the perspective of the browser
  // process, and avoids the crashed worker infobar from appearing to the new
  // page. It's ok to post several of theese, because the first executed task
  // will exit the message loop and subsequent ones won't be executed.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebSharedWorkerClientProxy::workerContextDestroyed,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kMaxTimeForRunawayWorkerSeconds));
}

}  // namespace content
