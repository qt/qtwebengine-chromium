// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/bindings/python/sender_bridge.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen::cast {

class SenderBridgeTest : public ::testing::Test {
 protected:
  SenderBridgeTest()
      : clock_(Clock::now()),
        task_runner_(clock_),
        bridge_(&task_runner_,
                IPAddress::Parse("127.0.0.1").value(),
                8009,
                "") {}

  FakeClock clock_;
  FakeTaskRunner task_runner_;
  CastSenderBridge bridge_;
};

TEST_F(SenderBridgeTest, ConstructionSucceedsWithValidIP) {
  CastSenderBridge bridge1(&task_runner_,
                           IPAddress::Parse("192.168.1.1").value(), 8009, "");
  CastSenderBridge bridge2(&task_runner_, IPAddress::Parse("::1").value(), 8009,
                           "");
}

TEST_F(SenderBridgeTest, StreamVideoFailsOnInvalidCodec) {
  CastSenderBridge::StreamConfig config;
  config.file_path = "test.mp4";
  config.codec_name = "invalid_codec";

  EXPECT_FALSE(bridge_.StreamVideo(config, /*is_debug=*/false));
}

TEST_F(SenderBridgeTest, TeardownSafeWhenNotStreaming) {
  bridge_.Teardown();
}

}  // namespace openscreen::cast
