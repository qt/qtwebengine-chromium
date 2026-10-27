// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_MIRRORING_APPLICATION_H_
#define CAST_STANDALONE_RECEIVER_MIRRORING_APPLICATION_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cast/receiver/application_agent.h"
#include "cast/standalone_receiver/streaming_playback_controller.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "util/scoped_wake_lock.h"

namespace openscreen {

class TaskRunner;

namespace cast {

class MessagePort;
class ReceiverSession;

// Implements a basic Cast V2 Mirroring Application which, at launch time,
// bootstraps a ReceiverSession and StreamingPlaybackController, which set-up
// and manage the media data streaming and play it out in an on-screen window.
class MirroringApplication final : public ApplicationAgent::Application,
                                   public StreamingPlaybackController::Client {
 public:
  MirroringApplication(TaskRunner& task_runner,
                       const IPAddress& interface_address,
                       ApplicationAgent& agent,
                       bool enable_dscp,
                       bool enable_input_events);

  ~MirroringApplication() final;

  // ApplicationAgent::Application overrides.
  const std::vector<std::string>& GetAppIds() const final;
  bool Launch(const std::string& app_id,
              const Json::Value& app_params,
              MessagePort* message_port) final;
  std::string GetSessionId() final;
  std::string GetDisplayName() final;
  std::vector<std::string> GetSupportedNamespaces() final;
  void Stop() final;

  // StreamingPlaybackController::Client overrides
  void OnPlaybackError(StreamingPlaybackController* controller,
                       const Error& error) final;

  void AddCustomNamespace(std::string_view message_namespace);
  void RemoveCustomNamespace(std::string_view message_namespace);

  using CustomMessageCallback =
      std::function<void(const std::string& /* source_id */,
                         const std::string& /* message_namespace */,
                         const std::string& /* message */)>;
  void SetCustomMessageHandler(std::string_view message_namespace,
                               CustomMessageCallback cb);

  void SendMessage(std::string_view destination_id,
                   std::string_view message_namespace,
                   std::string_view message);

 private:
  TaskRunner& task_runner_;
  const IPAddress interface_address_;
  const std::vector<std::string> app_ids_;
  ApplicationAgent& agent_;
  const bool enable_dscp_;
  const bool enable_input_events_;

  std::vector<std::string> custom_namespaces_;
  std::vector<std::pair<std::string, CustomMessageCallback>>
      custom_message_handlers_;

  ScopedWakeLockPtr wake_lock_;
  std::unique_ptr<Environment> environment_;
  std::unique_ptr<StreamingPlaybackController> controller_;
  std::unique_ptr<ReceiverSession> current_session_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_RECEIVER_MIRRORING_APPLICATION_H_
