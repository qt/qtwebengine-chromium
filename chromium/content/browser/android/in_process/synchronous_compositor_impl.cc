// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/context_provider_in_process.h"

namespace content {

namespace {

int GetInProcessRendererId() {
  content::RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  if (it.IsAtEnd()) {
    // There should always be one RPH in single process mode.
    NOTREACHED();
    return 0;
  }

  int id = it.GetCurrentValue()->GetID();
  it.Advance();
  DCHECK(it.IsAtEnd());  // Not multiprocess compatible.
  return id;
}

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl() {
    SynchronousCompositorFactory::SetInstance(this);
  }

  // SynchronousCompositorFactory
  virtual scoped_refptr<base::MessageLoopProxy>
      GetCompositorMessageLoop() OVERRIDE {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id) OVERRIDE {
    scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
        new SynchronousCompositorOutputSurface(routing_id));
    return output_surface.PassAs<cc::OutputSurface>();
  }

  virtual InputHandlerManagerClient* GetInputHandlerManagerClient() OVERRIDE {
    return synchronous_input_event_filter();
  }

  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

  virtual scoped_refptr<cc::ContextProvider>
  GetOffscreenContextProviderForMainThread() OVERRIDE {
    if (!offscreen_context_for_main_thread_.get() ||
        offscreen_context_for_main_thread_->DestroyedOnMainThread()) {
      offscreen_context_for_main_thread_ =
          webkit::gpu::ContextProviderInProcess::Create();
      if (offscreen_context_for_main_thread_.get() &&
          !offscreen_context_for_main_thread_->BindToCurrentThread())
        offscreen_context_for_main_thread_ = NULL;
    }
    return offscreen_context_for_main_thread_;
  }

  // This is called on both renderer main thread (offscreen context creation
  // path shared between cross-process and in-process platforms) and renderer
  // compositor impl thread (InitializeHwDraw) in order to support Android
  // WebView synchronously enable and disable hardware mode multiple times in
  // the same task. This is ok because in-process WGC3D creation may happen on
  // any thread and is lightweight.
  virtual scoped_refptr<cc::ContextProvider>
      GetOffscreenContextProviderForCompositorThread() OVERRIDE {
    base::AutoLock lock(offscreen_context_for_compositor_thread_creation_lock_);
    if (!offscreen_context_for_compositor_thread_.get() ||
        offscreen_context_for_compositor_thread_->DestroyedOnMainThread()) {
      offscreen_context_for_compositor_thread_ =
          webkit::gpu::ContextProviderInProcess::Create();
    }
    return offscreen_context_for_compositor_thread_;
  }

 private:
  SynchronousInputEventFilter synchronous_input_event_filter_;

  // Only guards construction of |offscreen_context_for_compositor_thread_|,
  // not usage.
  base::Lock offscreen_context_for_compositor_thread_creation_lock_;
  scoped_refptr<cc::ContextProvider> offscreen_context_for_main_thread_;
  scoped_refptr<cc::ContextProvider> offscreen_context_for_compositor_thread_;
};

base::LazyInstance<SynchronousCompositorFactoryImpl>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SynchronousCompositorImpl);

// static
SynchronousCompositorImpl* SynchronousCompositorImpl::FromID(int process_id,
                                                             int routing_id) {
  if (g_factory == NULL)
    return NULL;
  RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);
  if (!rvh)
    return NULL;
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  if (!contents)
    return NULL;
  return FromWebContents(contents);
}

SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  return FromID(GetInProcessRendererId(), routing_id);
}

SynchronousCompositorImpl::SynchronousCompositorImpl(WebContents* contents)
    : compositor_client_(NULL),
      output_surface_(NULL),
      contents_(contents),
      input_handler_(NULL) {
  DCHECK(contents);
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
  SetInputHandler(NULL);
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

bool SynchronousCompositorImpl::InitializeHwDraw(
    scoped_refptr<gfx::GLSurface> surface) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  return output_surface_->InitializeHwDraw(
      surface,
      g_factory.Get().GetOffscreenContextProviderForCompositorThread());
}

void SynchronousCompositorImpl::ReleaseHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  return output_surface_->ReleaseHwDraw();
}

bool SynchronousCompositorImpl::DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect damage_area,
      bool stencil_enabled) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawHw(
      view_size, transform, damage_area, stencil_enabled);
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawSw(canvas);
}

void SynchronousCompositorImpl::DidChangeRootLayerScrollOffset() {
  if (input_handler_)
    input_handler_->OnRootLayerDelegatedScrollOffsetChanged();
}

void SynchronousCompositorImpl::DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  output_surface_ = output_surface;
  if (compositor_client_)
    compositor_client_->DidInitializeCompositor(this);
}

void SynchronousCompositorImpl::DidDestroySynchronousOutputSurface(
       SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());

  // Allow for transient hand-over when two output surfaces may refer to
  // a single delegate.
  if (output_surface_ == output_surface) {
    output_surface_ = NULL;
    if (compositor_client_)
      compositor_client_->DidDestroyCompositor(this);
    compositor_client_ = NULL;
  }
}

void SynchronousCompositorImpl::SetInputHandler(
    cc::InputHandler* input_handler) {
  DCHECK(CalledOnValidThread());

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(NULL);

  input_handler_ = input_handler;

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(this);
}

void SynchronousCompositorImpl::DidOverscroll(
    const cc::DidOverscrollParams& params) {
  if (compositor_client_) {
    compositor_client_->DidOverscroll(params.accumulated_overscroll,
                                      params.latest_overscroll_delta,
                                      params.current_fling_velocity);
  }
}

void SynchronousCompositorImpl::SetContinuousInvalidate(bool enable) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetContinuousInvalidate(enable);
}

InputEventAckState SynchronousCompositorImpl::HandleInputEvent(
    const WebKit::WebInputEvent& input_event) {
  DCHECK(CalledOnValidThread());
  return g_factory.Get().synchronous_input_event_filter()->HandleInputEvent(
      contents_->GetRoutingID(), input_event);
}

void SynchronousCompositorImpl::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SynchronousFrameMetadata(frame_metadata);
}

void SynchronousCompositorImpl::DidActivatePendingTree() {
  if (compositor_client_)
    compositor_client_->DidUpdateContent();
}

void SynchronousCompositorImpl::SetTotalScrollOffset(gfx::Vector2dF new_value) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetTotalRootLayerScrollOffset(new_value);
}

gfx::Vector2dF SynchronousCompositorImpl::GetTotalScrollOffset() {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    return compositor_client_->GetTotalRootLayerScrollOffset();
  return gfx::Vector2dF();
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorImpl() must only be used on the UI thread.
bool SynchronousCompositorImpl::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  if (client) {
    g_factory.Get();  // Ensure it's initialized.
    SynchronousCompositorImpl::CreateForWebContents(contents);
  }
  if (SynchronousCompositorImpl* instance =
      SynchronousCompositorImpl::FromWebContents(contents)) {
    instance->SetClient(client);
  }
}

}  // namespace content
