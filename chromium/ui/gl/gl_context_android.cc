// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context.h"

#include "base/android/sys_utils.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sys_info.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

namespace gfx {

namespace {

// Used to render into an already current context+surface,
// that we do not have ownership of (draw callback).
// TODO(boliu): Make this inherit from GLContextEGL.
class GLNonOwnedContext : public GLContextReal {
 public:
  GLNonOwnedContext(GLShareGroup* share_group);

  // Implement GLContext.
  virtual bool Initialize(GLSurface* compatible_surface,
                          GpuPreference gpu_preference) OVERRIDE;
  virtual void Destroy() OVERRIDE {}
  virtual bool MakeCurrent(GLSurface* surface) OVERRIDE;
  virtual void ReleaseCurrent(GLSurface* surface) OVERRIDE {}
  virtual bool IsCurrent(GLSurface* surface) OVERRIDE { return true; }
  virtual void* GetHandle() OVERRIDE { return NULL; }
  virtual void SetSwapInterval(int interval) OVERRIDE {}
  virtual std::string GetExtensions() OVERRIDE;

 protected:
  virtual ~GLNonOwnedContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GLNonOwnedContext);

  EGLDisplay display_;
};

GLNonOwnedContext::GLNonOwnedContext(GLShareGroup* share_group)
  : GLContextReal(share_group), display_(NULL) {}

bool GLNonOwnedContext::Initialize(GLSurface* compatible_surface,
                        GpuPreference gpu_preference) {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  return true;
}

bool GLNonOwnedContext::MakeCurrent(GLSurface* surface) {
  SetCurrent(surface);
  SetRealGLApi();
  return true;
}

std::string GLNonOwnedContext::GetExtensions() {
  const char* extensions = eglQueryString(display_, EGL_EXTENSIONS);
  if (!extensions)
    return GLContext::GetExtensions();

  return GLContext::GetExtensions() + " " + extensions;
}

}  // anonymous namespace

// static
scoped_refptr<GLContext> GLContext::CreateGLContext(
    GLShareGroup* share_group,
    GLSurface* compatible_surface,
    GpuPreference gpu_preference) {
  if (GetGLImplementation() == kGLImplementationMockGL)
    return scoped_refptr<GLContext>(new GLContextStub());

  scoped_refptr<GLContext> context;
  if (compatible_surface->GetHandle())
    context = new GLContextEGL(share_group);
  else
    context = new GLNonOwnedContext(share_group);
  if (!context->Initialize(compatible_surface, gpu_preference))
    return NULL;
  return context;
}

bool GLContextEGL::GetTotalGpuMemory(size_t* bytes) {
  DCHECK(bytes);
  *bytes = 0;

  // We can't query available GPU memory from the system on Android.
  // Physical memory is also mis-reported sometimes (eg. Nexus 10 reports
  // 1262MB when it actually has 2GB, while Razr M has 1GB but only reports
  // 128MB java heap size). First we estimate physical memory using both.
  size_t dalvik_mb = base::SysInfo::DalvikHeapSizeMB();
  size_t physical_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  size_t physical_memory_mb = 0;
  if (dalvik_mb >= 256)
    physical_memory_mb = dalvik_mb * 4;
  else
    physical_memory_mb = std::max(dalvik_mb * 4,
                                  (physical_mb * 4) / 3);

  // Now we take a default of 1/8th of memory on high-memory devices,
  // and gradually scale that back for low-memory devices (to be nicer
  // to other apps so they don't get killed). Examples:
  // Nexus 4/10(2GB)    256MB
  // Droid Razr M(1GB)  91MB
  // Galaxy Nexus(1GB)  85MB
  // Xoom(1GB)          85MB
  // Nexus S(low-end)   8MB
  static size_t limit_bytes = 0;
  if (limit_bytes == 0) {
    if (!base::android::SysUtils::IsLowEndDevice()) {
      if (physical_memory_mb >= 1536)
        limit_bytes = physical_memory_mb / 8;
      else if (physical_memory_mb >= 1152)
        limit_bytes = physical_memory_mb / 10;
      else if (physical_memory_mb >= 768)
        limit_bytes = physical_memory_mb / 12;
      else
        limit_bytes = physical_memory_mb / 16;
    } else {
      // Low-end devices have 512MB or less memory by definition
      // so we hard code the limit rather than relying on the heuristics
      // above. Low-end devices use 4444 textures so we can use a lower limit.
      limit_bytes = 8;
    }
    limit_bytes = limit_bytes * 1024 * 1024;
  }
  *bytes = limit_bytes;
  return true;
}

}
