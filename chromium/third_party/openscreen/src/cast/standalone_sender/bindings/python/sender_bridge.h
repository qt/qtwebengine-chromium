// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_BINDINGS_PYTHON_SENDER_BRIDGE_H_
#define CAST_STANDALONE_SENDER_BINDINGS_PYTHON_SENDER_BRIDGE_H_

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "platform/base/ip_address.h"
#include "util/thread_annotations.h"

namespace openscreen {
class TaskRunner;
}

namespace openscreen::cast {

class LoopingFileCastAgent;

// Native C++ bridge for exporting Cast Streaming functionality to
// Python via pybind11. Handles thread synchronization between the Python
// execution environment and the TaskRunner implementation.
class CastSenderBridge {
 public:
  // `ip_address`: Target Cast Receiver's parsed IPAddress.
  // `port`: Cast V2 connection port (typically 8009).
  // `cert_path`: Path to a PEM developer root CA certificate. If omitted or
  //              empty, connections enforce official Google-signed trust
  //              stores.
  CastSenderBridge(TaskRunner* task_runner,
                   IPAddress ip_address,
                   int port,
                   std::string_view cert_path);
  ~CastSenderBridge();

  // `StreamConfig` holds the configuration for the streaming session.
  // `file_path`: Absolute local path to the video container.
  // `remote_transport_id`: Destination receiver transport ID. When non-empty,
  //                        bypasses receiver app LAUNCH and routes directly.
  // `session_id`: Pre-launched receiver app session ID. Used for clean STOP.
  // `codec_name`: Supported video encoder codec string.
  // `max_bitrate`: Peak aggregate bitrate (audio + video) across RTP streams.
  // `should_include_audio`: If false, negotiates a video-only mirroring
  // session.
  struct StreamConfig {
    std::string file_path;
    std::string remote_transport_id;
    std::string session_id;
    std::string codec_name = "vp8";
    int max_bitrate = 5242880;
    bool should_include_audio = true;
  };

  // Initiates Cast session connection, OFFER/ANSWER media negotiation, and RTP
  // media frame streaming over UDP. This is an asynchronous, non-blocking call
  // that schedules the media session on the background TaskRunner.
  //
  // `is_debug`: If true, set the log level to info cast streaming modules.
  //
  // Returns true if the session was successfully scheduled on the TaskRunner,
  // false on immediate validation failure (e.g. invalid codec).
  bool StreamVideo(const StreamConfig& config, bool is_debug);

  // Thread-safe method to asynchronously signal the active Open Screen
  // TaskRunner to terminate the running media session and shut down all network
  // sockets.
  void Teardown();

 private:
  enum class State { kInitializing, kStreaming, kTearingDown };
  State state_ OSP_GUARDED_BY(mutex_) = State::kInitializing;
  IPAddress ip_address_;
  int port_;
  std::string cert_path_;
  std::mutex mutex_;
  TaskRunner* const task_runner_;
  std::unique_ptr<LoopingFileCastAgent> cast_agent_ OSP_GUARDED_BY(mutex_);
};

}  // namespace openscreen::cast

#endif  // CAST_STANDALONE_SENDER_BINDINGS_PYTHON_SENDER_BRIDGE_H_
