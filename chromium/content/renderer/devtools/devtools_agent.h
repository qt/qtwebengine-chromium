// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_

#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebDevToolsAgentClient.h"

namespace blink {
class WebDevToolsAgent;
}

struct GpuTaskInfo;

namespace content {
class RenderViewImpl;

// DevToolsAgent belongs to the inspectable RenderView and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this communication agent.
// There is a corresponding DevToolsClient object on the client side.
class DevToolsAgent : public RenderViewObserver,
                      public base::SupportsWeakPtr<DevToolsAgent>,
                      public blink::WebDevToolsAgentClient {
 public:
  explicit DevToolsAgent(RenderViewImpl* render_view);
  virtual ~DevToolsAgent();

  // Returns agent instance for its host id.
  static DevToolsAgent* FromHostId(int host_id);

  blink::WebDevToolsAgent* GetWebAgent();

  bool IsAttached();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebDevToolsAgentClient implementation
  virtual void sendMessageToInspectorFrontend(const blink::WebString& data);

  virtual int hostIdentifier();
  virtual void saveAgentRuntimeState(const blink::WebString& state);
  virtual blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop();
  virtual void clearBrowserCache();
  virtual void clearBrowserCookies();
  virtual void visitAllocatedObjects(AllocatedObjectVisitor* visitor);

  typedef void (*TraceEventCallback)(
      char phase, const unsigned char*, const char* name, unsigned long long id,
      int numArgs, const char* const* argNames, const unsigned char* argTypes,
      const unsigned long long* argValues,
      unsigned char flags, double timestamp);
  virtual void setTraceEventCallback(TraceEventCallback cb) OVERRIDE;
  virtual void startGPUEventsRecording() OVERRIDE;
  virtual void stopGPUEventsRecording() OVERRIDE;

  virtual void enableDeviceEmulation(
      const blink::WebRect& device_rect,
      const blink::WebRect& view_rect, float device_scale_factor,
      bool fit_to_view);
  virtual void disableDeviceEmulation();

  void OnAttach();
  void OnReattach(const std::string& agent_state);
  void OnDetach();
  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnInspectElement(int x, int y);
  void OnAddMessageToConsole(ConsoleMessageLevel level,
                             const std::string& message);
  void OnGpuTasksChunk(const std::vector<GpuTaskInfo>& tasks);
  void ContinueProgram();
  void OnSetupDevToolsClient();

  static void TraceEventCallbackWrapper(
      base::TimeTicks timestamp,
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      unsigned long long id,
      int num_args,
      const char* const arg_names[],
      const unsigned char arg_types[],
      const unsigned long long arg_values[],
      unsigned char flags);

  bool is_attached_;
  bool is_devtools_client_;
  int32 gpu_route_id_;

  static base::subtle::AtomicWord /* TraceEventCallback */ event_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
