// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/cookie_data.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/media/media_param_traits.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/base/request_priority.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "third_party/WebKit/public/web/WebNotificationPresenter.h"
#include "ui/gfx/color_profile.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"
#include "content/common/mac/font_descriptor.h"
#include "ui/gl/io_surface_support_mac.h"
#else
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#endif
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#if defined(OS_WIN)
#include "content/browser/renderer_host/backing_store_win.h"
#include "content/common/font_cache_dispatcher_win.h"
#endif
#if defined(OS_ANDROID)
#include "media/base/android/webaudio_media_codec_bridge.h"
#endif

using net::CookieStore;

namespace content {
namespace {

#if defined(ENABLE_PLUGINS)
const int kPluginsRefreshThresholdInSeconds = 3;
#endif

// When two CPU usage queries arrive within this interval, we sample the CPU
// usage only once and send it as a response for both queries.
static const int64 kCPUUsageSampleIntervalMs = 900;

#if defined(OS_WIN)
// On Windows, |g_color_profile| can run on an arbitrary background thread.
// We avoid races by using LazyInstance's constructor lock to initialize the
// object.
base::LazyInstance<gfx::ColorProfile>::Leaky g_color_profile =
    LAZY_INSTANCE_INITIALIZER;
#endif

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual ~RenderMessageCompletionCallback() {
  }

  RenderMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    delete this;
  }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

class OpenChannelToPpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(RenderMessageFilter* filter,
                                   ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context) {
  }

  virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                   int* renderer_id) OVERRIDE {
    *renderer_handle = filter()->PeerHandle();
    *renderer_id = filter()->render_process_id();
  }

  virtual void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                                    base::ProcessId plugin_pid,
                                    int plugin_child_id) OVERRIDE {
    ViewHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), channel_handle, plugin_pid, plugin_child_id);
    SendReplyAndDeleteThis();
  }

  virtual bool OffTheRecord() OVERRIDE {
    return filter()->OffTheRecord();
  }

  virtual ResourceContext* GetResourceContext() OVERRIDE {
    return context_;
  }

 private:
  ResourceContext* context_;
};

class OpenChannelToPpapiBrokerCallback
    : public PpapiPluginProcessHost::BrokerClient {
 public:
  OpenChannelToPpapiBrokerCallback(RenderMessageFilter* filter,
                                   int routing_id)
      : filter_(filter),
        routing_id_(routing_id) {
  }

  virtual ~OpenChannelToPpapiBrokerCallback() {}

  virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                   int* renderer_id) OVERRIDE {
    *renderer_handle = filter_->PeerHandle();
    *renderer_id = filter_->render_process_id();
  }

  virtual void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                                    base::ProcessId plugin_pid,
                                    int /* plugin_child_id */) OVERRIDE {
    filter_->Send(new ViewMsg_PpapiBrokerChannelCreated(routing_id_,
                                                        plugin_pid,
                                                        channel_handle));
    delete this;
  }

  virtual bool OffTheRecord() OVERRIDE {
    return filter_->OffTheRecord();
  }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  int routing_id_;
};

#if defined(OS_MACOSX)
void AddBooleanValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     bool value) {
  CFDictionaryAddValue(
      dictionary, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32 value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}
#endif

}  // namespace

class RenderMessageFilter::OpenChannelToNpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PluginProcessHost::Client {
 public:
  OpenChannelToNpapiPluginCallback(RenderMessageFilter* filter,
                                   ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context),
        host_(NULL),
        sent_plugin_channel_request_(false) {
  }

  virtual int ID() OVERRIDE {
    return filter()->render_process_id();
  }

  virtual ResourceContext* GetResourceContext() OVERRIDE {
    return context_;
  }

  virtual bool OffTheRecord() OVERRIDE {
    if (filter()->OffTheRecord())
      return true;
    if (GetContentClient()->browser()->AllowSaveLocalState(context_))
      return false;

    // For now, only disallow storing data for Flash <http://crbug.com/97319>.
    for (size_t i = 0; i < info_.mime_types.size(); ++i) {
      if (info_.mime_types[i].mime_type == kFlashPluginSwfMimeType)
        return true;
    }
    return false;
  }

