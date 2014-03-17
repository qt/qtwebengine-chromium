// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/sync_system_resources.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "google/cacheinvalidation/client_gateway.pb.h"
#include "google/cacheinvalidation/deps/callback.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/notifier/invalidation_util.h"

namespace syncer {

SyncLogger::SyncLogger() {}
SyncLogger::~SyncLogger() {}

void SyncLogger::Log(LogLevel level, const char* file, int line,
                     const char* format, ...) {
  logging::LogSeverity log_severity = -2;  // VLOG(2)
  bool emit_log = false;
  switch (level) {
    case FINE_LEVEL:
      log_severity = -2;  // VLOG(2)
      emit_log = VLOG_IS_ON(2);
      break;
    case INFO_LEVEL:
      log_severity = -1;  // VLOG(1)
      emit_log = VLOG_IS_ON(1);
      break;
    case WARNING_LEVEL:
      log_severity = logging::LOG_WARNING;
      emit_log = LOG_IS_ON(WARNING);
      break;
    case SEVERE_LEVEL:
      log_severity = logging::LOG_ERROR;
      emit_log = LOG_IS_ON(ERROR);
      break;
  }
  if (emit_log) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    base::StringAppendV(&result, format, ap);
    logging::LogMessage(file, line, log_severity).stream() << result;
    va_end(ap);
  }
}

void SyncLogger::SetSystemResources(invalidation::SystemResources* resources) {
  // Do nothing.
}

SyncInvalidationScheduler::SyncInvalidationScheduler()
    : created_on_loop_(base::MessageLoop::current()),
      is_started_(false),
      is_stopped_(false),
      weak_factory_(this) {
  CHECK(created_on_loop_);
}

SyncInvalidationScheduler::~SyncInvalidationScheduler() {
  CHECK_EQ(created_on_loop_, base::MessageLoop::current());
  CHECK(is_stopped_);
}

void SyncInvalidationScheduler::Start() {
  CHECK_EQ(created_on_loop_, base::MessageLoop::current());
  CHECK(!is_started_);
  is_started_ = true;
  is_stopped_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

void SyncInvalidationScheduler::Stop() {
  CHECK_EQ(created_on_loop_, base::MessageLoop::current());
  is_stopped_ = true;
  is_started_ = false;
  weak_factory_.InvalidateWeakPtrs();
  STLDeleteElements(&posted_tasks_);
  posted_tasks_.clear();
}

void SyncInvalidationScheduler::Schedule(invalidation::TimeDelta delay,
                                         invalidation::Closure* task) {
  DCHECK(invalidation::IsCallbackRepeatable(task));
  CHECK_EQ(created_on_loop_, base::MessageLoop::current());

  if (!is_started_) {
    delete task;
    return;
  }

  posted_tasks_.insert(task);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&SyncInvalidationScheduler::RunPostedTask,
                            weak_factory_.GetWeakPtr(), task),
      delay);
}

bool SyncInvalidationScheduler::IsRunningOnThread() const {
  return created_on_loop_ == base::MessageLoop::current();
}

invalidation::Time SyncInvalidationScheduler::GetCurrentTime() const {
  CHECK_EQ(created_on_loop_, base::MessageLoop::current());
  return base::Time::Now();
}

void SyncInvalidationScheduler::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void SyncInvalidationScheduler::RunPostedTask(invalidation::Closure* task) {
  CHECK_EQ(created_on_loop_, base::MessageLoop::current());
  task->Run();
  posted_tasks_.erase(task);
  delete task;
}

SyncNetworkChannel::SyncNetworkChannel()
    : invalidator_state_(DEFAULT_INVALIDATION_ERROR),
      scheduling_hash_(0) {
}

SyncNetworkChannel::~SyncNetworkChannel() {
  STLDeleteElements(&network_status_receivers_);
}

void SyncNetworkChannel::SendMessage(const std::string& outgoing_message) {
  std::string encoded_message;
  EncodeMessage(&encoded_message, outgoing_message, service_context_,
      scheduling_hash_);
  SendEncodedMessage(encoded_message);
}

void SyncNetworkChannel::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  incoming_receiver_.reset(incoming_receiver);
}

void SyncNetworkChannel::AddNetworkStatusReceiver(
    invalidation::NetworkStatusCallback* network_status_receiver) {
  network_status_receiver->Run(invalidator_state_ == INVALIDATIONS_ENABLED);
  network_status_receivers_.push_back(network_status_receiver);
}

void SyncNetworkChannel::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void SyncNetworkChannel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncNetworkChannel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::string& SyncNetworkChannel::GetServiceContextForTest() const {
  return service_context_;
}

int64 SyncNetworkChannel::GetSchedulingHashForTest() const {
  return scheduling_hash_;
}

std::string SyncNetworkChannel::EncodeMessageForTest(
    const std::string& message, const std::string& service_context,
    int64 scheduling_hash) {
  std::string encoded_message;
  EncodeMessage(&encoded_message, message, service_context, scheduling_hash);
  return encoded_message;
}

bool SyncNetworkChannel::DecodeMessageForTest(
    const std::string& data,
    std::string* message,
    std::string* service_context,
    int64* scheduling_hash) {
  return DecodeMessage(data, message, service_context, scheduling_hash);
}

