// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_IMPL_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_IMPL_H_

#include <map>

#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "ui/gfx/size.h"

namespace gfx {
class Point;
}

namespace content {

class BrowserPluginManagerImpl : public BrowserPluginManager {
 public:
  explicit BrowserPluginManagerImpl(RenderViewImpl* render_view);

  // BrowserPluginManager implementation.
  virtual BrowserPlugin* CreateBrowserPlugin(
      RenderViewImpl* render_view,
      blink::WebFrame* frame) OVERRIDE;
  virtual void AllocateInstanceID(
      const base::WeakPtr<BrowserPlugin>& browser_plugin) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // RenderViewObserver override. Call on render thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCommitCompositorFrame() OVERRIDE;

 private:
  virtual ~BrowserPluginManagerImpl();

  void OnAllocateInstanceIDACK(const IPC::Message& message,
                               int request_id,
                               int guest_instance_id);
  void OnPluginAtPositionRequest(const IPC::Message& message,
                                 int request_id,
                                 const gfx::Point& position);

  int request_id_counter_;
  typedef std::map<int, const base::WeakPtr<BrowserPlugin> > InstanceIDMap;
  InstanceIDMap pending_allocate_guest_instance_id_requests_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginManagerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_IMPL_H_
