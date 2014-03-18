// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/common/gpu/texture_image_transport_surface.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace content {

ImageTransportSurface::ImageTransportSurface() {}

ImageTransportSurface::~ImageTransportSurface() {}

scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  scoped_refptr<gfx::GLSurface> surface;
  if (handle.transport_type == gfx::TEXTURE_TRANSPORT)
    surface = new TextureImageTransportSurface(manager, stub, handle);
  else
    surface = CreateNativeSurface(manager, stub, handle);

  if (!surface.get() || !surface->Initialize())
    return NULL;
  return surface;
}

ImageTransportHelper::ImageTransportHelper(ImageTransportSurface* surface,
                                           GpuChannelManager* manager,
                                           GpuCommandBufferStub* stub,
                                           gfx::PluginWindowHandle handle)
    : surface_(surface),
      manager_(manager),
      stub_(stub->AsWeakPtr()),
      handle_(handle) {
  route_id_ = manager_->GenerateRouteID();
  manager_->AddRoute(route_id_, this);
}

ImageTransportHelper::~ImageTransportHelper() {
  if (stub_.get()) {
    stub_->SetLatencyInfoCallback(
        base::Callback<void(const ui::LatencyInfo&)>());
  }
  manager_->RemoveRoute(route_id_);
}

bool ImageTransportHelper::Initialize() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();

  if (!decoder)
    return false;

  decoder->SetResizeCallback(
       base::Bind(&ImageTransportHelper::Resize, base::Unretained(this)));

  stub_->SetLatencyInfoCallback(
      base::Bind(&ImageTransportHelper::SetLatencyInfo,
                 base::Unretained(this)));

  manager_->Send(new GpuHostMsg_AcceleratedSurfaceInitialized(
      stub_->surface_id(), route_id_));

  return true;
}

void ImageTransportHelper::Destroy() {}

bool ImageTransportHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageTransportHelper, message)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_BufferPresented,
                        OnBufferPresented)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_ResizeViewACK, OnResizeViewACK);
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_WakeUpGpu, OnWakeUpGpu);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageTransportHelper::SendAcceleratedSurfaceBuffersSwapped(
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params) {
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers",
                       TRACE_EVENT_SCOPE_THREAD,
                       "GLImpl", static_cast<int>(gfx::GetGLImplementation()),
                       "width", params.size.width());
  params.surface_id = stub_->surface_id();
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
}

void ImageTransportHelper::SendAcceleratedSurfacePostSubBuffer(
    GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params) {
  params.surface_id = stub_->surface_id();
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfacePostSubBuffer(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceRelease() {
  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  params.surface_id = stub_->surface_id();
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceRelease(params));
}

void ImageTransportHelper::SendResizeView(const gfx::Size& size) {
  manager_->Send(new GpuHostMsg_ResizeView(stub_->surface_id(),
                                           route_id_,
                                           size));
}

void ImageTransportHelper::SendUpdateVSyncParameters(
      base::TimeTicks timebase, base::TimeDelta interval) {
  manager_->Send(new GpuHostMsg_UpdateVSyncParameters(stub_->surface_id(),
                                                      timebase,
                                                      interval));
}

void ImageTransportHelper::SendLatencyInfo(
    const ui::LatencyInfo& latency_info) {
  manager_->Send(new GpuHostMsg_FrameDrawn(latency_info));
}

void ImageTransportHelper::SetScheduled(bool is_scheduled) {
  gpu::GpuScheduler* scheduler = Scheduler();
  if (!scheduler)
    return;

  scheduler->SetScheduled(is_scheduled);
}

void ImageTransportHelper::DeferToFence(base::Closure task) {
  gpu::GpuScheduler* scheduler = Scheduler();
  DCHECK(scheduler);

  scheduler->DeferToFence(task);
}

void ImageTransportHelper::SetPreemptByFlag(
    scoped_refptr<gpu::PreemptionFlag> preemption_flag) {
  stub_->channel()->SetPreemptByFlag(preemption_flag);
}

bool ImageTransportHelper::MakeCurrent() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();
  if (!decoder)
    return false;
  return decoder->MakeCurrent();
}

void ImageTransportHelper::SetSwapInterval(gfx::GLContext* context) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    context->SetSwapInterval(0);
  else
    context->SetSwapInterval(1);
}

void ImageTransportHelper::Suspend() {
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceSuspend(stub_->surface_id()));
}

gpu::GpuScheduler* ImageTransportHelper::Scheduler() {
  if (!stub_.get())
    return NULL;
  return stub_->scheduler();
}

gpu::gles2::GLES2Decoder* ImageTransportHelper::Decoder() {
  if (!stub_.get())
    return NULL;
  return stub_->decoder();
}

void ImageTransportHelper::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  surface_->OnBufferPresented(params);
}

