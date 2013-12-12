// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/mock_renderer_ppapi_host.h"

#include "ui/gfx/point.h"

namespace content {

MockRendererPpapiHost::MockRendererPpapiHost(RenderView* render_view,
                                             PP_Instance instance)
    : sink_(),
      ppapi_host_(&sink_, ppapi::PpapiPermissions()),
      render_view_(render_view),
      pp_instance_(instance),
      has_user_gesture_(false) {
}

MockRendererPpapiHost::~MockRendererPpapiHost() {
}

ppapi::host::PpapiHost* MockRendererPpapiHost::GetPpapiHost() {
  return &ppapi_host_;
}

bool MockRendererPpapiHost::IsValidInstance(PP_Instance instance) const {
  return instance == pp_instance_;
}

PepperPluginInstance* MockRendererPpapiHost::GetPluginInstance(
    PP_Instance instance) const {
  NOTIMPLEMENTED();
  return NULL;
}

RenderView* MockRendererPpapiHost::GetRenderViewForInstance(
    PP_Instance instance) const {
  if (instance == pp_instance_)
    return render_view_;
  return NULL;
}

WebKit::WebPluginContainer* MockRendererPpapiHost::GetContainerForInstance(
    PP_Instance instance) const {
  NOTIMPLEMENTED();
  return NULL;
}

base::ProcessId MockRendererPpapiHost::GetPluginPID() const {
  NOTIMPLEMENTED();
  return base::kNullProcessId;
}

bool MockRendererPpapiHost::HasUserGesture(PP_Instance instance) const {
  return has_user_gesture_;
}

int MockRendererPpapiHost::GetRoutingIDForWidget(PP_Instance instance) const {
  return 0;
}

gfx::Point MockRendererPpapiHost::PluginPointToRenderView(
    PP_Instance instance,
    const gfx::Point& pt) const {
  return gfx::Point();
}

IPC::PlatformFileForTransit MockRendererPpapiHost::ShareHandleWithRemote(
    base::PlatformFile handle,
    bool should_close_source) {
  NOTIMPLEMENTED();
  return IPC::InvalidPlatformFileForTransit();
}

bool MockRendererPpapiHost::IsRunningInProcess() const {
  NOTIMPLEMENTED();
  return false;
}

void MockRendererPpapiHost::CreateBrowserResourceHosts(
    PP_Instance instance,
    const std::vector<IPC::Message>& nested_msgs,
    const base::Callback<void(const std::vector<int>&)>& callback) const {
  callback.Run(std::vector<int>(nested_msgs.size(), 0));
  return;
}

}  // namespace content
