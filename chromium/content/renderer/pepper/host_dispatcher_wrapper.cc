// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_dispatcher_wrapper.h"

#include "content/common/view_messages.h"
#include "content/renderer/pepper/pepper_hung_plugin_filter.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/pepper/renderer_restrict_dispatch_group.h"
#include "content/renderer/render_view_impl.h"

namespace content {

HostDispatcherWrapper::HostDispatcherWrapper(
    PluginModule* module,
    base::ProcessId peer_pid,
    int plugin_child_id,
    const ppapi::PpapiPermissions& perms,
    bool is_external)
    : module_(module),
      peer_pid_(peer_pid),
      plugin_child_id_(plugin_child_id),
      permissions_(perms),
      is_external_(is_external) {
}

HostDispatcherWrapper::~HostDispatcherWrapper() {
}

bool HostDispatcherWrapper::Init(const IPC::ChannelHandle& channel_handle,
                                 PP_GetInterface_Func local_get_interface,
                                 const ppapi::Preferences& preferences,
                                 PepperHungPluginFilter* filter) {
  if (channel_handle.name.empty())
    return false;

#if defined(OS_POSIX)
  DCHECK_NE(-1, channel_handle.socket.fd);
  if (channel_handle.socket.fd == -1)
    return false;
#endif

  dispatcher_delegate_.reset(new PepperProxyChannelDelegateImpl);
  dispatcher_.reset(new ppapi::proxy::HostDispatcher(
      module_->pp_module(), local_get_interface, filter, permissions_));

  if (!dispatcher_->InitHostWithChannel(dispatcher_delegate_.get(),
                                        peer_pid_,
                                        channel_handle,
                                        true,  // Client.
                                        preferences)) {
    dispatcher_.reset();
    dispatcher_delegate_.reset();
    return false;
  }
  dispatcher_->channel()->SetRestrictDispatchChannelGroup(
      kRendererRestrictDispatchGroup_Pepper);
  return true;
}

const void* HostDispatcherWrapper::GetProxiedInterface(const char* name) {
  return dispatcher_->GetProxiedInterface(name);
}

void HostDispatcherWrapper::AddInstance(PP_Instance instance) {
  ppapi::proxy::HostDispatcher::SetForInstance(instance, dispatcher_.get());

  RendererPpapiHostImpl* host =
      RendererPpapiHostImpl::GetForPPInstance(instance);
  // TODO(brettw) remove this null check when the old-style pepper-based
  // browser tag is removed from this file. Getting this notification should
  // always give us an instance we can find in the map otherwise, but that
  // isn't true for browser tag support.
  if (host) {
    RenderView* render_view = host->GetRenderViewForInstance(instance);
    PepperPluginInstance* plugin_instance = host->GetPluginInstance(instance);
    render_view->Send(new ViewHostMsg_DidCreateOutOfProcessPepperInstance(
        plugin_child_id_,
        instance,
        PepperRendererInstanceData(
            0,  // The render process id will be supplied in the browser.
            render_view->GetRoutingID(),
            host->GetDocumentURL(instance),
            plugin_instance->GetPluginURL()),
        is_external_));
  }
}

void HostDispatcherWrapper::RemoveInstance(PP_Instance instance) {
  ppapi::proxy::HostDispatcher::RemoveForInstance(instance);

  RendererPpapiHostImpl* host =
      RendererPpapiHostImpl::GetForPPInstance(instance);
  // TODO(brettw) remove null check as described in AddInstance.
  if (host) {
    RenderView* render_view = host->GetRenderViewForInstance(instance);
    render_view->Send(new ViewHostMsg_DidDeleteOutOfProcessPepperInstance(
        plugin_child_id_,
        instance,
        is_external_));
  }
}

}  // namespace content
