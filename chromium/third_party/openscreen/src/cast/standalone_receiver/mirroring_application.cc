// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/mirroring_application.h"

#include <algorithm>
#include <utility>

#include "build/build_config.h"
#include "cast/common/public/cast_streaming_app_ids.h"
#include "cast/common/public/message_port.h"
#include "cast/streaming/message_fields.h"
#include "cast/streaming/public/constants.h"
#include "cast/streaming/public/environment.h"
#include "cast/streaming/public/receiver_constraints.h"
#include "cast/streaming/public/receiver_session.h"
#include "platform/api/task_runner.h"
#include "util/osp_logging.h"

namespace openscreen::cast {

namespace {
constexpr char kMirroringDisplayName[] = "Chrome Mirroring";
constexpr char kRemotingRpcNamespace[] = "urn:x-cast:com.google.cast.remoting";
}  // namespace

MirroringApplication::MirroringApplication(TaskRunner& task_runner,
                                           const IPAddress& interface_address,
                                           ApplicationAgent& agent,
                                           bool enable_dscp,
                                           bool enable_input_events)
    : task_runner_(task_runner),
      interface_address_(interface_address),
      app_ids_(GetCastStreamingAppIds()),
      agent_(agent),
      enable_dscp_(enable_dscp),
      enable_input_events_(enable_input_events) {
  agent_.RegisterApplication(this);
}

MirroringApplication::~MirroringApplication() {
  agent_.UnregisterApplication(this);  // ApplicationAgent may call Stop().
  OSP_CHECK(!current_session_);
}

const std::vector<std::string>& MirroringApplication::GetAppIds() const {
  return app_ids_;
}

bool MirroringApplication::Launch(const std::string& app_id,
                                  const Json::Value& app_params,
                                  MessagePort* message_port) {
  if (!IsCastStreamingAppId(app_id) || !message_port || current_session_) {
    return false;
  }

#if BUILDFLAG(IS_APPLE)
  wake_lock_ = ScopedWakeLock::Create(task_runner_);
#endif  // BUILDFLAG(IS_APPLE)
  environment_ = std::make_unique<Environment>(
      &Clock::now, task_runner_,
      IPEndpoint{interface_address_, kDefaultCastStreamingPort});
#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  controller_ = std::make_unique<StreamingPlaybackController>(
      task_runner_, this, enable_input_events_);
#else
  controller_ =
      std::make_unique<StreamingPlaybackController>(this, enable_input_events_);
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  ReceiverConstraints constraints;
  constraints.video_codecs.insert(constraints.video_codecs.begin(),
                                  {VideoCodec::kAv1, VideoCodec::kVp9});
  constraints.remoting = std::make_unique<RemotingConstraints>();
  constraints.enable_dscp = enable_dscp_;
  constraints.supports_input_events = enable_input_events_;
  current_session_ = std::make_unique<ReceiverSession>(
      *controller_, *environment_, *message_port, std::move(constraints));

  for (auto const& [ns, handler] : custom_message_handlers_) {
    current_session_->SetCustomMessageHandler(ns, handler);
  }

  return true;
}

std::string MirroringApplication::GetSessionId() {
  return current_session_ ? current_session_->session_id() : std::string();
}

std::string MirroringApplication::GetDisplayName() {
  return current_session_ ? kMirroringDisplayName : std::string();
}

std::vector<std::string> MirroringApplication::GetSupportedNamespaces() {
  std::vector<std::string> namespaces = {kCastWebrtcNamespace,
                                         kRemotingRpcNamespace};
  namespaces.insert(namespaces.end(), custom_namespaces_.begin(),
                    custom_namespaces_.end());
  return namespaces;
}

void MirroringApplication::Stop() {
  if (current_session_) {
    for (auto const& [ns, handler] : custom_message_handlers_) {
      current_session_->SetCustomMessageHandler(ns, nullptr);
    }
  }
  current_session_.reset();
  controller_.reset();
  environment_.reset();
  wake_lock_.reset();
}

void MirroringApplication::OnPlaybackError(StreamingPlaybackController*,
                                           const Error& error) {
  OSP_LOG_ERROR << "[MirroringApplication] " << error;
  agent_.StopApplicationIfRunning(this);  // ApplicationAgent calls Stop().
}

void MirroringApplication::AddCustomNamespace(
    std::string_view message_namespace) {
  if (std::find(custom_namespaces_.begin(), custom_namespaces_.end(),
                message_namespace) == custom_namespaces_.end()) {
    custom_namespaces_.push_back(std::string(message_namespace));
  }
}

void MirroringApplication::RemoveCustomNamespace(
    std::string_view message_namespace) {
  auto it = std::find(custom_namespaces_.begin(), custom_namespaces_.end(),
                      message_namespace);
  if (it != custom_namespaces_.end()) {
    custom_namespaces_.erase(it);
  }
}

void MirroringApplication::SetCustomMessageHandler(
    std::string_view message_namespace,
    CustomMessageCallback cb) {
  auto it = std::find_if(custom_message_handlers_.begin(),
                         custom_message_handlers_.end(),
                         [&message_namespace](const auto& pair) {
                           return pair.first == message_namespace;
                         });

  if (!cb) {
    if (it != custom_message_handlers_.end()) {
      custom_message_handlers_.erase(it);
    }
    if (current_session_) {
      current_session_->SetCustomMessageHandler(message_namespace, nullptr);
    }
    return;
  }

  if (it != custom_message_handlers_.end()) {
    OSP_LOG_ERROR << "Handler already exists for namespace: "
                  << message_namespace;
    return;
  } else {
    custom_message_handlers_.emplace_back(std::string(message_namespace), cb);
  }

  if (current_session_) {
    current_session_->SetCustomMessageHandler(message_namespace, std::move(cb));
  }
}

void MirroringApplication::SendMessage(std::string_view destination_id,
                                       std::string_view message_namespace,
                                       std::string_view message) {
  if (!current_session_ || !current_session_->messenger()) {
    OSP_LOG_ERROR << "Cannot send message: session or messenger is null";
    return;
  }

  const Error error = current_session_->messenger()->SendMessage(
      destination_id, message_namespace, message);
  if (!error.ok()) {
    OSP_LOG_ERROR << "Failed to send message: " << error;
  }
}

}  // namespace openscreen::cast
