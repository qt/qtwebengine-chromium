// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_globals.h"

#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppb_message_loop_proxy.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {

// It performs necessary locking/unlocking of the proxy lock, and forwards all
// messages to the underlying sender.
class PluginGlobals::BrowserSender : public IPC::Sender {
 public:
  // |underlying_sender| must outlive this object.
  explicit BrowserSender(IPC::Sender* underlying_sender)
      : underlying_sender_(underlying_sender) {
  }

  virtual ~BrowserSender() {}

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    if (msg->is_sync()) {
      // Synchronous messages might be re-entrant, so we need to drop the lock.
      ProxyAutoUnlock unlock;
      return underlying_sender_->Send(msg);
    }

    return underlying_sender_->Send(msg);
  }

 private:
  // Non-owning pointer.
  IPC::Sender* underlying_sender_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSender);
};

PluginGlobals* PluginGlobals::plugin_globals_ = NULL;

PluginGlobals::PluginGlobals()
    : ppapi::PpapiGlobals(),
      plugin_proxy_delegate_(NULL),
      callback_tracker_(new CallbackTracker),
      resource_reply_thread_registrar_(
          new ResourceReplyThreadRegistrar(GetMainThreadMessageLoop())) {
  DCHECK(!plugin_globals_);
  plugin_globals_ = this;

  // ResourceTracker asserts that we have the lock when we add new resources,
  // so we lock when creating the MessageLoopResource even though there is no
  // chance of race conditions.
  ProxyAutoLock lock;
  loop_for_main_thread_ =
      new MessageLoopResource(MessageLoopResource::ForMainThread());
}

PluginGlobals::PluginGlobals(PerThreadForTest per_thread_for_test)
    : ppapi::PpapiGlobals(per_thread_for_test),
      plugin_proxy_delegate_(NULL),
      callback_tracker_(new CallbackTracker),
      resource_reply_thread_registrar_(
          new ResourceReplyThreadRegistrar(GetMainThreadMessageLoop())) {
  DCHECK(!plugin_globals_);
}

PluginGlobals::~PluginGlobals() {
  DCHECK(plugin_globals_ == this || !plugin_globals_);
  {
    ProxyAutoLock lock;
    // Release the main-thread message loop. We should have the last reference
    // count, so this will delete the MessageLoop resource. We do this before
    // we clear plugin_globals_, because the Resource destructor tries to access
    // this PluginGlobals.
    DCHECK(!loop_for_main_thread_.get() || loop_for_main_thread_->HasOneRef());
    loop_for_main_thread_ = NULL;
  }
  plugin_globals_ = NULL;
}

ResourceTracker* PluginGlobals::GetResourceTracker() {
  return &plugin_resource_tracker_;
}

VarTracker* PluginGlobals::GetVarTracker() {
  return &plugin_var_tracker_;
}

CallbackTracker* PluginGlobals::GetCallbackTrackerForInstance(
    PP_Instance instance) {
  // In the plugin process, the callback tracker is always the same, regardless
  // of the instance.
  return callback_tracker_.get();
}

thunk::PPB_Instance_API* PluginGlobals::GetInstanceAPI(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (dispatcher)
    return dispatcher->GetInstanceAPI();
  return NULL;
}

thunk::ResourceCreationAPI* PluginGlobals::GetResourceCreationAPI(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (dispatcher)
    return dispatcher->GetResourceCreationAPI();
  return NULL;
}

PP_Module PluginGlobals::GetModuleForInstance(PP_Instance instance) {
  // Currently proxied plugins don't use the PP_Module for anything useful.
  return 0;
}

std::string PluginGlobals::GetCmdLine() {
  return command_line_;
}

void PluginGlobals::PreCacheFontForFlash(const void* logfontw) {
  ProxyAutoUnlock unlock;
  plugin_proxy_delegate_->PreCacheFont(logfontw);
}

void PluginGlobals::LogWithSource(PP_Instance instance,
                                  PP_LogLevel level,
                                  const std::string& source,
                                  const std::string& value) {
  const std::string& fixed_up_source = source.empty() ? plugin_name_ : source;
  PluginDispatcher::LogWithSource(instance, level, fixed_up_source, value);
}

void PluginGlobals::BroadcastLogWithSource(PP_Module /* module */,
                                           PP_LogLevel level,
                                           const std::string& source,
                                           const std::string& value) {
  // Since we have only one module in a plugin process, broadcast is always
  // the same as "send to everybody" which is what the dispatcher implements
  // for the "instance = 0" case.
  LogWithSource(0, level, source, value);
}

MessageLoopShared* PluginGlobals::GetCurrentMessageLoop() {
  return MessageLoopResource::GetCurrent();
}

base::TaskRunner* PluginGlobals::GetFileTaskRunner() {
  if (!file_thread_.get()) {
    file_thread_.reset(new base::Thread("Plugin::File"));
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    file_thread_->StartWithOptions(options);
  }
  return file_thread_->message_loop_proxy();
}

IPC::Sender* PluginGlobals::GetBrowserSender() {
  if (!browser_sender_.get()) {
    browser_sender_.reset(
        new BrowserSender(plugin_proxy_delegate_->GetBrowserSender()));
  }

  return browser_sender_.get();
}

std::string PluginGlobals::GetUILanguage() {
  return plugin_proxy_delegate_->GetUILanguage();
}

void PluginGlobals::SetActiveURL(const std::string& url) {
  plugin_proxy_delegate_->SetActiveURL(url);
}

PP_Resource PluginGlobals::CreateBrowserFont(
    Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const ppapi::Preferences& prefs) {
  return plugin_proxy_delegate_->CreateBrowserFont(
      connection, instance, desc, prefs);
}

MessageLoopResource* PluginGlobals::loop_for_main_thread() {
  return loop_for_main_thread_.get();
}

bool PluginGlobals::IsPluginGlobals() const {
  return true;
}

}  // namespace proxy
}  // namespace ppapi
