// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/renderer_ppapi_host_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "content/common/sandbox_util.h"
#include "content/renderer/pepper/fullscreen_container.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_in_process_resource_creation.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "ipc/ipc_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "ui/gfx/point.h"

namespace content {
// static
CONTENT_EXPORT RendererPpapiHost*
RendererPpapiHost::GetForPPInstance(PP_Instance instance) {
  return RendererPpapiHostImpl::GetForPPInstance(instance);
}

// Out-of-process constructor.
RendererPpapiHostImpl::RendererPpapiHostImpl(
    PluginModule* module,
    ppapi::proxy::HostDispatcher* dispatcher,
    const ppapi::PpapiPermissions& permissions)
    : module_(module),
      dispatcher_(dispatcher) {
  // Hook the PpapiHost up to the dispatcher for out-of-process communication.
  ppapi_host_.reset(
      new ppapi::host::PpapiHost(dispatcher, permissions));
  ppapi_host_->AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentRendererPepperHostFactory(this)));
  dispatcher->AddFilter(ppapi_host_.get());
  is_running_in_process_ = false;
}

// In-process constructor.
RendererPpapiHostImpl::RendererPpapiHostImpl(
    PluginModule* module,
    const ppapi::PpapiPermissions& permissions)
    : module_(module),
      dispatcher_(NULL) {
  // Hook the host up to the in-process router.
  in_process_router_.reset(new PepperInProcessRouter(this));
  ppapi_host_.reset(new ppapi::host::PpapiHost(
      in_process_router_->GetRendererToPluginSender(), permissions));
  ppapi_host_->AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentRendererPepperHostFactory(this)));
  is_running_in_process_ = true;
}

