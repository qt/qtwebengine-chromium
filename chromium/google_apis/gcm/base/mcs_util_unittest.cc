// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/mcs_util.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

const uint64 kAuthId = 4421448356646222460;
const uint64 kAuthToken = 12345;

// Build a login request protobuf.
TEST(MCSUtilTest, BuildLoginRequest) {
  scoped_ptr<mcs_proto::LoginRequest> login_request =
      BuildLoginRequest(kAuthId, kAuthToken);
  ASSERT_EQ("login-1", login_request->id());
  ASSERT_EQ(base::Uint64ToString(kAuthToken), login_request->auth_token());
  ASSERT_EQ(base::Uint64ToString(kAuthId), login_request->user());
  ASSERT_EQ("android-3d5c23dac2a1fa7c", login_request->device_id());
  // TODO(zea): test the other fields once they have valid values.
}

// Test building a protobuf and extracting the tag from a protobuf.
TEST(MCSUtilTest, ProtobufToTag) {
  for (size_t i = 0; i < kNumProtoTypes; ++i) {
    scoped_ptr<google::protobuf::MessageLite> protobuf =
        BuildProtobufFromTag(i);
    if (!protobuf.get())  // Not all tags have protobuf definitions.
      continue;
    ASSERT_EQ((int)i, GetMCSProtoTag(*protobuf)) << "Type " << i;
  }
}

// Test getting and setting persistent ids.
TEST(MCSUtilTest, PersistentIds) {
  COMPILE_ASSERT(kNumProtoTypes == 16U, UpdatePersistentIds);
  const int kTagsWithPersistentIds[] = {
    kIqStanzaTag,
    kDataMessageStanzaTag
  };
  for (size_t i = 0; i < arraysize(kTagsWithPersistentIds); ++i) {
    int tag = kTagsWithPersistentIds[i];
    scoped_ptr<google::protobuf::MessageLite> protobuf =
        BuildProtobufFromTag(tag);
    ASSERT_TRUE(protobuf.get());
    SetPersistentId(base::IntToString(tag), protobuf.get());
    int get_val = 0;
    base::StringToInt(GetPersistentId(*protobuf), &get_val);
    ASSERT_EQ(tag, get_val);
  }
}

// Test getting and setting stream ids.
TEST(MCSUtilTest, StreamIds) {
  COMPILE_ASSERT(kNumProtoTypes == 16U, UpdateStreamIds);
  const int kTagsWithStreamIds[] = {
    kIqStanzaTag,
    kDataMessageStanzaTag,
    kHeartbeatPingTag,
    kHeartbeatAckTag,
    kLoginResponseTag,
  };
  for (size_t i = 0; i < arraysize(kTagsWithStreamIds); ++i) {
    int tag = kTagsWithStreamIds[i];
    scoped_ptr<google::protobuf::MessageLite> protobuf =
        BuildProtobufFromTag(tag);
    ASSERT_TRUE(protobuf.get());
    SetLastStreamIdReceived(tag, protobuf.get());
    int get_id = GetLastStreamIdReceived(*protobuf);
    ASSERT_EQ(tag, get_id);
  }
}

}  // namespace
}  // namespace gcm