  virtual void SetPluginInfo(const WebPluginInfo& info) OVERRIDE {
    info_ = info;
  }

  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
    DCHECK(host);
    host_ = host;
  }

  virtual void OnSentPluginChannelRequest() OVERRIDE {
    sent_plugin_channel_request_ = true;
  }

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE {
    WriteReplyAndDeleteThis(handle);
  }

  virtual void OnError() OVERRIDE {
    WriteReplyAndDeleteThis(IPC::ChannelHandle());
  }

  PluginProcessHost* host() const {
    return host_;
  }

  bool sent_plugin_channel_request() const {
    return sent_plugin_channel_request_;
  }

  void Cancel() {
    delete this;
  }

 private:
  void WriteReplyAndDeleteThis(const IPC::ChannelHandle& handle) {
    FrameHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg(),
                                                       handle, info_);
    filter()->OnCompletedOpenChannelToNpapiPlugin(this);
    SendReplyAndDeleteThis();
  }

  ResourceContext* context_;
  WebPluginInfo info_;
  PluginProcessHost* host_;
  bool sent_plugin_channel_request_;
};

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    bool is_guest,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper,
    media::AudioManager* audio_manager,
    MediaInternals* media_internals,
    DOMStorageContextWrapper* dom_storage_context)
    : resource_dispatcher_host_(ResourceDispatcherHostImpl::Get()),
      plugin_service_(plugin_service),
      profile_data_directory_(browser_context->GetPath()),
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      dom_storage_context_(dom_storage_context),
      render_process_id_(render_process_id),
      is_guest_(is_guest),
      cpu_usage_(0),
      audio_manager_(audio_manager),
      media_internals_(media_internals) {
  DCHECK(request_context_.get());

  render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(plugin_host_clients_.empty());
}

void RenderMessageFilter::OnChannelClosing() {
#if defined(ENABLE_PLUGINS)
  for (std::set<OpenChannelToNpapiPluginCallback*>::iterator it =
       plugin_host_clients_.begin(); it != plugin_host_clients_.end(); ++it) {
    OpenChannelToNpapiPluginCallback* client = *it;
    if (client->host()) {
      if (client->sent_plugin_channel_request()) {
        client->host()->CancelSentRequest(client);
      } else {
        client->host()->CancelPendingRequest(client);
      }
    } else {
      plugin_service_->CancelOpenChannelToNpapiPlugin(client);
    }
    client->Cancel();
  }
#endif  // defined(ENABLE_PLUGINS)
  plugin_host_clients_.clear();
}

void RenderMessageFilter::OnChannelConnected(int32 peer_id) {
  base::ProcessHandle handle = PeerHandle();
#if defined(OS_MACOSX)
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle,
                                                                    NULL));
#else
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
#endif
  cpu_usage_ = process_metrics_->GetCPUUsage(); // Initialize CPU usage counters
  cpu_usage_sample_time_ = base::TimeTicks::Now();
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderMessageFilter, message, *message_was_ok)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PreCacheFontCharacters,
                        OnPreCacheFontCharacters)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetProcessMemorySizes,
                        OnGetProcessMemorySizes)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GenerateRoutingID, OnGenerateRoutingID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateFullscreenWidget,
                        OnCreateFullscreenWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRawCookies, OnGetRawCookies)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DeleteCookie, OnDeleteCookie)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CookiesEnabled, OnCookiesEnabled)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_LoadFont, OnLoadFont)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(FrameHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidCreateOutOfProcessPepperInstance,
                        OnDidCreateOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDeleteOutOfProcessPepperInstance,
                        OnDidDeleteOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToPpapiBroker,
                        OnOpenChannelToPpapiBroker)
#endif
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
        render_widget_helper_->DidReceiveBackingStoreMsg(message))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateIsDelayed, OnUpdateIsDelayed)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_CheckPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_SyncAllocateSharedMemory,
                        OnAllocateSharedMemory)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer,
                        OnAllocateGpuMemoryBuffer)
#if defined(OS_POSIX) && !defined(TOOLKIT_GTK) && !defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB, OnAllocTransportDIB)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB, OnFreeTransportDIB)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGenerateCacheableMetadata,
                        OnCacheableMetadataAvailable)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_Keygen, OnKeygen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPUUsage, OnGetCPUUsage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioHardwareConfig,
                        OnGetAudioHardwareConfig)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMonitorColorProfile,
                        OnGetMonitorColorProfile)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaLogEvents, OnMediaLogEvents)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Are3DAPIsBlocked, OnAre3DAPIsBlocked)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLose3DContext, OnDidLose3DContext)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunWebAudioMediaCodec, OnWebAudioMediaCodec)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void RenderMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

