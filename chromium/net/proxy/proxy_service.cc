// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"
#include "net/proxy/multi_threaded_proxy_resolver.h"
#include "net/proxy/network_delegate_error_observer.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_script_decider.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "net/proxy/proxy_config_service_win.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#elif defined(OS_IOS)
#include "net/proxy/proxy_config_service_ios.h"
#include "net/proxy/proxy_resolver_mac.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_config_service_mac.h"
#include "net/proxy/proxy_resolver_mac.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/proxy/proxy_config_service_linux.h"
#elif defined(OS_ANDROID)
#include "net/proxy/proxy_config_service_android.h"
#endif

#if defined(SPDY_PROXY_AUTH_ORIGIN)
#include "base/metrics/histogram.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {

// When the IP address changes we don't immediately re-run proxy auto-config.
// Instead, we  wait for |kDelayAfterNetworkChangesMs| before
// attempting to re-valuate proxy auto-config.
//
// During this time window, any resolve requests sent to the ProxyService will
// be queued. Once we have waited the required amount of them, the proxy
// auto-config step will be run, and the queued requests resumed.
//
// The reason we play this game is that our signal for detecting network
// changes (NetworkChangeNotifier) may fire *before* the system's networking
// dependencies are fully configured. This is a problem since it means if
// we were to run proxy auto-config right away, it could fail due to spurious
// DNS failures. (see http://crbug.com/50779 for more details.)
//
// By adding the wait window, we give things a better chance to get properly
// set up. Network failures can happen at any time though, so we additionally
// poll the PAC script for changes, which will allow us to recover from these
// sorts of problems.
const int64 kDelayAfterNetworkChangesMs = 2000;

// This is the default policy for polling the PAC script.
//
// In response to a failure, the poll intervals are:
//    0: 8 seconds  (scheduled on timer)
//    1: 32 seconds
//    2: 2 minutes
//    3+: 4 hours
//
// In response to a success, the poll intervals are:
//    0+: 12 hours
//
// Only the 8 second poll is scheduled on a timer, the rest happen in response
// to network activity (and hence will take longer than the written time).
//
// Explanation for these values:
//
// TODO(eroman): These values are somewhat arbitrary, and need to be tuned
// using some histograms data. Trying to be conservative so as not to break
// existing setups when deployed. A simple exponential retry scheme would be
// more elegant, but places more load on server.
//
// The motivation for trying quickly after failures (8 seconds) is to recover
// from spurious network failures, which are common after the IP address has
// just changed (like DNS failing to resolve). The next 32 second boundary is
// to try and catch other VPN weirdness which anecdotally I have seen take
// 10+ seconds for some users.
//
// The motivation for re-trying after a success is to check for possible
// content changes to the script, or to the WPAD auto-discovery results. We are
// not very aggressive with these checks so as to minimize the risk of
// overloading existing PAC setups. Moreover it is unlikely that PAC scripts
// change very frequently in existing setups. More research is needed to
// motivate what safe values are here, and what other user agents do.
//
// Comparison to other browsers:
//
// In Firefox the PAC URL is re-tried on failures according to
// network.proxy.autoconfig_retry_interval_min and
// network.proxy.autoconfig_retry_interval_max. The defaults are 5 seconds and
// 5 minutes respectively. It doubles the interval at each attempt.
//
// TODO(eroman): Figure out what Internet Explorer does.
class DefaultPollPolicy : public ProxyService::PacPollPolicy {
 public:
  DefaultPollPolicy() {}

  virtual Mode GetNextDelay(int initial_error,
                            TimeDelta current_delay,
                            TimeDelta* next_delay) const OVERRIDE {
    if (initial_error != OK) {
      // Re-try policy for failures.
      const int kDelay1Seconds = 8;
      const int kDelay2Seconds = 32;
      const int kDelay3Seconds = 2 * 60;  // 2 minutes
      const int kDelay4Seconds = 4 * 60 * 60;  // 4 Hours

      // Initial poll.
      if (current_delay < TimeDelta()) {
        *next_delay = TimeDelta::FromSeconds(kDelay1Seconds);
        return MODE_USE_TIMER;
      }
      switch (current_delay.InSeconds()) {
        case kDelay1Seconds:
          *next_delay = TimeDelta::FromSeconds(kDelay2Seconds);
          return MODE_START_AFTER_ACTIVITY;
        case kDelay2Seconds:
          *next_delay = TimeDelta::FromSeconds(kDelay3Seconds);
          return MODE_START_AFTER_ACTIVITY;
        default:
          *next_delay = TimeDelta::FromSeconds(kDelay4Seconds);
          return MODE_START_AFTER_ACTIVITY;
      }
    } else {
      // Re-try policy for succeses.
      *next_delay = TimeDelta::FromHours(12);
      return MODE_START_AFTER_ACTIVITY;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPollPolicy);
};

// Config getter that always returns direct settings.
class ProxyConfigServiceDirect : public ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual ConfigAvailability GetLatestProxyConfig(ProxyConfig* config)
      OVERRIDE {
    *config = ProxyConfig::CreateDirect();
    config->set_source(PROXY_CONFIG_SOURCE_UNKNOWN);
    return CONFIG_VALID;
  }
};

// Proxy resolver that fails every time.
class ProxyResolverNull : public ProxyResolver {
 public:
  ProxyResolverNull() : ProxyResolver(false /*expects_pac_bytes*/) {}

  // ProxyResolver implementation.
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             const CompletionCallback& callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE {
    return ERR_NOT_IMPLEMENTED;
  }

  virtual void CancelRequest(RequestHandle request) OVERRIDE {
    NOTREACHED();
  }

  virtual LoadState GetLoadState(RequestHandle request) const OVERRIDE {
    NOTREACHED();
    return LOAD_STATE_IDLE;
  }

  virtual void CancelSetPacScript() OVERRIDE {
    NOTREACHED();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& /*script_data*/,
      const CompletionCallback& /*callback*/) OVERRIDE {
    return ERR_NOT_IMPLEMENTED;
  }
};

// ProxyResolver that simulates a PAC script which returns
// |pac_string| for every single URL.
class ProxyResolverFromPacString : public ProxyResolver {
 public:
  explicit ProxyResolverFromPacString(const std::string& pac_string)
      : ProxyResolver(false /*expects_pac_bytes*/),
        pac_string_(pac_string) {}

  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             const CompletionCallback& callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE {
    results->UsePacString(pac_string_);
    return OK;
  }

  virtual void CancelRequest(RequestHandle request) OVERRIDE {
    NOTREACHED();
  }

