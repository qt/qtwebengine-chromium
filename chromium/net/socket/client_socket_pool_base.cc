// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/client_socket_handle.h"

using base::TimeDelta;

namespace net {

namespace {

// Indicate whether we should enable idle socket cleanup timer. When timer is
// disabled, sockets are closed next time a socket request is made.
bool g_cleanup_timer_enabled = true;

// The timeout value, in seconds, used to clean up idle sockets that can't be
// reused.
//
// Note: It's important to close idle sockets that have received data as soon
// as possible because the received data may cause BSOD on Windows XP under
// some conditions.  See http://crbug.com/4606.
const int kCleanupInterval = 10;  // DO NOT INCREASE THIS TIMEOUT.

// Indicate whether or not we should establish a new transport layer connection
// after a certain timeout has passed without receiving an ACK.
bool g_connect_backup_jobs_enabled = true;

// Compares the effective priority of two results, and returns 1 if |request1|
// has greater effective priority than |request2|, 0 if they have the same
// effective priority, and -1 if |request2| has the greater effective priority.
// Requests with |ignore_limits| set have higher effective priority than those
// without.  If both requests have |ignore_limits| set/unset, then the request
// with the highest Pririoty has the highest effective priority.  Does not take
// into account the fact that Requests are serviced in FIFO order if they would
// otherwise have the same priority.
int CompareEffectiveRequestPriority(
    const internal::ClientSocketPoolBaseHelper::Request& request1,
    const internal::ClientSocketPoolBaseHelper::Request& request2) {
  if (request1.ignore_limits() && !request2.ignore_limits())
    return 1;
  if (!request1.ignore_limits() && request2.ignore_limits())
    return -1;
  if (request1.priority() > request2.priority())
    return 1;
  if (request1.priority() < request2.priority())
    return -1;
  return 0;
}

}  // namespace

ConnectJob::ConnectJob(const std::string& group_name,
                       base::TimeDelta timeout_duration,
                       Delegate* delegate,
                       const BoundNetLog& net_log)
    : group_name_(group_name),
      timeout_duration_(timeout_duration),
      delegate_(delegate),
      net_log_(net_log),
      idle_(true) {
  DCHECK(!group_name.empty());
  DCHECK(delegate);
  net_log.BeginEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                     NetLog::StringCallback("group_name", &group_name_));
}

ConnectJob::~ConnectJob() {
  net_log().EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB);
}

int ConnectJob::Connect() {
  if (timeout_duration_ != base::TimeDelta())
    timer_.Start(FROM_HERE, timeout_duration_, this, &ConnectJob::OnTimeout);

  idle_ = false;

  LogConnectStart();

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    delegate_ = NULL;
  }

  return rv;
}

void ConnectJob::set_socket(StreamSocket* socket) {
  if (socket) {
    net_log().AddEvent(NetLog::TYPE_CONNECT_JOB_SET_SOCKET,
                       socket->NetLog().source().ToEventParametersCallback());
  }
  socket_.reset(socket);
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  // The delegate will delete |this|.
  Delegate* delegate = delegate_;
  delegate_ = NULL;

  LogConnectCompletion(rv);
  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::ResetTimer(base::TimeDelta remaining_time) {
  timer_.Stop();
  timer_.Start(FROM_HERE, remaining_time, this, &ConnectJob::OnTimeout);
}

void ConnectJob::LogConnectStart() {
  connect_timing_.connect_start = base::TimeTicks::Now();
  net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT);
}

void ConnectJob::LogConnectCompletion(int net_error) {
  connect_timing_.connect_end = base::TimeTicks::Now();
  net_log().EndEventWithNetErrorCode(
      NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT, net_error);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  set_socket(NULL);

  net_log_.AddEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_TIMED_OUT);

  NotifyDelegateOfCompletion(ERR_TIMED_OUT);
}

namespace internal {

ClientSocketPoolBaseHelper::Request::Request(
    ClientSocketHandle* handle,
    const CompletionCallback& callback,
    RequestPriority priority,
    bool ignore_limits,
    Flags flags,
    const BoundNetLog& net_log)
    : handle_(handle),
      callback_(callback),
      priority_(priority),
      ignore_limits_(ignore_limits),
      flags_(flags),
      net_log_(net_log) {}

ClientSocketPoolBaseHelper::Request::~Request() {}

ClientSocketPoolBaseHelper::ClientSocketPoolBaseHelper(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    ConnectJobFactory* connect_job_factory)
    : idle_socket_count_(0),
      connecting_socket_count_(0),
      handed_out_socket_count_(0),
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      use_cleanup_timer_(g_cleanup_timer_enabled),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      connect_job_factory_(connect_job_factory),
      connect_backup_jobs_enabled_(false),
      pool_generation_number_(0),
      weak_factory_(this) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  NetworkChangeNotifier::AddIPAddressObserver(this);
}

