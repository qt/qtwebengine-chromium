// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/public/renderer/content_renderer_client.h"

namespace blink {
class WebFrame;
class WebPlugin;
struct WebPluginParams;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace content {

class MockWebClipboardImpl;
class ShellRenderProcessObserver;

class ShellContentRendererClient : public ContentRendererClient {
 public:
  static ShellContentRendererClient* Get();

  ShellContentRendererClient();
  virtual ~ShellContentRendererClient();

  // ContentRendererClient implementation.
  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderViewCreated(RenderView* render_view) OVERRIDE;
  virtual bool OverrideCreatePlugin(
      RenderFrame* render_frame,
      blink::WebFrame* frame,
      const blink::WebPluginParams& params,
      blink::WebPlugin** plugin) OVERRIDE;
  virtual blink::WebMediaStreamCenter* OverrideCreateWebMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client) OVERRIDE;
  virtual blink::WebRTCPeerConnectionHandler*
  OverrideCreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client) OVERRIDE;
  virtual blink::WebMIDIAccessor* OverrideCreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client) OVERRIDE;
  virtual blink::WebAudioDevice* OverrideCreateAudioDevice(
      double sample_rate) OVERRIDE;
  virtual blink::WebClipboard* OverrideWebClipboard() OVERRIDE;
  virtual blink::WebThemeEngine* OverrideThemeEngine() OVERRIDE;
  virtual bool AllowBrowserPlugin(
      blink::WebPluginContainer* container) OVERRIDE;

 private:
   void WebTestProxyCreated(RenderView* render_view,
                            WebTestRunner::WebTestProxyBase* proxy);

  scoped_ptr<ShellRenderProcessObserver> shell_observer_;
  scoped_ptr<MockWebClipboardImpl> clipboard_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_
