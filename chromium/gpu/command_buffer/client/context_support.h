// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
#define GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_

#include "base/callback.h"
#include "ui/gfx/rect.h"

namespace gpu {
struct ManagedMemoryStats;

class ContextSupport {
 public:
  // Runs |callback| when a sync point is reached.
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) = 0;

  // Runs |callback| when a query created via glCreateQueryEXT() has cleared
  // passed the glEndQueryEXT() point.
  virtual void SignalQuery(uint32 query, const base::Closure& callback) = 0;

  // For onscreen contexts, indicates that the surface visibility has changed.
  // Clients aren't expected to draw to an invisible surface.
  virtual void SetSurfaceVisible(bool visible) = 0;

  virtual void SendManagedMemoryStats(const ManagedMemoryStats& stats) = 0;

  virtual void Swap() = 0;
  virtual void PartialSwapBuffers(gfx::Rect sub_buffer) = 0;

  virtual void SetSwapBuffersCompleteCallback(
      const base::Closure& callback) = 0;

 protected:
  ContextSupport() {}
  virtual ~ContextSupport() {}
};

}

#endif  // GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