  virtual LoadState GetLoadState(RequestHandle request) const OVERRIDE {
    NOTREACHED();
    return LOAD_STATE_IDLE;
  }

  virtual void CancelSetPacScript() OVERRIDE {
    NOTREACHED();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      const CompletionCallback& callback) OVERRIDE {
    return OK;
  }

 private:
  const std::string pac_string_;
};

// Creates ProxyResolvers using a platform-specific implementation.
class ProxyResolverFactoryForSystem : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryForSystem()
      : ProxyResolverFactory(false /*expects_pac_bytes*/) {}

  virtual ProxyResolver* CreateProxyResolver() OVERRIDE {
    DCHECK(IsSupported());
#if defined(OS_WIN)
    return new ProxyResolverWinHttp();
#elif defined(OS_MACOSX)
    return new ProxyResolverMac();
#else
    NOTREACHED();
    return NULL;
#endif
  }

  static bool IsSupported() {
#if defined(OS_WIN) || defined(OS_MACOSX)
    return true;
#else
    return false;
#endif
  }
};

// Returns NetLog parameters describing a proxy configuration change.
base::Value* NetLogProxyConfigChangedCallback(
    const ProxyConfig* old_config,
    const ProxyConfig* new_config,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // The "old_config" is optional -- the first notification will not have
  // any "previous" configuration.
  if (old_config->is_valid())
    dict->Set("old_config", old_config->ToValue());
  dict->Set("new_config", new_config->ToValue());
  return dict;
}

base::Value* NetLogBadProxyListCallback(const ProxyRetryInfoMap* retry_info,
                                        NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::ListValue* list = new base::ListValue();

  for (ProxyRetryInfoMap::const_iterator iter = retry_info->begin();
       iter != retry_info->end(); ++iter) {
    list->Append(new base::StringValue(iter->first));
  }
  dict->Set("bad_proxy_list", list);
  return dict;
}

// Returns NetLog parameters on a successfuly proxy resolution.
base::Value* NetLogFinishedResolvingProxyCallback(
    ProxyInfo* result,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("pac_string", result->ToPacString());
  return dict;
}

#if defined(OS_CHROMEOS)
class UnsetProxyConfigService : public ProxyConfigService {
 public:
  UnsetProxyConfigService() {}
  virtual ~UnsetProxyConfigService() {}

  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual ConfigAvailability GetLatestProxyConfig(
      ProxyConfig* config) OVERRIDE {
    return CONFIG_UNSET;
  }
};
#endif

}  // namespace

// ProxyService::InitProxyResolver --------------------------------------------

// This glues together two asynchronous steps:
//   (1) ProxyScriptDecider -- try to fetch/validate a sequence of PAC scripts
//       to figure out what we should configure against.
//   (2) Feed the fetched PAC script into the ProxyResolver.
//
// InitProxyResolver is a single-use class which encapsulates cancellation as
// part of its destructor. Start() or StartSkipDecider() should be called just
// once. The instance can be destroyed at any time, and the request will be
// cancelled.

class ProxyService::InitProxyResolver {
 public:
  InitProxyResolver()
      : proxy_resolver_(NULL),
        next_state_(STATE_NONE) {
  }

  ~InitProxyResolver() {
    // Note that the destruction of ProxyScriptDecider will automatically cancel
    // any outstanding work.
    if (next_state_ == STATE_SET_PAC_SCRIPT_COMPLETE) {
      proxy_resolver_->CancelSetPacScript();
    }
  }

  // Begins initializing the proxy resolver; calls |callback| when done.
  int Start(ProxyResolver* proxy_resolver,
            ProxyScriptFetcher* proxy_script_fetcher,
            DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
            NetLog* net_log,
            const ProxyConfig& config,
            TimeDelta wait_delay,
            const CompletionCallback& callback) {
    DCHECK_EQ(STATE_NONE, next_state_);
    proxy_resolver_ = proxy_resolver;

    decider_.reset(new ProxyScriptDecider(
        proxy_script_fetcher, dhcp_proxy_script_fetcher, net_log));
    config_ = config;
    wait_delay_ = wait_delay;
    callback_ = callback;

    next_state_ = STATE_DECIDE_PROXY_SCRIPT;
    return DoLoop(OK);
  }

  // Similar to Start(), however it skips the ProxyScriptDecider stage. Instead
  // |effective_config|, |decider_result| and |script_data| will be used as the
  // inputs for initializing the ProxyResolver.
  int StartSkipDecider(ProxyResolver* proxy_resolver,
                       const ProxyConfig& effective_config,
                       int decider_result,
                       ProxyResolverScriptData* script_data,
                       const CompletionCallback& callback) {
    DCHECK_EQ(STATE_NONE, next_state_);
    proxy_resolver_ = proxy_resolver;

    effective_config_ = effective_config;
    script_data_ = script_data;
    callback_ = callback;

    if (decider_result != OK)
      return decider_result;

    next_state_ = STATE_SET_PAC_SCRIPT;
    return DoLoop(OK);
  }

  // Returns the proxy configuration that was selected by ProxyScriptDecider.
  // Should only be called upon completion of the initialization.
  const ProxyConfig& effective_config() const {
    DCHECK_EQ(STATE_NONE, next_state_);
    return effective_config_;
  }

  // Returns the PAC script data that was selected by ProxyScriptDecider.
  // Should only be called upon completion of the initialization.
  ProxyResolverScriptData* script_data() {
    DCHECK_EQ(STATE_NONE, next_state_);
    return script_data_.get();
  }

  LoadState GetLoadState() const {
    if (next_state_ == STATE_DECIDE_PROXY_SCRIPT_COMPLETE) {
      // In addition to downloading, this state may also include the stall time
      // after network change events (kDelayAfterNetworkChangesMs).
      return LOAD_STATE_DOWNLOADING_PROXY_SCRIPT;
    }
    return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
  }

 private:
  enum State {
    STATE_NONE,
    STATE_DECIDE_PROXY_SCRIPT,
    STATE_DECIDE_PROXY_SCRIPT_COMPLETE,
    STATE_SET_PAC_SCRIPT,
    STATE_SET_PAC_SCRIPT_COMPLETE,
  };

