// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVICE_H_
#define NET_PROXY_PROXY_SERVICE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"

class GURL;

namespace base {
class MessageLoop;
class SingleThreadTaskRunner;
class TimeDelta;
}  // namespace base

namespace net {

class DhcpProxyScriptFetcher;
class HostResolver;
class NetworkDelegate;
class ProxyResolver;
class ProxyResolverScriptData;
class ProxyScriptDecider;
class ProxyScriptFetcher;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverV8 for example.
class NET_EXPORT ProxyService : public NetworkChangeNotifier::IPAddressObserver,
                                public NetworkChangeNotifier::DNSObserver,
                                public ProxyConfigService::Observer,
                                NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  static const size_t kDefaultNumPacThreads = 4;

  // This interface defines the set of policies for when to poll the PAC
  // script for changes.
  //
  // The polling policy decides what the next poll delay should be in
  // milliseconds. It also decides how to wait for this delay -- either
  // by starting a timer to do the poll at exactly |next_delay_ms|
  // (MODE_USE_TIMER) or by waiting for the first network request issued after
  // |next_delay_ms| (MODE_START_AFTER_ACTIVITY).
  //
  // The timer method is more precise and guarantees that polling happens when
  // it was requested. However it has the disadvantage of causing spurious CPU
  // and network activity. It is a reasonable choice to use for short poll
  // intervals which only happen a couple times.
  //
  // However for repeated timers this will prevent the browser from going
  // idle. MODE_START_AFTER_ACTIVITY solves this problem by only polling in
  // direct response to network activity. The drawback to
  // MODE_START_AFTER_ACTIVITY is since the poll is initiated only after the
  // request is received, the first couple requests initiated after a long
  // period of inactivity will likely see a stale version of the PAC script
  // until the background polling gets a chance to update things.
  class NET_EXPORT_PRIVATE PacPollPolicy {
   public:
    enum Mode {
      MODE_USE_TIMER,
      MODE_START_AFTER_ACTIVITY,
    };

    virtual ~PacPollPolicy() {}

    // Decides the next poll delay. |current_delay| is the delay used
    // by the preceding poll, or a negative TimeDelta value if determining
    // the delay for the initial poll. |initial_error| is the network error
    // code that the last PAC fetch (or WPAD initialization) failed with,
    // or OK if it completed successfully. Implementations must set
    // |next_delay| to a non-negative value.
    virtual Mode GetNextDelay(int initial_error,
                              base::TimeDelta current_delay,
                              base::TimeDelta* next_delay) const = 0;
  };

  // The instance takes ownership of |config_service| and |resolver|.
  // |net_log| is a possibly NULL destination to send log events to. It must
  // remain alive for the lifetime of this ProxyService.
  ProxyService(ProxyConfigService* config_service,
               ProxyResolver* resolver,
               NetLog* net_log);

  virtual ~ProxyService();

  // Used internally to handle PAC queries.
  // TODO(eroman): consider naming this simply "Request".
  class PacRequest;

  // Returns ERR_IO_PENDING if the proxy information could not be provided
  // synchronously, to indicate that the result will be available when the
  // callback is run.  The callback is run on the thread that calls
  // ResolveProxy.
  //
  // The caller is responsible for ensuring that |results| and |callback|
  // remain valid until the callback is run or until |pac_request| is cancelled
  // via CancelPacRequest.  |pac_request| is only valid while the completion
  // callback is still pending. NULL can be passed for |pac_request| if
  // the caller will not need to cancel the request.
  //
  // We use the three possible proxy access types in the following order,
  // doing fallback if one doesn't work.  See "pac_script_decider.h"
  // for the specifics.
  //   1.  WPAD auto-detection
  //   2.  PAC URL
  //   3.  named proxy
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  int ResolveProxy(const GURL& url,
                   ProxyInfo* results,
                   const net::CompletionCallback& callback,
                   PacRequest** pac_request,
                   const BoundNetLog& net_log);

