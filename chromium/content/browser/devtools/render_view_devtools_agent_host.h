// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class DevToolsTracingHandler;
class RendererOverridesHandler;
class RenderViewHost;

class CONTENT_EXPORT RenderViewDevToolsAgentHost
    : public IPCDevToolsAgentHost,
      private WebContentsObserver {
 public:
  static void OnCancelPendingNavigation(RenderViewHost* pending,
                                        RenderViewHost* current);

  RenderViewDevToolsAgentHost(RenderViewHost*);

  RenderViewHost* render_view_host() { return render_view_host_; }

 private:
  friend class DevToolsAgentHost;
  class DevToolsAgentHostRvhObserver;

  virtual ~RenderViewDevToolsAgentHost();

  // DevTooolsAgentHost overrides.
  virtual void DisconnectRenderViewHost() OVERRIDE;
  virtual void ConnectRenderViewHost(RenderViewHost* rvh) OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() OVERRIDE;

  // IPCDevToolsAgentHost overrides.
  virtual void DispatchOnInspectorBackend(const std::string& message) OVERRIDE;
  virtual void SendMessageToAgent(IPC::Message* msg) OVERRIDE;
  virtual void OnClientAttached() OVERRIDE;
  virtual void OnClientDetached() OVERRIDE;

  // WebContentsObserver overrides.
  virtual void AboutToNavigateRenderView(RenderViewHost* dest_rvh) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void DidAttachInterstitialPage() OVERRIDE;

  void SetRenderViewHost(RenderViewHost* rvh);

  void RenderViewHostDestroyed(RenderViewHost* rvh);
  void RenderViewCrashed();
  bool OnRvhMessageReceived(const IPC::Message& message);

  void OnDispatchOnInspectorFrontend(const std::string& message);
  void OnSaveAgentRuntimeState(const std::string& state);
  void OnClearBrowserCache();
  void OnClearBrowserCookies();

  RenderViewHost* render_view_host_;
  scoped_ptr<DevToolsAgentHostRvhObserver> rvh_observer_;
  scoped_ptr<RendererOverridesHandler> overrides_handler_;
  scoped_ptr<DevToolsTracingHandler> tracing_handler_;
  std::string state_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_VIEW_DEVTOOLS_AGENT_HOST_H_