  int DoLoop(int result) {
    DCHECK_NE(next_state_, STATE_NONE);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_DECIDE_PROXY_SCRIPT:
          DCHECK_EQ(OK, rv);
          rv = DoDecideProxyScript();
          break;
        case STATE_DECIDE_PROXY_SCRIPT_COMPLETE:
          rv = DoDecideProxyScriptComplete(rv);
          break;
        case STATE_SET_PAC_SCRIPT:
          DCHECK_EQ(OK, rv);
          rv = DoSetPacScript();
          break;
        case STATE_SET_PAC_SCRIPT_COMPLETE:
          rv = DoSetPacScriptComplete(rv);
          break;
        default:
          NOTREACHED() << "bad state: " << state;
          rv = ERR_UNEXPECTED;
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
    return rv;
  }

  int DoDecideProxyScript() {
    next_state_ = STATE_DECIDE_PROXY_SCRIPT_COMPLETE;

    return decider_->Start(
        config_, wait_delay_, proxy_resolver_->expects_pac_bytes(),
        base::Bind(&InitProxyResolver::OnIOCompletion, base::Unretained(this)));
  }

  int DoDecideProxyScriptComplete(int result) {
    if (result != OK)
      return result;

    effective_config_ = decider_->effective_config();
    script_data_ = decider_->script_data();

    next_state_ = STATE_SET_PAC_SCRIPT;
    return OK;
  }

  int DoSetPacScript() {
    DCHECK(script_data_.get());
    // TODO(eroman): Should log this latency to the NetLog.
    next_state_ = STATE_SET_PAC_SCRIPT_COMPLETE;
    return proxy_resolver_->SetPacScript(
        script_data_,
        base::Bind(&InitProxyResolver::OnIOCompletion, base::Unretained(this)));
  }

  int DoSetPacScriptComplete(int result) {
    return result;
  }

  void OnIOCompletion(int result) {
    DCHECK_NE(STATE_NONE, next_state_);
    int rv = DoLoop(result);
    if (rv != ERR_IO_PENDING)
      DoCallback(rv);
  }

  void DoCallback(int result) {
    DCHECK_NE(ERR_IO_PENDING, result);
    callback_.Run(result);
  }

  ProxyConfig config_;
  ProxyConfig effective_config_;
  scoped_refptr<ProxyResolverScriptData> script_data_;
  TimeDelta wait_delay_;
  scoped_ptr<ProxyScriptDecider> decider_;
  ProxyResolver* proxy_resolver_;
  CompletionCallback callback_;
  State next_state_;

  DISALLOW_COPY_AND_ASSIGN(InitProxyResolver);
};

// ProxyService::ProxyScriptDeciderPoller -------------------------------------

// This helper class encapsulates the logic to schedule and run periodic
// background checks to see if the PAC script (or effective proxy configuration)
// has changed. If a change is detected, then the caller will be notified via
// the ChangeCallback.
class ProxyService::ProxyScriptDeciderPoller {
 public:
  typedef base::Callback<void(int, ProxyResolverScriptData*,
                              const ProxyConfig&)> ChangeCallback;

  // Builds a poller helper, and starts polling for updates. Whenever a change
  // is observed, |callback| will be invoked with the details.
  //
  //   |config| specifies the (unresolved) proxy configuration to poll.
  //   |proxy_resolver_expects_pac_bytes| the type of proxy resolver we expect
  //                                      to use the resulting script data with
  //                                      (so it can choose the right format).
  //   |proxy_script_fetcher| this pointer must remain alive throughout our
  //                          lifetime. It is the dependency that will be used
  //                          for downloading proxy scripts.
  //   |dhcp_proxy_script_fetcher| similar to |proxy_script_fetcher|, but for
  //                               the DHCP dependency.
  //   |init_net_error| This is the initial network error (possibly success)
  //                    encountered by the first PAC fetch attempt. We use it
  //                    to schedule updates more aggressively if the initial
  //                    fetch resulted in an error.
  //   |init_script_data| the initial script data from the PAC fetch attempt.
  //                      This is the baseline used to determine when the
  //                      script's contents have changed.
  //   |net_log| the NetLog to log progress into.
  ProxyScriptDeciderPoller(ChangeCallback callback,
                           const ProxyConfig& config,
                           bool proxy_resolver_expects_pac_bytes,
                           ProxyScriptFetcher* proxy_script_fetcher,
                           DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
                           int init_net_error,
                           ProxyResolverScriptData* init_script_data,
                           NetLog* net_log)
      : weak_factory_(this),
        change_callback_(callback),
        config_(config),
        proxy_resolver_expects_pac_bytes_(proxy_resolver_expects_pac_bytes),
        proxy_script_fetcher_(proxy_script_fetcher),
        dhcp_proxy_script_fetcher_(dhcp_proxy_script_fetcher),
        last_error_(init_net_error),
        last_script_data_(init_script_data),
        last_poll_time_(TimeTicks::Now()) {
    // Set the initial poll delay.
    next_poll_mode_ = poll_policy()->GetNextDelay(
        last_error_, TimeDelta::FromSeconds(-1), &next_poll_delay_);
    TryToStartNextPoll(false);
  }

  void OnLazyPoll() {
    // We have just been notified of network activity. Use this opportunity to
    // see if we can start our next poll.
    TryToStartNextPoll(true);
  }

  static const PacPollPolicy* set_policy(const PacPollPolicy* policy) {
    const PacPollPolicy* prev = poll_policy_;
    poll_policy_ = policy;
    return prev;
  }

 private:
  // Returns the effective poll policy (the one injected by unit-tests, or the
  // default).
  const PacPollPolicy* poll_policy() {
    if (poll_policy_)
      return poll_policy_;
    return &default_poll_policy_;
  }

