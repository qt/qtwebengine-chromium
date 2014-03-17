// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_agent.h"

#include <map>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/devtools_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/devtools/devtools_agent_filter.h"
#include "content/renderer/devtools/devtools_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

using blink::WebConsoleMessage;
using blink::WebDevToolsAgent;
using blink::WebDevToolsAgentClient;
using blink::WebFrame;
using blink::WebPoint;
using blink::WebString;
using blink::WebCString;
using blink::WebVector;
using blink::WebView;

using base::debug::TraceLog;

namespace content {

base::subtle::AtomicWord DevToolsAgent::event_callback_;

namespace {

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(base::MessageLoop::current()) {}
  virtual ~WebKitClientMessageLoopImpl() { message_loop_ = NULL; }
  virtual void run() {
    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop_);
    message_loop_->Run();
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  base::MessageLoop* message_loop_;
};

typedef std::map<int, DevToolsAgent*> IdToAgentMap;
base::LazyInstance<IdToAgentMap>::Leaky
    g_agent_for_routing_id = LAZY_INSTANCE_INITIALIZER;

} //  namespace

DevToolsAgent::DevToolsAgent(RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      is_attached_(false),
      is_devtools_client_(false),
      gpu_route_id_(MSG_ROUTING_NONE) {
  g_agent_for_routing_id.Get()[routing_id()] = this;

  render_view->webview()->setDevToolsAgentClient(this);
  render_view->webview()->devToolsAgent()->setProcessId(
      base::Process::Current().pid());
}

DevToolsAgent::~DevToolsAgent() {
  g_agent_for_routing_id.Get().erase(routing_id());
  setTraceEventCallback(NULL);
}

// Called on the Renderer thread.
bool DevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_InspectElement, OnInspectElement)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_AddMessageToConsole,
                        OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_GpuTasksChunk, OnGpuTasksChunk)
    IPC_MESSAGE_HANDLER(DevToolsMsg_SetupDevToolsClient, OnSetupDevToolsClient)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (message.type() == ViewMsg_Navigate::ID ||
      message.type() == ViewMsg_Close::ID)
    ContinueProgram();  // Don't want to swallow the message.

  return handled;
}

void DevToolsAgent::sendMessageToInspectorFrontend(
    const blink::WebString& message) {
  Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(routing_id(),
                                                         message.utf8()));
}

int DevToolsAgent::hostIdentifier() {
  return routing_id();
}

void DevToolsAgent::saveAgentRuntimeState(
    const blink::WebString& state) {
  Send(new DevToolsHostMsg_SaveAgentRuntimeState(routing_id(), state.utf8()));
}

blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
    DevToolsAgent::createClientMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void DevToolsAgent::clearBrowserCache() {
  Send(new DevToolsHostMsg_ClearBrowserCache(routing_id()));
}

void DevToolsAgent::clearBrowserCookies() {
  Send(new DevToolsHostMsg_ClearBrowserCookies(routing_id()));
}

void DevToolsAgent::setTraceEventCallback(TraceEventCallback cb) {
  TraceLog* trace_log = TraceLog::GetInstance();
  base::subtle::NoBarrier_Store(&event_callback_,
                                reinterpret_cast<base::subtle::AtomicWord>(cb));
  if (!!cb) {
    trace_log->SetEventCallbackEnabled(base::debug::CategoryFilter(
        base::debug::CategoryFilter::kDefaultCategoryFilterString),
        TraceEventCallbackWrapper);
  } else {
    trace_log->SetEventCallbackDisabled();
  }
}

// static
void DevToolsAgent::TraceEventCallbackWrapper(
    base::TimeTicks timestamp,
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char* const arg_names[],
    const unsigned char arg_types[],
    const unsigned long long arg_values[],
    unsigned char flags) {
  TraceEventCallback callback =
      reinterpret_cast<TraceEventCallback>(
          base::subtle::NoBarrier_Load(&event_callback_));
  if (callback) {
    double timestamp_seconds = (timestamp - base::TimeTicks()).InSecondsF();
    callback(phase, category_group_enabled, name, id, num_args,
             arg_names, arg_types, arg_values, flags, timestamp_seconds);
  }
}

void DevToolsAgent::startGPUEventsRecording() {
  GpuChannelHost* gpu_channel_host =
      RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel_host)
    return;
  DCHECK(gpu_route_id_ == MSG_ROUTING_NONE);
  gpu_channel_host->Send(
      new GpuChannelMsg_DevToolsStartEventsRecording(&gpu_route_id_));
  DCHECK(gpu_route_id_ != MSG_ROUTING_NONE);
  if (gpu_route_id_ != MSG_ROUTING_NONE) {
    gpu_channel_host->AddRoute(gpu_route_id_, AsWeakPtr());
  }
}

