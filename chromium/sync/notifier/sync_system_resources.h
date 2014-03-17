// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple system resources class that uses the current message loop
// for scheduling.  Assumes the current message loop is already
// running.

#ifndef SYNC_NOTIFIER_SYNC_SYSTEM_RESOURCES_H_
#define SYNC_NOTIFIER_SYNC_SYSTEM_RESOURCES_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "sync/base/sync_export.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/state_writer.h"

namespace syncer {

class SyncLogger : public invalidation::Logger {
 public:
  SyncLogger();

  virtual ~SyncLogger();

  // invalidation::Logger implementation.
  virtual void Log(LogLevel level, const char* file, int line,
                   const char* format, ...) OVERRIDE;

  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;
};

class SyncInvalidationScheduler : public invalidation::Scheduler {
 public:
  SyncInvalidationScheduler();

  virtual ~SyncInvalidationScheduler();

  // Start and stop the scheduler.
  void Start();
  void Stop();

  // invalidation::Scheduler implementation.
  virtual void Schedule(invalidation::TimeDelta delay,
                        invalidation::Closure* task) OVERRIDE;

  virtual bool IsRunningOnThread() const OVERRIDE;

  virtual invalidation::Time GetCurrentTime() const OVERRIDE;

  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;

 private:
  // Runs the task, deletes it, and removes it from |posted_tasks_|.
  void RunPostedTask(invalidation::Closure* task);

  // Holds all posted tasks that have not yet been run.
  std::set<invalidation::Closure*> posted_tasks_;

  const base::MessageLoop* created_on_loop_;
  bool is_started_;
  bool is_stopped_;

  base::WeakPtrFactory<SyncInvalidationScheduler> weak_factory_;
};

// SyncNetworkChannel implements common tasks needed to interact with
// invalidation library:
//  - registering message and network status callbacks
//  - Encoding/Decoding message to ClientGatewayMessage
//  - notifying observers about network channel state change
// Implementation of particular network protocol should implement
// SendEncodedMessage and call NotifyStateChange and DeliverIncomingMessage.
class SYNC_EXPORT_PRIVATE SyncNetworkChannel
    : public NON_EXPORTED_BASE(invalidation::NetworkChannel) {
 public:
  class Observer {
   public:
    // Called when network channel state changes. Possible states are:
    //  - INVALIDATIONS_ENABLED : connection is established and working
    //  - TRANSIENT_INVALIDATION_ERROR : no network, connection lost, etc.
    //  - INVALIDATION_CREDENTIALS_REJECTED : Issues with auth token
    virtual void OnNetworkChannelStateChanged(
        InvalidatorState invalidator_state) = 0;
  };

  SyncNetworkChannel();

  virtual ~SyncNetworkChannel();

  // invalidation::NetworkChannel implementation.
  virtual void SendMessage(const std::string& outgoing_message) OVERRIDE;
  virtual void SetMessageReceiver(
      invalidation::MessageCallback* incoming_receiver) OVERRIDE;
  virtual void AddNetworkStatusReceiver(
      invalidation::NetworkStatusCallback* network_status_receiver) OVERRIDE;
  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;

  // Subclass should implement SendEncodedMessage to send encoded message to
  // Tango over network.
  virtual void SendEncodedMessage(const std::string& encoded_message) = 0;

  // Classes interested in network channel state changes should implement
  // SyncNetworkChannel::Observer and register here.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::string& GetServiceContextForTest() const;

  int64 GetSchedulingHashForTest() const;

  static std::string EncodeMessageForTest(
      const std::string& message,
      const std::string& service_context,
      int64 scheduling_hash);

  static bool DecodeMessageForTest(
      const std::string& notification,
      std::string* message,
      std::string* service_context,
      int64* scheduling_hash);

 protected:
  // Subclass should notify about connection state through NotifyStateChange.
  void NotifyStateChange(InvalidatorState invalidator_state);
  // Subclass should call DeliverIncomingMessage for message to reach
  // invalidations library.
  void DeliverIncomingMessage(const std::string& message);

 private:
  typedef std::vector<invalidation::NetworkStatusCallback*>
      NetworkStatusReceiverList;

  static void EncodeMessage(
      std::string* encoded_message,
      const std::string& message,
      const std::string& service_context,
      int64 scheduling_hash);

  static bool DecodeMessage(
      const std::string& data,
      std::string* message,
      std::string* service_context,
      int64* scheduling_hash);

  // Callbacks into invalidation library
  scoped_ptr<invalidation::MessageCallback> incoming_receiver_;
  NetworkStatusReceiverList network_status_receivers_;

  // Last channel state for new network status receivers.
  InvalidatorState invalidator_state_;

  ObserverList<Observer> observers_;

  std::string service_context_;
  int64 scheduling_hash_;
};

class SyncStorage : public invalidation::Storage {
 public:
  SyncStorage(StateWriter* state_writer, invalidation::Scheduler* scheduler);

  virtual ~SyncStorage();

  void SetInitialState(const std::string& value) {
    cached_state_ = value;
  }

  // invalidation::Storage implementation.
  virtual void WriteKey(const std::string& key, const std::string& value,
                        invalidation::WriteKeyCallback* done) OVERRIDE;

  virtual void ReadKey(const std::string& key,
                       invalidation::ReadKeyCallback* done) OVERRIDE;

  virtual void DeleteKey(const std::string& key,
                         invalidation::DeleteKeyCallback* done) OVERRIDE;

  virtual void ReadAllKeys(
      invalidation::ReadAllKeysCallback* key_callback) OVERRIDE;

  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;

 private:
  // Runs the given storage callback with SUCCESS status and deletes it.
  void RunAndDeleteWriteKeyCallback(
      invalidation::WriteKeyCallback* callback);

  // Runs the given callback with the given value and deletes it.
  void RunAndDeleteReadKeyCallback(
      invalidation::ReadKeyCallback* callback, const std::string& value);

  StateWriter* state_writer_;
  invalidation::Scheduler* scheduler_;
  std::string cached_state_;
};

class SYNC_EXPORT_PRIVATE SyncSystemResources
    : public NON_EXPORTED_BASE(invalidation::SystemResources) {
 public:
  SyncSystemResources(SyncNetworkChannel* sync_network_channel,
                      StateWriter* state_writer);

  virtual ~SyncSystemResources();

  // invalidation::SystemResources implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual void set_platform(const std::string& platform);
  virtual std::string platform() const OVERRIDE;
  virtual SyncLogger* logger() OVERRIDE;
  virtual SyncStorage* storage() OVERRIDE;
  virtual SyncNetworkChannel* network() OVERRIDE;
  virtual SyncInvalidationScheduler* internal_scheduler() OVERRIDE;
  virtual SyncInvalidationScheduler* listener_scheduler() OVERRIDE;

 private:
  bool is_started_;
  std::string platform_;
  scoped_ptr<SyncLogger> logger_;
  scoped_ptr<SyncInvalidationScheduler> internal_scheduler_;
  scoped_ptr<SyncInvalidationScheduler> listener_scheduler_;
  scoped_ptr<SyncStorage> storage_;
  // sync_network_channel_ is owned by SyncInvalidationListener.
  SyncNetworkChannel* sync_network_channel_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_SYNC_SYSTEM_RESOURCES_H_