  void StartPollTimer() {
    DCHECK(!decider_.get());

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ProxyScriptDeciderPoller::DoPoll,
                   weak_factory_.GetWeakPtr()),
        next_poll_delay_);
  }

  void TryToStartNextPoll(bool triggered_by_activity) {
    switch (next_poll_mode_) {
      case PacPollPolicy::MODE_USE_TIMER:
        if (!triggered_by_activity)
          StartPollTimer();
        break;

      case PacPollPolicy::MODE_START_AFTER_ACTIVITY:
        if (triggered_by_activity && !decider_.get()) {
          TimeDelta elapsed_time = TimeTicks::Now() - last_poll_time_;
          if (elapsed_time >= next_poll_delay_)
            DoPoll();
        }
        break;
    }
  }

  void DoPoll() {
    last_poll_time_ = TimeTicks::Now();

    // Start the proxy script decider to see if anything has changed.
    // TODO(eroman): Pass a proper NetLog rather than NULL.
    decider_.reset(new ProxyScriptDecider(
        proxy_script_fetcher_, dhcp_proxy_script_fetcher_, NULL));
    int result = decider_->Start(
        config_, TimeDelta(), proxy_resolver_expects_pac_bytes_,
        base::Bind(&ProxyScriptDeciderPoller::OnProxyScriptDeciderCompleted,
                   base::Unretained(this)));

    if (result != ERR_IO_PENDING)
      OnProxyScriptDeciderCompleted(result);
  }

  void OnProxyScriptDeciderCompleted(int result) {
    if (HasScriptDataChanged(result, decider_->script_data())) {
      // Something has changed, we must notify the ProxyService so it can
      // re-initialize its ProxyResolver. Note that we post a notification task
      // rather than calling it directly -- this is done to avoid an ugly
      // destruction sequence, since |this| might be destroyed as a result of
      // the notification.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ProxyScriptDeciderPoller::NotifyProxyServiceOfChange,
                     weak_factory_.GetWeakPtr(),
                     result,
                     make_scoped_refptr(decider_->script_data()),
                     decider_->effective_config()));
      return;
    }

    decider_.reset();

    // Decide when the next poll should take place, and possibly start the
    // next timer.
    next_poll_mode_ = poll_policy()->GetNextDelay(
        last_error_, next_poll_delay_, &next_poll_delay_);
    TryToStartNextPoll(false);
  }

  bool HasScriptDataChanged(int result, ProxyResolverScriptData* script_data) {
    if (result != last_error_) {
      // Something changed -- it was failing before and now it succeeded, or
      // conversely it succeeded before and now it failed. Or it failed in
      // both cases, however the specific failure error codes differ.
      return true;
    }

    if (result != OK) {
      // If it failed last time and failed again with the same error code this
      // time, then nothing has actually changed.
      return false;
    }

    // Otherwise if it succeeded both this time and last time, we need to look
    // closer and see if we ended up downloading different content for the PAC
    // script.
    return !script_data->Equals(last_script_data_.get());
  }

  void NotifyProxyServiceOfChange(
      int result,
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      const ProxyConfig& effective_config) {
    // Note that |this| may be deleted after calling into the ProxyService.
    change_callback_.Run(result, script_data.get(), effective_config);
  }

  base::WeakPtrFactory<ProxyScriptDeciderPoller> weak_factory_;

  ChangeCallback change_callback_;
  ProxyConfig config_;
  bool proxy_resolver_expects_pac_bytes_;
  ProxyScriptFetcher* proxy_script_fetcher_;
  DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher_;

  int last_error_;
  scoped_refptr<ProxyResolverScriptData> last_script_data_;

  scoped_ptr<ProxyScriptDecider> decider_;
  TimeDelta next_poll_delay_;
  PacPollPolicy::Mode next_poll_mode_;

  TimeTicks last_poll_time_;

  // Polling policy injected by unit-tests. Otherwise this is NULL and the
  // default policy will be used.
  static const PacPollPolicy* poll_policy_;

  const DefaultPollPolicy default_poll_policy_;

  DISALLOW_COPY_AND_ASSIGN(ProxyScriptDeciderPoller);
};

// static
const ProxyService::PacPollPolicy*
    ProxyService::ProxyScriptDeciderPoller::poll_policy_ = NULL;

// ProxyService::PacRequest ---------------------------------------------------

class ProxyService::PacRequest
    : public base::RefCounted<ProxyService::PacRequest> {
 public:
    PacRequest(ProxyService* service,
               const GURL& url,
               ProxyInfo* results,
               const net::CompletionCallback& user_callback,
               const BoundNetLog& net_log)
      : service_(service),
        user_callback_(user_callback),
        results_(results),
        url_(url),
        resolve_job_(NULL),
        config_id_(ProxyConfig::kInvalidConfigID),
        config_source_(PROXY_CONFIG_SOURCE_UNKNOWN),
        net_log_(net_log) {
    DCHECK(!user_callback.is_null());
  }

  // Starts the resolve proxy request.
  int Start() {
    DCHECK(!was_cancelled());
    DCHECK(!is_started());

    DCHECK(service_->config_.is_valid());

    config_id_ = service_->config_.id();
    config_source_ = service_->config_.source();
    proxy_resolve_start_time_ = TimeTicks::Now();

    return resolver()->GetProxyForURL(
        url_, results_,
        base::Bind(&PacRequest::QueryComplete, base::Unretained(this)),
        &resolve_job_, net_log_);
  }

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_;
  }

  void StartAndCompleteCheckingForSynchronous() {
    int rv = service_->TryToCompleteSynchronously(url_, results_);
    if (rv == ERR_IO_PENDING)
      rv = Start();
    if (rv != ERR_IO_PENDING)
      QueryComplete(rv);
  }

  void CancelResolveJob() {
    DCHECK(is_started());
    // The request may already be running in the resolver.
    resolver()->CancelRequest(resolve_job_);
    resolve_job_ = NULL;
    DCHECK(!is_started());
  }

  void Cancel() {
    net_log_.AddEvent(NetLog::TYPE_CANCELLED);

    if (is_started())
      CancelResolveJob();

    // Mark as cancelled, to prevent accessing this again later.
    service_ = NULL;
    user_callback_.Reset();
    results_ = NULL;

    net_log_.EndEvent(NetLog::TYPE_PROXY_SERVICE);
  }

  // Returns true if Cancel() has been called.
  bool was_cancelled() const {
    return user_callback_.is_null();
  }

  // Helper to call after ProxyResolver completion (both synchronous and
  // asynchronous). Fixes up the result that is to be returned to user.
  int QueryDidComplete(int result_code) {
    DCHECK(!was_cancelled());

    // Note that DidFinishResolvingProxy might modify |results_|.
    int rv = service_->DidFinishResolvingProxy(results_, result_code, net_log_);

    // Make a note in the results which configuration was in use at the
    // time of the resolve.
    results_->config_id_ = config_id_;
    results_->config_source_ = config_source_;
    results_->did_use_pac_script_ = true;
    results_->proxy_resolve_start_time_ = proxy_resolve_start_time_;
    results_->proxy_resolve_end_time_ = TimeTicks::Now();

    // Reset the state associated with in-progress-resolve.
    resolve_job_ = NULL;
    config_id_ = ProxyConfig::kInvalidConfigID;
    config_source_ = PROXY_CONFIG_SOURCE_UNKNOWN;

    return rv;
  }

  BoundNetLog* net_log() { return &net_log_; }

  LoadState GetLoadState() const {
    if (is_started())
      return resolver()->GetLoadState(resolve_job_);
    return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
  }

 private:
  friend class base::RefCounted<ProxyService::PacRequest>;

  ~PacRequest() {}

  // Callback for when the ProxyResolver request has completed.
  void QueryComplete(int result_code) {
    result_code = QueryDidComplete(result_code);

    // Remove this completed PacRequest from the service's pending list.
    /// (which will probably cause deletion of |this|).
    if (!user_callback_.is_null()) {
      net::CompletionCallback callback = user_callback_;
      service_->RemovePendingRequest(this);
      callback.Run(result_code);
    }
  }

  ProxyResolver* resolver() const { return service_->resolver_.get(); }

  // Note that we don't hold a reference to the ProxyService. Outstanding
  // requests are cancelled during ~ProxyService, so this is guaranteed
  // to be valid throughout our lifetime.
  ProxyService* service_;
  net::CompletionCallback user_callback_;
  ProxyInfo* results_;
  GURL url_;
  ProxyResolver::RequestHandle resolve_job_;
  ProxyConfig::ID config_id_;  // The config id when the resolve was started.
  ProxyConfigSource config_source_;  // The source of proxy settings.
  BoundNetLog net_log_;
  // Time when the PAC is started.  Cached here since resetting ProxyInfo also
  // clears the proxy times.
  TimeTicks proxy_resolve_start_time_;
};