base::TaskRunner* RenderMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
#if defined(OS_WIN)
  // Windows monitor profile must be read from a file.
  if (message.type() == ViewHostMsg_GetMonitorColorProfile::ID)
    return BrowserThread::GetBlockingPool();
#endif
#if defined(OS_MACOSX)
  // OSX CoreAudio calls must all happen on the main thread.
  if (message.type() == ViewHostMsg_GetAudioHardwareConfig::ID)
    return audio_manager_->GetMessageLoop().get();
#endif
  return NULL;
}

bool RenderMessageFilter::OffTheRecord() const {
  return incognito_;
}

void RenderMessageFilter::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id,
    int* main_frame_route_id,
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  bool no_javascript_access;

  // Merge the additional features into the WebWindowFeatures struct before we
  // pass it on.
  blink::WebVector<blink::WebString> additional_features(
      params.additional_features.size());

  for (size_t i = 0; i < params.additional_features.size(); ++i)
    additional_features[i] = blink::WebString(params.additional_features[i]);

  blink::WebWindowFeatures features = params.features;
  features.additionalFeatures.swap(additional_features);

  bool can_create_window =
      GetContentClient()->browser()->CanCreateWindow(
          params.opener_url,
          params.opener_top_level_frame_url,
          params.opener_security_origin,
          params.window_container_type,
          params.target_url,
          params.referrer,
          params.disposition,
          features,
          params.user_gesture,
          params.opener_suppressed,
          resource_context_,
          render_process_id_,
          is_guest_,
          params.opener_id,
          &no_javascript_access);

  if (!can_create_window) {
    *route_id = MSG_ROUTING_NONE;
    *main_frame_route_id = MSG_ROUTING_NONE;
    *surface_id = 0;
    return;
  }

  // This will clone the sessionStorage for namespace_id_to_clone.
  scoped_refptr<SessionStorageNamespaceImpl> cloned_namespace =
      new SessionStorageNamespaceImpl(dom_storage_context_.get(),
                                      params.session_storage_namespace_id);
  *cloned_session_storage_namespace_id = cloned_namespace->id();

  render_widget_helper_->CreateNewWindow(params,
                                         no_javascript_access,
                                         PeerHandle(),
                                         route_id,
                                         main_frame_route_id,
                                         surface_id,
                                         cloned_namespace.get());
}

void RenderMessageFilter::OnCreateWidget(int opener_id,
                                         blink::WebPopupType popup_type,
                                         int* route_id,
                                         int* surface_id) {
  render_widget_helper_->CreateNewWidget(
      opener_id, popup_type, route_id, surface_id);
}

void RenderMessageFilter::OnCreateFullscreenWidget(int opener_id,
                                                   int* route_id,
                                                   int* surface_id) {
  render_widget_helper_->CreateNewFullscreenWidget(
      opener_id, route_id, surface_id);
}

void RenderMessageFilter::OnGetProcessMemorySizes(size_t* private_bytes,
                                                  size_t* shared_bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  using base::ProcessMetrics;
#if !defined(OS_MACOSX) || defined(OS_IOS)
  scoped_ptr<ProcessMetrics> metrics(ProcessMetrics::CreateProcessMetrics(
      PeerHandle()));
#else
  scoped_ptr<ProcessMetrics> metrics(ProcessMetrics::CreateProcessMetrics(
      PeerHandle(), content::BrowserChildProcessHost::GetPortProvider()));
#endif
  if (!metrics->GetMemoryBytes(private_bytes, shared_bytes)) {
    *private_bytes = 0;
    *shared_bytes = 0;
  }
}

void RenderMessageFilter::OnSetCookie(const IPC::Message& message,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& cookie) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(render_process_id_, url))
    return;

  net::CookieOptions options;
  if (GetContentClient()->browser()->AllowSetCookie(
          url, first_party_for_cookies, cookie,
          resource_context_, render_process_id_, message.routing_id(),
          &options)) {
    net::URLRequestContext* context = GetRequestContextForURL(url);
    // Pass a null callback since we don't care about when the 'set' completes.
    context->cookie_store()->SetCookieWithOptionsAsync(
        url, cookie, options, net::CookieMonster::SetCookiesCallback());
  }
}

