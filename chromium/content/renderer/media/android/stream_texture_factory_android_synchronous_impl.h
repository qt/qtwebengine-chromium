// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_SYNCHRONOUS_IMPL_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_SYNCHRONOUS_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/renderer/media/android/stream_texture_factory_android.h"

namespace gfx {
class SurfaceTexture;
}

namespace WebKit {
class WebGraphicsContext3D;
}

namespace content {

// Factory for when using synchronous compositor in Android WebView.
class StreamTextureFactorySynchronousImpl : public StreamTextureFactory {
 public:
  class ContextProvider : public base::RefCountedThreadSafe<ContextProvider> {
   public:
    virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
        uint32 stream_id) = 0;

    virtual WebKit::WebGraphicsContext3D* Context3d() = 0;

   protected:
    friend class base::RefCountedThreadSafe<ContextProvider>;
    virtual ~ContextProvider() {}
  };

  StreamTextureFactorySynchronousImpl(ContextProvider* context_provider,
                                      int view_id);
  virtual ~StreamTextureFactorySynchronousImpl();

  virtual StreamTextureProxy* CreateProxy() OVERRIDE;
  virtual void EstablishPeer(int32 stream_id, int player_id) OVERRIDE;
  virtual unsigned CreateStreamTexture(
      unsigned texture_target,
      unsigned* texture_id,
      gpu::Mailbox* texture_mailbox,
      unsigned* texture_mailbox_sync_point) OVERRIDE;
  virtual void DestroyStreamTexture(unsigned texture_id) OVERRIDE;
  virtual void SetStreamTextureSize(int32 stream_id,
                                    const gfx::Size& size) OVERRIDE;

 private:
  scoped_refptr<ContextProvider> context_provider_;
  int view_id_;

  DISALLOW_COPY_AND_ASSIGN(StreamTextureFactorySynchronousImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_SYNCHRONOUS_IMPL_H_