// ProxyService ---------------------------------------------------------------

ProxyService::ProxyService(ProxyConfigService* config_service,
                           ProxyResolver* resolver,
                           NetLog* net_log)
    : resolver_(resolver),
      next_config_id_(1),
      current_state_(STATE_NONE) ,
      net_log_(net_log),
      stall_proxy_auto_config_delay_(TimeDelta::FromMilliseconds(
          kDelayAfterNetworkChangesMs)) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddDNSObserver(this);
  ResetConfigService(config_service);
}

// static
ProxyService* ProxyService::CreateUsingSystemProxyResolver(
    ProxyConfigService* proxy_config_service,
    size_t num_pac_threads,
    NetLog* net_log) {
  DCHECK(proxy_config_service);

  if (!ProxyResolverFactoryForSystem::IsSupported()) {
    LOG(WARNING) << "PAC support disabled because there is no "
                    "system implementation";
    return CreateWithoutProxyResolver(proxy_config_service, net_log);
  }

  if (num_pac_threads == 0)
    num_pac_threads = kDefaultNumPacThreads;

  ProxyResolver* proxy_resolver = new MultiThreadedProxyResolver(
      new ProxyResolverFactoryForSystem(), num_pac_threads);

  return new ProxyService(proxy_config_service, proxy_resolver, net_log);
}

// static
ProxyService* ProxyService::CreateWithoutProxyResolver(
    ProxyConfigService* proxy_config_service,
    NetLog* net_log) {
  return new ProxyService(proxy_config_service,
                          new ProxyResolverNull(),
                          net_log);
}

// static
ProxyService* ProxyService::CreateFixed(const ProxyConfig& pc) {
  // TODO(eroman): This isn't quite right, won't work if |pc| specifies
  //               a PAC script.
  return CreateUsingSystemProxyResolver(new ProxyConfigServiceFixed(pc),
                                        0, NULL);
}

// static
ProxyService* ProxyService::CreateFixed(const std::string& proxy) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy);
  return ProxyService::CreateFixed(proxy_config);
}

// static
ProxyService* ProxyService::CreateDirect() {
  return CreateDirectWithNetLog(NULL);
}

ProxyService* ProxyService::CreateDirectWithNetLog(NetLog* net_log) {
  // Use direct connections.
  return new ProxyService(new ProxyConfigServiceDirect, new ProxyResolverNull,
                          net_log);
}

// static
ProxyService* ProxyService::CreateFixedFromPacResult(
    const std::string& pac_string) {

  // We need the settings to contain an "automatic" setting, otherwise the
  // ProxyResolver dependency we give it will never be used.
  scoped_ptr<ProxyConfigService> proxy_config_service(
      new ProxyConfigServiceFixed(ProxyConfig::CreateAutoDetect()));

  scoped_ptr<ProxyResolver> proxy_resolver(
      new ProxyResolverFromPacString(pac_string));

  return new ProxyService(proxy_config_service.release(),
                          proxy_resolver.release(),
                          NULL);
}

int ProxyService::ResolveProxy(const GURL& raw_url,
                               ProxyInfo* result,
                               const net::CompletionCallback& callback,
                               PacRequest** pac_request,
                               const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  net_log.BeginEvent(NetLog::TYPE_PROXY_SERVICE);

  // Notify our polling-based dependencies that a resolve is taking place.
  // This way they can schedule their polls in response to network activity.
  config_service_->OnLazyPoll();
  if (script_poller_.get())
     script_poller_->OnLazyPoll();

  if (current_state_ == STATE_NONE)
    ApplyProxyConfigIfAvailable();

  // Strip away any reference fragments and the username/password, as they
  // are not relevant to proxy resolution.
  GURL url = SimplifyUrlForRequest(raw_url);

  // Check if the request can be completed right away. (This is the case when
  // using a direct connection for example).
  int rv = TryToCompleteSynchronously(url, result);
  if (rv != ERR_IO_PENDING)
    return DidFinishResolvingProxy(result, rv, net_log);

  scoped_refptr<PacRequest> req(
      new PacRequest(this, url, result, callback, net_log));

  if (current_state_ == STATE_READY) {
    // Start the resolve request.
    rv = req->Start();
    if (rv != ERR_IO_PENDING)
      return req->QueryDidComplete(rv);
  } else {
    req->net_log()->BeginEvent(NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC);
  }

  DCHECK_EQ(ERR_IO_PENDING, rv);
  DCHECK(!ContainsPendingRequest(req.get()));
  pending_requests_.push_back(req);

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |pac_request|.
  if (pac_request)
    *pac_request = req.get();
  return rv;  // ERR_IO_PENDING
}

int ProxyService::TryToCompleteSynchronously(const GURL& url,
                                             ProxyInfo* result) {
  DCHECK_NE(STATE_NONE, current_state_);

  if (current_state_ != STATE_READY)
    return ERR_IO_PENDING;  // Still initializing.

  DCHECK_NE(config_.id(), ProxyConfig::kInvalidConfigID);

  // If it was impossible to fetch or parse the PAC script, we cannot complete
  // the request here and bail out.
  if (permanent_error_ != OK)
    return permanent_error_;

  if (config_.HasAutomaticSettings())
    return ERR_IO_PENDING;  // Must submit the request to the proxy resolver.

  // Use the manual proxy settings.
  config_.proxy_rules().Apply(url, result);
  result->config_source_ = config_.source();
  result->config_id_ = config_.id();
  return OK;
}

