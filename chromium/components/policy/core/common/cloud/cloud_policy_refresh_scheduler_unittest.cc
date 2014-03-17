// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const int64 kPolicyRefreshRate = 4 * 60 * 60 * 1000;

const int64 kInitialCacheAgeMinutes = 1;

}  // namespace

class CloudPolicyRefreshSchedulerTest : public testing::Test {
 protected:
  CloudPolicyRefreshSchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        network_change_notifier_(net::NetworkChangeNotifier::CreateMock()) {}

  virtual void SetUp() OVERRIDE {
    client_.SetDMToken("token");

    // Set up the protobuf timestamp to be one minute in the past. Since the
    // protobuf field only has millisecond precision, we convert the actual
    // value back to get a millisecond-clamped time stamp for the checks below.
    store_.policy_.reset(new em::PolicyData());
    base::Time now = base::Time::NowFromSystemTime();
    base::TimeDelta initial_age =
        base::TimeDelta::FromMinutes(kInitialCacheAgeMinutes);
    store_.policy_->set_timestamp(
        ((now - initial_age) - base::Time::UnixEpoch()).InMilliseconds());
    last_update_ =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(store_.policy_->timestamp());
  }

  CloudPolicyRefreshScheduler* CreateRefreshScheduler() {
    EXPECT_EQ(0u, task_runner_->GetPendingTasks().size());
    CloudPolicyRefreshScheduler* scheduler =
        new CloudPolicyRefreshScheduler(&client_, &store_, task_runner_);
    scheduler->SetRefreshDelay(kPolicyRefreshRate);
    // If the store has policy, run the wait-for-invalidations timeout task.
    if (store_.has_policy()) {
      EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
      task_runner_->RunPendingTasks();
    }
    return scheduler;
  }

  void NotifyIPAddressChanged() {
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    loop_.RunUntilIdle();
  }

  base::TimeDelta GetLastDelay() const {
    const std::deque<base::TestPendingTask>& pending_tasks =
        task_runner_->GetPendingTasks();
    return
        pending_tasks.empty() ? base::TimeDelta() : pending_tasks.back().delay;
  }

  void CheckTiming(int64 expected_delay_ms) const {
    CheckTimingWithAge(base::TimeDelta::FromMilliseconds(expected_delay_ms),
                       base::TimeDelta());
  }

  // Checks that the latest refresh scheduled used an offset of
  // |offset_from_last_refresh| from the time of the previous refresh.
  // |cache_age| is how old the cache was when the refresh was issued.
  void CheckTimingWithAge(const base::TimeDelta& offset_from_last_refresh,
                          const base::TimeDelta& cache_age) const {
    EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
    base::Time now(base::Time::NowFromSystemTime());
    // |last_update_| was updated and then a refresh was scheduled at time S,
    // so |last_update_| is a bit before that.
    // Now is a bit later, N.
    // GetLastDelay() + S is the time when the refresh will run, T.
    // |cache_age| is the age of the cache at time S. It was thus created at
    // S - cache_age.
    //
    // Schematically:
    //
    // . S . N . . . . . . . T . . . .
    //   |   |               |
    //   set "last_refresh_" and then scheduled the next refresh; the cache
    //   was "cache_age" old at this point.
    //       |               |
    //       some time elapsed on the test execution since then;
    //       this is the current time, "now"
    //                       |
    //                       the refresh will execute at this time
    //
    // So the exact delay is T - S - |cache_age|, but we don't have S here.
    //
    // |last_update_| was a bit before S, so if
    // elapsed = now - |last_update_| then the delay is more than
    // |offset_from_last_refresh| - elapsed.
    //
    // The delay is also less than offset_from_last_refresh, because some time
    // already elapsed. Additionally, if the cache was already considered old
    // when the schedule was performed then its age at that time has been
    // discounted from the delay. So the delay is a bit less than
    // |offset_from_last_refresh - cache_age|.
    EXPECT_GE(GetLastDelay(), offset_from_last_refresh - (now - last_update_));
    EXPECT_LE(GetLastDelay(), offset_from_last_refresh - cache_age);
  }

  void CheckInitialRefresh(bool with_invalidations) const {
#if defined(OS_ANDROID)
    // Android takes the cache age into account for the initial fetch.
    // Usually the cache age is ignored for the initial refresh, but Android
    // uses it to restrain from refreshing on every startup.
    base::TimeDelta rate = base::TimeDelta::FromMilliseconds(
        with_invalidations
            ? CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs
            : kPolicyRefreshRate);
    CheckTimingWithAge(rate,
                       base::TimeDelta::FromMinutes(kInitialCacheAgeMinutes));
#else
    // Other platforms refresh immediately.
    EXPECT_EQ(base::TimeDelta(), GetLastDelay());
#endif
  }

