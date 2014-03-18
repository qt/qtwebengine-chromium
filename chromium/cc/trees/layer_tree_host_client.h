// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Vector2d;
}

namespace cc {
class ContextProvider;
class InputHandlerClient;
class OutputSurface;

class LayerTreeHostClient {
 public:
  virtual void WillBeginMainFrame(int frame_id) = 0;
  // Marks finishing compositing-related tasks on the main thread. In threaded
  // mode, this corresponds to DidCommit().
  virtual void DidBeginMainFrame() = 0;
  virtual void Animate(double frame_begin_time) = 0;
  virtual void Layout() = 0;
  virtual void ApplyScrollAndScale(gfx::Vector2d scroll_delta,
                                   float page_scale) = 0;
  // Creates an OutputSurface. If fallback is true, it should attempt to
  // create an OutputSurface that is guaranteed to initialize correctly.
  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback) = 0;
  virtual void DidInitializeOutputSurface(bool success) = 0;
  virtual void WillCommit() = 0;
  virtual void DidCommit() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
  virtual void DidCompleteSwapBuffers() = 0;

  // If the client provides an OutputSurface bound to a 3d context for direct
  // rendering, this must return a provider that provides contexts usable from
  // the same thread as the OutputSurface's context.
  virtual scoped_refptr<ContextProvider> OffscreenContextProvider() = 0;

  // Requests that the client insert a rate limiting token in the shared main
  // thread context's command stream that will block if the context gets too far
  // ahead of the compositor's command stream. Only needed if the tree contains
  // a TextureLayer that calls SetRateLimitContext(true).
  virtual void RateLimitSharedMainThreadContext() {}

  // This hook is for testing.
  virtual void DidFailToInitializeOutputSurface() {}

 protected:
  virtual ~LayerTreeHostClient() {}
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_CLIENT_H_
