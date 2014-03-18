// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shm.h"

namespace content {

scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImpl::Create(
    gfx::GpuMemoryBufferHandle handle,
    gfx::Size size,
    unsigned internalformat) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      scoped_ptr<GpuMemoryBufferImplShm> buffer(
          new GpuMemoryBufferImplShm(size, internalformat));
      if (!buffer->Initialize(handle))
        return scoped_ptr<GpuMemoryBufferImpl>();

      return buffer.PassAs<GpuMemoryBufferImpl>();
    }
    case gfx::IO_SURFACE_BUFFER: {
      scoped_ptr<GpuMemoryBufferImplIOSurface> buffer(
          new GpuMemoryBufferImplIOSurface(size, internalformat));
      if (!buffer->Initialize(handle))
        return scoped_ptr<GpuMemoryBufferImpl>();

      return buffer.PassAs<GpuMemoryBufferImpl>();
    }
    default:
      return scoped_ptr<GpuMemoryBufferImpl>();
  }
}

}  // namespace content