ClientSocketPoolBaseHelper::~ClientSocketPoolBaseHelper() {
  // Clean up any idle sockets and pending connect jobs.  Assert that we have no
  // remaining active sockets or pending requests.  They should have all been
  // cleaned up prior to |this| being destroyed.
  FlushWithError(ERR_ABORTED);
  DCHECK(group_map_.empty());
  DCHECK(pending_callback_map_.empty());
  DCHECK_EQ(0, connecting_socket_count_);
  CHECK(higher_layer_pools_.empty());

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

ClientSocketPoolBaseHelper::CallbackResultPair::CallbackResultPair()
    : result(OK) {
}

ClientSocketPoolBaseHelper::CallbackResultPair::CallbackResultPair(
    const CompletionCallback& callback_in, int result_in)
    : callback(callback_in),
      result(result_in) {
}

ClientSocketPoolBaseHelper::CallbackResultPair::~CallbackResultPair() {}

// static
void ClientSocketPoolBaseHelper::InsertRequestIntoQueue(
    const Request* r, RequestQueue* pending_requests) {
  RequestQueue::iterator it = pending_requests->begin();
  // TODO(mmenke):  Should the network stack require requests with
  //                |ignore_limits| have the highest priority?
  while (it != pending_requests->end() &&
         CompareEffectiveRequestPriority(*r, *(*it)) <= 0) {
    ++it;
  }
  pending_requests->insert(it, r);
}

// static
const ClientSocketPoolBaseHelper::Request*
ClientSocketPoolBaseHelper::RemoveRequestFromQueue(
    const RequestQueue::iterator& it, Group* group) {
  const Request* req = *it;
  group->mutable_pending_requests()->erase(it);
  // If there are no more requests, we kill the backup timer.
  if (group->pending_requests().empty())
    group->CleanupBackupJob();
  return req;
}

void ClientSocketPoolBaseHelper::AddLayeredPool(LayeredPool* pool) {
  CHECK(pool);
  CHECK(!ContainsKey(higher_layer_pools_, pool));
  higher_layer_pools_.insert(pool);
}

void ClientSocketPoolBaseHelper::RemoveLayeredPool(LayeredPool* pool) {
  CHECK(pool);
  CHECK(ContainsKey(higher_layer_pools_, pool));
  higher_layer_pools_.erase(pool);
}

int ClientSocketPoolBaseHelper::RequestSocket(
    const std::string& group_name,
    const Request* request) {
  CHECK(!request->callback().is_null());
  CHECK(request->handle());

  // Cleanup any timed-out idle sockets if no timer is used.
  if (!use_cleanup_timer_)
    CleanupIdleSockets(false);

  request->net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL);
  Group* group = GetOrCreateGroup(group_name);

  int rv = RequestSocketInternal(group_name, request);
  if (rv != ERR_IO_PENDING) {
    request->net_log().EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, rv);
    CHECK(!request->handle()->is_initialized());
    delete request;
  } else {
    InsertRequestIntoQueue(request, group->mutable_pending_requests());
    // Have to do this asynchronously, as closing sockets in higher level pools
    // call back in to |this|, which will cause all sorts of fun and exciting
    // re-entrancy issues if the socket pool is doing something else at the
    // time.
    if (group->IsStalledOnPoolMaxSockets(max_sockets_per_group_)) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(
              &ClientSocketPoolBaseHelper::TryToCloseSocketsInLayeredPools,
              weak_factory_.GetWeakPtr()));
    }
  }
  return rv;
}

void ClientSocketPoolBaseHelper::RequestSockets(
    const std::string& group_name,
    const Request& request,
    int num_sockets) {
  DCHECK(request.callback().is_null());
  DCHECK(!request.handle());

  // Cleanup any timed out idle sockets if no timer is used.
  if (!use_cleanup_timer_)
    CleanupIdleSockets(false);

  if (num_sockets > max_sockets_per_group_) {
    num_sockets = max_sockets_per_group_;
  }

  request.net_log().BeginEvent(
      NetLog::TYPE_SOCKET_POOL_CONNECTING_N_SOCKETS,
      NetLog::IntegerCallback("num_sockets", num_sockets));

  Group* group = GetOrCreateGroup(group_name);

  // RequestSocketsInternal() may delete the group.
  bool deleted_group = false;

  int rv = OK;
  for (int num_iterations_left = num_sockets;
       group->NumActiveSocketSlots() < num_sockets &&
       num_iterations_left > 0 ; num_iterations_left--) {
    rv = RequestSocketInternal(group_name, &request);
    if (rv < 0 && rv != ERR_IO_PENDING) {
      // We're encountering a synchronous error.  Give up.
      if (!ContainsKey(group_map_, group_name))
        deleted_group = true;
      break;
    }
    if (!ContainsKey(group_map_, group_name)) {
      // Unexpected.  The group should only be getting deleted on synchronous
      // error.
      NOTREACHED();
      deleted_group = true;
      break;
    }
  }

  if (!deleted_group && group->IsEmpty())
    RemoveGroup(group_name);

  if (rv == ERR_IO_PENDING)
    rv = OK;
  request.net_log().EndEventWithNetErrorCode(
      NetLog::TYPE_SOCKET_POOL_CONNECTING_N_SOCKETS, rv);
}