ProxyService::~ProxyService() {
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveDNSObserver(this);
  config_service_->RemoveObserver(this);

  // Cancel any inprogress requests.
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    (*it)->Cancel();
  }
}

void ProxyService::SuspendAllPendingRequests() {
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    PacRequest* req = it->get();
    if (req->is_started()) {
      req->CancelResolveJob();

      req->net_log()->BeginEvent(
          NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC);
    }
  }
}

void ProxyService::SetReady() {
  DCHECK(!init_proxy_resolver_.get());
  current_state_ = STATE_READY;

  // Make a copy in case |this| is deleted during the synchronous completion
  // of one of the requests. If |this| is deleted then all of the PacRequest
  // instances will be Cancel()-ed.
  PendingRequests pending_copy = pending_requests_;

  for (PendingRequests::iterator it = pending_copy.begin();
       it != pending_copy.end();
       ++it) {
    PacRequest* req = it->get();
    if (!req->is_started() && !req->was_cancelled()) {
      req->net_log()->EndEvent(NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC);

      // Note that we re-check for synchronous completion, in case we are
      // no longer using a ProxyResolver (can happen if we fell-back to manual).
      req->StartAndCompleteCheckingForSynchronous();
    }
  }
}

void ProxyService::ApplyProxyConfigIfAvailable() {
  DCHECK_EQ(STATE_NONE, current_state_);

  config_service_->OnLazyPoll();

  // If we have already fetched the configuration, start applying it.
  if (fetched_config_.is_valid()) {
    InitializeUsingLastFetchedConfig();
    return;
  }

  // Otherwise we need to first fetch the configuration.
  current_state_ = STATE_WAITING_FOR_PROXY_CONFIG;

  // Retrieve the current proxy configuration from the ProxyConfigService.
  // If a configuration is not available yet, we will get called back later
  // by our ProxyConfigService::Observer once it changes.
  ProxyConfig config;
  ProxyConfigService::ConfigAvailability availability =
      config_service_->GetLatestProxyConfig(&config);
  if (availability != ProxyConfigService::CONFIG_PENDING)
    OnProxyConfigChanged(config, availability);
}

void ProxyService::OnInitProxyResolverComplete(int result) {
  DCHECK_EQ(STATE_WAITING_FOR_INIT_PROXY_RESOLVER, current_state_);
  DCHECK(init_proxy_resolver_.get());
  DCHECK(fetched_config_.HasAutomaticSettings());
  config_ = init_proxy_resolver_->effective_config();

  // At this point we have decided which proxy settings to use (i.e. which PAC
  // script if any). We start up a background poller to periodically revisit
  // this decision. If the contents of the PAC script change, or if the
  // result of proxy auto-discovery changes, this poller will notice it and
  // will trigger a re-initialization using the newly discovered PAC.
  script_poller_.reset(new ProxyScriptDeciderPoller(
      base::Bind(&ProxyService::InitializeUsingDecidedConfig,
                 base::Unretained(this)),
      fetched_config_,
      resolver_->expects_pac_bytes(),
      proxy_script_fetcher_.get(),
      dhcp_proxy_script_fetcher_.get(),
      result,
      init_proxy_resolver_->script_data(),
      NULL));

  init_proxy_resolver_.reset();

  if (result != OK) {
    if (fetched_config_.pac_mandatory()) {
      VLOG(1) << "Failed configuring with mandatory PAC script, blocking all "
                 "traffic.";
      config_ = fetched_config_;
      result = ERR_MANDATORY_PROXY_CONFIGURATION_FAILED;
    } else {
      VLOG(1) << "Failed configuring with PAC script, falling-back to manual "
                 "proxy servers.";
      config_ = fetched_config_;
      config_.ClearAutomaticSettings();
      result = OK;
    }
  }
  permanent_error_ = result;

  // TODO(eroman): Make this ID unique in the case where configuration changed
  //               due to ProxyScriptDeciderPoller.
  config_.set_id(fetched_config_.id());
  config_.set_source(fetched_config_.source());

  // Resume any requests which we had to defer until the PAC script was
  // downloaded.
  SetReady();
}

int ProxyService::ReconsiderProxyAfterError(const GURL& url,
                                            ProxyInfo* result,
                                            const CompletionCallback& callback,
                                            PacRequest** pac_request,
                                            const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());

  // Check to see if we have a new config since ResolveProxy was called.  We
  // want to re-run ResolveProxy in two cases: 1) we have a new config, or 2) a
  // direct connection failed and we never tried the current config.

  bool re_resolve = result->config_id_ != config_.id();

  if (re_resolve) {
    // If we have a new config or the config was never tried, we delete the
    // list of bad proxies and we try again.
    proxy_retry_info_.clear();
    return ResolveProxy(url, result, callback, pac_request, net_log);
  }

#if defined(SPDY_PROXY_AUTH_ORIGIN)
  if (result->proxy_server().isDataReductionProxy()) {
    RecordDataReductionProxyBypassInfo(
        true, result->proxy_server(), ERROR_BYPASS);
  } else if (result->proxy_server().isDataReductionProxyFallback()) {
    RecordDataReductionProxyBypassInfo(
        false, result->proxy_server(), ERROR_BYPASS);
  }
#endif

  // We don't have new proxy settings to try, try to fallback to the next proxy
  // in the list.
  bool did_fallback = result->Fallback(net_log);

  // Return synchronous failure if there is nothing left to fall-back to.
  // TODO(eroman): This is a yucky API, clean it up.
  return did_fallback ? OK : ERR_FAILED;
}

bool ProxyService::MarkProxiesAsBad(
    const ProxyInfo& result,
    base::TimeDelta retry_delay,
    const ProxyServer& another_bad_proxy,
    const BoundNetLog& net_log) {
  result.proxy_list_.UpdateRetryInfoOnFallback(&proxy_retry_info_, retry_delay,
                                               another_bad_proxy,
                                               net_log);
  return result.proxy_list_.HasUntriedProxies(proxy_retry_info_);
}

