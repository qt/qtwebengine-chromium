// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_output_surface.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/ipc_sync_channel.h"

namespace {
// There are several compositor surfaces in a process, but they share the same
// compositor thread, so we use a simple int here to track prefer-smoothness.
int g_prefer_smoothness_count = 0;
} // namespace

namespace content {

//------------------------------------------------------------------------------

// static
IPC::ForwardingMessageFilter* CompositorOutputSurface::CreateFilter(
    base::TaskRunner* target_task_runner)
{
  uint32 messages_to_filter[] = {
    ViewMsg_UpdateVSyncParameters::ID,
    ViewMsg_SwapCompositorFrameAck::ID,
    ViewMsg_ReclaimCompositorResources::ID,
#if defined(OS_ANDROID)
    ViewMsg_BeginFrame::ID
#endif
  };

  return new IPC::ForwardingMessageFilter(
      messages_to_filter, arraysize(messages_to_filter),
      target_task_runner);
}

CompositorOutputSurface::CompositorOutputSurface(
    int32 routing_id,
    uint32 output_surface_id,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    bool use_swap_compositor_frame_message)
    : OutputSurface(context_provider, software_device.Pass()),
      output_surface_id_(output_surface_id),
      use_swap_compositor_frame_message_(use_swap_compositor_frame_message),
      output_surface_filter_(
          RenderThreadImpl::current()->compositor_output_surface_filter()),
      routing_id_(routing_id),
      prefers_smoothness_(false),
#if defined(OS_WIN)
      // TODO(epenner): Implement PlatformThread::CurrentHandle() on windows.
      main_thread_handle_(base::PlatformThreadHandle())
#else
      main_thread_handle_(base::PlatformThread::CurrentHandle())
#endif
{
  DCHECK(output_surface_filter_.get());
  DetachFromThread();
  message_sender_ = RenderThreadImpl::current()->sync_message_filter();
  DCHECK(message_sender_.get());
  if (OutputSurface::software_device())
    capabilities_.max_frames_pending = 1;
}

CompositorOutputSurface::~CompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  SetNeedsBeginImplFrame(false);
  if (!HasClient())
    return;
  UpdateSmoothnessTakesPriority(false);
  if (output_surface_proxy_.get())
    output_surface_proxy_->ClearOutputSurface();
  output_surface_filter_->RemoveRoute(routing_id_);
}

bool CompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(CalledOnValidThread());

  if (!cc::OutputSurface::BindToClient(client))
    return false;

  output_surface_proxy_ = new CompositorOutputSurfaceProxy(this);
  output_surface_filter_->AddRoute(
      routing_id_,
      base::Bind(&CompositorOutputSurfaceProxy::OnMessageReceived,
                 output_surface_proxy_));

  if (!context_provider()) {
    // Without a GPU context, the memory policy otherwise wouldn't be set.
    client->SetMemoryPolicy(cc::ManagedMemoryPolicy(
        64 * 1024 * 1024,
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
        cc::ManagedMemoryPolicy::kDefaultNumResourcesLimit));
  }

  return true;
}

void CompositorOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  if (use_swap_compositor_frame_message_) {
    Send(new ViewHostMsg_SwapCompositorFrame(routing_id_,
                                             output_surface_id_,
                                             *frame));
    DidSwapBuffers();
    return;
  }

  if (frame->gl_frame_data) {
    WebGraphicsContext3DCommandBufferImpl* command_buffer_context =
        static_cast<WebGraphicsContext3DCommandBufferImpl*>(
            context_provider_->Context3d());
    CommandBufferProxyImpl* command_buffer_proxy =
        command_buffer_context->GetCommandBufferProxy();
    DCHECK(command_buffer_proxy);
    context_provider_->Context3d()->shallowFlushCHROMIUM();
    command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);
  }

  OutputSurface::SwapBuffers(frame);
}

void CompositorOutputSurface::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  if (!HasClient())
    return;
  IPC_BEGIN_MESSAGE_MAP(CompositorOutputSurface, message)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateVSyncParameters, OnUpdateVSyncParameters);
    IPC_MESSAGE_HANDLER(ViewMsg_SwapCompositorFrameAck, OnSwapAck);
    IPC_MESSAGE_HANDLER(ViewMsg_ReclaimCompositorResources, OnReclaimResources);
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_BeginFrame, OnBeginImplFrame);
#endif
  IPC_END_MESSAGE_MAP()
}

void CompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase, base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  OnVSyncParametersChanged(timebase, interval);
}

#if defined(OS_ANDROID)
void CompositorOutputSurface::SetNeedsBeginImplFrame(bool enable) {
  DCHECK(CalledOnValidThread());
  if (needs_begin_impl_frame_ != enable)
    Send(new ViewHostMsg_SetNeedsBeginFrame(routing_id_, enable));
  OutputSurface::SetNeedsBeginImplFrame(enable);
}

void CompositorOutputSurface::OnBeginImplFrame(const cc::BeginFrameArgs& args) {
  DCHECK(CalledOnValidThread());
  BeginImplFrame(args);
}
#endif  // defined(OS_ANDROID)

void CompositorOutputSurface::OnSwapAck(uint32 output_surface_id,
                                        const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
  OnSwapBuffersComplete();
}

void CompositorOutputSurface::OnReclaimResources(
    uint32 output_surface_id,
    const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
}

bool CompositorOutputSurface::Send(IPC::Message* message) {
  return message_sender_->Send(message);
}

namespace {
#if defined(OS_ANDROID)
  void SetThreadPriorityToIdle(base::PlatformThreadHandle handle) {
    base::PlatformThread::SetThreadPriority(
       handle, base::kThreadPriority_Background);
  }
  void SetThreadPriorityToDefault(base::PlatformThreadHandle handle) {
    base::PlatformThread::SetThreadPriority(
       handle, base::kThreadPriority_Normal);
  }
#else
  void SetThreadPriorityToIdle(base::PlatformThreadHandle handle) {}
  void SetThreadPriorityToDefault(base::PlatformThreadHandle handle) {}
#endif
}

void CompositorOutputSurface::UpdateSmoothnessTakesPriority(
    bool prefers_smoothness) {
#ifndef NDEBUG
  // If we use different compositor threads, we need to
  // use an atomic int to track prefer smoothness count.
  base::PlatformThreadId g_last_thread = base::PlatformThread::CurrentId();
  DCHECK_EQ(g_last_thread, base::PlatformThread::CurrentId());
#endif
  if (prefers_smoothness_ == prefers_smoothness)
    return;
  // If this is the first surface to start preferring smoothness,
  // Throttle the main thread's priority.
  if (prefers_smoothness_ == false &&
      ++g_prefer_smoothness_count == 1) {
    SetThreadPriorityToIdle(main_thread_handle_);
  }
  // If this is the last surface to stop preferring smoothness,
  // Reset the main thread's priority to the default.
  if (prefers_smoothness_ == true &&
      --g_prefer_smoothness_count == 0) {
    SetThreadPriorityToDefault(main_thread_handle_);
  }
  prefers_smoothness_ = prefers_smoothness;
}

}  // namespace content
