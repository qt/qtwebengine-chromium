// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_PRESENTATION_TESTING_MOCK_CONNECTION_DELEGATE_H_
#define OSP_IMPL_PRESENTATION_TESTING_MOCK_CONNECTION_DELEGATE_H_

#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "osp/public/presentation/presentation_connection.h"

namespace openscreen::osp {

class MockConnectionDelegate : public Connection::Delegate {
 public:
  MockConnectionDelegate() = default;
  MockConnectionDelegate(const MockConnectionDelegate&) = delete;
  MockConnectionDelegate& operator=(const MockConnectionDelegate&) = delete;
  MockConnectionDelegate(MockConnectionDelegate&&) noexcept = delete;
  MockConnectionDelegate& operator=(MockConnectionDelegate&&) noexcept = delete;
  ~MockConnectionDelegate() override = default;

  MOCK_METHOD(void, OnConnected, (), (override));
  MOCK_METHOD(void, OnClosedByRemote, (), (override));
  MOCK_METHOD(void, OnDiscarded, (), (override));
  MOCK_METHOD(void, OnError, (const std::string_view message), (override));
  MOCK_METHOD(void, OnTerminated, (), (override));
  MOCK_METHOD(void,
              OnStringMessage,
              (const std::string_view message),
              (override));
  MOCK_METHOD(void,
              OnBinaryMessage,
              (const std::vector<uint8_t>& data),
              (override));
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_PRESENTATION_TESTING_MOCK_CONNECTION_DELEGATE_H_
