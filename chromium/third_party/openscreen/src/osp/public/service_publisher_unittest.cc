// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/service_publisher.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen::osp {

namespace {

using ::testing::_;
using ::testing::Expectation;
using ::testing::NiceMock;

using State = ServicePublisher::State;

class MockObserver final : public ServicePublisher::Observer {
 public:
  ~MockObserver() = default;

  MOCK_METHOD(void, OnStarted, (), (override));
  MOCK_METHOD(void, OnStopped, (), (override));
  MOCK_METHOD(void, OnSuspended, (), (override));

  MOCK_METHOD(void, OnError, (const Error& error), (override));
};

class MockMdnsDelegate : public ServicePublisher::Delegate {
 public:
  MockMdnsDelegate() = default;
  ~MockMdnsDelegate() override = default;

  using ServicePublisher::Delegate::SetState;

  MOCK_METHOD(void,
              StartPublisher,
              (const ServicePublisher::Config&),
              (override));
  MOCK_METHOD(void,
              StartAndSuspendPublisher,
              (const ServicePublisher::Config&),
              (override));
  MOCK_METHOD(void, StopPublisher, (), (override));
  MOCK_METHOD(void, SuspendPublisher, (), (override));
  MOCK_METHOD(void,
              ResumePublisher,
              (const ServicePublisher::Config&),
              (override));
  MOCK_METHOD(void, RunTasksPublisher, ());
};

class ServicePublisherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mock_delegate = std::make_unique<NiceMock<MockMdnsDelegate>>();
    mock_delegate_ = mock_delegate.get();
    service_publisher_ =
        std::make_unique<ServicePublisher>(std::move(mock_delegate));
    service_publisher_->SetConfig(config);
  }

  NiceMock<MockMdnsDelegate>* mock_delegate_ = nullptr;
  std::unique_ptr<ServicePublisher> service_publisher_;
  ServicePublisher::Config config;
};

}  // namespace

TEST_F(ServicePublisherTest, NormalStartStop) {
  ASSERT_EQ(State::kStopped, service_publisher_->state());

  EXPECT_CALL(*mock_delegate_, StartPublisher(_));
  EXPECT_TRUE(service_publisher_->Start());
  EXPECT_FALSE(service_publisher_->Start());
  EXPECT_EQ(State::kStarting, service_publisher_->state());

  mock_delegate_->SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, service_publisher_->state());

  EXPECT_CALL(*mock_delegate_, StopPublisher());
  EXPECT_TRUE(service_publisher_->Stop());
  EXPECT_FALSE(service_publisher_->Stop());
  EXPECT_EQ(State::kStopping, service_publisher_->state());

  mock_delegate_->SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, service_publisher_->state());
}

TEST_F(ServicePublisherTest, StopBeforeRunning) {
  EXPECT_CALL(*mock_delegate_, StartPublisher(_));
  EXPECT_TRUE(service_publisher_->Start());
  EXPECT_EQ(State::kStarting, service_publisher_->state());

  EXPECT_CALL(*mock_delegate_, StopPublisher());
  EXPECT_TRUE(service_publisher_->Stop());
  EXPECT_FALSE(service_publisher_->Stop());
  EXPECT_EQ(State::kStopping, service_publisher_->state());

  mock_delegate_->SetState(State::kStopped);
  EXPECT_EQ(State::kStopped, service_publisher_->state());
}

TEST_F(ServicePublisherTest, StartSuspended) {
  EXPECT_CALL(*mock_delegate_, StartAndSuspendPublisher(_));
  EXPECT_CALL(*mock_delegate_, StartPublisher(_)).Times(0);
  EXPECT_TRUE(service_publisher_->StartAndSuspend());
  EXPECT_FALSE(service_publisher_->Start());
  EXPECT_EQ(State::kStarting, service_publisher_->state());

  mock_delegate_->SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, service_publisher_->state());
}

TEST_F(ServicePublisherTest, SuspendAndResume) {
  EXPECT_TRUE(service_publisher_->Start());
  mock_delegate_->SetState(State::kRunning);

  EXPECT_CALL(*mock_delegate_, ResumePublisher(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, SuspendPublisher()).Times(2);
  EXPECT_FALSE(service_publisher_->Resume());
  EXPECT_TRUE(service_publisher_->Suspend());
  EXPECT_TRUE(service_publisher_->Suspend());

  mock_delegate_->SetState(State::kSuspended);
  EXPECT_EQ(State::kSuspended, service_publisher_->state());

  EXPECT_CALL(*mock_delegate_, StartPublisher(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, SuspendPublisher()).Times(0);
  EXPECT_CALL(*mock_delegate_, ResumePublisher(_)).Times(2);
  EXPECT_FALSE(service_publisher_->Start());
  EXPECT_FALSE(service_publisher_->Suspend());
  EXPECT_TRUE(service_publisher_->Resume());
  EXPECT_TRUE(service_publisher_->Resume());

  mock_delegate_->SetState(State::kRunning);
  EXPECT_EQ(State::kRunning, service_publisher_->state());

  EXPECT_CALL(*mock_delegate_, ResumePublisher(_)).Times(0);
  EXPECT_FALSE(service_publisher_->Resume());
}

TEST_F(ServicePublisherTest, ObserverTransitions) {
  MockObserver observer;
  auto mock_delegate_ptr = std::make_unique<NiceMock<MockMdnsDelegate>>();
  NiceMock<MockMdnsDelegate>& mock_delegate = *mock_delegate_ptr;
  auto service_publisher =
      std::make_unique<ServicePublisher>(std::move(mock_delegate_ptr));
  service_publisher->AddObserver(observer);

  service_publisher->Start();
  Expectation start_from_stopped = EXPECT_CALL(observer, OnStarted());
  mock_delegate.SetState(State::kRunning);

  service_publisher->Suspend();
  Expectation suspend_from_running =
      EXPECT_CALL(observer, OnSuspended()).After(start_from_stopped);
  mock_delegate.SetState(State::kSuspended);

  service_publisher->Resume();
  Expectation resume_from_suspended =
      EXPECT_CALL(observer, OnStarted()).After(suspend_from_running);
  mock_delegate.SetState(State::kRunning);

  service_publisher->Stop();
  EXPECT_CALL(observer, OnStopped()).After(resume_from_suspended);
  mock_delegate.SetState(State::kStopped);
}

}  // namespace openscreen::osp