void RenderMessageFilter::OnGetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies,
                                       IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(render_process_id_, url)) {
    SendGetCookiesResponse(reply_msg, std::string());
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/99242
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  net::URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster =
      context->cookie_store()->GetCookieMonster();
  cookie_monster->GetAllCookiesForURLAsync(
      url, base::Bind(&RenderMessageFilter::CheckPolicyForCookies, this, url,
                      first_party_for_cookies, reply_msg));
}

void RenderMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  // Only return raw cookies to trusted renderers or if this request is
  // not targeted to an an external host like ChromeFrame.
  // TODO(ananta) We need to support retreiving raw cookies from external
  // hosts.
  if (!policy->CanReadRawCookies(render_process_id_) ||
      !policy->CanAccessCookiesForOrigin(render_process_id_, url)) {
    SendGetRawCookiesResponse(reply_msg, net::CookieList());
    return;
  }

  // We check policy here to avoid sending back cookies that would not normally
  // be applied to outbound requests for the given URL.  Since this cookie info
  // is visible in the developer tools, it is helpful to make it match reality.
  net::URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster =
      context->cookie_store()->GetCookieMonster();
  cookie_monster->GetAllCookiesForURLAsync(
      url, base::Bind(&RenderMessageFilter::SendGetRawCookiesResponse,
                      this, reply_msg));
}

void RenderMessageFilter::OnDeleteCookie(const GURL& url,
                                         const std::string& cookie_name) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(render_process_id_, url))
    return;

  net::URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookieAsync(url, cookie_name, base::Closure());
}

void RenderMessageFilter::OnCookiesEnabled(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* cookies_enabled) {
  // TODO(ananta): If this render view is associated with an automation channel,
  // aka ChromeFrame then we need to retrieve cookie settings from the external
  // host.
  *cookies_enabled = GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, net::CookieList(), resource_context_,
      render_process_id_, MSG_ROUTING_CONTROL);
}

#if defined(OS_MACOSX)
void RenderMessageFilter::OnLoadFont(const FontDescriptor& font,
                                     IPC::Message* reply_msg) {
  FontLoader::Result* result = new FontLoader::Result;

  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FontLoader::LoadFont, font, result),
      base::Bind(&RenderMessageFilter::SendLoadFontReply, this, reply_msg,
                 base::Owned(result)));
}

void RenderMessageFilter::SendLoadFontReply(IPC::Message* reply,
                                            FontLoader::Result* result) {
  base::SharedMemoryHandle handle;
  if (result->font_data_size == 0 || result->font_id == 0) {
    result->font_data_size = 0;
    result->font_id = 0;
    handle = base::SharedMemory::NULLHandle();
  } else {
    result->font_data.GiveToProcess(base::GetCurrentProcessHandle(), &handle);
  }
  ViewHostMsg_LoadFont::WriteReplyParams(
      reply, result->font_data_size, handle, result->font_id);
  Send(reply);
}
#endif  // OS_MACOSX

#if defined(ENABLE_PLUGINS)
void RenderMessageFilter::OnGetPlugins(
    bool refresh,
    IPC::Message* reply_msg) {
  // Don't refresh if the specified threshold has not been passed.  Note that
  // this check is performed before off-loading to the file thread.  The reason
  // we do this is that some pages tend to request that the list of plugins be
  // refreshed at an excessive rate.  This instigates disk scanning, as the list
  // is accumulated by doing multiple reads from disk.  This effect is
  // multiplied when we have several pages requesting this operation.
  if (refresh) {
    const base::TimeDelta threshold = base::TimeDelta::FromSeconds(
        kPluginsRefreshThresholdInSeconds);
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_plugin_refresh_time_ >= threshold) {
      // Only refresh if the threshold hasn't been exceeded yet.
      PluginServiceImpl::GetInstance()->RefreshPlugins();
      last_plugin_refresh_time_ = now;
    }
  }

  PluginServiceImpl::GetInstance()->GetPlugins(
      base::Bind(&RenderMessageFilter::GetPluginsCallback, this, reply_msg));
}

