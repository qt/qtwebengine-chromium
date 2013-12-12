// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_controller_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/tracing/trace_subscriber_stdio.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/content_switches.h"

using base::debug::TraceLog;

namespace content {

namespace {

base::LazyInstance<TraceControllerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

class AutoStopTraceSubscriberStdio : public TraceSubscriberStdio {
 public:
  AutoStopTraceSubscriberStdio(const base::FilePath& file_path)
      : TraceSubscriberStdio(file_path,
                             FILE_TYPE_PROPERTY_LIST,
                             false) {}

  static void EndStartupTrace(AutoStopTraceSubscriberStdio* subscriber) {
    if (!TraceControllerImpl::GetInstance()->EndTracingAsync(subscriber))
      delete subscriber;
    // else, the tracing will end asynchronously in OnEndTracingComplete().
  }

  virtual void OnEndTracingComplete() OVERRIDE {
    TraceSubscriberStdio::OnEndTracingComplete();
    delete this;
    // TODO(joth): this would be the time to automatically open up
    // chrome://tracing/ and load up the trace data collected.
  }
};

}  // namespace

TraceController* TraceController::GetInstance() {
  return TraceControllerImpl::GetInstance();
}

TraceControllerImpl::TraceControllerImpl() :
    subscriber_(NULL),
    pending_end_ack_count_(0),
    pending_bpf_ack_count_(0),
    maximum_bpf_(0.0f),
    is_tracing_(false),
    is_get_category_groups_(false),
    category_filter_(
        base::debug::CategoryFilter::kDefaultCategoryFilterString) {
  TraceLog::GetInstance()->SetNotificationCallback(
      base::Bind(&TraceControllerImpl::OnTraceNotification,
                 base::Unretained(this)));
}

TraceControllerImpl::~TraceControllerImpl() {
  // No need to SetNotificationCallback(nil) on the TraceLog since this is a
  // Leaky instance.
  NOTREACHED();
}

TraceControllerImpl* TraceControllerImpl::GetInstance() {
  return g_controller.Pointer();
}

void TraceControllerImpl::InitStartupTracing(const CommandLine& command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FilePath trace_file = command_line.GetSwitchValuePath(
      switches::kTraceStartupFile);
  // trace_file = "none" means that startup events will show up for the next
  // begin/end tracing (via about:tracing or AutomationProxy::BeginTracing/
  // EndTracing, for example).
  if (trace_file == base::FilePath().AppendASCII("none"))
    return;

  if (trace_file.empty()) {
    // Default to saving the startup trace into the current dir.
    trace_file = base::FilePath().AppendASCII("chrometrace.log");
  }
  scoped_ptr<AutoStopTraceSubscriberStdio> subscriber(
      new AutoStopTraceSubscriberStdio(trace_file));
  DCHECK(can_begin_tracing(subscriber.get()));

  std::string delay_str = command_line.GetSwitchValueASCII(
      switches::kTraceStartupDuration);
  int delay_secs = 5;
  if (!delay_str.empty() && !base::StringToInt(delay_str, &delay_secs)) {
    DLOG(WARNING) << "Could not parse --" << switches::kTraceStartupDuration
        << "=" << delay_str << " defaulting to 5 (secs)";
    delay_secs = 5;
  }

  OnTracingBegan(subscriber.get());
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AutoStopTraceSubscriberStdio::EndStartupTrace,
                 base::Unretained(subscriber.release())),
      base::TimeDelta::FromSeconds(delay_secs));
}

bool TraceControllerImpl::GetKnownCategoryGroupsAsync(
    TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Known categories come back from child processes with the EndTracingAck
  // message. So to get known categories, just begin and end tracing immediately
  // afterwards. This will ping all the child processes for categories.
  is_get_category_groups_ = true;
  bool success = BeginTracing(subscriber, "*",
                              TraceLog::GetInstance()->trace_options()) &&
                 EndTracingAsync(subscriber);
  is_get_category_groups_ = success;
  return success;
}

bool TraceControllerImpl::BeginTracing(TraceSubscriber* subscriber,
                                       const std::string& category_patterns,
                                       base::debug::TraceLog::Options options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_begin_tracing(subscriber))
    return false;

  // Enable tracing
  TraceLog::GetInstance()->SetEnabled(
      base::debug::CategoryFilter(category_patterns), options);

  OnTracingBegan(subscriber);

  return true;
}

bool TraceControllerImpl::EndTracingAsync(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_end_tracing() || subscriber != subscriber_)
    return false;

  // Disable local trace early to avoid traces during end-tracing process from
  // interfering with the process.
  TraceLog::GetInstance()->SetDisabled();

  // There could be a case where there are no child processes and filters_
  // is empty. In that case we can immediately tell the subscriber that tracing
  // has ended. To avoid recursive calls back to the subscriber, we will just
  // use the existing asynchronous OnEndTracingAck code.
  // Count myself (local trace) in pending_end_ack_count_, acked below.
  pending_end_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_end_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    std::vector<std::string> category_groups;
    TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnEndTracingAck,
                   base::Unretained(this), category_groups));
  }

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendEndTracing();
  }

  return true;
}

