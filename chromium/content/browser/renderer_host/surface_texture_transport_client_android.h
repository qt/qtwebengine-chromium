// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SURFACE_TEXTURE_TRANSPORT_CLIENT_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_SURFACE_TEXTURE_TRANSPORT_CLIENT_ANDROID_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

struct ANativeWindow;

namespace cc {
class Layer;
class VideoLayer;
}

namespace gfx {
class SurfaceTexture;
}

namespace content {

class SurfaceTextureTransportClient : public cc::VideoFrameProvider {
 public:
  SurfaceTextureTransportClient();
  virtual ~SurfaceTextureTransportClient();

  scoped_refptr<cc::Layer> Initialize();
  gfx::GLSurfaceHandle GetCompositingSurface(int surface_id);
  void SetSize(const gfx::Size& size);

  // cc::VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(Client*) OVERRIDE {}
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

 private:
  void OnSurfaceTextureFrameAvailable();

  scoped_refptr<cc::VideoLayer> video_layer_;
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;
  ANativeWindow* window_;
  scoped_refptr<media::VideoFrame> video_frame_;
  uint32 texture_id_;
  gpu::Mailbox texture_mailbox_;
  uint32 texture_mailbox_sync_point_;
  int surface_id_;
  base::WeakPtrFactory<SurfaceTextureTransportClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureTransportClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SURFACE_TEXTURE_TRANSPORT_CLIENT_ANDROID_H_