int ClientSocketPoolBaseHelper::RequestSocketInternal(
    const std::string& group_name,
    const Request* request) {
  ClientSocketHandle* const handle = request->handle();
  const bool preconnecting = !handle;
  Group* group = GetOrCreateGroup(group_name);

  if (!(request->flags() & NO_IDLE_SOCKETS)) {
    // Try to reuse a socket.
    if (AssignIdleSocketToRequest(request, group))
      return OK;
  }

  // If there are more ConnectJobs than pending requests, don't need to do
  // anything.  Can just wait for the extra job to connect, and then assign it
  // to the request.
  if (!preconnecting && group->TryToUseUnassignedConnectJob())
    return ERR_IO_PENDING;

  // Can we make another active socket now?
  if (!group->HasAvailableSocketSlot(max_sockets_per_group_) &&
      !request->ignore_limits()) {
    // TODO(willchan): Consider whether or not we need to close a socket in a
    // higher layered group. I don't think this makes sense since we would just
    // reuse that socket then if we needed one and wouldn't make it down to this
    // layer.
    request->net_log().AddEvent(
        NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP);
    return ERR_IO_PENDING;
  }

  if (ReachedMaxSocketsLimit() && !request->ignore_limits()) {
    // NOTE(mmenke):  Wonder if we really need different code for each case
    // here.  Only reason for them now seems to be preconnects.
    if (idle_socket_count() > 0) {
      // There's an idle socket in this pool. Either that's because there's
      // still one in this group, but we got here due to preconnecting bypassing
      // idle sockets, or because there's an idle socket in another group.
      bool closed = CloseOneIdleSocketExceptInGroup(group);
      if (preconnecting && !closed)
        return ERR_PRECONNECT_MAX_SOCKET_LIMIT;
    } else {
      // We could check if we really have a stalled group here, but it requires
      // a scan of all groups, so just flip a flag here, and do the check later.
      request->net_log().AddEvent(NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS);
      return ERR_IO_PENDING;
    }
  }

  // We couldn't find a socket to reuse, and there's space to allocate one,
  // so allocate and connect a new one.
  scoped_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, *request, this));

  int rv = connect_job->Connect();
  if (rv == OK) {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    if (!preconnecting) {
      HandOutSocket(connect_job->ReleaseSocket(), false /* not reused */,
                    connect_job->connect_timing(), handle, base::TimeDelta(),
                    group, request->net_log());
    } else {
      AddIdleSocket(connect_job->ReleaseSocket(), group);
    }
  } else if (rv == ERR_IO_PENDING) {
    // If we don't have any sockets in this group, set a timer for potentially
    // creating a new one.  If the SYN is lost, this backup socket may complete
    // before the slow socket, improving end user latency.
    if (connect_backup_jobs_enabled_ &&
        group->IsEmpty() && !group->HasBackupJob()) {
      group->StartBackupSocketTimer(group_name, this);
    }

    connecting_socket_count_++;

    group->AddJob(connect_job.release(), preconnecting);
  } else {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    StreamSocket* error_socket = NULL;
    if (!preconnecting) {
      DCHECK(handle);
      connect_job->GetAdditionalErrorState(handle);
      error_socket = connect_job->ReleaseSocket();
    }
    if (error_socket) {
      HandOutSocket(error_socket, false /* not reused */,
                    connect_job->connect_timing(), handle, base::TimeDelta(),
                    group, request->net_log());
    } else if (group->IsEmpty()) {
      RemoveGroup(group_name);
    }
  }

  return rv;
}

bool ClientSocketPoolBaseHelper::AssignIdleSocketToRequest(
    const Request* request, Group* group) {
  std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();
  std::list<IdleSocket>::iterator idle_socket_it = idle_sockets->end();

  // Iterate through the idle sockets forwards (oldest to newest)
  //   * Delete any disconnected ones.
  //   * If we find a used idle socket, assign to |idle_socket|.  At the end,
  //   the |idle_socket_it| will be set to the newest used idle socket.
  for (std::list<IdleSocket>::iterator it = idle_sockets->begin();
       it != idle_sockets->end();) {
    if (!it->socket->IsConnectedAndIdle()) {
      DecrementIdleCount();
      delete it->socket;
      it = idle_sockets->erase(it);
      continue;
    }

    if (it->socket->WasEverUsed()) {
      // We found one we can reuse!
      idle_socket_it = it;
    }

    ++it;
  }

  // If we haven't found an idle socket, that means there are no used idle
  // sockets.  Pick the oldest (first) idle socket (FIFO).

  if (idle_socket_it == idle_sockets->end() && !idle_sockets->empty())
    idle_socket_it = idle_sockets->begin();

  if (idle_socket_it != idle_sockets->end()) {
    DecrementIdleCount();
    base::TimeDelta idle_time =
        base::TimeTicks::Now() - idle_socket_it->start_time;
    IdleSocket idle_socket = *idle_socket_it;
    idle_sockets->erase(idle_socket_it);
    HandOutSocket(
        idle_socket.socket,
        idle_socket.socket->WasEverUsed(),
        LoadTimingInfo::ConnectTiming(),
        request->handle(),
        idle_time,
        group,
        request->net_log());
    return true;
  }

  return false;
}

