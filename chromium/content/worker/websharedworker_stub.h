// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
#define CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_

#include "base/memory/scoped_ptr.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/worker/websharedworkerclient_proxy.h"
#include "content/worker/worker_webapplicationcachehost_impl.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebSharedWorker.h"
#include "url/gurl.h"

namespace blink {
class WebSharedWorker;
}

namespace content {

class SharedWorkerDevToolsAgent;

// This class creates a WebSharedWorker, and translates incoming IPCs to the
// appropriate WebSharedWorker APIs.
class WebSharedWorkerStub : public IPC::Listener {
 public:
  WebSharedWorkerStub(const base::string16& name, int route_id,
                      const WorkerAppCacheInitInfo& appcache_init_info);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Invoked when the WebSharedWorkerClientProxy is shutting down.
  void Shutdown();

  // Called after terminating the worker context to make sure that the worker
  // actually terminates (is not stuck in an infinite loop).
  void EnsureWorkerContextTerminates();

  WebSharedWorkerClientProxy* client() { return &client_; }

  const WorkerAppCacheInitInfo& appcache_init_info() const {
    return appcache_init_info_;
  }

  // Returns the script url of this worker.
  const GURL& url();


 private:
  virtual ~WebSharedWorkerStub();

  void OnConnect(int sent_message_port_id, int routing_id);
  void OnStartWorkerContext(
      const GURL& url, const base::string16& user_agent,
      const base::string16& source_code,
      const base::string16& content_security_policy,
      blink::WebContentSecurityPolicyType policy_type);

  void OnTerminateWorkerContext();

  ScopedChildProcessReference process_ref_;

  int route_id_;
  WorkerAppCacheInitInfo appcache_init_info_;

  // WebSharedWorkerClient that responds to outgoing API calls
  // from the worker object.
  WebSharedWorkerClientProxy client_;

  blink::WebSharedWorker* impl_;
  base::string16 name_;
  bool started_;
  GURL url_;
  scoped_ptr<SharedWorkerDevToolsAgent> worker_devtools_agent_;

  typedef std::pair<int, int> PendingConnectInfo;
  typedef std::vector<PendingConnectInfo> PendingConnectInfoList;
  PendingConnectInfoList pending_connects_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerStub);
};

}  // namespace content

#endif  // CONTENT_WORKER_WEBSHAREDWORKER_STUB_H_