void RenderMessageFilter::GetPluginsCallback(
    IPC::Message* reply_msg,
    const std::vector<WebPluginInfo>& all_plugins) {
  // Filter the plugin list.
  PluginServiceFilter* filter = PluginServiceImpl::GetInstance()->GetFilter();
  std::vector<WebPluginInfo> plugins;

  int child_process_id = -1;
  int routing_id = MSG_ROUTING_NONE;
  for (size_t i = 0; i < all_plugins.size(); ++i) {
    // Copy because the filter can mutate.
    WebPluginInfo plugin(all_plugins[i]);
    if (!filter || filter->IsPluginAvailable(child_process_id,
                                             routing_id,
                                             resource_context_,
                                             GURL(),
                                             GURL(),
                                             &plugin)) {
      plugins.push_back(plugin);
    }
  }

  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  Send(reply_msg);
}

void RenderMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    bool* found,
    WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, render_frame_id, resource_context_,
      url, page_url, mime_type, allow_wildcard,
      NULL, info, actual_mime_type);
}

void RenderMessageFilter::OnOpenChannelToPlugin(int render_frame_id,
                                                const GURL& url,
                                                const GURL& policy_url,
                                                const std::string& mime_type,
                                                IPC::Message* reply_msg) {
  OpenChannelToNpapiPluginCallback* client =
      new OpenChannelToNpapiPluginCallback(this, resource_context_, reply_msg);
  DCHECK(!ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.insert(client);
  plugin_service_->OpenChannelToNpapiPlugin(
      render_process_id_, render_frame_id,
      url, policy_url, mime_type, client);
}

void RenderMessageFilter::OnOpenChannelToPepperPlugin(
    const base::FilePath& path,
    IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPpapiPlugin(
      render_process_id_,
      path,
      profile_data_directory_,
      new OpenChannelToPpapiPluginCallback(this, resource_context_, reply_msg));
}

void RenderMessageFilter::OnDidCreateOutOfProcessPepperInstance(
    int plugin_child_id,
    int32 pp_instance,
    PepperRendererInstanceData instance_data,
    bool is_external) {
  // It's important that we supply the render process ID ourselves based on the
  // channel the message arrived on. We use the
  //   PP_Instance -> (process id, view id)
  // mapping to decide how to handle messages received from the (untrusted)
  // plugin, so an exploited renderer must not be able to insert fake mappings
  // that may allow it access to other render processes.
  DCHECK_EQ(0, instance_data.render_process_id);
  instance_data.render_process_id = render_process_id_;
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->AddInstance(pp_instance, instance_data);
  } else {
    PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
        plugin_child_id, pp_instance, instance_data);
  }
}

void RenderMessageFilter::OnDidDeleteOutOfProcessPepperInstance(
    int plugin_child_id,
    int32 pp_instance,
    bool is_external) {
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->DeleteInstance(pp_instance);
  } else {
    PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
        plugin_child_id, pp_instance);
  }
}

void RenderMessageFilter::OnOpenChannelToPpapiBroker(
    int routing_id,
    const base::FilePath& path) {
  plugin_service_->OpenChannelToPpapiBroker(
      render_process_id_,
      path,
      new OpenChannelToPpapiBrokerCallback(this, routing_id));
}
#endif  // defined(ENABLE_PLUGINS)

void RenderMessageFilter::OnGenerateRoutingID(int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
}

void RenderMessageFilter::OnGetCPUUsage(int* cpu_usage) {
  base::TimeTicks now = base::TimeTicks::Now();
  int64 since_last_sample_ms = (now - cpu_usage_sample_time_).InMilliseconds();
  if (since_last_sample_ms > kCPUUsageSampleIntervalMs) {
    cpu_usage_sample_time_ = now;
    cpu_usage_ = static_cast<int>(process_metrics_->GetCPUUsage());
  }
  *cpu_usage = cpu_usage_;
}

void RenderMessageFilter::OnGetAudioHardwareConfig(
    media::AudioParameters* input_params,
    media::AudioParameters* output_params) {
  DCHECK(input_params);
  DCHECK(output_params);
  *output_params = audio_manager_->GetDefaultOutputStreamParameters();

  // TODO(henrika): add support for all available input devices.
  *input_params = audio_manager_->GetInputStreamParameters(
      media::AudioManagerBase::kDefaultDeviceId);
}

#if defined(OS_WIN)
void RenderMessageFilter::OnGetMonitorColorProfile(std::vector<char>* profile) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (BackingStoreWin::ColorManagementEnabled())
    return;
  *profile = g_color_profile.Get().profile();
}
#endif

void RenderMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                        const GURL& url,
                                        const Referrer& referrer,
                                        const base::string16& suggested_name) {
  scoped_ptr<DownloadSaveInfo> save_info(new DownloadSaveInfo());
  save_info->suggested_name = suggested_name;
  scoped_ptr<net::URLRequest> request(
      resource_context_->GetRequestContext()->CreateRequest(
          url, net::DEFAULT_PRIORITY, NULL));
  RecordDownloadSource(INITIATED_BY_RENDERER);
  resource_dispatcher_host_->BeginDownload(
      request.Pass(),
      referrer,
      true,  // is_content_initiated
      resource_context_,
      render_process_id_,
      message.routing_id(),
      false,
      save_info.Pass(),
      content::DownloadItem::kInvalidId,
      ResourceDispatcherHostImpl::DownloadStartedCallback());
}

void RenderMessageFilter::OnCheckNotificationPermission(
    const GURL& source_origin, int* result) {
#if defined(ENABLE_NOTIFICATIONS)
  *result = GetContentClient()->browser()->
      CheckDesktopNotificationPermission(source_origin, resource_context_,
                                         render_process_id_);
#else
  *result = blink::WebNotificationPresenter::PermissionAllowed;
#endif
}

void RenderMessageFilter::OnAllocateSharedMemory(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  ChildProcessHostImpl::AllocateSharedMemory(
      buffer_size, PeerHandle(), handle);
}

net::URLRequestContext* RenderMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequestContext* context =
      GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context_);
  if (!context)
    context = request_context_->GetURLRequestContext();

  return context;
}

#if defined(OS_POSIX) && !defined(TOOLKIT_GTK) && !defined(OS_ANDROID)
void RenderMessageFilter::OnAllocTransportDIB(
    uint32 size, bool cache_in_browser, TransportDIB::Handle* handle) {
  render_widget_helper_->AllocTransportDIB(size, cache_in_browser, handle);
}

void RenderMessageFilter::OnFreeTransportDIB(
    TransportDIB::Id dib_id) {
  render_widget_helper_->FreeTransportDIB(dib_id);
}
#endif

bool RenderMessageFilter::CheckPreparsedJsCachingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnablePreparsedJsCaching);
    checked = true;
  }
  return result;
}

void RenderMessageFilter::OnCacheableMetadataAvailable(
    const GURL& url,
    double expected_response_time,
    const std::vector<char>& data) {
  if (!CheckPreparsedJsCachingEnabled())
    return;

  net::HttpCache* cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  DCHECK(cache);

  // Use the same priority for the metadata write as for script
  // resources (see defaultPriorityForResourceType() in WebKit's
  // CachedResource.cpp). Note that WebURLRequest::PriorityMedium
  // corresponds to net::LOW (see ConvertWebKitPriorityToNetPriority()
  // in weburlloader_impl.cc).
  const net::RequestPriority kPriority = net::LOW;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(data.size()));
  memcpy(buf->data(), &data.front(), data.size());
  cache->WriteMetadata(url,
                       kPriority,
                       base::Time::FromDoubleT(expected_response_time),
                       buf.get(),
                       data.size());
}

void RenderMessageFilter::OnKeygen(uint32 key_size_index,
                                   const std::string& challenge_string,
                                   const GURL& url,
                                   IPC::Message* reply_msg) {
  // Map displayed strings indicating level of keysecurity in the <keygen>
  // menu to the key size in bits. (See SSLKeyGeneratorChromium.cpp in WebCore.)
  int key_size_in_bits;
  switch (key_size_index) {
    case 0:
      key_size_in_bits = 2048;
      break;
    case 1:
      key_size_in_bits = 1024;
      break;
    default:
      DCHECK(false) << "Illegal key_size_index " << key_size_index;
      ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
      Send(reply_msg);
      return;
  }

  resource_context_->CreateKeygenHandler(
      key_size_in_bits,
      challenge_string,
      url,
      base::Bind(
          &RenderMessageFilter::PostKeygenToWorkerThread, this, reply_msg));
}

void RenderMessageFilter::PostKeygenToWorkerThread(
    IPC::Message* reply_msg,
    scoped_ptr<net::KeygenHandler> keygen_handler) {
  VLOG(1) << "Dispatching keygen task to worker pool.";
  // Dispatch to worker pool, so we do not block the IO thread.
  if (!base::WorkerPool::PostTask(
           FROM_HERE,
           base::Bind(&RenderMessageFilter::OnKeygenOnWorkerThread,
                      this,
                      base::Passed(&keygen_handler),
                      reply_msg),
           true)) {
    NOTREACHED() << "Failed to dispatch keygen task to worker pool";
    ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
    Send(reply_msg);
  }
}

