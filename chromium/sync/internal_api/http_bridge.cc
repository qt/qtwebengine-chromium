// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/http_bridge.h"

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "sync/internal_api/public/base/cancelation_signal.h"

namespace syncer {

HttpBridge::RequestContextGetter::RequestContextGetter(
    net::URLRequestContextGetter* baseline_context_getter,
    const std::string& user_agent)
    : baseline_context_getter_(baseline_context_getter),
      network_task_runner_(
          baseline_context_getter_->GetNetworkTaskRunner()),
      user_agent_(user_agent) {
  DCHECK(baseline_context_getter_.get());
  DCHECK(network_task_runner_.get());
  DCHECK(!user_agent_.empty());
}

HttpBridge::RequestContextGetter::~RequestContextGetter() {}

net::URLRequestContext*
HttpBridge::RequestContextGetter::GetURLRequestContext() {
  // Lazily create the context.
  if (!context_) {
    net::URLRequestContext* baseline_context =
        baseline_context_getter_->GetURLRequestContext();
    context_.reset(
        new RequestContext(baseline_context, GetNetworkTaskRunner(),
                           user_agent_));
    baseline_context_getter_ = NULL;
  }

  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
HttpBridge::RequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

HttpBridgeFactory::HttpBridgeFactory(
    net::URLRequestContextGetter* baseline_context_getter,
    const NetworkTimeUpdateCallback& network_time_update_callback,
    CancelationSignal* cancelation_signal)
    : baseline_request_context_getter_(baseline_context_getter),
      network_time_update_callback_(network_time_update_callback),
      cancelation_signal_(cancelation_signal) {
  // Registration should never fail.  This should happen on the UI thread during
  // init.  It would be impossible for a shutdown to have been requested at this
  // point.
  bool result = cancelation_signal_->TryRegisterHandler(this);
  DCHECK(result);
}

HttpBridgeFactory::~HttpBridgeFactory() {
  cancelation_signal_->UnregisterHandler(this);
}

void HttpBridgeFactory::Init(const std::string& user_agent) {
  base::AutoLock lock(context_getter_lock_);

  if (!baseline_request_context_getter_.get()) {
    // Uh oh.  We've been aborted before we finished initializing.  There's no
    // point in initializating further; let's just return right away.
    return;
  }

  request_context_getter_ =
      new HttpBridge::RequestContextGetter(
          baseline_request_context_getter_, user_agent);
}

HttpPostProviderInterface* HttpBridgeFactory::Create() {
  base::AutoLock lock(context_getter_lock_);

  // If we've been asked to shut down (something which may happen asynchronously
  // and at pretty much any time), then we won't have a request_context_getter_.
  // Some external mechanism must ensure that this function is not called after
  // we've been asked to shut down.
  CHECK(request_context_getter_.get());

  HttpBridge* http = new HttpBridge(request_context_getter_.get(),
                                    network_time_update_callback_);
  http->AddRef();
  return http;
}

void HttpBridgeFactory::Destroy(HttpPostProviderInterface* http) {
  static_cast<HttpBridge*>(http)->Release();
}

void HttpBridgeFactory::OnSignalReceived() {
  base::AutoLock lock(context_getter_lock_);
  // Release |baseline_request_context_getter_| as soon as possible so that it
  // is destroyed in the right order on its network task runner.  The
  // |request_context_getter_| has a reference to the baseline, so we must
  // drop our reference to it, too.
  baseline_request_context_getter_ = NULL;
  request_context_getter_ = NULL;
}

HttpBridge::RequestContext::RequestContext(
    net::URLRequestContext* baseline_context,
    const scoped_refptr<base::SingleThreadTaskRunner>&
        network_task_runner,
    const std::string& user_agent)
    : baseline_context_(baseline_context),
      network_task_runner_(network_task_runner) {
  DCHECK(!user_agent.empty());

  // Create empty, in-memory cookie store.
  set_cookie_store(new net::CookieMonster(NULL, NULL));

  // We don't use a cache for bridged loads, but we do want to share proxy info.
  set_host_resolver(baseline_context->host_resolver());
  set_proxy_service(baseline_context->proxy_service());
  set_ssl_config_service(baseline_context->ssl_config_service());

  // We want to share the HTTP session data with the network layer factory,
  // which includes auth_cache for proxies.
  // Session is not refcounted so we need to be careful to not lose the parent
  // context.
  net::HttpNetworkSession* session =
      baseline_context->http_transaction_factory()->GetSession();
  DCHECK(session);
  set_http_transaction_factory(new net::HttpNetworkLayer(session));

  // TODO(timsteele): We don't currently listen for pref changes of these
  // fields or CookiePolicy; I'm not sure we want to strictly follow the
  // default settings, since for example if the user chooses to block all
  // cookies, sync will start failing. Also it seems like accept_lang/charset
  // should be tied to whatever the sync servers expect (if anything). These
  // fields should probably just be settable by sync backend; though we should
  // figure out if we need to give the user explicit control over policies etc.
  http_user_agent_settings_.reset(new net::StaticHttpUserAgentSettings(
      baseline_context->GetAcceptLanguage(),
      user_agent));
  set_http_user_agent_settings(http_user_agent_settings_.get());

  set_net_log(baseline_context->net_log());
}

HttpBridge::RequestContext::~RequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delete http_transaction_factory();
}

HttpBridge::URLFetchState::URLFetchState() : url_poster(NULL),
                                             aborted(false),
                                             request_completed(false),
                                             request_succeeded(false),
                                             http_response_code(-1),
                                             error_code(-1) {}
HttpBridge::URLFetchState::~URLFetchState() {}

HttpBridge::HttpBridge(
    HttpBridge::RequestContextGetter* context_getter,
    const NetworkTimeUpdateCallback& network_time_update_callback)
    : created_on_loop_(base::MessageLoop::current()),
      http_post_completed_(false, false),
      context_getter_for_request_(context_getter),
      network_task_runner_(
          context_getter_for_request_->GetNetworkTaskRunner()),
      network_time_update_callback_(network_time_update_callback) {
}

HttpBridge::~HttpBridge() {
}

void HttpBridge::SetExtraRequestHeaders(const char * headers) {
  DCHECK(extra_headers_.empty())
      << "HttpBridge::SetExtraRequestHeaders called twice.";
  extra_headers_.assign(headers);
}

void HttpBridge::SetURL(const char* url, int port) {
  DCHECK_EQ(base::MessageLoop::current(), created_on_loop_);
  if (DCHECK_IS_ON()) {
    base::AutoLock lock(fetch_state_lock_);
    DCHECK(!fetch_state_.request_completed);
  }
  DCHECK(url_for_request_.is_empty())
      << "HttpBridge::SetURL called more than once?!";
  GURL temp(url);
  GURL::Replacements replacements;
  std::string port_str = base::IntToString(port);
  replacements.SetPort(port_str.c_str(),
                       url_parse::Component(0, port_str.length()));
  url_for_request_ = temp.ReplaceComponents(replacements);
}

void HttpBridge::SetPostPayload(const char* content_type,
                                int content_length,
                                const char* content) {
  DCHECK_EQ(base::MessageLoop::current(), created_on_loop_);
  if (DCHECK_IS_ON()) {
    base::AutoLock lock(fetch_state_lock_);
    DCHECK(!fetch_state_.request_completed);
  }
  DCHECK(content_type_.empty()) << "Bridge payload already set.";
  DCHECK_GE(content_length, 0) << "Content length < 0";
  content_type_ = content_type;
  if (!content || (content_length == 0)) {
    DCHECK_EQ(content_length, 0);
    request_content_ = " ";  // TODO(timsteele): URLFetcher requires non-empty
                             // content for POSTs whereas CURL does not, for now
                             // we hack this to support the sync backend.
  } else {
    request_content_.assign(content, content_length);
  }
}

bool HttpBridge::MakeSynchronousPost(int* error_code, int* response_code) {
  DCHECK_EQ(base::MessageLoop::current(), created_on_loop_);
  if (DCHECK_IS_ON()) {
    base::AutoLock lock(fetch_state_lock_);
    DCHECK(!fetch_state_.request_completed);
  }
  DCHECK(url_for_request_.is_valid()) << "Invalid URL for request";
  DCHECK(!content_type_.empty()) << "Payload not set";

  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&HttpBridge::CallMakeAsynchronousPost, this))) {
    // This usually happens when we're in a unit test.
    LOG(WARNING) << "Could not post CallMakeAsynchronousPost task";
    return false;
  }

  // Block until network request completes or is aborted. See
  // OnURLFetchComplete and Abort.
  http_post_completed_.Wait();

  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed || fetch_state_.aborted);
  *error_code = fetch_state_.error_code;
  *response_code = fetch_state_.http_response_code;
  return fetch_state_.request_succeeded;
}

