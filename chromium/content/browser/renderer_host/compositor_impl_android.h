// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host_client.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/browser/android/compositor.h"

struct ANativeWindow;

namespace cc {
class InputHandlerClient;
class Layer;
class LayerTreeHost;
class ScopedUIResource;
}

namespace content {
class CompositorClient;
class GraphicsContext;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public WebGraphicsContext3DSwapBuffersClient,
      public ImageTransportFactoryAndroidObserver {
 public:
  explicit CompositorImpl(CompositorClient* client);
  virtual ~CompositorImpl();

  static bool IsInitialized();
  static bool IsThreadingEnabled();

  // Returns true if initialized with DIRECT_CONTEXT_ON_DRAW_THREAD.
  static bool UsesDirectGL();

  // Returns the Java Surface object for a given view surface id.
  static jobject GetSurface(int surface_id);

  // Compositor implementation.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) OVERRIDE;
  virtual void SetWindowSurface(ANativeWindow* window) OVERRIDE;
  virtual void SetSurface(jobject surface) OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void setDeviceScaleFactor(float factor) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Size& size) OVERRIDE;
  virtual void SetHasTransparentBackground(bool flag) OVERRIDE;
  virtual bool CompositeAndReadback(
      void *pixels, const gfx::Rect& rect) OVERRIDE;
  virtual void SetNeedsRedraw() OVERRIDE;
  virtual void Composite() OVERRIDE;
  virtual cc::UIResourceId GenerateUIResource(
      const cc::UIResourceBitmap& bitmap) OVERRIDE;
  virtual void DeleteUIResource(cc::UIResourceId resource_id) OVERRIDE;
  virtual WebKit::WebGLId GenerateTexture(gfx::JavaBitmap& bitmap) OVERRIDE;
  virtual WebKit::WebGLId GenerateCompressedTexture(
      gfx::Size& size, int data_size, void* data) OVERRIDE;
  virtual void DeleteTexture(WebKit::WebGLId texture_id) OVERRIDE;
  virtual bool CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   gfx::JavaBitmap& bitmap) OVERRIDE;
  virtual bool CopyTextureToBitmap(WebKit::WebGLId texture_id,
                                   const gfx::Rect& sub_rect,
                                   gfx::JavaBitmap& bitmap) OVERRIDE;

  // LayerTreeHostClient implementation.
  virtual void WillBeginFrame() OVERRIDE {}
  virtual void DidBeginFrame() OVERRIDE {}
  virtual void Animate(double frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE {}
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) OVERRIDE {}
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface(bool success) OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE {}
  virtual void DidCommitAndDrawFrame() OVERRIDE {}
  virtual void DidCompleteSwapBuffers() OVERRIDE;
  virtual void ScheduleComposite() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

  // WebGraphicsContext3DSwapBuffersClient implementation.
  virtual void OnViewContextSwapBuffersPosted() OVERRIDE;
  virtual void OnViewContextSwapBuffersComplete() OVERRIDE;
  virtual void OnViewContextSwapBuffersAborted() OVERRIDE;

  // ImageTransportFactoryAndroidObserver implementation.
  virtual void OnLostResources() OVERRIDE;

 private:
  WebKit::WebGLId BuildBasicTexture();
  WebKit::WGC3Denum GetGLFormatForBitmap(gfx::JavaBitmap& bitmap);
  WebKit::WGC3Denum GetGLTypeForBitmap(gfx::JavaBitmap& bitmap);

  scoped_refptr<cc::Layer> root_layer_;
  scoped_ptr<cc::LayerTreeHost> host_;

  gfx::Size size_;
  bool has_transparent_background_;

  ANativeWindow* window_;
  int surface_id_;

  CompositorClient* client_;
  base::WeakPtrFactory<CompositorImpl> weak_factory_;

  scoped_refptr<cc::ContextProvider> null_offscreen_context_provider_;

  typedef base::ScopedPtrHashMap<cc::UIResourceId, cc::ScopedUIResource>
        UIResourceMap;
  UIResourceMap ui_resource_map_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