void RenderMessageFilter::OnKeygenOnWorkerThread(
    scoped_ptr<net::KeygenHandler> keygen_handler,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);

  // Generate a signed public key and challenge, then send it back.
  ViewHostMsg_Keygen::WriteReplyParams(
      reply_msg,
      keygen_handler->GenKeyAndSignChallenge());
  Send(reply_msg);
}

void RenderMessageFilter::OnMediaLogEvents(
    const std::vector<media::MediaLogEvent>& events) {
  if (media_internals_)
    media_internals_->OnMediaEvents(render_process_id_, events);
}

void RenderMessageFilter::CheckPolicyForCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  // Check the policy for get cookies, and pass cookie_list to the
  // TabSpecificContentSetting for logging purpose.
  if (GetContentClient()->browser()->AllowGetCookie(
          url, first_party_for_cookies, cookie_list, resource_context_,
          render_process_id_, reply_msg->routing_id())) {
    // Gets the cookies from cookie store if allowed.
    context->cookie_store()->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(),
        base::Bind(&RenderMessageFilter::SendGetCookiesResponse,
                   this, reply_msg));
  } else {
    SendGetCookiesResponse(reply_msg, std::string());
  }
}

void RenderMessageFilter::SendGetCookiesResponse(IPC::Message* reply_msg,
                                                 const std::string& cookies) {
  ViewHostMsg_GetCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderMessageFilter::SendGetRawCookiesResponse(
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  std::vector<CookieData> cookies;
  for (size_t i = 0; i < cookie_list.size(); ++i)
    cookies.push_back(CookieData(cookie_list[i]));
  ViewHostMsg_GetRawCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderMessageFilter::OnCompletedOpenChannelToNpapiPlugin(
    OpenChannelToNpapiPluginCallback* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.erase(client);
}

void RenderMessageFilter::OnUpdateIsDelayed(const IPC::Message& msg) {
  // When not in accelerated compositing mode, in certain cases (e.g. waiting
  // for a resize or if no backing store) the RenderWidgetHost is blocking the
  // UI thread for some time, waiting for an UpdateRect from the renderer. If we
  // are going to switch to accelerated compositing, the GPU process may need
  // round-trips to the UI thread before finishing the frame, causing deadlocks
  // if we delay the UpdateRect until we receive the OnSwapBuffersComplete. So
  // the renderer sent us this message, so that we can unblock the UI thread.
  // We will simply re-use the UpdateRect unblock mechanism, just with a
  // different message.
  render_widget_helper_->DidReceiveBackingStoreMsg(msg);
}

void RenderMessageFilter::OnAre3DAPIsBlocked(int render_view_id,
                                             const GURL& top_origin_url,
                                             ThreeDAPIType requester,
                                             bool* blocked) {
  *blocked = GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      top_origin_url, render_process_id_, render_view_id, requester);
}

void RenderMessageFilter::OnDidLose3DContext(
    const GURL& top_origin_url,
    ThreeDAPIType /* unused */,
    int arb_robustness_status_code) {
#if defined(OS_MACOSX)
    // TODO(kbr): this file indirectly includes npapi.h, which on Mac
    // OS pulls in the system OpenGL headers. For some
    // not-yet-investigated reason this breaks the build with the 10.6
    // SDK but not 10.7. For now work around this in a way compatible
    // with the Khronos headers.
#ifndef GL_GUILTY_CONTEXT_RESET_ARB
#define GL_GUILTY_CONTEXT_RESET_ARB 0x8253
#endif
#ifndef GL_INNOCENT_CONTEXT_RESET_ARB
#define GL_INNOCENT_CONTEXT_RESET_ARB 0x8254
#endif
#ifndef GL_UNKNOWN_CONTEXT_RESET_ARB
#define GL_UNKNOWN_CONTEXT_RESET_ARB 0x8255
#endif

#endif
  GpuDataManagerImpl::DomainGuilt guilt;
  switch (arb_robustness_status_code) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_KNOWN;
      break;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN;
      break;
    default:
      // Ignore lost contexts known to be innocent.
      return;
  }

  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(
      top_origin_url, guilt);
}

