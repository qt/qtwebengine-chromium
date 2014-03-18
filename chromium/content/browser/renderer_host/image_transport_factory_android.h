// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class GLShareGroup;
}

namespace blink {
class WebGraphicsContext3D;
}

namespace content {
class GLHelper;
class GLContextLostListener;

class ImageTransportFactoryAndroidObserver {
 public:
  virtual ~ImageTransportFactoryAndroidObserver() {}
  virtual void OnLostResources() = 0;
};

class ImageTransportFactoryAndroid {
 public:
  virtual ~ImageTransportFactoryAndroid();

  static ImageTransportFactoryAndroid* GetInstance();

  virtual uint32_t InsertSyncPoint() = 0;
  virtual void WaitSyncPoint(uint32_t sync_point) = 0;
  virtual uint32_t CreateTexture() = 0;
  virtual void DeleteTexture(uint32_t id) = 0;
  virtual void AcquireTexture(
      uint32 texture_id, const signed char* mailbox_name) = 0;

  virtual blink::WebGraphicsContext3D* GetContext3D() = 0;
  virtual GLHelper* GetGLHelper() = 0;
  virtual uint32 GetChannelID() = 0;

  static void AddObserver(ImageTransportFactoryAndroidObserver* observer);
  static void RemoveObserver(ImageTransportFactoryAndroidObserver* observer);

protected:
  ImageTransportFactoryAndroid();

  scoped_ptr<GLContextLostListener> context_lost_listener_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_IMAGE_TRANSPORT_FACTORY_ANDROID_H_