void ImageTransportHelper::OnResizeViewACK() {
  surface_->OnResizeViewACK();
}

void ImageTransportHelper::OnWakeUpGpu() {
  surface_->WakeUpGpu();
}

void ImageTransportHelper::Resize(gfx::Size size, float scale_factor) {
  surface_->OnResize(size, scale_factor);

#if defined(OS_ANDROID)
  manager_->gpu_memory_manager()->ScheduleManage(
      GpuMemoryManager::kScheduleManageNow);
#endif
}

void ImageTransportHelper::SetLatencyInfo(
    const ui::LatencyInfo& latency_info) {
  surface_->SetLatencyInfo(latency_info);
}

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::GLSurface* surface,
    bool transport)
    : GLSurfaceAdapter(surface),
      transport_(transport),
      did_set_swap_interval_(false),
      did_unschedule_(false),
      is_swap_buffers_pending_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

bool PassThroughImageTransportSurface::Initialize() {
  // The surface is assumed to have already been initialized.
  return helper_->Initialize();
}

void PassThroughImageTransportSurface::Destroy() {
  helper_->Destroy();
  GLSurfaceAdapter::Destroy();
}

bool PassThroughImageTransportSurface::DeferDraws() {
  if (is_swap_buffers_pending_) {
    DCHECK(!did_unschedule_);
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
}

void PassThroughImageTransportSurface::SetLatencyInfo(
    const ui::LatencyInfo& latency_info) {
  latency_info_ = latency_info;
}

bool PassThroughImageTransportSurface::SwapBuffers() {
  // GetVsyncValues before SwapBuffers to work around Mali driver bug:
  // crbug.com/223558.
  SendVSyncUpdateIfAvailable();
  bool result = gfx::GLSurfaceAdapter::SwapBuffers();
  latency_info_.AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0);

  if (transport_) {
    DCHECK(!is_swap_buffers_pending_);
    is_swap_buffers_pending_ = true;

    // Round trip to the browser UI thread, for throttling, by sending a dummy
    // SwapBuffers message.
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.surface_handle = 0;
    params.latency_info = latency_info_;
    params.size = surface()->GetSize();
    helper_->SendAcceleratedSurfaceBuffersSwapped(params);
  } else {
    helper_->SendLatencyInfo(latency_info_);
  }
  return result;
}

bool PassThroughImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  SendVSyncUpdateIfAvailable();
  bool result = gfx::GLSurfaceAdapter::PostSubBuffer(x, y, width, height);
  latency_info_.AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0);

  if (transport_) {
    DCHECK(!is_swap_buffers_pending_);
    is_swap_buffers_pending_ = true;

    // Round trip to the browser UI thread, for throttling, by sending a dummy
    // PostSubBuffer message.
    GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
    params.surface_handle = 0;
    params.latency_info = latency_info_;
    params.surface_size = surface()->GetSize();
    params.x = x;
    params.y = y;
    params.width = width;
    params.height = height;
    helper_->SendAcceleratedSurfacePostSubBuffer(params);

    helper_->SetScheduled(false);
  } else {
    helper_->SendLatencyInfo(latency_info_);
  }
  return result;
}

bool PassThroughImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (!did_set_swap_interval_) {
    ImageTransportHelper::SetSwapInterval(context);
    did_set_swap_interval_ = true;
  }
  return true;
}

void PassThroughImageTransportSurface::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& /* params */) {
  DCHECK(transport_);
  DCHECK(is_swap_buffers_pending_);
  is_swap_buffers_pending_ = false;
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void PassThroughImageTransportSurface::OnResizeViewACK() {
  DCHECK(transport_);
  Resize(new_size_);

  TRACE_EVENT_ASYNC_END0("gpu", "OnResize", this);
  helper_->SetScheduled(true);
}

void PassThroughImageTransportSurface::OnResize(gfx::Size size,
                                                float scale_factor) {
  new_size_ = size;

  if (transport_) {
    helper_->SendResizeView(size);
    helper_->SetScheduled(false);
    TRACE_EVENT_ASYNC_BEGIN2("gpu", "OnResize", this,
                             "width", size.width(), "height", size.height());
  } else {
    Resize(new_size_);
  }
}

gfx::Size PassThroughImageTransportSurface::GetSize() {
  return GLSurfaceAdapter::GetSize();
}

void PassThroughImageTransportSurface::WakeUpGpu() {
  NOTIMPLEMENTED();
}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {}

void PassThroughImageTransportSurface::SendVSyncUpdateIfAvailable() {
  gfx::VSyncProvider* vsync_provider = GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(
      base::Bind(&ImageTransportHelper::SendUpdateVSyncParameters,
                 helper_->AsWeakPtr()));
  }
}

}  // namespace content