#if defined(OS_WIN)
void RenderMessageFilter::OnPreCacheFontCharacters(const LOGFONT& font,
                                                   const base::string16& str) {
  // First, comments from FontCacheDispatcher::OnPreCacheFont do apply here too.
  // Except that for True Type fonts,
  // GetTextMetrics will not load the font in memory.
  // The only way windows seem to load properly, it is to create a similar
  // device (like the one in which we print), then do an ExtTextOut,
  // as we do in the printing thread, which is sandboxed.
  HDC hdc = CreateEnhMetaFile(NULL, NULL, NULL, NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  ExtTextOut(hdc, 0, 0, ETO_GLYPH_INDEX, 0, str.c_str(), str.length(), NULL);

  SelectObject(hdc, old_font);
  DeleteObject(font_handle);

  HENHMETAFILE metafile = CloseEnhMetaFile(hdc);

  if (metafile) {
    DeleteEnhMetaFile(metafile);
  }
}
#endif

#if defined(OS_ANDROID)
void RenderMessageFilter::OnWebAudioMediaCodec(
    base::SharedMemoryHandle encoded_data_handle,
    base::FileDescriptor pcm_output,
    uint32_t data_size) {
  // Let a WorkerPool handle this request since the WebAudio
  // MediaCodec bridge is slow and can block while sending the data to
  // the renderer.
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&media::WebAudioMediaCodecBridge::RunWebAudioMediaCodec,
                 encoded_data_handle, pcm_output, data_size),
      true);
}
#endif

void RenderMessageFilter::OnAllocateGpuMemoryBuffer(
    uint32 width,
    uint32 height,
    uint32 internalformat,
    gfx::GpuMemoryBufferHandle* handle) {
  if (!GpuMemoryBufferImpl::IsFormatValid(internalformat)) {
    handle->type = gfx::EMPTY_BUFFER;
    return;
  }

#if defined(OS_MACOSX)
  if (GpuMemoryBufferImplIOSurface::IsFormatSupported(internalformat)) {
    IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
    if (io_surface_support) {
      base::ScopedCFTypeRef<CFMutableDictionaryRef> properties;
      properties.reset(
          CFDictionaryCreateMutable(kCFAllocatorDefault,
                                    0,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks));
      AddIntegerValue(properties,
                      io_surface_support->GetKIOSurfaceWidth(),
                      width);
      AddIntegerValue(properties,
                      io_surface_support->GetKIOSurfaceHeight(),
                      height);
      AddIntegerValue(properties,
                      io_surface_support->GetKIOSurfaceBytesPerElement(),
                      GpuMemoryBufferImpl::BytesPerPixel(internalformat));
      AddIntegerValue(properties,
                      io_surface_support->GetKIOSurfacePixelFormat(),
                      GpuMemoryBufferImplIOSurface::PixelFormat(
                          internalformat));
      // TODO(reveman): Remove this when using a mach_port_t to transfer
      // IOSurface to renderer process. crbug.com/323304
      AddBooleanValue(properties,
                      io_surface_support->GetKIOSurfaceIsGlobal(),
                      true);

      base::ScopedCFTypeRef<CFTypeRef> io_surface(
          io_surface_support->IOSurfaceCreate(properties));
      if (io_surface) {
        handle->type = gfx::IO_SURFACE_BUFFER;
        handle->io_surface_id = io_surface_support->IOSurfaceGetID(io_surface);

        // TODO(reveman): This makes the assumption that the renderer will
        // grab a reference to the surface before sending another message.
        // crbug.com/325045
        last_io_surface_ = io_surface;
        return;
      }
    }
  }
#endif

  uint64 stride = static_cast<uint64>(width) *
      GpuMemoryBufferImpl::BytesPerPixel(internalformat);
  if (stride > std::numeric_limits<uint32>::max()) {
    handle->type = gfx::EMPTY_BUFFER;
    return;
  }

  uint64 buffer_size = stride * static_cast<uint64>(height);
  if (buffer_size > std::numeric_limits<size_t>::max()) {
    handle->type = gfx::EMPTY_BUFFER;
    return;
  }

  // Fallback to fake GpuMemoryBuffer that is backed by shared memory and
  // requires an upload before it can be used as a texture.
  handle->type = gfx::SHARED_MEMORY_BUFFER;
  ChildProcessHostImpl::AllocateSharedMemory(
      static_cast<size_t>(buffer_size), PeerHandle(), &handle->handle);
}

}  // namespace content