void HttpBridge::MakeAsynchronousPost() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(!fetch_state_.request_completed);
  if (fetch_state_.aborted)
    return;

  DCHECK(context_getter_for_request_.get());
  fetch_state_.url_poster = net::URLFetcher::Create(
      url_for_request_, net::URLFetcher::POST, this);
  fetch_state_.url_poster->SetRequestContext(context_getter_for_request_.get());
  fetch_state_.url_poster->SetUploadData(content_type_, request_content_);
  fetch_state_.url_poster->SetExtraRequestHeaders(extra_headers_);
  fetch_state_.url_poster->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES);
  fetch_state_.start_time = base::Time::Now();
  fetch_state_.url_poster->Start();
}

int HttpBridge::GetResponseContentLength() const {
  DCHECK_EQ(base::MessageLoop::current(), created_on_loop_);
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed);
  return fetch_state_.response_content.size();
}

const char* HttpBridge::GetResponseContent() const {
  DCHECK_EQ(base::MessageLoop::current(), created_on_loop_);
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed);
  return fetch_state_.response_content.data();
}

const std::string HttpBridge::GetResponseHeaderValue(
    const std::string& name) const {

  DCHECK_EQ(base::MessageLoop::current(), created_on_loop_);
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed);

  std::string value;
  fetch_state_.response_headers->EnumerateHeader(NULL, name, &value);
  return value;
}

