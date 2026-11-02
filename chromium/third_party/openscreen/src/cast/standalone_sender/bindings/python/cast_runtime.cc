// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/bindings/python/cast_runtime.h"

#include <chrono>
#include <memory>
#include <utility>

#include "platform/api/time.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
class TaskRunnerImpl;
}

namespace openscreen::cast {

CastRuntime::CastRuntime() {
  auto task_runner = std::make_unique<TaskRunnerImpl>(&Clock::now);
  task_runner_ = task_runner.get();
  PlatformClientPosix::Create(std::chrono::milliseconds(50),
                              std::move(task_runner));

  event_loop_thread_ =
      std::thread([runner = task_runner_] { runner->RunUntilStopped(); });
}

CastRuntime::~CastRuntime() {
  if (task_runner_) {
    task_runner_->RequestStopSoon();
  }

  if (event_loop_thread_.joinable()) {
    event_loop_thread_.join();
  }

  PlatformClientPosix::ShutDown();
}

}  // namespace openscreen::cast