  // This method is called after a failure to connect or resolve a host name.
  // It gives the proxy service an opportunity to reconsider the proxy to use.
  // The |results| parameter contains the results returned by an earlier call
  // to ResolveProxy.  The semantics of this call are otherwise similar to
  // ResolveProxy.
  //
  // NULL can be passed for |pac_request| if the caller will not need to
  // cancel the request.
  //
  // Returns ERR_FAILED if there is not another proxy config to try.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* results,
                                const CompletionCallback& callback,
                                PacRequest** pac_request,
                                const BoundNetLog& net_log);

  // Explicitly trigger proxy fallback for the given |results| by updating our
  // list of bad proxies to include the first entry of |results|, and,
  // optionally, another bad proxy. Will retry after |retry_delay| if positive,
  // and will use the default proxy retry duration otherwise. Returns true if
  // there will be at least one proxy remaining in the list after fallback and
  // false otherwise.
  bool MarkProxiesAsBad(const ProxyInfo& results,
                        base::TimeDelta retry_delay,
                        const ProxyServer& another_bad_proxy,
                        const BoundNetLog& net_log);

  // Called to report that the last proxy connection succeeded.  If |proxy_info|
  // has a non empty proxy_retry_info map, the proxies that have been tried (and
  // failed) for this request will be marked as bad.
  void ReportSuccess(const ProxyInfo& proxy_info);

  // Call this method with a non-null |pac_request| to cancel the PAC request.
  void CancelPacRequest(PacRequest* pac_request);

  // Returns the LoadState for this |pac_request| which must be non-NULL.
  LoadState GetLoadState(const PacRequest* pac_request) const;

  // Sets the ProxyScriptFetcher and DhcpProxyScriptFetcher dependencies. This
  // is needed if the ProxyResolver is of type ProxyResolverWithoutFetch.
  // ProxyService takes ownership of both objects.
  void SetProxyScriptFetchers(
      ProxyScriptFetcher* proxy_script_fetcher,
      DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher);
  ProxyScriptFetcher* GetProxyScriptFetcher() const;

  // Tells this ProxyService to start using a new ProxyConfigService to
  // retrieve its ProxyConfig from. The new ProxyConfigService will immediately
  // be queried for new config info which will be used for all subsequent
  // ResolveProxy calls. ProxyService takes ownership of
  // |new_proxy_config_service|.
  void ResetConfigService(ProxyConfigService* new_proxy_config_service);

  // Tells the resolver to purge any memory it does not need.
  void PurgeMemory();


  // Returns the last configuration fetched from ProxyConfigService.
  const ProxyConfig& fetched_config() {
    return fetched_config_;
  }

  // Returns the current configuration being used by ProxyConfigService.
  const ProxyConfig& config() {
    return config_;
  }

  // Returns the map of proxies which have been marked as "bad".
  const ProxyRetryInfoMap& proxy_retry_info() const {
    return proxy_retry_info_;
  }

  // Clears the list of bad proxy servers that has been cached.
  void ClearBadProxiesCache() {
    proxy_retry_info_.clear();
  }

  // Forces refetching the proxy configuration, and applying it.
  // This re-does everything from fetching the system configuration,
  // to downloading and testing the PAC files.
  void ForceReloadProxyConfig();

  // Same as CreateProxyServiceUsingV8ProxyResolver, except it uses system
  // libraries for evaluating the PAC script if available, otherwise skips
  // proxy autoconfig.
  static ProxyService* CreateUsingSystemProxyResolver(
      ProxyConfigService* proxy_config_service,
      size_t num_pac_threads,
      NetLog* net_log);

  // Creates a ProxyService without support for proxy autoconfig.
  static ProxyService* CreateWithoutProxyResolver(
      ProxyConfigService* proxy_config_service,
      NetLog* net_log);

  // Convenience methods that creates a proxy service using the
  // specified fixed settings.
  static ProxyService* CreateFixed(const ProxyConfig& pc);
  static ProxyService* CreateFixed(const std::string& proxy);

  // Creates a proxy service that uses a DIRECT connection for all requests.
  static ProxyService* CreateDirect();
  // |net_log|'s lifetime must exceed ProxyService.
  static ProxyService* CreateDirectWithNetLog(NetLog* net_log);

  // This method is used by tests to create a ProxyService that returns a
  // hardcoded proxy fallback list (|pac_string|) for every URL.
  //
  // |pac_string| is a list of proxy servers, in the format that a PAC script
  // would return it. For example, "PROXY foobar:99; SOCKS fml:2; DIRECT"
  static ProxyService* CreateFixedFromPacResult(const std::string& pac_string);

  // Creates a config service appropriate for this platform that fetches the
  // system proxy settings.
  static ProxyConfigService* CreateSystemProxyConfigService(
      base::SingleThreadTaskRunner* io_thread_task_runner,
      base::MessageLoop* file_loop);

  // This method should only be used by unit tests.
  void set_stall_proxy_auto_config_delay(base::TimeDelta delay) {
    stall_proxy_auto_config_delay_ = delay;
  }

  // This method should only be used by unit tests. Returns the previously
  // active policy.
  static const PacPollPolicy* set_pac_script_poll_policy(
      const PacPollPolicy* policy);

  // This method should only be used by unit tests. Creates an instance
  // of the default internal PacPollPolicy used by ProxyService.
  static scoped_ptr<PacPollPolicy> CreateDefaultPacPollPolicy();

