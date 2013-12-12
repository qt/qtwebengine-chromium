// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_CLIENT_H_
#define CC_LAYERS_TEXTURE_LAYER_CLIENT_H_

#include "cc/resources/single_release_callback.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {
class ResourceUpdateQueue;
class TextureMailbox;

class TextureLayerClient {
 public:
  // Called to prepare this layer's texture for compositing.
  // Returns the texture ID to be used for compositing.
  virtual unsigned PrepareTexture() = 0;

  // Returns the context that is providing the texture. Used for rate limiting
  // and detecting lost context.
  virtual WebKit::WebGraphicsContext3D* Context3d() = 0;

  // Returns true and provides a mailbox if a new frame is available.
  // Returns false if no new data is available
  // and the old mailbox is to be reused.
  virtual bool PrepareTextureMailbox(
      TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) = 0;

 protected:
  virtual ~TextureLayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_CLIENT_H_
