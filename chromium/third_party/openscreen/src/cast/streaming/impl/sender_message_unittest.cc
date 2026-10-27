// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_message.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/json/json_serialization.h"

namespace openscreen::cast {

TEST(SenderMessageTest, ErrorOnNonObjectMessage) {
  Json::Value array_val(Json::arrayValue);
  EXPECT_TRUE(SenderMessage::Parse(array_val).is_error());

  Json::Value string_val("string");
  EXPECT_TRUE(SenderMessage::Parse(string_val).is_error());

  Json::Value int_val(42);
  EXPECT_TRUE(SenderMessage::Parse(int_val).is_error());
}

TEST(SenderMessageTest, ParsesGetCapabilities) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "GET_CAPABILITIES",
    "seqNum": 123
  })");
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kGetCapabilities, message.value().type);
  EXPECT_EQ(123, message.value().sequence_number);
  EXPECT_TRUE(message.value().valid);

  ErrorOr<Json::Value> serialized = message.value().ToJson();
  ASSERT_TRUE(serialized.is_value());
  EXPECT_EQ(json::Stringify(root.value()).value(),
            json::Stringify(serialized.value()).value());
}

TEST(SenderMessageTest, ParsesRpc) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "RPC",
    "seqNum": 456,
    "rpc": "SGVsbG8="
  })");
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kRpc, message.value().type);
  EXPECT_EQ(456, message.value().sequence_number);
  EXPECT_TRUE(message.value().valid);
  ASSERT_TRUE(
      std::holds_alternative<std::vector<uint8_t>>(message.value().body));
  std::vector<uint8_t> expected_body = {'H', 'e', 'l', 'l', 'o'};
  EXPECT_EQ(expected_body,
            std::get<std::vector<uint8_t>>(message.value().body));

  ErrorOr<Json::Value> serialized = message.value().ToJson();
  ASSERT_TRUE(serialized.is_value());
  EXPECT_EQ(json::Stringify(root.value()).value(),
            json::Stringify(serialized.value()).value());
}

TEST(SenderMessageTest, ParsesInput) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "INPUT",
    "seqNum": 789,
    "input": "V29ybGQ="
  })");
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kInput, message.value().type);
  EXPECT_EQ(789, message.value().sequence_number);
  EXPECT_TRUE(message.value().valid);
  ASSERT_TRUE(
      std::holds_alternative<std::vector<uint8_t>>(message.value().body));
  std::vector<uint8_t> expected_body = {'W', 'o', 'r', 'l', 'd'};
  EXPECT_EQ(expected_body,
            std::get<std::vector<uint8_t>>(message.value().body));

  ErrorOr<Json::Value> serialized = message.value().ToJson();
  ASSERT_TRUE(serialized.is_value());
  EXPECT_EQ(json::Stringify(root.value()).value(),
            json::Stringify(serialized.value()).value());
}

TEST(SenderMessageTest, ParsesOffer) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "OFFER",
    "seqNum": 101,
    "offer": {
      "castMode": "mirroring",
      "supportedStreams": null
    }
  })");
  ASSERT_TRUE(root.is_value());

  // ... (The json parser fails on supportedStreams being null or empty array
  // because Offer requires it to be an array and have items. Let's just
  // use a valid Offer that actually deserializes to the exact same JSON.

  const char kOfferJson[] = R"({
    "type": "OFFER",
    "seqNum": 101,
    "offer": {
      "castMode": "mirroring",
      "supportedStreams": [
        {
          "index": 2,
          "type": "audio_source",
          "codecName": "opus",
          "rtpProfile": "cast",
          "rtpPayloadType": 96,
          "ssrc": 19088743,
          "bitRate": 124000,
          "timeBase": "1/48000",
          "channels": 2,
          "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
          "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1",
          "codecParameter": "",
          "receiverRtcpEventLog": true,
          "targetDelay": 400
        }
      ]
    }
  })";

  root = json::Parse(kOfferJson);
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kOffer, message.value().type);
  EXPECT_EQ(101, message.value().sequence_number);
  EXPECT_TRUE(message.value().valid);
  ASSERT_TRUE(std::holds_alternative<Offer>(message.value().body));

  ErrorOr<Json::Value> serialized = message.value().ToJson();
  ASSERT_TRUE(serialized.is_value());
  EXPECT_EQ(json::Stringify(root.value()).value(),
            json::Stringify(serialized.value()).value());
}

TEST(SenderMessageTest, HandlesMissingSequenceNumber) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "GET_CAPABILITIES"
  })");
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kGetCapabilities, message.value().type);
  EXPECT_EQ(-1, message.value().sequence_number);
  EXPECT_TRUE(message.value().valid);

  ErrorOr<Json::Value> serialized = message.value().ToJson();
  ASSERT_TRUE(serialized.is_value());
  EXPECT_EQ(json::Stringify(root.value()).value(),
            json::Stringify(serialized.value()).value());
}

TEST(SenderMessageTest, InvalidMissingBody) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "RPC",
    "seqNum": 456
  })");
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kRpc, message.value().type);
  EXPECT_FALSE(message.value().valid);
}

TEST(SenderMessageTest, HandlesUnknownType) {
  ErrorOr<Json::Value> root = json::Parse(R"({
    "type": "UNKNOWN_GARBAGE",
    "seqNum": 123
  })");
  ASSERT_TRUE(root.is_value());

  ErrorOr<SenderMessage> message = SenderMessage::Parse(root.value());
  ASSERT_TRUE(message.is_value());
  EXPECT_EQ(SenderMessage::Type::kUnknown, message.value().type);
  EXPECT_FALSE(message.value().valid);
}

}  // namespace openscreen::cast