void HttpBridge::Abort() {
  base::AutoLock lock(fetch_state_lock_);

  // Release |request_context_getter_| as soon as possible so that it is
  // destroyed in the right order on its network task runner.
  context_getter_for_request_ = NULL;

  DCHECK(!fetch_state_.aborted);
  if (fetch_state_.aborted || fetch_state_.request_completed)
    return;

  fetch_state_.aborted = true;
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&HttpBridge::DestroyURLFetcherOnIOThread, this,
                     fetch_state_.url_poster))) {
    // Madness ensues.
    NOTREACHED() << "Could not post task to delete URLFetcher";
  }

  fetch_state_.url_poster = NULL;
  fetch_state_.error_code = net::ERR_ABORTED;
  http_post_completed_.Signal();
}

void HttpBridge::DestroyURLFetcherOnIOThread(net::URLFetcher* fetcher) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delete fetcher;
}

void HttpBridge::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(fetch_state_lock_);
  if (fetch_state_.aborted)
    return;

  fetch_state_.end_time = base::Time::Now();
  fetch_state_.request_completed = true;
  fetch_state_.request_succeeded =
      (net::URLRequestStatus::SUCCESS == source->GetStatus().status());
  fetch_state_.http_response_code = source->GetResponseCode();
  fetch_state_.error_code = source->GetStatus().error();

  // Use a real (non-debug) log to facilitate troubleshooting in the wild.
  VLOG(2) << "HttpBridge::OnURLFetchComplete for: "
          << fetch_state_.url_poster->GetURL().spec();
  VLOG(1) << "HttpBridge received response code: "
          << fetch_state_.http_response_code;

  source->GetResponseAsString(&fetch_state_.response_content);
  fetch_state_.response_headers = source->GetResponseHeaders();
  UpdateNetworkTime();

  // End of the line for url_poster_. It lives only on the IO loop.
  // We defer deletion because we're inside a callback from a component of the
  // URLFetcher, so it seems most natural / "polite" to let the stack unwind.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, fetch_state_.url_poster);
  fetch_state_.url_poster = NULL;

  // Wake the blocked syncer thread in MakeSynchronousPost.
  // WARNING: DONT DO ANYTHING AFTER THIS CALL! |this| may be deleted!
  http_post_completed_.Signal();
}

net::URLRequestContextGetter* HttpBridge::GetRequestContextGetterForTest()
    const {
  base::AutoLock lock(fetch_state_lock_);
  return context_getter_for_request_.get();
}

void HttpBridge::UpdateNetworkTime() {
  std::string sane_time_str;
  if (!fetch_state_.request_succeeded || fetch_state_.start_time.is_null() ||
      fetch_state_.end_time < fetch_state_.start_time ||
      !fetch_state_.response_headers->EnumerateHeader(NULL, "Sane-Time-Millis",
                                                      &sane_time_str)) {
    return;
  }

  int64 sane_time_ms = 0;
  if (base::StringToInt64(sane_time_str, &sane_time_ms)) {
    network_time_update_callback_.Run(
        base::Time::FromJsTime(sane_time_ms),
        base::TimeDelta::FromMilliseconds(1),
        fetch_state_.end_time - fetch_state_.start_time);
  }
}

}  // namespace syncer