  base::MessageLoop loop_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  // Base time for the refresh that the scheduler should be using.
  base::Time last_update_;
};

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshNoPolicy) {
  store_.policy_.reset();
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshUnmanaged) {
  store_.policy_->set_state(em::PolicyData::UNMANAGED);
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  CheckTiming(CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedNotYetFetched) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
  CheckInitialRefresh(false);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, InitialRefreshManagedAlreadyFetched) {
  last_update_ = base::Time::NowFromSystemTime();
  client_.SetPolicy(PolicyNamespaceKey(dm_protocol::kChromeUserPolicyType,
                                       std::string()),
      em::PolicyFetchResponse());
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  CheckTiming(kPolicyRefreshRate);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
}

TEST_F(CloudPolicyRefreshSchedulerTest, Unregistered) {
  client_.SetDMToken(std::string());
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  client_.NotifyPolicyFetched();
  client_.NotifyRegistrationStateChanged();
  client_.NotifyClientError();
  scheduler->SetRefreshDelay(12 * 60 * 60 * 1000);
  store_.NotifyStoreLoaded();
  store_.NotifyStoreError();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(CloudPolicyRefreshSchedulerTest, RefreshSoonRateLimit) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(CreateRefreshScheduler());
  // Max out the request rate.
  for (int i = 0; i < 5; ++i) {
    EXPECT_CALL(client_, FetchPolicy()).Times(1);
    scheduler->RefreshSoon();
    task_runner_->RunUntilIdle();
    Mock::VerifyAndClearExpectations(&client_);
  }
  // The next refresh is throttled.
  EXPECT_CALL(client_, FetchPolicy()).Times(0);
  scheduler->RefreshSoon();
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsAvailable) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(&client_, &store_, task_runner_));
  scheduler->SetRefreshDelay(kPolicyRefreshRate);

  // The scheduler is currently waiting for the invalidations service to
  // initialize.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());

  // Signal that invalidations are available. The scheduler is currently
  // waiting for any pending invalidations to be received.
  scheduler->SetInvalidationServiceAvailability(true);
  EXPECT_EQ(2u, task_runner_->GetPendingTasks().size());

  // Run the invalidation service timeout task.
  EXPECT_CALL(client_, FetchPolicy()).Times(0);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // The initial refresh is scheduled.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  CheckInitialRefresh(true);

  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // Complete that fetch.
  last_update_ = base::Time::NowFromSystemTime();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsNotAvailable) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(&client_, &store_, task_runner_));
  scheduler->SetRefreshDelay(kPolicyRefreshRate);

  // The scheduler is currently waiting for the invalidations service to
  // initialize.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());

  // Signal that invalidations are not available. The scheduler will keep
  // waiting for us.
  for (int i = 0; i < 10; ++i) {
    scheduler->SetInvalidationServiceAvailability(false);
    EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  }

  // Run the timeout task.
  EXPECT_CALL(client_, FetchPolicy()).Times(0);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // This scheduled the initial refresh.
  CheckInitialRefresh(false);

  // Perform that fetch now.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // Complete that fetch.
  last_update_ = base::Time::NowFromSystemTime();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled at the normal rate.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsOffAndOn) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(&client_, &store_, task_runner_));
  scheduler->SetRefreshDelay(kPolicyRefreshRate);
  scheduler->SetInvalidationServiceAvailability(true);
  // Initial fetch.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
  last_update_ = base::Time::NowFromSystemTime();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  // Flush that task.
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // If the service goes down and comes back up before the timeout then a
  // refresh is rescheduled at the lower rate again; after executing all
  // pending tasks only 1 fetch is performed.
  EXPECT_CALL(client_, FetchPolicy()).Times(0);
  scheduler->SetInvalidationServiceAvailability(false);
  scheduler->SetInvalidationServiceAvailability(true);
  // Run the invalidation service timeout task.
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);
  // The next refresh has been scheduled using a lower refresh rate.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(CloudPolicyRefreshSchedulerTest, InvalidationsDisconnected) {
  scoped_ptr<CloudPolicyRefreshScheduler> scheduler(
      new CloudPolicyRefreshScheduler(&client_, &store_, task_runner_));
  scheduler->SetRefreshDelay(kPolicyRefreshRate);
  scheduler->SetInvalidationServiceAvailability(true);
  // Initial fetch.
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);
  last_update_ = base::Time::NowFromSystemTime();
  client_.NotifyPolicyFetched();

  // The next refresh has been scheduled using a lower refresh rate.
  // Flush that task.
  CheckTiming(CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs);
  EXPECT_CALL(client_, FetchPolicy()).Times(1);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);

  // If the service goes down then the refresh scheduler falls back on the
  // default polling rate after a timeout.
  EXPECT_CALL(client_, FetchPolicy()).Times(0);
  scheduler->SetInvalidationServiceAvailability(false);
  task_runner_->RunPendingTasks();
  Mock::VerifyAndClearExpectations(&client_);
  // The next refresh has been scheduled at the normal rate.
  CheckTiming(kPolicyRefreshRate);
}