void DevToolsAgent::stopGPUEventsRecording() {
  GpuChannelHost* gpu_channel_host =
      RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel_host || gpu_route_id_ == MSG_ROUTING_NONE)
    return;
  gpu_channel_host->Send(new GpuChannelMsg_DevToolsStopEventsRecording());
  gpu_channel_host->RemoveRoute(gpu_route_id_);
  gpu_route_id_ = MSG_ROUTING_NONE;
}

void DevToolsAgent::OnGpuTasksChunk(const std::vector<GpuTaskInfo>& tasks) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (!web_agent)
    return;
  for (size_t i = 0; i < tasks.size(); i++) {
    const GpuTaskInfo& task = tasks[i];
    WebDevToolsAgent::GPUEvent event(task.timestamp, task.phase, task.foreign,
        static_cast<size_t>(task.used_gpu_memory_bytes));
    web_agent->processGPUEvent(event);
  }
}

void DevToolsAgent::enableDeviceEmulation(
    const blink::WebRect& device_rect,
    const blink::WebRect& view_rect,
    float device_scale_factor,
    bool fit_to_view) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(render_view());
  impl->webview()->settings()->setForceCompositingMode(true);
  impl->EnableScreenMetricsEmulation(gfx::Rect(device_rect),
      gfx::Rect(view_rect), device_scale_factor, fit_to_view);
}

void DevToolsAgent::disableDeviceEmulation() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(render_view());
  impl->DisableScreenMetricsEmulation();
}

#if defined(USE_TCMALLOC) && !defined(OS_WIN)
static void AllocationVisitor(void* data, const void* ptr) {
    typedef blink::WebDevToolsAgentClient::AllocatedObjectVisitor Visitor;
    Visitor* visitor = reinterpret_cast<Visitor*>(data);
    visitor->visitObject(ptr);
}
#endif

void DevToolsAgent::visitAllocatedObjects(AllocatedObjectVisitor* visitor) {
#if defined(USE_TCMALLOC) && !defined(OS_WIN)
  IterateAllocatedObjects(&AllocationVisitor, visitor);
#endif
}

// static
DevToolsAgent* DevToolsAgent::FromHostId(int host_id) {
  IdToAgentMap::iterator it = g_agent_for_routing_id.Get().find(host_id);
  if (it != g_agent_for_routing_id.Get().end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsAgent::OnAttach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
    is_attached_ = true;
  }
}

void DevToolsAgent::OnReattach(const std::string& agent_state) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->reattach(WebString::fromUTF8(agent_state));
    is_attached_ = true;
  }
}

void DevToolsAgent::OnDetach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->detach();
    is_attached_ = false;
  }
}

void DevToolsAgent::OnDispatchOnInspectorBackend(const std::string& message) {
  TRACE_EVENT0("devtools", "DevToolsAgent::OnDispatchOnInspectorBackend");
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->dispatchOnInspectorBackend(WebString::fromUTF8(message));
}

void DevToolsAgent::OnInspectElement(int x, int y) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
    web_agent->inspectElementAt(WebPoint(x, y));
  }
}

void DevToolsAgent::OnAddMessageToConsole(ConsoleMessageLevel level,
                                          const std::string& message) {
  WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return;

  WebFrame* main_frame = web_view->mainFrame();
  if (!main_frame)
    return;

  WebConsoleMessage::Level target_level = WebConsoleMessage::LevelLog;
  switch (level) {
    case CONSOLE_MESSAGE_LEVEL_DEBUG:
      target_level = WebConsoleMessage::LevelDebug;
      break;
    case CONSOLE_MESSAGE_LEVEL_LOG:
      target_level = WebConsoleMessage::LevelLog;
      break;
    case CONSOLE_MESSAGE_LEVEL_WARNING:
      target_level = WebConsoleMessage::LevelWarning;
      break;
    case CONSOLE_MESSAGE_LEVEL_ERROR:
      target_level = WebConsoleMessage::LevelError;
      break;
  }
  main_frame->addMessageToConsole(
      WebConsoleMessage(target_level, WebString::fromUTF8(message)));
}

void DevToolsAgent::ContinueProgram() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  // TODO(pfeldman): rename didNavigate to continueProgram upstream.
  // That is in fact the purpose of the signal.
  if (web_agent)
    web_agent->didNavigate();
}

void DevToolsAgent::OnSetupDevToolsClient() {
  // We only want to register once per render view.
  if (is_devtools_client_)
    return;
  is_devtools_client_ = true;
  new DevToolsClient(static_cast<RenderViewImpl*>(render_view()));
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return NULL;
  return web_view->devToolsAgent();
}

bool DevToolsAgent::IsAttached() {
  return is_attached_;
}

}  // namespace content
