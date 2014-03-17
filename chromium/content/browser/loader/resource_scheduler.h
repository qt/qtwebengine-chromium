// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"

namespace net {
class HostPortPair;
class URLRequest;
}

namespace content {
class ResourceThrottle;

// There is one ResourceScheduler. All renderer-initiated HTTP requests are
// expected to pass through it.
//
// There are two types of input to the scheduler:
// 1. Requests to start, cancel, or finish fetching a resource.
// 2. Notifications for renderer events, such as new tabs, navigation and
//    painting.
//
// These input come from different threads, so they may not be in sync. The UI
// thread is considered the authority on renderer lifetime, which means some
// IPCs may be meaningless if they arrive after the UI thread signals a renderer
// has been deleted.
//
// The ResourceScheduler tracks many Clients, which should correlate with tabs.
// A client is uniquely identified by its child_id and route_id.
//
// Each Client may have many Requests in flight. Requests are uniquely
// identified within a Client by its ScheduledResourceRequest.
//
// Users should call ScheduleRequest() to notify this ResourceScheduler of a
// new request. The returned ResourceThrottle should be destroyed when the load
// finishes or is canceled.
//
// The scheduler may defer issuing the request via the ResourceThrottle
// interface or it may alter the request's priority by calling set_priority() on
// the URLRequest.
class CONTENT_EXPORT ResourceScheduler : public base::NonThreadSafe {
 public:
  ResourceScheduler();
  ~ResourceScheduler();

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled.
  scoped_ptr<ResourceThrottle> ScheduleRequest(
      int child_id, int route_id, net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created.
  void OnClientCreated(int child_id, int route_id);

  // Called when a renderer is destroyed.
  void OnClientDeleted(int child_id, int route_id);

  // Signals from IPC messages directly from the renderers:

  // Called when a client navigates to a new main document.
  void OnNavigate(int child_id, int route_id);

  // Called when the client has parsed the <body> element. This is a signal that
  // resource loads won't interfere with first paint.
  void OnWillInsertBody(int child_id, int route_id);

 private:
  class RequestQueue;
  class ScheduledResourceRequest;
  struct Client;

  typedef int64 ClientId;
  typedef std::map<ClientId, Client*> ClientMap;
  typedef std::set<ScheduledResourceRequest*> RequestSet;

  // Called when a ScheduledResourceRequest is destroyed.
  void RemoveRequest(ScheduledResourceRequest* request);

  // Unthrottles the |request| and adds it to |client|.
  void StartRequest(ScheduledResourceRequest* request, Client* client);

  // Update the queue position for |request|, possibly causing it to start
  // loading.
  //
  // Queues are maintained for each priority level. When |request| is
  // reprioritized, it will move to the end of the queue for that priority
  // level.
  void ReprioritizeRequest(ScheduledResourceRequest* request,
                           net::RequestPriority new_priority);

  // Attempts to load any pending requests in |client|, based on the
  // results of ShouldStartRequest().
  void LoadAnyStartablePendingRequests(Client* client);

  // Returns the number of requests with priority < LOW that are currently in
  // flight.
  void GetNumDelayableRequestsInFlight(
      Client* client,
      const net::HostPortPair& active_request_host,
      size_t* total_delayable,
      size_t* total_for_active_host) const;

  enum ShouldStartReqResult {
    DO_NOT_START_REQUEST_AND_STOP_SEARCHING = -2,
    DO_NOT_START_REQUEST_AND_KEEP_SEARCHING = -1,
    START_REQUEST = 1,
  };

  // Returns true if the request should start. This is the core scheduling
  // algorithm.
  ShouldStartReqResult ShouldStartRequest(ScheduledResourceRequest* request,
                                          Client* client) const;

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  ClientMap client_map_;
  RequestSet unowned_requests_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