void ProxyService::ReportSuccess(const ProxyInfo& result) {
  DCHECK(CalledOnValidThread());

  const ProxyRetryInfoMap& new_retry_info = result.proxy_retry_info();
  if (new_retry_info.empty())
    return;

  for (ProxyRetryInfoMap::const_iterator iter = new_retry_info.begin();
       iter != new_retry_info.end(); ++iter) {
    ProxyRetryInfoMap::iterator existing = proxy_retry_info_.find(iter->first);
    if (existing == proxy_retry_info_.end())
      proxy_retry_info_[iter->first] = iter->second;
    else if (existing->second.bad_until < iter->second.bad_until)
      existing->second.bad_until = iter->second.bad_until;
  }
  if (net_log_) {
    net_log_->AddGlobalEntry(
        NetLog::TYPE_BAD_PROXY_LIST_REPORTED,
        base::Bind(&NetLogBadProxyListCallback, &new_retry_info));
  }
}

void ProxyService::CancelPacRequest(PacRequest* req) {
  DCHECK(CalledOnValidThread());
  DCHECK(req);
  req->Cancel();
  RemovePendingRequest(req);
}

LoadState ProxyService::GetLoadState(const PacRequest* req) const {
  CHECK(req);
  if (current_state_ == STATE_WAITING_FOR_INIT_PROXY_RESOLVER)
    return init_proxy_resolver_->GetLoadState();
  return req->GetLoadState();
}

bool ProxyService::ContainsPendingRequest(PacRequest* req) {
  PendingRequests::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), req);
  return pending_requests_.end() != it;
}

void ProxyService::RemovePendingRequest(PacRequest* req) {
  DCHECK(ContainsPendingRequest(req));
  PendingRequests::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), req);
  pending_requests_.erase(it);
}

int ProxyService::DidFinishResolvingProxy(ProxyInfo* result,
                                          int result_code,
                                          const BoundNetLog& net_log) {
  // Log the result of the proxy resolution.
  if (result_code == OK) {
    // When logging all events is enabled, dump the proxy list.
    if (net_log.IsLoggingAllEvents()) {
      net_log.AddEvent(
          NetLog::TYPE_PROXY_SERVICE_RESOLVED_PROXY_LIST,
          base::Bind(&NetLogFinishedResolvingProxyCallback, result));
    }
    result->DeprioritizeBadProxies(proxy_retry_info_);
  } else {
    net_log.AddEventWithNetErrorCode(
        NetLog::TYPE_PROXY_SERVICE_RESOLVED_PROXY_LIST, result_code);

    if (!config_.pac_mandatory()) {
      // Fall-back to direct when the proxy resolver fails. This corresponds
      // with a javascript runtime error in the PAC script.
      //
      // This implicit fall-back to direct matches Firefox 3.5 and
      // Internet Explorer 8. For more information, see:
      //
      // http://www.chromium.org/developers/design-documents/proxy-settings-fallback
      result->UseDirect();
      result_code = OK;
    } else {
      result_code = ERR_MANDATORY_PROXY_CONFIGURATION_FAILED;
    }
  }

  net_log.EndEvent(NetLog::TYPE_PROXY_SERVICE);
  return result_code;
}

void ProxyService::SetProxyScriptFetchers(
    ProxyScriptFetcher* proxy_script_fetcher,
    DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher) {
  DCHECK(CalledOnValidThread());
  State previous_state = ResetProxyConfig(false);
  proxy_script_fetcher_.reset(proxy_script_fetcher);
  dhcp_proxy_script_fetcher_.reset(dhcp_proxy_script_fetcher);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

ProxyScriptFetcher* ProxyService::GetProxyScriptFetcher() const {
  DCHECK(CalledOnValidThread());
  return proxy_script_fetcher_.get();
}

ProxyService::State ProxyService::ResetProxyConfig(bool reset_fetched_config) {
  DCHECK(CalledOnValidThread());
  State previous_state = current_state_;

  permanent_error_ = OK;
  proxy_retry_info_.clear();
  script_poller_.reset();
  init_proxy_resolver_.reset();
  SuspendAllPendingRequests();
  config_ = ProxyConfig();
  if (reset_fetched_config)
    fetched_config_ = ProxyConfig();
  current_state_ = STATE_NONE;

  return previous_state;
}

void ProxyService::ResetConfigService(
    ProxyConfigService* new_proxy_config_service) {
  DCHECK(CalledOnValidThread());
  State previous_state = ResetProxyConfig(true);

  // Release the old configuration service.
  if (config_service_.get())
    config_service_->RemoveObserver(this);

  // Set the new configuration service.
  config_service_.reset(new_proxy_config_service);
  config_service_->AddObserver(this);

  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ProxyService::PurgeMemory() {
  DCHECK(CalledOnValidThread());
  if (resolver_.get())
    resolver_->PurgeMemory();
}

void ProxyService::ForceReloadProxyConfig() {
  DCHECK(CalledOnValidThread());
  ResetProxyConfig(false);
  ApplyProxyConfigIfAvailable();
}

// static
ProxyConfigService* ProxyService::CreateSystemProxyConfigService(
    base::SingleThreadTaskRunner* io_thread_task_runner,
    base::MessageLoop* file_loop) {
#if defined(OS_WIN)
  return new ProxyConfigServiceWin();
#elif defined(OS_IOS)
  return new ProxyConfigServiceIOS();
#elif defined(OS_MACOSX)
  return new ProxyConfigServiceMac(io_thread_task_runner);
#elif defined(OS_CHROMEOS)
  LOG(ERROR) << "ProxyConfigService for ChromeOS should be created in "
             << "profile_io_data.cc::CreateProxyConfigService and this should "
             << "be used only for examples.";
  return new UnsetProxyConfigService;
#elif defined(OS_LINUX)
  ProxyConfigServiceLinux* linux_config_service =
      new ProxyConfigServiceLinux();

  // Assume we got called on the thread that runs the default glib
  // main loop, so the current thread is where we should be running
  // gconf calls from.
  scoped_refptr<base::SingleThreadTaskRunner> glib_thread_task_runner =
      base::ThreadTaskRunnerHandle::Get();

  // The file loop should be a MessageLoopForIO on Linux.
  DCHECK_EQ(base::MessageLoop::TYPE_IO, file_loop->type());

  // Synchronously fetch the current proxy config (since we are
  // running on glib_default_loop). Additionally register for
  // notifications (delivered in either |glib_default_loop| or
  // |file_loop|) to keep us updated when the proxy config changes.
  linux_config_service->SetupAndFetchInitialConfig(
      glib_thread_task_runner.get(),
      io_thread_task_runner,
      static_cast<base::MessageLoopForIO*>(file_loop));

  return linux_config_service;
#elif defined(OS_ANDROID)
  return new ProxyConfigServiceAndroid(
      io_thread_task_runner,
      base::MessageLoop::current()->message_loop_proxy());
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return new ProxyConfigServiceDirect();
#endif
}

// static
const ProxyService::PacPollPolicy* ProxyService::set_pac_script_poll_policy(
    const PacPollPolicy* policy) {
  return ProxyScriptDeciderPoller::set_policy(policy);
}

// static
scoped_ptr<ProxyService::PacPollPolicy>
  ProxyService::CreateDefaultPacPollPolicy() {
  return scoped_ptr<PacPollPolicy>(new DefaultPollPolicy());
}

#if defined(SPDY_PROXY_AUTH_ORIGIN)
void ProxyService::RecordDataReductionProxyBypassInfo(
    bool is_primary,
    const ProxyServer& proxy_server,
    DataReductionProxyBypassEventType bypass_type) const {
  // Only record UMA if the proxy isn't already on the retry list.
  if (proxy_retry_info_.find(proxy_server.ToURI()) != proxy_retry_info_.end())
    return;

  if (is_primary) {
    UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassInfoPrimary",
                              bypass_type, BYPASS_EVENT_TYPE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassInfoFallback",
                              bypass_type, BYPASS_EVENT_TYPE_MAX);
  }
}
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

void ProxyService::OnProxyConfigChanged(
    const ProxyConfig& config,
    ProxyConfigService::ConfigAvailability availability) {
  // Retrieve the current proxy configuration from the ProxyConfigService.
  // If a configuration is not available yet, we will get called back later
  // by our ProxyConfigService::Observer once it changes.
  ProxyConfig effective_config;
  switch (availability) {
    case ProxyConfigService::CONFIG_PENDING:
      // ProxyConfigService implementors should never pass CONFIG_PENDING.
      NOTREACHED() << "Proxy config change with CONFIG_PENDING availability!";
      return;
    case ProxyConfigService::CONFIG_VALID:
      effective_config = config;
      break;
    case ProxyConfigService::CONFIG_UNSET:
      effective_config = ProxyConfig::CreateDirect();
      break;
  }

  // Emit the proxy settings change to the NetLog stream.
  if (net_log_) {
    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_PROXY_CONFIG_CHANGED,
        base::Bind(&NetLogProxyConfigChangedCallback,
                   &fetched_config_, &effective_config));
  }

  // Set the new configuration as the most recently fetched one.
  fetched_config_ = effective_config;
  fetched_config_.set_id(1);  // Needed for a later DCHECK of is_valid().

  InitializeUsingLastFetchedConfig();
}