// static
void ClientSocketPoolBaseHelper::LogBoundConnectJobToRequest(
    const NetLog::Source& connect_job_source, const Request* request) {
  request->net_log().AddEvent(NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
                              connect_job_source.ToEventParametersCallback());
}

void ClientSocketPoolBaseHelper::CancelRequest(
    const std::string& group_name, ClientSocketHandle* handle) {
  PendingCallbackMap::iterator callback_it = pending_callback_map_.find(handle);
  if (callback_it != pending_callback_map_.end()) {
    int result = callback_it->second.result;
    pending_callback_map_.erase(callback_it);
    StreamSocket* socket = handle->release_socket();
    if (socket) {
      if (result != OK)
        socket->Disconnect();
      ReleaseSocket(handle->group_name(), socket, handle->id());
    }
    return;
  }

  CHECK(ContainsKey(group_map_, group_name));

  Group* group = GetOrCreateGroup(group_name);

  // Search pending_requests for matching handle.
  RequestQueue::iterator it = group->mutable_pending_requests()->begin();
  for (; it != group->pending_requests().end(); ++it) {
    if ((*it)->handle() == handle) {
      scoped_ptr<const Request> req(RemoveRequestFromQueue(it, group));
      req->net_log().AddEvent(NetLog::TYPE_CANCELLED);
      req->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);

      // We let the job run, unless we're at the socket limit and there is
      // not another request waiting on the job.
      if (group->jobs().size() > group->pending_requests().size() &&
          ReachedMaxSocketsLimit()) {
        RemoveConnectJob(*group->jobs().begin(), group);
        CheckForStalledSocketGroups();
      }
      break;
    }
  }
}

bool ClientSocketPoolBaseHelper::HasGroup(const std::string& group_name) const {
  return ContainsKey(group_map_, group_name);
}

void ClientSocketPoolBaseHelper::CloseIdleSockets() {
  CleanupIdleSockets(true);
  DCHECK_EQ(0, idle_socket_count_);
}

int ClientSocketPoolBaseHelper::IdleSocketCountInGroup(
    const std::string& group_name) const {
  GroupMap::const_iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  return i->second->idle_sockets().size();
}

LoadState ClientSocketPoolBaseHelper::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (ContainsKey(pending_callback_map_, handle))
    return LOAD_STATE_CONNECTING;

  if (!ContainsKey(group_map_, group_name)) {
    NOTREACHED() << "ClientSocketPool does not contain group: " << group_name
                 << " for handle: " << handle;
    return LOAD_STATE_IDLE;
  }

  // Can't use operator[] since it is non-const.
  const Group& group = *group_map_.find(group_name)->second;

  // Search the first group.jobs().size() |pending_requests| for |handle|.
  // If it's farther back in the deque than that, it doesn't have a
  // corresponding ConnectJob.
  size_t connect_jobs = group.jobs().size();
  RequestQueue::const_iterator it = group.pending_requests().begin();
  for (size_t i = 0; it != group.pending_requests().end() && i < connect_jobs;
       ++it, ++i) {
    if ((*it)->handle() != handle)
      continue;

    // Just return the state  of the farthest along ConnectJob for the first
    // group.jobs().size() pending requests.
    LoadState max_state = LOAD_STATE_IDLE;
    for (ConnectJobSet::const_iterator job_it = group.jobs().begin();
         job_it != group.jobs().end(); ++job_it) {
      max_state = std::max(max_state, (*job_it)->GetLoadState());
    }
    return max_state;
  }

  if (group.IsStalledOnPoolMaxSockets(max_sockets_per_group_))
    return LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL;
  return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
}

base::DictionaryValue* ClientSocketPoolBaseHelper::GetInfoAsValue(
    const std::string& name, const std::string& type) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("name", name);
  dict->SetString("type", type);
  dict->SetInteger("handed_out_socket_count", handed_out_socket_count_);
  dict->SetInteger("connecting_socket_count", connecting_socket_count_);
  dict->SetInteger("idle_socket_count", idle_socket_count_);
  dict->SetInteger("max_socket_count", max_sockets_);
  dict->SetInteger("max_sockets_per_group", max_sockets_per_group_);
  dict->SetInteger("pool_generation_number", pool_generation_number_);

  if (group_map_.empty())
    return dict;

  base::DictionaryValue* all_groups_dict = new base::DictionaryValue();
  for (GroupMap::const_iterator it = group_map_.begin();
       it != group_map_.end(); it++) {
    const Group* group = it->second;
    base::DictionaryValue* group_dict = new base::DictionaryValue();

    group_dict->SetInteger("pending_request_count",
                           group->pending_requests().size());
    if (!group->pending_requests().empty()) {
      group_dict->SetInteger("top_pending_priority",
                             group->TopPendingPriority());
    }

    group_dict->SetInteger("active_socket_count", group->active_socket_count());

    base::ListValue* idle_socket_list = new base::ListValue();
    std::list<IdleSocket>::const_iterator idle_socket;
    for (idle_socket = group->idle_sockets().begin();
         idle_socket != group->idle_sockets().end();
         idle_socket++) {
      int source_id = idle_socket->socket->NetLog().source().id;
      idle_socket_list->Append(new base::FundamentalValue(source_id));
    }
    group_dict->Set("idle_sockets", idle_socket_list);

    base::ListValue* connect_jobs_list = new base::ListValue();
    std::set<ConnectJob*>::const_iterator job = group->jobs().begin();
    for (job = group->jobs().begin(); job != group->jobs().end(); job++) {
      int source_id = (*job)->net_log().source().id;
      connect_jobs_list->Append(new base::FundamentalValue(source_id));
    }
    group_dict->Set("connect_jobs", connect_jobs_list);

    group_dict->SetBoolean("is_stalled",
                           group->IsStalledOnPoolMaxSockets(
                               max_sockets_per_group_));
    group_dict->SetBoolean("has_backup_job", group->HasBackupJob());

    all_groups_dict->SetWithoutPathExpansion(it->first, group_dict);
  }
  dict->Set("groups", all_groups_dict);
  return dict;
}

