// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_AURA_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/aura/image_transport_factory.h"

namespace cc {
class ContextProvider;
}

namespace content {

// An ImageTransportFactory that disables transport.
class NoTransportImageTransportFactory : public ImageTransportFactory {
 public:
  explicit NoTransportImageTransportFactory(
      scoped_ptr<ui::ContextFactory> context_factory);
  virtual ~NoTransportImageTransportFactory();

  // ImageTransportFactory implementation.
  virtual ui::ContextFactory* AsContextFactory() OVERRIDE;
  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE;
  virtual void DestroySharedSurfaceHandle(gfx::GLSurfaceHandle surface)
      OVERRIDE;
  virtual scoped_refptr<ui::Texture> CreateTransportClient(
      float device_scale_factor) OVERRIDE;
  virtual scoped_refptr<ui::Texture> CreateOwnedTexture(
      const gfx::Size& size,
      float device_scale_factor,
      unsigned int texture_id) OVERRIDE;
  virtual GLHelper* GetGLHelper() OVERRIDE;
  virtual uint32 InsertSyncPoint() OVERRIDE;
  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE;
  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ImageTransportFactoryObserver* observer) OVERRIDE;

 private:
  scoped_ptr<ui::ContextFactory> context_factory_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  scoped_ptr<GLHelper> gl_helper_;

  DISALLOW_COPY_AND_ASSIGN(NoTransportImageTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