void ProxyService::InitializeUsingLastFetchedConfig() {
  ResetProxyConfig(false);

  DCHECK(fetched_config_.is_valid());

  // Increment the ID to reflect that the config has changed.
  fetched_config_.set_id(next_config_id_++);

  if (!fetched_config_.HasAutomaticSettings()) {
    config_ = fetched_config_;
    SetReady();
    return;
  }

  // Start downloading + testing the PAC scripts for this new configuration.
  current_state_ = STATE_WAITING_FOR_INIT_PROXY_RESOLVER;

  // If we changed networks recently, we should delay running proxy auto-config.
  TimeDelta wait_delay =
      stall_proxy_autoconfig_until_ - TimeTicks::Now();

  init_proxy_resolver_.reset(new InitProxyResolver());
  int rv = init_proxy_resolver_->Start(
      resolver_.get(),
      proxy_script_fetcher_.get(),
      dhcp_proxy_script_fetcher_.get(),
      net_log_,
      fetched_config_,
      wait_delay,
      base::Bind(&ProxyService::OnInitProxyResolverComplete,
                 base::Unretained(this)));

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ProxyService::InitializeUsingDecidedConfig(
    int decider_result,
    ProxyResolverScriptData* script_data,
    const ProxyConfig& effective_config) {
  DCHECK(fetched_config_.is_valid());
  DCHECK(fetched_config_.HasAutomaticSettings());

  ResetProxyConfig(false);

  current_state_ = STATE_WAITING_FOR_INIT_PROXY_RESOLVER;

  init_proxy_resolver_.reset(new InitProxyResolver());
  int rv = init_proxy_resolver_->StartSkipDecider(
      resolver_.get(),
      effective_config,
      decider_result,
      script_data,
      base::Bind(&ProxyService::OnInitProxyResolverComplete,
                 base::Unretained(this)));

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ProxyService::OnIPAddressChanged() {
  // See the comment block by |kDelayAfterNetworkChangesMs| for info.
  stall_proxy_autoconfig_until_ =
      TimeTicks::Now() + stall_proxy_auto_config_delay_;

  State previous_state = ResetProxyConfig(false);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ProxyService::OnDNSChanged() {
  OnIPAddressChanged();
}

SyncProxyServiceHelper::SyncProxyServiceHelper(
    base::MessageLoop* io_message_loop,
    ProxyService* proxy_service)
    : io_message_loop_(io_message_loop),
      proxy_service_(proxy_service),
      event_(false, false),
      callback_(base::Bind(&SyncProxyServiceHelper::OnCompletion,
                           base::Unretained(this))) {
  DCHECK(io_message_loop_ != base::MessageLoop::current());
}

int SyncProxyServiceHelper::ResolveProxy(const GURL& url,
                                         ProxyInfo* proxy_info,
                                         const BoundNetLog& net_log) {
  DCHECK(io_message_loop_ != base::MessageLoop::current());

  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SyncProxyServiceHelper::StartAsyncResolve, this, url,
                 net_log));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

int SyncProxyServiceHelper::ReconsiderProxyAfterError(
    const GURL& url, ProxyInfo* proxy_info, const BoundNetLog& net_log) {
  DCHECK(io_message_loop_ != base::MessageLoop::current());

  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SyncProxyServiceHelper::StartAsyncReconsider, this, url,
                 net_log));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

SyncProxyServiceHelper::~SyncProxyServiceHelper() {}

void SyncProxyServiceHelper::StartAsyncResolve(const GURL& url,
                                               const BoundNetLog& net_log) {
  result_ = proxy_service_->ResolveProxy(
      url, &proxy_info_, callback_, NULL, net_log);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::StartAsyncReconsider(const GURL& url,
                                                  const BoundNetLog& net_log) {
  result_ = proxy_service_->ReconsiderProxyAfterError(
      url, &proxy_info_, callback_, NULL, net_log);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::OnCompletion(int rv) {
  result_ = rv;
  event_.Signal();
}

}  // namespace net
