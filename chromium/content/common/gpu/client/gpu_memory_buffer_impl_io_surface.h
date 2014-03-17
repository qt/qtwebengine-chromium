// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_

#include "base/mac/scoped_cftyperef.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

class IOSurfaceSupport;

namespace content {

// Provides implementation of a GPU memory buffer based
// on an IO surface handle.
class GpuMemoryBufferImplIOSurface : public GpuMemoryBufferImpl {
 public:
  GpuMemoryBufferImplIOSurface(gfx::Size size, unsigned internalformat);
  virtual ~GpuMemoryBufferImplIOSurface();

  static bool IsFormatSupported(unsigned internalformat);
  static uint32 PixelFormat(unsigned internalformat);

  bool Initialize(gfx::GpuMemoryBufferHandle handle);

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void Map(AccessMode mode, void** vaddr) OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 GetStride() const OVERRIDE;
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;

 private:
  IOSurfaceSupport* io_surface_support_;
  base::ScopedCFTypeRef<CFTypeRef> io_surface_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplIOSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_IO_SURFACE_H_
