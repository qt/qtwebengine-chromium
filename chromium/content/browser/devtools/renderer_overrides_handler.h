// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/devtools/devtools_protocol.h"

class SkBitmap;

namespace IPC {
class Message;
}

namespace content {

class DevToolsAgentHost;
class DevToolsTracingHandler;

// Overrides Inspector commands before they are sent to the renderer.
// May override the implementation completely, ignore it, or handle
// additional browser process implementation details.
class RendererOverridesHandler : public DevToolsProtocol::Handler {
 public:
  explicit RendererOverridesHandler(DevToolsAgentHost* agent);
  virtual ~RendererOverridesHandler();

  void OnClientDetached();
  void OnSwapCompositorFrame(const IPC::Message& message);
  void OnVisibilityChanged(bool visible);

 private:
  void InnerSwapCompositorFrame();
  void ParseCaptureParameters(DevToolsProtocol::Command* command,
                              std::string* format, int* quality,
                              double* scale);

  // DOM domain.
  scoped_refptr<DevToolsProtocol::Response>
      GrantPermissionsForSetFileInputFiles(
          scoped_refptr<DevToolsProtocol::Command> command);

  // Page domain.
  scoped_refptr<DevToolsProtocol::Response> PageDisable(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageHandleJavaScriptDialog(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageNavigate(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageReload(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageGetNavigationHistory(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageNavigateToHistoryEntry(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageCaptureScreenshot(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageStartScreencast(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> PageStopScreencast(
      scoped_refptr<DevToolsProtocol::Command> command);

  void ScreenshotCaptured(
      scoped_refptr<DevToolsProtocol::Command> command,
      const std::string& format,
      int quality,
      const cc::CompositorFrameMetadata& metadata,
      bool success,
      const SkBitmap& bitmap);

  void NotifyScreencastVisibility(bool visible);

  // Input domain.
  scoped_refptr<DevToolsProtocol::Response> InputDispatchMouseEvent(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> InputDispatchGestureEvent(
      scoped_refptr<DevToolsProtocol::Command> command);

  DevToolsAgentHost* agent_;
  base::WeakPtrFactory<RendererOverridesHandler> weak_factory_;
  scoped_refptr<DevToolsProtocol::Command> screencast_command_;
  cc::CompositorFrameMetadata last_compositor_frame_metadata_;
  base::TimeTicks last_frame_time_;
  DISALLOW_COPY_AND_ASSIGN(RendererOverridesHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDERER_OVERRIDES_HANDLER_H_