bool ClientSocketPoolBaseHelper::IdleSocket::ShouldCleanup(
    base::TimeTicks now,
    base::TimeDelta timeout) const {
  bool timed_out = (now - start_time) >= timeout;
  if (timed_out)
    return true;
  if (socket->WasEverUsed())
    return !socket->IsConnectedAndIdle();
  return !socket->IsConnected();
}

void ClientSocketPoolBaseHelper::CleanupIdleSockets(bool force) {
  if (idle_socket_count_ == 0)
    return;

  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  base::TimeTicks now = base::TimeTicks::Now();

  GroupMap::iterator i = group_map_.begin();
  while (i != group_map_.end()) {
    Group* group = i->second;

    std::list<IdleSocket>::iterator j = group->mutable_idle_sockets()->begin();
    while (j != group->idle_sockets().end()) {
      base::TimeDelta timeout =
          j->socket->WasEverUsed() ?
          used_idle_socket_timeout_ : unused_idle_socket_timeout_;
      if (force || j->ShouldCleanup(now, timeout)) {
        delete j->socket;
        j = group->mutable_idle_sockets()->erase(j);
        DecrementIdleCount();
      } else {
        ++j;
      }
    }

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      RemoveGroup(i++);
    } else {
      ++i;
    }
  }
}

ClientSocketPoolBaseHelper::Group* ClientSocketPoolBaseHelper::GetOrCreateGroup(
    const std::string& group_name) {
  GroupMap::iterator it = group_map_.find(group_name);
  if (it != group_map_.end())
    return it->second;
  Group* group = new Group;
  group_map_[group_name] = group;
  return group;
}

void ClientSocketPoolBaseHelper::RemoveGroup(const std::string& group_name) {
  GroupMap::iterator it = group_map_.find(group_name);
  CHECK(it != group_map_.end());

  RemoveGroup(it);
}

void ClientSocketPoolBaseHelper::RemoveGroup(GroupMap::iterator it) {
  delete it->second;
  group_map_.erase(it);
}

// static
bool ClientSocketPoolBaseHelper::connect_backup_jobs_enabled() {
  return g_connect_backup_jobs_enabled;
}

// static
bool ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(bool enabled) {
  bool old_value = g_connect_backup_jobs_enabled;
  g_connect_backup_jobs_enabled = enabled;
  return old_value;
}

void ClientSocketPoolBaseHelper::EnableConnectBackupJobs() {
  connect_backup_jobs_enabled_ = g_connect_backup_jobs_enabled;
}

void ClientSocketPoolBaseHelper::IncrementIdleCount() {
  if (++idle_socket_count_ == 1 && use_cleanup_timer_)
    StartIdleSocketTimer();
}

void ClientSocketPoolBaseHelper::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

// static
bool ClientSocketPoolBaseHelper::cleanup_timer_enabled() {
  return g_cleanup_timer_enabled;
}

// static
bool ClientSocketPoolBaseHelper::set_cleanup_timer_enabled(bool enabled) {
  bool old_value = g_cleanup_timer_enabled;
  g_cleanup_timer_enabled = enabled;
  return old_value;
}

void ClientSocketPoolBaseHelper::StartIdleSocketTimer() {
  timer_.Start(FROM_HERE, TimeDelta::FromSeconds(kCleanupInterval), this,
               &ClientSocketPoolBaseHelper::OnCleanupTimerFired);
}

void ClientSocketPoolBaseHelper::ReleaseSocket(const std::string& group_name,
                                               StreamSocket* socket,
                                               int id) {
  GroupMap::iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  Group* group = i->second;

  CHECK_GT(handed_out_socket_count_, 0);
  handed_out_socket_count_--;

  CHECK_GT(group->active_socket_count(), 0);
  group->DecrementActiveSocketCount();

  const bool can_reuse = socket->IsConnectedAndIdle() &&
      id == pool_generation_number_;
  if (can_reuse) {
    // Add it to the idle list.
    AddIdleSocket(socket, group);
    OnAvailableSocketSlot(group_name, group);
  } else {
    delete socket;
  }

  CheckForStalledSocketGroups();
}

