// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_DELEGATING_RENDERER_H_
#define CC_OUTPUT_DELEGATING_RENDERER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/renderer.h"

namespace cc {

class OutputSurface;
class ResourceProvider;

class CC_EXPORT DelegatingRenderer : public Renderer {
 public:
  static scoped_ptr<DelegatingRenderer> Create(
      RendererClient* client,
      const LayerTreeSettings* settings,
      OutputSurface* output_surface,
      ResourceProvider* resource_provider);
  virtual ~DelegatingRenderer();

  virtual const RendererCapabilities& Capabilities() const OVERRIDE;

  virtual bool CanReadPixels() const OVERRIDE;

  virtual void DrawFrame(RenderPassList* render_passes_in_draw_order,
                         ContextProvider* offscreen_context_provider,
                         float device_scale_factor,
                         gfx::Rect device_viewport_rect,
                         gfx::Rect device_clip_rect,
                         bool allow_partial_swap,
                         bool disable_picture_quad_image_filtering) OVERRIDE;

  virtual void Finish() OVERRIDE {}

  virtual void SwapBuffers(const CompositorFrameMetadata& metadata) OVERRIDE;
  virtual void ReceiveSwapBuffersAck(const CompositorFrameAck&) OVERRIDE;

  virtual void GetFramebufferPixels(void* pixels, gfx::Rect rect) OVERRIDE;

  virtual bool IsContextLost() OVERRIDE;

  virtual void SetVisible(bool visible) OVERRIDE;

  virtual void SendManagedMemoryStats(size_t bytes_visible,
                                      size_t bytes_visible_and_nearby,
                                      size_t bytes_allocated) OVERRIDE;

 private:
  DelegatingRenderer(RendererClient* client,
                     const LayerTreeSettings* settings,
                     OutputSurface* output_surface,
                     ResourceProvider* resource_provider);
  bool Initialize();

  OutputSurface* output_surface_;
  ResourceProvider* resource_provider_;
  RendererCapabilities capabilities_;
  scoped_ptr<DelegatedFrameData> delegated_frame_data_;
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_DELEGATING_RENDERER_H_