class CloudPolicyRefreshSchedulerSteadyStateTest
    : public CloudPolicyRefreshSchedulerTest {
 protected:
  CloudPolicyRefreshSchedulerSteadyStateTest() {}

  virtual void SetUp() OVERRIDE {
    refresh_scheduler_.reset(CreateRefreshScheduler());
    refresh_scheduler_->SetRefreshDelay(kPolicyRefreshRate);
    CloudPolicyRefreshSchedulerTest::SetUp();
    last_update_ = base::Time::NowFromSystemTime();
    client_.NotifyPolicyFetched();
    CheckTiming(kPolicyRefreshRate);
  }

  scoped_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;
};

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnPolicyFetched) {
  client_.NotifyPolicyFetched();
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnRegistrationStateChanged) {
  client_.SetDMToken("new_token");
  client_.NotifyRegistrationStateChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());

  task_runner_->ClearPendingTasks();
  client_.SetDMToken(std::string());
  client_.NotifyRegistrationStateChanged();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreLoaded) {
  store_.NotifyStoreLoaded();
  CheckTiming(kPolicyRefreshRate);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnStoreError) {
  task_runner_->ClearPendingTasks();
  store_.NotifyStoreError();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, RefreshDelayChange) {
  const int delay_short_ms = 5 * 60 * 1000;
  refresh_scheduler_->SetRefreshDelay(delay_short_ms);
  CheckTiming(CloudPolicyRefreshScheduler::kRefreshDelayMinMs);

  const int delay_ms = 12 * 60 * 60 * 1000;
  refresh_scheduler_->SetRefreshDelay(delay_ms);
  CheckTiming(delay_ms);

  const int delay_long_ms = 20 * 24 * 60 * 60 * 1000;
  refresh_scheduler_->SetRefreshDelay(delay_long_ms);
  CheckTiming(CloudPolicyRefreshScheduler::kRefreshDelayMaxMs);
}

TEST_F(CloudPolicyRefreshSchedulerSteadyStateTest, OnIPAddressChanged) {
  NotifyIPAddressChanged();
  CheckTiming(kPolicyRefreshRate);

  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  NotifyIPAddressChanged();
  EXPECT_EQ(GetLastDelay(), base::TimeDelta());
}

struct ClientErrorTestParam {
  DeviceManagementStatus client_error;
  int64 expected_delay_ms;
  int backoff_factor;
};

static const ClientErrorTestParam kClientErrorTestCases[] = {
  { DM_STATUS_REQUEST_INVALID,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_REQUEST_FAILED,
    CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2 },
  { DM_STATUS_TEMPORARY_UNAVAILABLE,
    CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs, 2 },
  { DM_STATUS_HTTP_STATUS_ERROR,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_RESPONSE_DECODING_ERROR,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
    CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs, 1 },
  { DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
    -1, 1 },
  { DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
    -1, 1 },
  { DM_STATUS_SERVICE_ACTIVATION_PENDING,
    kPolicyRefreshRate, 1 },
  { DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
    -1, 1 },
  { DM_STATUS_SERVICE_MISSING_LICENSES,
    -1, 1 },
  { DM_STATUS_SERVICE_DEVICE_ID_CONFLICT,
    -1, 1 },
  { DM_STATUS_SERVICE_POLICY_NOT_FOUND,
    kPolicyRefreshRate, 1 },
};

class CloudPolicyRefreshSchedulerClientErrorTest
    : public CloudPolicyRefreshSchedulerSteadyStateTest,
      public testing::WithParamInterface<ClientErrorTestParam> {
};

TEST_P(CloudPolicyRefreshSchedulerClientErrorTest, OnClientError) {
  client_.SetStatus(GetParam().client_error);
  task_runner_->ClearPendingTasks();

  // See whether the error triggers the right refresh delay.
  int64 expected_delay_ms = GetParam().expected_delay_ms;
  client_.NotifyClientError();
  if (expected_delay_ms >= 0) {
    CheckTiming(expected_delay_ms);

    // Check whether exponential backoff is working as expected and capped at
    // the regular refresh rate (if applicable).
    do {
      expected_delay_ms *= GetParam().backoff_factor;
      last_update_ = base::Time::NowFromSystemTime();
      client_.NotifyClientError();
      CheckTiming(std::max(std::min(expected_delay_ms, kPolicyRefreshRate),
                           GetParam().expected_delay_ms));
    } while (GetParam().backoff_factor > 1 &&
             expected_delay_ms <= kPolicyRefreshRate);
  } else {
    EXPECT_EQ(base::TimeDelta(), GetLastDelay());
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  }
}

INSTANTIATE_TEST_CASE_P(CloudPolicyRefreshSchedulerClientErrorTest,
                        CloudPolicyRefreshSchedulerClientErrorTest,
                        testing::ValuesIn(kClientErrorTestCases));

}  // namespace policy