void ClientSocketPoolBaseHelper::CheckForStalledSocketGroups() {
  // If we have idle sockets, see if we can give one to the top-stalled group.
  std::string top_group_name;
  Group* top_group = NULL;
  if (!FindTopStalledGroup(&top_group, &top_group_name))
    return;

  if (ReachedMaxSocketsLimit()) {
    if (idle_socket_count() > 0) {
      CloseOneIdleSocket();
    } else {
      // We can't activate more sockets since we're already at our global
      // limit.
      return;
    }
  }

  // Note:  we don't loop on waking stalled groups.  If the stalled group is at
  //        its limit, may be left with other stalled groups that could be
  //        woken.  This isn't optimal, but there is no starvation, so to avoid
  //        the looping we leave it at this.
  OnAvailableSocketSlot(top_group_name, top_group);
}

// Search for the highest priority pending request, amongst the groups that
// are not at the |max_sockets_per_group_| limit. Note: for requests with
// the same priority, the winner is based on group hash ordering (and not
// insertion order).
bool ClientSocketPoolBaseHelper::FindTopStalledGroup(
    Group** group,
    std::string* group_name) const {
  CHECK((group && group_name) || (!group && !group_name));
  Group* top_group = NULL;
  const std::string* top_group_name = NULL;
  bool has_stalled_group = false;
  for (GroupMap::const_iterator i = group_map_.begin();
       i != group_map_.end(); ++i) {
    Group* curr_group = i->second;
    const RequestQueue& queue = curr_group->pending_requests();
    if (queue.empty())
      continue;
    if (curr_group->IsStalledOnPoolMaxSockets(max_sockets_per_group_)) {
      if (!group)
        return true;
      has_stalled_group = true;
      bool has_higher_priority = !top_group ||
          curr_group->TopPendingPriority() > top_group->TopPendingPriority();
      if (has_higher_priority) {
        top_group = curr_group;
        top_group_name = &i->first;
      }
    }
  }

  if (top_group) {
    CHECK(group);
    *group = top_group;
    *group_name = *top_group_name;
  } else {
    CHECK(!has_stalled_group);
  }
  return has_stalled_group;
}

void ClientSocketPoolBaseHelper::OnConnectJobComplete(
    int result, ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  GroupMap::iterator group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group* group = group_it->second;

  scoped_ptr<StreamSocket> socket(job->ReleaseSocket());

  // Copies of these are needed because |job| may be deleted before they are
  // accessed.
  BoundNetLog job_log = job->net_log();
  LoadTimingInfo::ConnectTiming connect_timing = job->connect_timing();

  if (result == OK) {
    DCHECK(socket.get());
    RemoveConnectJob(job, group);
    if (!group->pending_requests().empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group->mutable_pending_requests()->begin(), group));
      LogBoundConnectJobToRequest(job_log.source(), r.get());
      HandOutSocket(
          socket.release(), false /* unused socket */, connect_timing,
          r->handle(), base::TimeDelta(), group, r->net_log());
      r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);
      InvokeUserCallbackLater(r->handle(), r->callback(), result);
    } else {
      AddIdleSocket(socket.release(), group);
      OnAvailableSocketSlot(group_name, group);
      CheckForStalledSocketGroups();
    }
  } else {
    // If we got a socket, it must contain error information so pass that
    // up so that the caller can retrieve it.
    bool handed_out_socket = false;
    if (!group->pending_requests().empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group->mutable_pending_requests()->begin(), group));
      LogBoundConnectJobToRequest(job_log.source(), r.get());
      job->GetAdditionalErrorState(r->handle());
      RemoveConnectJob(job, group);
      if (socket.get()) {
        handed_out_socket = true;
        HandOutSocket(socket.release(), false /* unused socket */,
                      connect_timing, r->handle(), base::TimeDelta(), group,
                      r->net_log());
      }
      r->net_log().EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, result);
      InvokeUserCallbackLater(r->handle(), r->callback(), result);
    } else {
      RemoveConnectJob(job, group);
    }
    if (!handed_out_socket) {
      OnAvailableSocketSlot(group_name, group);
      CheckForStalledSocketGroups();
    }
  }
}

void ClientSocketPoolBaseHelper::OnIPAddressChanged() {
  FlushWithError(ERR_NETWORK_CHANGED);
}

void ClientSocketPoolBaseHelper::FlushWithError(int error) {
  pool_generation_number_++;
  CancelAllConnectJobs();
  CloseIdleSockets();
  CancelAllRequestsWithError(error);
}

bool ClientSocketPoolBaseHelper::IsStalled() const {
  // If we are not using |max_sockets_|, then clearly we are not stalled
  if ((handed_out_socket_count_ + connecting_socket_count_) < max_sockets_)
    return false;
  // So in order to be stalled we need to be using |max_sockets_| AND
  // we need to have a request that is actually stalled on the global
  // socket limit.  To find such a request, we look for a group that
  // a has more requests that jobs AND where the number of jobs is less
  // than |max_sockets_per_group_|.  (If the number of jobs is equal to
  // |max_sockets_per_group_|, then the request is stalled on the group,
  // which does not count.)
  for (GroupMap::const_iterator it = group_map_.begin();
       it != group_map_.end(); ++it) {
    if (it->second->IsStalledOnPoolMaxSockets(max_sockets_per_group_))
      return true;
  }
  return false;
}

