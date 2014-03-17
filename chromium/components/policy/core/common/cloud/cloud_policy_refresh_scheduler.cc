// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_switches.h"

namespace policy {

namespace {

// The maximum rate at which to refresh policies.
const size_t kMaxRefreshesPerHour = 5;

// The maximum time to wait for the invalidations service to become available
// before starting to issue requests.
const int kWaitForInvalidationsTimeoutSeconds = 5;

}  // namespace

#if defined(OS_ANDROID)

const int64 CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
const int64 CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
// Delay for periodic refreshes when the invalidations service is available,
// in milliseconds.
// TODO(joaodasilva): increase this value once we're confident that the
// invalidations channel works as expected.
const int64 CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
const int64 CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs =
    5 * 60 * 1000;  // 5 minutes.
const int64 CloudPolicyRefreshScheduler::kRefreshDelayMinMs =
    30 * 60 * 1000;  // 30 minutes.
const int64 CloudPolicyRefreshScheduler::kRefreshDelayMaxMs =
    7 * 24 * 60 * 60 * 1000;  // 1 week.

#else

const int64 CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs =
    3 * 60 * 60 * 1000;  // 3 hours.
const int64 CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
// Delay for periodic refreshes when the invalidations service is available,
// in milliseconds.
// TODO(joaodasilva): increase this value once we're confident that the
// invalidations channel works as expected.
const int64 CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs =
    3 * 60 * 60 * 1000;  // 3 hours.
const int64 CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs =
    5 * 60 * 1000;  // 5 minutes.
const int64 CloudPolicyRefreshScheduler::kRefreshDelayMinMs =
    30 * 60 * 1000;  // 30 minutes.
const int64 CloudPolicyRefreshScheduler::kRefreshDelayMaxMs =
    24 * 60 * 60 * 1000;  // 1 day.

#endif

CloudPolicyRefreshScheduler::CloudPolicyRefreshScheduler(
    CloudPolicyClient* client,
    CloudPolicyStore* store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : client_(client),
      store_(store),
      task_runner_(task_runner),
      error_retry_delay_ms_(kInitialErrorRetryDelayMs),
      refresh_delay_ms_(kDefaultRefreshDelayMs),
      rate_limiter_(kMaxRefreshesPerHour,
                    base::TimeDelta::FromHours(1),
                    base::Bind(&CloudPolicyRefreshScheduler::RefreshNow,
                               base::Unretained(this)),
                    task_runner_,
                    scoped_ptr<base::TickClock>(new base::DefaultTickClock())),
      invalidations_available_(false),
      creation_time_(base::Time::NowFromSystemTime()) {
  client_->AddObserver(this);
  store_->AddObserver(this);
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  UpdateLastRefreshFromPolicy();

  // Give some time for the invalidation service to become available before the
  // first refresh if there is already policy present.
  if (store->has_policy())
    WaitForInvalidationService();
  else
    ScheduleRefresh();
}

CloudPolicyRefreshScheduler::~CloudPolicyRefreshScheduler() {
  store_->RemoveObserver(this);
  client_->RemoveObserver(this);
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void CloudPolicyRefreshScheduler::SetRefreshDelay(int64 refresh_delay) {
  refresh_delay_ms_ = std::min(std::max(refresh_delay, kRefreshDelayMinMs),
                               kRefreshDelayMaxMs);
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::RefreshSoon() {
  // An external consumer needs a policy update now (e.g. a new extension, or
  // the InvalidationService received a policy invalidation), so don't wait
  // before fetching anymore.
  wait_for_invalidations_timeout_callback_.Cancel();
  rate_limiter_.PostRequest();
}

void CloudPolicyRefreshScheduler::SetInvalidationServiceAvailability(
    bool is_available) {
  if (!creation_time_.is_null()) {
    base::TimeDelta elapsed = base::Time::NowFromSystemTime() - creation_time_;
    UMA_HISTOGRAM_MEDIUM_TIMES("Enterprise.PolicyInvalidationsStartupTime",
                               elapsed);
    creation_time_ = base::Time();
  }

  if (is_available == invalidations_available_) {
    // No change in state. If we're currently WaitingForInvalidationService
    // then the timeout task will eventually execute and trigger a reschedule;
    // let the InvalidationService keep retrying until that happens.
    return;
  }

  wait_for_invalidations_timeout_callback_.Cancel();
  invalidations_available_ = is_available;

  // Schedule a refresh since the refresh delay has been updated; however, allow
  // some time for the invalidation service to update. If it is now online, the
  // wait allows pending invalidations to be delivered. If it is now offline,
  // then the wait allows for the service to recover from transient failure
  // before falling back on the polling behavior.
  WaitForInvalidationService();
}

void CloudPolicyRefreshScheduler::OnPolicyFetched(CloudPolicyClient* client) {
  error_retry_delay_ms_ = kInitialErrorRetryDelayMs;

  // Schedule the next refresh.
  last_refresh_ = base::Time::NowFromSystemTime();
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  error_retry_delay_ms_ = kInitialErrorRetryDelayMs;

  // The client might have registered, so trigger an immediate refresh.
  RefreshNow();
}

void CloudPolicyRefreshScheduler::OnClientError(CloudPolicyClient* client) {
  // Save the status for below.
  DeviceManagementStatus status = client_->status();

  // Schedule an error retry if applicable.
  last_refresh_ = base::Time::NowFromSystemTime();
  ScheduleRefresh();

  // Update the retry delay.
  if (client->is_registered() &&
      (status == DM_STATUS_REQUEST_FAILED ||
       status == DM_STATUS_TEMPORARY_UNAVAILABLE)) {
    error_retry_delay_ms_ = std::min(error_retry_delay_ms_ * 2,
                                     refresh_delay_ms_);
  } else {
    error_retry_delay_ms_ = kInitialErrorRetryDelayMs;
  }
}

void CloudPolicyRefreshScheduler::OnStoreLoaded(CloudPolicyStore* store) {
  UpdateLastRefreshFromPolicy();

  // Re-schedule the next refresh in case the is_managed bit changed.
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::OnStoreError(CloudPolicyStore* store) {
  // If |store_| fails, the is_managed bit that it provides may become stale.
  // The best guess in that situation is to assume is_managed didn't change and
  // continue using the stale information. Thus, no specific response to a store
  // error is required. NB: Changes to is_managed fire OnStoreLoaded().
}

void CloudPolicyRefreshScheduler::OnIPAddressChanged() {
  if (client_->status() == DM_STATUS_REQUEST_FAILED)
    RefreshSoon();
}

void CloudPolicyRefreshScheduler::UpdateLastRefreshFromPolicy() {
  if (!last_refresh_.is_null())
    return;

  // If the client has already fetched policy, assume that happened recently. If
  // that assumption ever breaks, the proper thing to do probably is to move the
  // |last_refresh_| bookkeeping to CloudPolicyClient.
  if (!client_->responses().empty()) {
    last_refresh_ = base::Time::NowFromSystemTime();
    return;
  }

#if defined(OS_ANDROID)
  // Refreshing on Android:
  // - if no user is signed-in then the |client_| is never registered and
  //   nothing happens here.
  // - if the user is signed-in but isn't enterprise then the |client_| is
  //   never registered and nothing happens here.
  // - if the user is signed-in but isn't registered for policy yet then the
  //   |client_| isn't registered either; the UserPolicySigninService will try
  //   to register, and OnRegistrationStateChanged() will be invoked later.
  // - if the client is signed-in and has policy then its timestamp is used to
  //   determine when to perform the next fetch, which will be once the cached
  //   version is considered "old enough".
  //
  // If there is an old policy cache then a fetch will be performed "soon"; if
  // that fetch fails then a retry is attempted after a delay, with exponential
  // backoff. If those fetches keep failing then the cached timestamp is *not*
  // updated, and another fetch (and subsequent retries) will be attempted
  // again on the next startup.
  //
  // But if the cached policy is considered fresh enough then we try to avoid
  // fetching again on startup; the Android logic differs from the desktop in
  // this aspect.
  if (store_->has_policy() && store_->policy()->has_timestamp()) {
    last_refresh_ =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(store_->policy()->timestamp());
  }
#else
  // If there is a cached non-managed response, make sure to only re-query the
  // server after kUnmanagedRefreshDelayMs. NB: For existing policy, an
  // immediate refresh is intentional.
  if (store_->has_policy() && store_->policy()->has_timestamp() &&
      !store_->is_managed()) {
    last_refresh_ =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(store_->policy()->timestamp());
  }
#endif
}

void CloudPolicyRefreshScheduler::RefreshNow() {
  last_refresh_ = base::Time();
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::ScheduleRefresh() {
  // If the client isn't registered, there is nothing to do.
  if (!client_->is_registered()) {
    refresh_callback_.Cancel();
    return;
  }

  // Don't schedule anything yet if we're still waiting for the invalidations
  // service.
  if (WaitingForInvalidationService())
    return;

  // If policy invalidations are available then periodic updates are done at
  // a much lower rate; otherwise use the |refresh_delay_ms_| value.
  int64 refresh_delay_ms =
      invalidations_available_ ? kWithInvalidationsRefreshDelayMs
                               : refresh_delay_ms_;

  // If there is a registration, go by the client's status. That will tell us
  // what the appropriate refresh delay should be.
  switch (client_->status()) {
    case DM_STATUS_SUCCESS:
      if (store_->is_managed())
        RefreshAfter(refresh_delay_ms);
      else
        RefreshAfter(kUnmanagedRefreshDelayMs);
      return;
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      RefreshAfter(refresh_delay_ms);
      return;
    case DM_STATUS_REQUEST_FAILED:
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
      RefreshAfter(error_retry_delay_ms_);
      return;
    case DM_STATUS_REQUEST_INVALID:
    case DM_STATUS_HTTP_STATUS_ERROR:
    case DM_STATUS_RESPONSE_DECODING_ERROR:
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      RefreshAfter(kUnmanagedRefreshDelayMs);
      return;
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
    case DM_STATUS_SERVICE_MISSING_LICENSES:
    case DM_STATUS_SERVICE_DEPROVISIONED:
      // Need a re-registration, no use in retrying.
      refresh_callback_.Cancel();
      return;
  }

  NOTREACHED() << "Invalid client status " << client_->status();
  RefreshAfter(kUnmanagedRefreshDelayMs);
}

void CloudPolicyRefreshScheduler::PerformRefresh() {
  if (client_->is_registered()) {
    // Update |last_refresh_| so another fetch isn't triggered inadvertently.
    last_refresh_ = base::Time::NowFromSystemTime();

    // The result of this operation will be reported through a callback, at
    // which point the next refresh will be scheduled.
    client_->FetchPolicy();
    return;
  }

  // This should never happen, as the registration change should have been
  // handled via OnRegistrationStateChanged().
  NOTREACHED();
}

void CloudPolicyRefreshScheduler::RefreshAfter(int delta_ms) {
  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(delta_ms));
  refresh_callback_.Cancel();

  // Schedule the callback.
  base::TimeDelta delay =
      std::max((last_refresh_ + delta) - base::Time::NowFromSystemTime(),
               base::TimeDelta());
  refresh_callback_.Reset(
      base::Bind(&CloudPolicyRefreshScheduler::PerformRefresh,
                 base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, refresh_callback_.callback(), delay);
}

void CloudPolicyRefreshScheduler::WaitForInvalidationService() {
  DCHECK(!WaitingForInvalidationService());
  wait_for_invalidations_timeout_callback_.Reset(
      base::Bind(
          &CloudPolicyRefreshScheduler::OnWaitForInvalidationServiceTimeout,
          base::Unretained(this)));
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kWaitForInvalidationsTimeoutSeconds);
  // Do not wait for the invalidation service if the feature is disabled.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCloudPolicyPush)) {
    delay = base::TimeDelta();
  }
  task_runner_->PostDelayedTask(
      FROM_HERE,
      wait_for_invalidations_timeout_callback_.callback(),
      delay);
}

void CloudPolicyRefreshScheduler::OnWaitForInvalidationServiceTimeout() {
  wait_for_invalidations_timeout_callback_.Cancel();
  ScheduleRefresh();
}

bool CloudPolicyRefreshScheduler::WaitingForInvalidationService() const {
  return !wait_for_invalidations_timeout_callback_.IsCancelled();
}

}  // namespace policy
