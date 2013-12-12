// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_CLIENT_H_
#define CC_OUTPUT_OUTPUT_SURFACE_CLIENT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/context_provider.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Transform;
}

namespace cc {

class CompositorFrameAck;
struct ManagedMemoryPolicy;

class CC_EXPORT OutputSurfaceClient {
 public:
  // Called to synchronously re-initialize using the Context3D. Upon returning
  // the compositor should be able to draw using GL what was previously
  // committed.
  virtual bool DeferredInitialize(
      scoped_refptr<ContextProvider> offscreen_context_provider) = 0;
  virtual void ReleaseGL() = 0;
  virtual void SetNeedsRedrawRect(gfx::Rect damage_rect) = 0;
  virtual void BeginFrame(const BeginFrameArgs& args) = 0;
  virtual void OnSwapBuffersComplete() = 0;
  virtual void ReclaimResources(const CompositorFrameAck* ack) = 0;
  virtual void DidLoseOutputSurface() = 0;
  virtual void SetExternalDrawConstraints(const gfx::Transform& transform,
                                          gfx::Rect viewport,
                                          gfx::Rect clip,
                                          bool valid_for_tile_management) = 0;
  virtual void SetDiscardBackBufferWhenNotVisible(bool discard) = 0;
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;
  // If set, |callback| will be called subsequent to each new tree activation,
  // regardless of the compositor visibility or damage. |callback| must remain
  // valid for the lifetime of the OutputSurfaceClient or until unregisted --
  // use SetTreeActivationCallback(base::Closure()) to unregister it.
  virtual void SetTreeActivationCallback(const base::Closure& callback) = 0;

 protected:
  virtual ~OutputSurfaceClient() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_CLIENT_H_