void ClientSocketPoolBaseHelper::RemoveConnectJob(ConnectJob* job,
                                                  Group* group) {
  CHECK_GT(connecting_socket_count_, 0);
  connecting_socket_count_--;

  DCHECK(group);
  DCHECK(ContainsKey(group->jobs(), job));
  group->RemoveJob(job);

  // If we've got no more jobs for this group, then we no longer need a
  // backup job either.
  if (group->jobs().empty())
    group->CleanupBackupJob();

  DCHECK(job);
  delete job;
}

void ClientSocketPoolBaseHelper::OnAvailableSocketSlot(
    const std::string& group_name, Group* group) {
  DCHECK(ContainsKey(group_map_, group_name));
  if (group->IsEmpty())
    RemoveGroup(group_name);
  else if (!group->pending_requests().empty())
    ProcessPendingRequest(group_name, group);
}

void ClientSocketPoolBaseHelper::ProcessPendingRequest(
    const std::string& group_name, Group* group) {
  int rv = RequestSocketInternal(group_name,
                                 *group->pending_requests().begin());
  if (rv != ERR_IO_PENDING) {
    scoped_ptr<const Request> request(RemoveRequestFromQueue(
          group->mutable_pending_requests()->begin(), group));
    if (group->IsEmpty())
      RemoveGroup(group_name);

    request->net_log().EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, rv);
    InvokeUserCallbackLater(request->handle(), request->callback(), rv);
  }
}

void ClientSocketPoolBaseHelper::HandOutSocket(
    StreamSocket* socket,
    bool reused,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    ClientSocketHandle* handle,
    base::TimeDelta idle_time,
    Group* group,
    const BoundNetLog& net_log) {
  DCHECK(socket);
  handle->set_socket(socket);
  handle->set_is_reused(reused);
  handle->set_idle_time(idle_time);
  handle->set_pool_id(pool_generation_number_);
  handle->set_connect_timing(connect_timing);

  if (reused) {
    net_log.AddEvent(
        NetLog::TYPE_SOCKET_POOL_REUSED_AN_EXISTING_SOCKET,
        NetLog::IntegerCallback(
            "idle_ms", static_cast<int>(idle_time.InMilliseconds())));
  }

  net_log.AddEvent(NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                   socket->NetLog().source().ToEventParametersCallback());

  handed_out_socket_count_++;
  group->IncrementActiveSocketCount();
}

void ClientSocketPoolBaseHelper::AddIdleSocket(
    StreamSocket* socket, Group* group) {
  DCHECK(socket);
  IdleSocket idle_socket;
  idle_socket.socket = socket;
  idle_socket.start_time = base::TimeTicks::Now();

  group->mutable_idle_sockets()->push_back(idle_socket);
  IncrementIdleCount();
}

void ClientSocketPoolBaseHelper::CancelAllConnectJobs() {
  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    connecting_socket_count_ -= group->jobs().size();
    group->RemoveAllJobs();

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      // RemoveGroup() will call .erase() which will invalidate the iterator,
      // but i will already have been incremented to a valid iterator before
      // RemoveGroup() is called.
      RemoveGroup(i++);
    } else {
      ++i;
    }
  }
  DCHECK_EQ(0, connecting_socket_count_);
}

void ClientSocketPoolBaseHelper::CancelAllRequestsWithError(int error) {
  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;

    RequestQueue pending_requests;
    pending_requests.swap(*group->mutable_pending_requests());
    for (RequestQueue::iterator it2 = pending_requests.begin();
         it2 != pending_requests.end(); ++it2) {
      scoped_ptr<const Request> request(*it2);
      InvokeUserCallbackLater(
          request->handle(), request->callback(), error);
    }

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      // RemoveGroup() will call .erase() which will invalidate the iterator,
      // but i will already have been incremented to a valid iterator before
      // RemoveGroup() is called.
      RemoveGroup(i++);
    } else {
      ++i;
    }
  }
}

bool ClientSocketPoolBaseHelper::ReachedMaxSocketsLimit() const {
  // Each connecting socket will eventually connect and be handed out.
  int total = handed_out_socket_count_ + connecting_socket_count_ +
      idle_socket_count();
  // There can be more sockets than the limit since some requests can ignore
  // the limit
  if (total < max_sockets_)
    return false;
  return true;
}

bool ClientSocketPoolBaseHelper::CloseOneIdleSocket() {
  if (idle_socket_count() == 0)
    return false;
  return CloseOneIdleSocketExceptInGroup(NULL);
}

bool ClientSocketPoolBaseHelper::CloseOneIdleSocketExceptInGroup(
    const Group* exception_group) {
  CHECK_GT(idle_socket_count(), 0);

  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end(); ++i) {
    Group* group = i->second;
    if (exception_group == group)
      continue;
    std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();

    if (!idle_sockets->empty()) {
      delete idle_sockets->front().socket;
      idle_sockets->pop_front();
      DecrementIdleCount();
      if (group->IsEmpty())
        RemoveGroup(i);

      return true;
    }
  }

  return false;
}