RendererPpapiHostImpl::~RendererPpapiHostImpl() {
  // Delete the host explicitly first. This shutdown will destroy the
  // resources, which may want to do cleanup in their destructors and expect
  // their pointers to us to be valid.
  ppapi_host_.reset();
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::CreateOnModuleForOutOfProcess(
    PluginModule* module,
    ppapi::proxy::HostDispatcher* dispatcher,
    const ppapi::PpapiPermissions& permissions) {
  DCHECK(!module->renderer_ppapi_host());
  RendererPpapiHostImpl* result = new RendererPpapiHostImpl(
      module, dispatcher, permissions);

  // Takes ownership of pointer.
  module->SetRendererPpapiHost(scoped_ptr<RendererPpapiHostImpl>(result));

  return result;
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::CreateOnModuleForInProcess(
    PluginModule* module,
    const ppapi::PpapiPermissions& permissions) {
  DCHECK(!module->renderer_ppapi_host());
  RendererPpapiHostImpl* result = new RendererPpapiHostImpl(
      module, permissions);

  // Takes ownership of pointer.
  module->SetRendererPpapiHost(scoped_ptr<RendererPpapiHostImpl>(result));

  return result;
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::GetForPPInstance(
    PP_Instance pp_instance) {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return NULL;

  // All modules created by content will have their embedder state be the
  // host impl.
  return instance->module()->renderer_ppapi_host();
}

scoped_ptr<ppapi::thunk::ResourceCreationAPI>
RendererPpapiHostImpl::CreateInProcessResourceCreationAPI(
    PepperPluginInstanceImpl* instance) {
  return scoped_ptr<ppapi::thunk::ResourceCreationAPI>(
      new PepperInProcessResourceCreation(this, instance));
}

PepperPluginInstanceImpl* RendererPpapiHostImpl::GetPluginInstanceImpl(
    PP_Instance instance) const {
  return GetAndValidateInstance(instance);
}

ppapi::host::PpapiHost* RendererPpapiHostImpl::GetPpapiHost() {
  return ppapi_host_.get();
}

RenderFrame* RendererPpapiHostImpl::GetRenderFrameForInstance(
    PP_Instance instance) const  {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return NULL;

  // Since we're the embedder, we can make assumptions about the helper on
  // the instance and get back to our RenderFrame.
  return instance_object->render_frame();
}

RenderView* RendererPpapiHostImpl::GetRenderViewForInstance(
    PP_Instance instance) const {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return NULL;

  // Since we're the embedder, we can make assumptions about the helper on
  // the instance and get back to our RenderView.
  return instance_object->render_frame()->render_view();
}

bool RendererPpapiHostImpl::IsValidInstance(PP_Instance instance) const {
  return !!GetAndValidateInstance(instance);
}

PepperPluginInstance* RendererPpapiHostImpl::GetPluginInstance(
    PP_Instance instance) const {
  return GetAndValidateInstance(instance);
}

blink::WebPluginContainer* RendererPpapiHostImpl::GetContainerForInstance(
      PP_Instance instance) const {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return NULL;
  return instance_object->container();
}

base::ProcessId RendererPpapiHostImpl::GetPluginPID() const {
  if (dispatcher_)
    return dispatcher_->channel()->peer_pid();
  return base::kNullProcessId;
}

bool RendererPpapiHostImpl::HasUserGesture(PP_Instance instance) const {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return false;

  if (instance_object->module()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE))
    return true;
  return instance_object->IsProcessingUserGesture();
}

int RendererPpapiHostImpl::GetRoutingIDForWidget(PP_Instance instance) const {
  PepperPluginInstanceImpl* plugin_instance = GetAndValidateInstance(instance);
  if (!plugin_instance)
    return 0;
  if (plugin_instance->flash_fullscreen()) {
    FullscreenContainer* container = plugin_instance->fullscreen_container();
    return static_cast<RenderWidgetFullscreenPepper*>(container)->routing_id();
  }
  return GetRenderViewForInstance(instance)->GetRoutingID();
}

gfx::Point RendererPpapiHostImpl::PluginPointToRenderFrame(
    PP_Instance instance,
    const gfx::Point& pt) const {
  PepperPluginInstanceImpl* plugin_instance = GetAndValidateInstance(instance);
  if (!plugin_instance)
    return pt;

  RenderFrameImpl* render_frame = static_cast<RenderFrameImpl*>(
      GetRenderFrameForInstance(instance));
  if (plugin_instance->view_data().is_fullscreen ||
      plugin_instance->flash_fullscreen()) {
    blink::WebRect window_rect = render_frame->GetRenderWidget()->windowRect();
    blink::WebRect screen_rect =
        render_frame->GetRenderWidget()->screenInfo().rect;
    return gfx::Point(pt.x() - window_rect.x + screen_rect.x,
                      pt.y() - window_rect.y + screen_rect.y);
  }
  return gfx::Point(pt.x() + plugin_instance->view_data().rect.point.x,
                    pt.y() + plugin_instance->view_data().rect.point.y);
}

IPC::PlatformFileForTransit RendererPpapiHostImpl::ShareHandleWithRemote(
    base::PlatformFile handle,
    bool should_close_source) {
  if (!dispatcher_) {
    DCHECK(is_running_in_process_);
    // Duplicate the file handle for in process mode so this function
    // has the same semantics for both in process mode and out of
    // process mode (i.e., the remote side must cloes the handle).
    return BrokerGetFileHandleForProcess(handle,
                                         base::GetCurrentProcId(),
                                         should_close_source);
  }
  return dispatcher_->ShareHandleWithRemote(handle, should_close_source);
}

bool RendererPpapiHostImpl::IsRunningInProcess() const {
  return is_running_in_process_;
}

void RendererPpapiHostImpl::CreateBrowserResourceHosts(
    PP_Instance instance,
    const std::vector<IPC::Message>& nested_msgs,
    const base::Callback<void(const std::vector<int>&)>& callback) const {
  RenderFrame* render_frame = GetRenderFrameForInstance(instance);
  PepperBrowserConnection* browser_connection =
      PepperBrowserConnection::Get(render_frame);
  if (!browser_connection) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, std::vector<int>(nested_msgs.size(), 0)));
  } else {
    browser_connection->SendBrowserCreate(module_->GetPluginChildId(),
                                          instance,
                                          nested_msgs,
                                          callback);
  }
}

GURL RendererPpapiHostImpl::GetDocumentURL(PP_Instance instance) const {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return GURL();

  return instance_object->container()->element().document().url();
}

PepperPluginInstanceImpl* RendererPpapiHostImpl::GetAndValidateInstance(
    PP_Instance pp_instance) const {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return NULL;
  if (!instance->IsValidInstanceOf(module_))
    return NULL;
  return instance;
}

}  // namespace content
