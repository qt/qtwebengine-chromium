// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/web_contents_user_data.h"

namespace cc {
class InputHandler;
struct DidOverscrollParams;
}

namespace WebKit {
class WebInputEvent;
}

namespace content {
class InputHandlerManager;

// The purpose of this class is to act as the intermediary between the various
// components that make up the 'synchronous compositor mode' implementation and
// expose their functionality via the SynchronousCompositor interface.
// This class is created on the main thread but most of the APIs are called
// from the Compositor thread.
class SynchronousCompositorImpl
    : public cc::LayerScrollOffsetDelegate,
      public SynchronousCompositor,
      public SynchronousCompositorOutputSurfaceDelegate,
      public WebContentsUserData<SynchronousCompositorImpl> {
 public:
  // When used from browser code, use both |process_id| and |routing_id|.
  static SynchronousCompositorImpl* FromID(int process_id, int routing_id);
  // When handling upcalls from renderer code, use this version; the process id
  // is implicitly that of the in-process renderer.
  static SynchronousCompositorImpl* FromRoutingID(int routing_id);

  InputEventAckState HandleInputEvent(const WebKit::WebInputEvent& input_event);

  // SynchronousCompositor
  virtual void SetClient(SynchronousCompositorClient* compositor_client)
      OVERRIDE;
  virtual bool InitializeHwDraw(
      scoped_refptr<gfx::GLSurface> surface) OVERRIDE;
  virtual void ReleaseHwDraw() OVERRIDE;
  virtual bool DemandDrawHw(
      gfx::Size surface_size,
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      bool stencil_enabled) OVERRIDE;
  virtual bool DemandDrawSw(SkCanvas* canvas) OVERRIDE;
  virtual void SetMemoryPolicy(
      const SynchronousCompositorMemoryPolicy& policy) OVERRIDE;
  virtual void DidChangeRootLayerScrollOffset() OVERRIDE;

  // SynchronousCompositorOutputSurfaceDelegate
  virtual void DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) OVERRIDE;
  virtual void DidDestroySynchronousOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) OVERRIDE;
  virtual void SetContinuousInvalidate(bool enable) OVERRIDE;
  virtual void UpdateFrameMetaData(
      const cc::CompositorFrameMetadata& frame_info) OVERRIDE;
  virtual void DidActivatePendingTree() OVERRIDE;

  // LayerScrollOffsetDelegate
  virtual void SetTotalScrollOffset(gfx::Vector2dF new_value) OVERRIDE;
  virtual gfx::Vector2dF GetTotalScrollOffset() OVERRIDE;

  void SetInputHandler(cc::InputHandler* input_handler);
  void DidOverscroll(const cc::DidOverscrollParams& params);

 private:
  explicit SynchronousCompositorImpl(WebContents* contents);
  virtual ~SynchronousCompositorImpl();
  friend class WebContentsUserData<SynchronousCompositorImpl>;

  void DidCreateSynchronousOutputSurface(
      SynchronousCompositorOutputSurface* output_surface);
  bool CalledOnValidThread() const;

  SynchronousCompositorClient* compositor_client_;
  SynchronousCompositorOutputSurface* output_surface_;
  WebContents* contents_;
  cc::InputHandler* input_handler_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