#if defined(SPDY_PROXY_AUTH_ORIGIN)
  // Values of the UMA DataReductionProxy.BypassInfo{Primary|Fallback}
  // histograms. This enum must remain synchronized with the enum of the same
  // name in metrics/histograms/histograms.xml.
  enum DataReductionProxyBypassEventType {
    // Bypass the proxy for less than 30 minutes.
    SHORT_BYPASS = 0,

    // Bypass the proxy for 30 minutes or more.
    LONG_BYPASS,

    // Bypass the proxy because of an internal server error.
    INTERNAL_SERVER_ERROR_BYPASS,

    // Bypass the proxy because of any other error.
    ERROR_BYPASS,

    // Bypass the proxy because responses appear not to be coming via it.
    MISSING_VIA_HEADER,

    // This must always be last.
    BYPASS_EVENT_TYPE_MAX
  };

  // Records a |DataReductionProxyBypassEventType| for either the data reduction
  // proxy (|is_primary| is true) or the data reduction proxy fallback.
  void RecordDataReductionProxyBypassInfo(
      bool is_primary,
      const ProxyServer& proxy_server,
      DataReductionProxyBypassEventType bypass_type) const;
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ProxyServiceTest, UpdateConfigAfterFailedAutodetect);
  FRIEND_TEST_ALL_PREFIXES(ProxyServiceTest, UpdateConfigFromPACToDirect);
  friend class PacRequest;
  class InitProxyResolver;
  class ProxyScriptDeciderPoller;

  // TODO(eroman): change this to a std::set. Note that this requires updating
  // some tests in proxy_service_unittest.cc such as:
  //   ProxyServiceTest.InitialPACScriptDownload
  // which expects requests to finish in the order they were added.
  typedef std::vector<scoped_refptr<PacRequest> > PendingRequests;

  enum State {
    STATE_NONE,
    STATE_WAITING_FOR_PROXY_CONFIG,
    STATE_WAITING_FOR_INIT_PROXY_RESOLVER,
    STATE_READY,
  };

  // Resets all the variables associated with the current proxy configuration,
  // and rewinds the current state to |STATE_NONE|. Returns the previous value
  // of |current_state_|.  If |reset_fetched_config| is true then
  // |fetched_config_| will also be reset, otherwise it will be left as-is.
  // Resetting it means that we will have to re-fetch the configuration from
  // the ProxyConfigService later.
  State ResetProxyConfig(bool reset_fetched_config);

  // Retrieves the current proxy configuration from the ProxyConfigService, and
  // starts initializing for it.
  void ApplyProxyConfigIfAvailable();

  // Callback for when the proxy resolver has been initialized with a
  // PAC script.
  void OnInitProxyResolverComplete(int result);

  // Returns ERR_IO_PENDING if the request cannot be completed synchronously.
  // Otherwise it fills |result| with the proxy information for |url|.
  // Completing synchronously means we don't need to query ProxyResolver.
  int TryToCompleteSynchronously(const GURL& url, ProxyInfo* result);

  // Cancels all of the requests sent to the ProxyResolver. These will be
  // restarted when calling SetReady().
  void SuspendAllPendingRequests();

  // Advances the current state to |STATE_READY|, and resumes any pending
  // requests which had been stalled waiting for initialization to complete.
  void SetReady();

  // Returns true if |pending_requests_| contains |req|.
  bool ContainsPendingRequest(PacRequest* req);

  // Removes |req| from the list of pending requests.
  void RemovePendingRequest(PacRequest* req);

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(ProxyInfo* result,
                              int result_code,
                              const BoundNetLog& net_log);

  // Start initialization using |fetched_config_|.
  void InitializeUsingLastFetchedConfig();

  // Start the initialization skipping past the "decision" phase.
  void InitializeUsingDecidedConfig(
      int decider_result,
      ProxyResolverScriptData* script_data,
      const ProxyConfig& effective_config);

  // NetworkChangeNotifier::IPAddressObserver
  // When this is called, we re-fetch PAC scripts and re-run WPAD.
  virtual void OnIPAddressChanged() OVERRIDE;

  // NetworkChangeNotifier::DNSObserver
  // We respond as above.
  virtual void OnDNSChanged() OVERRIDE;

  // ProxyConfigService::Observer
  virtual void OnProxyConfigChanged(
      const ProxyConfig& config,
      ProxyConfigService::ConfigAvailability availability) OVERRIDE;

  scoped_ptr<ProxyConfigService> config_service_;
  scoped_ptr<ProxyResolver> resolver_;

  // We store the proxy configuration that was last fetched from the
  // ProxyConfigService, as well as the resulting "effective" configuration.
  // The effective configuration is what we condense the original fetched
  // settings to after testing the various automatic settings (auto-detect
  // and custom PAC url).
  ProxyConfig fetched_config_;
  ProxyConfig config_;

  // Increasing ID to give to the next ProxyConfig that we set.
  int next_config_id_;

  // The time when the proxy configuration was last read from the system.
  base::TimeTicks config_last_update_time_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/inprogress requests.
  PendingRequests pending_requests_;

  // The fetcher to use when downloading PAC scripts for the ProxyResolver.
  // This dependency can be NULL if our ProxyResolver has no need for
  // external PAC script fetching.
  scoped_ptr<ProxyScriptFetcher> proxy_script_fetcher_;

  // The fetcher to use when attempting to download the most appropriate PAC
  // script configured in DHCP, if any. Can be NULL if the ProxyResolver has
  // no need for DHCP PAC script fetching.
  scoped_ptr<DhcpProxyScriptFetcher> dhcp_proxy_script_fetcher_;

  // Helper to download the PAC script (wpad + custom) and apply fallback rules.
  //
  // Note that the declaration is important here: |proxy_script_fetcher_| and
  // |proxy_resolver_| must outlive |init_proxy_resolver_|.
  scoped_ptr<InitProxyResolver> init_proxy_resolver_;

  // Helper to poll the PAC script for changes.
  scoped_ptr<ProxyScriptDeciderPoller> script_poller_;

  State current_state_;

  // Either OK or an ERR_* value indicating that a permanent error (e.g.
  // failed to fetch the PAC script) prevents proxy resolution.
  int permanent_error_;

  // This is the log where any events generated by |init_proxy_resolver_| are
  // sent to.
  NetLog* net_log_;

  // The earliest time at which we should run any proxy auto-config. (Used to
  // stall re-configuration following an IP address change).
  base::TimeTicks stall_proxy_autoconfig_until_;

  // The amount of time to stall requests following IP address changes.
  base::TimeDelta stall_proxy_auto_config_delay_;

  DISALLOW_COPY_AND_ASSIGN(ProxyService);
};

// Wrapper for invoking methods on a ProxyService synchronously.
class NET_EXPORT SyncProxyServiceHelper
    : public base::RefCountedThreadSafe<SyncProxyServiceHelper> {
 public:
  SyncProxyServiceHelper(base::MessageLoop* io_message_loop,
                         ProxyService* proxy_service);

  int ResolveProxy(const GURL& url,
                   ProxyInfo* proxy_info,
                   const BoundNetLog& net_log);
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* proxy_info,
                                const BoundNetLog& net_log);

 private:
  friend class base::RefCountedThreadSafe<SyncProxyServiceHelper>;

  virtual ~SyncProxyServiceHelper();

  void StartAsyncResolve(const GURL& url, const BoundNetLog& net_log);
  void StartAsyncReconsider(const GURL& url, const BoundNetLog& net_log);

  void OnCompletion(int result);

  base::MessageLoop* io_message_loop_;
  ProxyService* proxy_service_;

  base::WaitableEvent event_;
  CompletionCallback callback_;
  ProxyInfo proxy_info_;
  int result_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SERVICE_H_
