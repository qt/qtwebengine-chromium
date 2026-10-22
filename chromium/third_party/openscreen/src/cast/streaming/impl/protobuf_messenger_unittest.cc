// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/protobuf_messenger.h"

#include <utility>
#include <vector>

#include "cast/streaming/input.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen::cast {

using ::testing::_;

TEST(ProtobufMessengerTest, ProcessesMessageFromRemote) {
  bool called = false;
  ProtobufMessenger<InputMessage> messenger(
      [](std::vector<uint8_t> message) {},
      [&called](std::unique_ptr<InputMessage> received) {
        called = true;
        EXPECT_EQ(received->events_size(), 1);
        EXPECT_EQ(received->events(0).type(),
                  InputMessage::INPUT_TYPE_KEY_DOWN);
        EXPECT_EQ(received->events(0).key_event().key_value(), "a");
      });

  InputMessage message;
  auto* event = message.add_events();
  event->set_type(InputMessage::INPUT_TYPE_KEY_DOWN);
  auto* timestamp = event->mutable_timestamp();
  timestamp->set_seconds(1234);
  timestamp->set_nanos(500000000);
  auto* key_event = event->mutable_key_event();
  key_event->set_key_code("KeyA");
  key_event->set_key_value("a");

  std::vector<uint8_t> serialized(message.ByteSizeLong());
  message.SerializeToArray(serialized.data(), serialized.size());

  messenger.ProcessMessageFromRemote(serialized);
  EXPECT_TRUE(called);
}

TEST(ProtobufMessengerTest, SendsMessageToRemote) {
  std::vector<uint8_t> captured_message;
  ProtobufMessenger<InputMessage> messenger(
      [&captured_message](std::vector<uint8_t> message) {
        captured_message = std::move(message);
      });

  InputMessage message;
  auto* event = message.add_events();
  event->set_type(InputMessage::INPUT_TYPE_MOUSE_MOVE);
  auto* mouse_event = event->mutable_mouse_event();
  mouse_event->mutable_location()->set_x(0.5f);
  mouse_event->mutable_location()->set_y(0.5f);

  messenger.SendMessageToRemote(message);

  ASSERT_FALSE(captured_message.empty());
  InputMessage parsed;
  ASSERT_TRUE(
      parsed.ParseFromArray(captured_message.data(), captured_message.size()));
  EXPECT_EQ(parsed.events_size(), 1);
  EXPECT_EQ(parsed.events(0).type(), InputMessage::INPUT_TYPE_MOUSE_MOVE);
  EXPECT_EQ(parsed.events(0).mouse_event().location().x(), 0.5f);
}

TEST(ProtobufMessengerTest, HandlesInvalidData) {
  bool called = false;
  ProtobufMessenger<InputMessage> messenger(
      [](std::vector<uint8_t> message) {},
      [&called](std::unique_ptr<InputMessage> received) { called = true; });

  uint8_t invalid_data[] = {0xff, 0x00, 0xff};
  messenger.ProcessMessageFromRemote(invalid_data);
  EXPECT_FALSE(called);
}

}  // namespace openscreen::cast
