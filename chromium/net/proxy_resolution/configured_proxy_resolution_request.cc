// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/configured_proxy_resolution_request.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"

namespace net {

ConfiguredProxyResolutionRequest::ConfiguredProxyResolutionRequest(
    ConfiguredProxyResolutionService* service,
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log)
    : service_(service),
      user_callback_(std::move(user_callback)),
      results_(results),
      url_(url),
      method_(method),
      network_anonymization_key_(network_anonymization_key),
      net_log_(net_log),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(!user_callback_.is_null());
}

ConfiguredProxyResolutionRequest::~ConfiguredProxyResolutionRequest() {
  if (service_) {
    service_->RemovePendingRequest(this);
    net_log_.AddEvent(NetLogEventType::CANCELLED);

    if (is_started())
      CancelResolveJob();

    // This should be emitted last, after any message |CancelResolveJob()| may
    // trigger.
    net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  }
}

// Starts the resolve proxy request.
int ConfiguredProxyResolutionRequest::Start() {
  DCHECK(!was_completed());
  DCHECK(!is_started());

  DCHECK(service_->config_);
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(
      service_->config_->traffic_annotation());

  if (service_->ApplyPacBypassRules(url_, results_))
    return OK;

  return service_->GetProxyResolver()->GetProxyForURL(
      url_, network_anonymization_key_, results_,
      base::BindOnce(&ConfiguredProxyResolutionRequest::QueryComplete,
                     base::Unretained(this)),
      &resolve_job_, net_log_);
}

void ConfiguredProxyResolutionRequest::
    StartAndCompleteCheckingForSynchronous() {
  int rv = service_->TryToCompleteSynchronously(url_, results_);
  if (rv == ERR_IO_PENDING)
    rv = Start();
  if (rv != ERR_IO_PENDING)
    QueryComplete(rv);
}

void ConfiguredProxyResolutionRequest::CancelResolveJob() {
  DCHECK(is_started());
  // The request may already be running in the resolver.
  resolve_job_.reset();
  DCHECK(!is_started());
}

int ConfiguredProxyResolutionRequest::QueryDidComplete(int result_code) {
  DCHECK(!was_completed());

  // Clear |resolve_job_| so is_started() returns false while
  // DidFinishResolvingProxy() runs.
  resolve_job_.reset();

  // Detach `service_` and `results_` before `DidFinishResolvingProxy()` in case
  // a sibling callback synchronously destroys `this` or `results` (avoids
  // double-removal in our destructor and dangling raw_ptrs).
  ConfiguredProxyResolutionService* service = std::exchange(service_, nullptr);
  ProxyInfo* results = std::exchange(results_, nullptr);
  base::WeakPtr<ConfiguredProxyResolutionRequest> weak_this =
      weak_factory_.GetWeakPtr();

  // Note that DidFinishResolvingProxy might modify `results`.
  int rv =
      service->DidFinishResolvingProxy(url_, network_anonymization_key_,
                                       method_, results, result_code, net_log_);
  if (!weak_this) {
    return ERR_ABORTED;
  }

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results->set_proxy_resolve_start_time(creation_time_);
  results->set_proxy_resolve_end_time(base::TimeTicks::Now());

  // If annotation is not already set, e.g. through TryToCompleteSynchronously
  // function, use in-progress-resolve annotation.
  if (!results->traffic_annotation().is_valid())
    results->set_traffic_annotation(traffic_annotation_);

  // If proxy is set without error, ensure that an annotation is provided.
  if (result_code != ERR_ABORTED && !rv)
    DCHECK(results->traffic_annotation().is_valid());

  // Reset the state associated with in-progress-resolve.
  traffic_annotation_.reset();

  return rv;
}

int ConfiguredProxyResolutionRequest::QueryDidCompleteSynchronously(
    int result_code) {
  return QueryDidComplete(result_code);
}

LoadState ConfiguredProxyResolutionRequest::GetLoadState() const {
  LoadState load_state = LOAD_STATE_IDLE;
  if (service_ && service_->GetLoadStateIfAvailable(&load_state))
    return load_state;

  if (is_started())
    return resolve_job_->GetLoadState();
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

// Callback for when the ProxyResolver request has completed.
void ConfiguredProxyResolutionRequest::QueryComplete(int result_code) {
  CHECK(!was_completed());

  // Remove `this` from the service before notifying it of completion to
  // prevent re-entrant dispatch if the service re-initializes synchronously.
  service_->RemovePendingRequest(this);

  base::WeakPtr<ConfiguredProxyResolutionRequest> weak_this =
      weak_factory_.GetWeakPtr();
  result_code = QueryDidComplete(result_code);
  if (!weak_this) {
    return;
  }

  CompletionOnceCallback callback = std::move(user_callback_);
  user_callback_.Reset();
  std::move(callback).Run(result_code);
}

}  // namespace net