bool ClientSocketPoolBaseHelper::CloseOneIdleConnectionInLayeredPool() {
  // This pool doesn't have any idle sockets. It's possible that a pool at a
  // higher layer is holding one of this sockets active, but it's actually idle.
  // Query the higher layers.
  for (std::set<LayeredPool*>::const_iterator it = higher_layer_pools_.begin();
       it != higher_layer_pools_.end(); ++it) {
    if ((*it)->CloseOneIdleConnection())
      return true;
  }
  return false;
}

void ClientSocketPoolBaseHelper::InvokeUserCallbackLater(
    ClientSocketHandle* handle, const CompletionCallback& callback, int rv) {
  CHECK(!ContainsKey(pending_callback_map_, handle));
  pending_callback_map_[handle] = CallbackResultPair(callback, rv);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClientSocketPoolBaseHelper::InvokeUserCallback,
                 weak_factory_.GetWeakPtr(), handle));
}

void ClientSocketPoolBaseHelper::InvokeUserCallback(
    ClientSocketHandle* handle) {
  PendingCallbackMap::iterator it = pending_callback_map_.find(handle);

  // Exit if the request has already been cancelled.
  if (it == pending_callback_map_.end())
    return;

  CHECK(!handle->is_initialized());
  CompletionCallback callback = it->second.callback;
  int result = it->second.result;
  pending_callback_map_.erase(it);
  callback.Run(result);
}

void ClientSocketPoolBaseHelper::TryToCloseSocketsInLayeredPools() {
  while (IsStalled()) {
    // Closing a socket will result in calling back into |this| to use the freed
    // socket slot, so nothing else is needed.
    if (!CloseOneIdleConnectionInLayeredPool())
      return;
  }
}

ClientSocketPoolBaseHelper::Group::Group()
    : unassigned_job_count_(0),
      active_socket_count_(0),
      weak_factory_(this) {}

ClientSocketPoolBaseHelper::Group::~Group() {
  CleanupBackupJob();
  DCHECK_EQ(0u, unassigned_job_count_);
}

void ClientSocketPoolBaseHelper::Group::StartBackupSocketTimer(
    const std::string& group_name,
    ClientSocketPoolBaseHelper* pool) {
  // Only allow one timer pending to create a backup socket.
  if (weak_factory_.HasWeakPtrs())
    return;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Group::OnBackupSocketTimerFired, weak_factory_.GetWeakPtr(),
                 group_name, pool),
      pool->ConnectRetryInterval());
}

bool ClientSocketPoolBaseHelper::Group::TryToUseUnassignedConnectJob() {
  SanityCheck();

  if (unassigned_job_count_ == 0)
    return false;
  --unassigned_job_count_;
  return true;
}

void ClientSocketPoolBaseHelper::Group::AddJob(ConnectJob* job,
                                               bool is_preconnect) {
  SanityCheck();

  if (is_preconnect)
    ++unassigned_job_count_;
  jobs_.insert(job);
}

void ClientSocketPoolBaseHelper::Group::RemoveJob(ConnectJob* job) {
  SanityCheck();

  jobs_.erase(job);
  size_t job_count = jobs_.size();
  if (job_count < unassigned_job_count_)
    unassigned_job_count_ = job_count;
}

void ClientSocketPoolBaseHelper::Group::OnBackupSocketTimerFired(
    std::string group_name,
    ClientSocketPoolBaseHelper* pool) {
  // If there are no more jobs pending, there is no work to do.
  // If we've done our cleanups correctly, this should not happen.
  if (jobs_.empty()) {
    NOTREACHED();
    return;
  }

  // If our old job is waiting on DNS, or if we can't create any sockets
  // right now due to limits, just reset the timer.
  if (pool->ReachedMaxSocketsLimit() ||
      !HasAvailableSocketSlot(pool->max_sockets_per_group_) ||
      (*jobs_.begin())->GetLoadState() == LOAD_STATE_RESOLVING_HOST) {
    StartBackupSocketTimer(group_name, pool);
    return;
  }

  if (pending_requests_.empty())
    return;

  ConnectJob* backup_job = pool->connect_job_factory_->NewConnectJob(
      group_name, **pending_requests_.begin(), pool);
  backup_job->net_log().AddEvent(NetLog::TYPE_SOCKET_BACKUP_CREATED);
  SIMPLE_STATS_COUNTER("socket.backup_created");
  int rv = backup_job->Connect();
  pool->connecting_socket_count_++;
  AddJob(backup_job, false);
  if (rv != ERR_IO_PENDING)
    pool->OnConnectJobComplete(rv, backup_job);
}

void ClientSocketPoolBaseHelper::Group::SanityCheck() {
  DCHECK_LE(unassigned_job_count_, jobs_.size());
}

void ClientSocketPoolBaseHelper::Group::RemoveAllJobs() {
  SanityCheck();

  // Delete active jobs.
  STLDeleteElements(&jobs_);
  unassigned_job_count_ = 0;

  // Cancel pending backup job.
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace internal

}  // namespace net
