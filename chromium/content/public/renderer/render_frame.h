// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"

struct WebPreferences;

namespace blink {
class WebFrame;
class WebPlugin;
class WebURLRequest;
struct WebPluginParams;
}

namespace content {
class ContextMenuClient;
class RenderView;
struct ContextMenuParams;
struct WebPluginInfo;

// This interface wraps functionality, which is specific to frames, such as
// navigation. It provides communication with a corresponding RenderFrameHost
// in the browser process.
class CONTENT_EXPORT RenderFrame : public IPC::Listener,
                                   public IPC::Sender {
 public:
  // Returns the RenderView associated with this frame.
  virtual RenderView* GetRenderView() = 0;

  // Get the routing ID of the frame.
  virtual int GetRoutingID() = 0;

   // Gets WebKit related preferences associated with this frame.
  virtual WebPreferences& GetWebkitPreferences() = 0;

  // Shows a context menu with the given information. The given client will
  // be called with the result.
  //
  // The request ID will be returned by this function. This is passed to the
  // client functions for identification.
  //
  // If the client is destroyed, CancelContextMenu() should be called with the
  // request ID returned by this function.
  //
  // Note: if you end up having clients outliving the RenderFrame, we should add
  // a CancelContextMenuCallback function that takes a request id.
  virtual int ShowContextMenu(ContextMenuClient* client,
                              const ContextMenuParams& params) = 0;

  // Cancels a context menu in the event that the client is destroyed before the
  // menu is closed.
  virtual void CancelContextMenu(int request_id) = 0;

  // Create a new NPAPI/Pepper plugin depending on |info|. Returns NULL if no
  // plugin was found.
  virtual blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params) = 0;

  // The client should handle the navigation externally.
  virtual void LoadURLExternally(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      blink::WebNavigationPolicy policy) = 0;

 protected:
  virtual ~RenderFrame() {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameImpl;
  RenderFrame() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_H_
