// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/bindings/python/sender_bridge.h"

#include <csignal>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "cast/common/public/trust_store.h"
#include "cast/standalone_sender/looping_file_cast_agent.h"
#include "cast/streaming/message_fields.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"
#include "platform/impl/logging.h"

namespace openscreen::cast {

CastSenderBridge::CastSenderBridge(TaskRunner* task_runner,
                                   IPAddress ip_address,
                                   int port,
                                   std::string_view cert_path)
    : ip_address_(ip_address),
      port_(port),
      cert_path_(cert_path),
      task_runner_(task_runner) {}

CastSenderBridge::~CastSenderBridge() {
  Teardown();
}

bool CastSenderBridge::StreamVideo(const StreamConfig& config, bool is_debug) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kTearingDown) {
      OSP_LOG_ERROR << "StreamVideo called during active teardown.";
      return false;
    }
    if (state_ != State::kInitializing) {
      OSP_LOG_ERROR << "StreamVideo called while streaming is already active.";
      return false;
    }
    state_ = State::kStreaming;
  }

  const auto log_level =
      is_debug ? openscreen::LogLevel::kInfo : openscreen::LogLevel::kWarning;
  openscreen::SetLogLevel(log_level);
  std::signal(SIGPIPE, SIG_IGN);

  const auto parsed_codec = StringToVideoCodec(config.codec_name);
  if (parsed_codec.is_error()) {
    OSP_LOG_ERROR << "Unsupported video codec requested: " << config.codec_name;
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }

  // Return false if av1 codec is requested but not supported
#if !defined(CAST_STANDALONE_SENDER_HAVE_LIBAOM)
  if (parsed_codec.value() == VideoCodec::kAv1) {
    OSP_LOG_ERROR
        << "AV1 codec requested but LibAOM is not installed in this build.";
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kInitializing;
    return false;
  }
#endif

  const std::optional<ConnectionSettings::PreconfiguredSessionInfo>
      preconfigured_session_info =
          config.remote_transport_id.empty()
              ? std::nullopt
              : std::make_optional<
                    ConnectionSettings::PreconfiguredSessionInfo>(
                    ConnectionSettings::PreconfiguredSessionInfo{
                        .remote_transport_id = config.remote_transport_id,
                        .app_session_id = config.session_id,
                    });

  const ConnectionSettings settings{
      .receiver_endpoint =
          IPEndpoint{ip_address_, static_cast<uint16_t>(port_)},
      .path_to_file = config.file_path,
      .max_bitrate = config.max_bitrate,
      .should_include_video = true,
      .should_include_audio = config.should_include_audio,
      .should_loop_video = false,
      .codec = parsed_codec.value(),
      .preconfigured_session_info = preconfigured_session_info,
  };

  std::unique_ptr<TrustStore> cast_trust_store;
  if (!cert_path_.empty()) {
    cast_trust_store = TrustStore::CreateInstanceFromPemFile(cert_path_);
  }
  if (!cast_trust_store) {
    cast_trust_store = CastTrustStore::Create();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kTearingDown) {
      state_ = State::kInitializing;
      return false;
    }
    task_runner_->PostTask(
        [this, settings, store = std::move(cast_trust_store)]() mutable {
          std::lock_guard<std::mutex> lock(mutex_);
          if (state_ != State::kStreaming) {
            return;
          }
          cast_agent_ = std::make_unique<LoopingFileCastAgent>(
              *task_runner_, std::move(store), [this] {
                task_runner_->PostTask([this] {
                  std::lock_guard<std::mutex> lock(mutex_);
                  cast_agent_.reset();
                  state_ = State::kInitializing;
                });
              });
          cast_agent_->Connect(settings);
        });
  }

  return true;
}

void CastSenderBridge::Teardown() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kInitializing || state_ == State::kTearingDown) {
      return;
    }
    state_ = State::kTearingDown;
  }

  OSP_CHECK(!task_runner_->IsRunningOnTaskRunner());

  std::promise<void> destroy_promise;
  std::future<void> destroy_future = destroy_promise.get_future();

  task_runner_->PostTask(
      [this, promise = std::move(destroy_promise)]() mutable {
        {
          std::lock_guard<std::mutex> lock(mutex_);
          cast_agent_.reset();
          state_ = State::kInitializing;
        }
        promise.set_value();
      });

  destroy_future.wait();
}

}  // namespace openscreen::cast
