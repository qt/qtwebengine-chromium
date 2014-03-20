// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBSHAREDWORKER_PROXY_H_
#define CONTENT_RENDERER_WEBSHAREDWORKER_PROXY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebSharedWorkerConnector.h"
#include "url/gurl.h"

namespace content {

class ChildThread;

// Implementation of the WebSharedWorker APIs. This object is intended to only
// live long enough to allow the caller to send a "connect" event to the worker
// thread. Once the connect event has been sent, all future communication will
// happen via the WebMessagePortChannel, and the WebSharedWorker instance will
// be freed.
class WebSharedWorkerProxy : public blink::WebSharedWorkerConnector,
                             private IPC::Listener {
 public:
  // If the worker not loaded yet, route_id == MSG_ROUTING_NONE
  WebSharedWorkerProxy(ChildThread* child_thread,
                       unsigned long long document_id,
                       bool exists,
                       int route_id,
                       int render_view_route_id);
  virtual ~WebSharedWorkerProxy();

  // Implementations of WebSharedWorker APIs
  virtual bool isStarted();
  virtual void connect(blink::WebMessagePortChannel* channel,
                       ConnectListener* listener);

  virtual void startWorkerContext(
      const blink::WebURL& script_url,
      const blink::WebString& name,
      const blink::WebString& user_agent,
      const blink::WebString& source_code,
      const blink::WebString& content_security_policy,
      blink::WebContentSecurityPolicyType policy_type,
      long long script_resource_appcache_id);

 private:
  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Returns true if the worker is running (can send messages to it).
  bool IsStarted();

  // Disconnects the worker (stops listening for incoming messages).
  void Disconnect();

  // Sends a message to the worker thread (forwarded via the RenderViewHost).
  // If WorkerStarted() has not yet been called, message is queued.
  bool Send(IPC::Message*);

  // Returns true if there are queued messages.
  bool HasQueuedMessages() { return !queued_messages_.empty(); }

  // Sends any messages currently in the queue.
  void SendQueuedMessages();

  void CreateWorkerContext(const GURL& script_url,
                           bool is_shared,
                           const base::string16& name,
                           const base::string16& user_agent,
                           const base::string16& source_code,
                           const base::string16& content_security_policy,
                           blink::WebContentSecurityPolicyType policy_type,
                           int pending_route_id,
                           int64 script_resource_appcache_id);
  void OnWorkerCreated();


  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  int route_id_;

  // The routing id for the RenderView that created this worker.
  int render_view_route_id_;

  ChildThread* child_thread_;

  // ID of our parent document (used to shutdown workers when the parent
  // document is detached).
  unsigned long long document_id_;

  // Stores messages that were sent before the StartWorkerContext message.
  std::vector<IPC::Message*> queued_messages_;

  // The id for the placeholder worker instance we've stored on the
  // browser process (we need to pass this same route id back in when creating
  // the worker).
  int pending_route_id_;
  ConnectListener* connect_listener_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBSHAREDWORKER_PROXY_H_