void SyncNetworkChannel::NotifyStateChange(InvalidatorState invalidator_state) {
  // Remember state for future NetworkStatusReceivers.
  invalidator_state_ = invalidator_state;
  // Notify NetworkStatusReceivers in cacheinvalidation.
  for (NetworkStatusReceiverList::const_iterator it =
           network_status_receivers_.begin();
       it != network_status_receivers_.end(); ++it) {
    (*it)->Run(invalidator_state_ == INVALIDATIONS_ENABLED);
  }
  // Notify observers.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnNetworkChannelStateChanged(invalidator_state_));
}

void SyncNetworkChannel::DeliverIncomingMessage(const std::string& data) {
  if (!incoming_receiver_) {
    DLOG(ERROR) << "No receiver for incoming notification";
    return;
  }
  std::string message;
  if (!DecodeMessage(data,
                     &message, &service_context_, &scheduling_hash_)) {
    DLOG(ERROR) << "Could not parse ClientGatewayMessage";
    return;
  }
  incoming_receiver_->Run(message);
}

void SyncNetworkChannel::EncodeMessage(
    std::string* encoded_message,
    const std::string& message,
    const std::string& service_context,
    int64 scheduling_hash) {
  ipc::invalidation::ClientGatewayMessage envelope;
  envelope.set_is_client_to_server(true);
  if (!service_context.empty()) {
    envelope.set_service_context(service_context);
    envelope.set_rpc_scheduling_hash(scheduling_hash);
  }
  envelope.set_network_message(message);
  envelope.SerializeToString(encoded_message);
}


bool SyncNetworkChannel::DecodeMessage(
    const std::string& data,
    std::string* message,
    std::string* service_context,
    int64* scheduling_hash) {
  ipc::invalidation::ClientGatewayMessage envelope;
  if (!envelope.ParseFromString(data)) {
    return false;
  }
  *message = envelope.network_message();
  if (envelope.has_service_context()) {
    *service_context = envelope.service_context();
  }
  if (envelope.has_rpc_scheduling_hash()) {
    *scheduling_hash = envelope.rpc_scheduling_hash();
  }
  return true;
}


SyncStorage::SyncStorage(StateWriter* state_writer,
                         invalidation::Scheduler* scheduler)
    : state_writer_(state_writer),
      scheduler_(scheduler) {
  DCHECK(state_writer_);
  DCHECK(scheduler_);
}

SyncStorage::~SyncStorage() {}

void SyncStorage::WriteKey(const std::string& key, const std::string& value,
                           invalidation::WriteKeyCallback* done) {
  CHECK(state_writer_);
  // TODO(ghc): actually write key,value associations, and don't invoke the
  // callback until the operation completes.
  state_writer_->WriteState(value);
  cached_state_ = value;
  // According to the cache invalidation API folks, we can do this as
  // long as we make sure to clear the persistent state that we start
  // up the cache invalidation client with.  However, we musn't do it
  // right away, as we may be called under a lock that the callback
  // uses.
  scheduler_->Schedule(
      invalidation::Scheduler::NoDelay(),
      invalidation::NewPermanentCallback(
          this, &SyncStorage::RunAndDeleteWriteKeyCallback,
          done));
}

void SyncStorage::ReadKey(const std::string& key,
                          invalidation::ReadKeyCallback* done) {
  DCHECK(scheduler_->IsRunningOnThread()) << "not running on scheduler thread";
  RunAndDeleteReadKeyCallback(done, cached_state_);
}

void SyncStorage::DeleteKey(const std::string& key,
                            invalidation::DeleteKeyCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to DeleteKey(" << key << ", callback)";
}

void SyncStorage::ReadAllKeys(invalidation::ReadAllKeysCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to ReadAllKeys(callback)";
}

void SyncStorage::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void SyncStorage::RunAndDeleteWriteKeyCallback(
    invalidation::WriteKeyCallback* callback) {
  callback->Run(
      invalidation::Status(invalidation::Status::SUCCESS, std::string()));
  delete callback;
}

void SyncStorage::RunAndDeleteReadKeyCallback(
    invalidation::ReadKeyCallback* callback, const std::string& value) {
  callback->Run(std::make_pair(
      invalidation::Status(invalidation::Status::SUCCESS, std::string()),
      value));
  delete callback;
}

SyncSystemResources::SyncSystemResources(
    SyncNetworkChannel* sync_network_channel,
    StateWriter* state_writer)
    : is_started_(false),
      logger_(new SyncLogger()),
      internal_scheduler_(new SyncInvalidationScheduler()),
      listener_scheduler_(new SyncInvalidationScheduler()),
      storage_(new SyncStorage(state_writer, internal_scheduler_.get())),
      sync_network_channel_(sync_network_channel) {
}

SyncSystemResources::~SyncSystemResources() {
  Stop();
}

void SyncSystemResources::Start() {
  internal_scheduler_->Start();
  listener_scheduler_->Start();
  is_started_ = true;
}

void SyncSystemResources::Stop() {
  internal_scheduler_->Stop();
  listener_scheduler_->Stop();
}

bool SyncSystemResources::IsStarted() const {
  return is_started_;
}

void SyncSystemResources::set_platform(const std::string& platform) {
  platform_ = platform;
}

std::string SyncSystemResources::platform() const {
  return platform_;
}

SyncLogger* SyncSystemResources::logger() {
  return logger_.get();
}

SyncStorage* SyncSystemResources::storage() {
  return storage_.get();
}

SyncNetworkChannel* SyncSystemResources::network() {
  return sync_network_channel_;
}

SyncInvalidationScheduler* SyncSystemResources::internal_scheduler() {
  return internal_scheduler_.get();
}

SyncInvalidationScheduler* SyncSystemResources::listener_scheduler() {
  return listener_scheduler_.get();
}

}  // namespace syncer