bool TraceControllerImpl::GetTraceBufferPercentFullAsync(
    TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_get_buffer_percent_full() || subscriber != subscriber_)
    return false;

  maximum_bpf_ = 0.0f;
  pending_bpf_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_bpf_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    float bpf = TraceLog::GetInstance()->GetBufferPercentFull();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this), bpf));
  }

  // Message all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendGetTraceBufferPercentFull();
  }

  return true;
}

bool TraceControllerImpl::SetWatchEvent(TraceSubscriber* subscriber,
                                        const std::string& category_name,
                                        const std::string& event_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (subscriber != subscriber_)
    return false;

  watch_category_ = category_name;
  watch_name_ = event_name;

  TraceLog::GetInstance()->SetWatchEvent(category_name, event_name);
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it)
    it->get()->SendSetWatchEvent(category_name, event_name);

  return true;
}

bool TraceControllerImpl::CancelWatchEvent(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (subscriber != subscriber_)
    return false;

  watch_category_.clear();
  watch_name_.clear();

  TraceLog::GetInstance()->CancelWatchEvent();
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it)
    it->get()->SendCancelWatchEvent();

  return true;
}

void TraceControllerImpl::CancelSubscriber(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (subscriber == subscriber_) {
    subscriber_ = NULL;
    // End tracing if necessary.
    if (is_tracing_ && pending_end_ack_count_ == 0)
      EndTracingAsync(NULL);
  }
}

void TraceControllerImpl::AddFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::AddFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.insert(filter);
  if (is_tracing_enabled()) {
    std::string cf_str = category_filter_.ToString();
    filter->SendBeginTracing(cf_str, trace_options_);
    if (!watch_category_.empty())
      filter->SendSetWatchEvent(watch_category_, watch_name_);
  }
}

void TraceControllerImpl::RemoveFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::RemoveFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.erase(filter);
}

void TraceControllerImpl::OnTracingBegan(TraceSubscriber* subscriber) {
  is_tracing_ = true;

  subscriber_ = subscriber;

  category_filter_ = TraceLog::GetInstance()->GetCurrentCategoryFilter();
  trace_options_ = TraceLog::GetInstance()->trace_options();

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendBeginTracing(category_filter_.ToString(), trace_options_);
  }
}

void TraceControllerImpl::OnEndTracingAck(
    const std::vector<std::string>& known_category_groups) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnEndTracingAck,
                   base::Unretained(this), known_category_groups));
    return;
  }

  // Merge known_category_groups with known_category_groups_
  known_category_groups_.insert(known_category_groups.begin(),
                                known_category_groups.end());

  if (pending_end_ack_count_ == 0)
    return;


  if (--pending_end_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalTraceDataCollected will be
    // called with the last of the local trace data.
    TraceLog::GetInstance()->Flush(
        base::Bind(&TraceControllerImpl::OnLocalTraceDataCollected,
                   base::Unretained(this)));
  }

  if (pending_end_ack_count_ == 0) {
    // All acks (including from the subprocesses and the local trace) have been
    // received.
    is_tracing_ = false;

    // Trigger callback if one is set.
    if (subscriber_) {
      if (is_get_category_groups_)
        subscriber_->OnKnownCategoriesCollected(known_category_groups_);
      else
        subscriber_->OnEndTracingComplete();
      // Clear subscriber so that others can use TraceController.
      subscriber_ = NULL;
    }

    is_get_category_groups_ = false;
  }
}

void TraceControllerImpl::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnTraceDataCollected,
                   base::Unretained(this), events_str_ptr));
    return;
  }

  // Drop trace events if we are just getting categories.
  if (subscriber_ && !is_get_category_groups_)
    subscriber_->OnTraceDataCollected(events_str_ptr);
}

void TraceControllerImpl::OnLocalTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnTraceDataCollected(events_str_ptr);

  if (!has_more_events) {
    // Simulate an EndTrackingAck for the local trace.
    std::vector<std::string> category_groups;
    TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
    OnEndTracingAck(category_groups);
  }
}

void TraceControllerImpl::OnTraceNotification(int notification) {
  // OnTraceNotification may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnTraceNotification,
                   base::Unretained(this), notification));
    return;
  }

  if (notification & base::debug::TraceLog::TRACE_BUFFER_FULL) {
    // EndTracingAsync may return false if tracing is already in the process
    // of being ended. That is ok.
    EndTracingAsync(subscriber_);
  }
  if (notification & base::debug::TraceLog::EVENT_WATCH_NOTIFICATION) {
    if (subscriber_)
      subscriber_->OnEventWatchNotification();
  }
}

void TraceControllerImpl::OnTraceBufferPercentFullReply(float percent_full) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this), percent_full));
    return;
  }

  if (pending_bpf_ack_count_ == 0)
    return;

  maximum_bpf_ = (maximum_bpf_ > percent_full)? maximum_bpf_ : percent_full;

  if (--pending_bpf_ack_count_ == 0) {
    // Trigger callback if one is set.
    if (subscriber_)
      subscriber_->OnTraceBufferPercentFullReply(maximum_bpf_);
  }

  if (pending_bpf_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    float bpf = TraceLog::GetInstance()->GetBufferPercentFull();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this), bpf));
  }
}

}  // namespace content
