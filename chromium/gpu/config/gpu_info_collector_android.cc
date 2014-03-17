// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace {

std::string GetDriverVersionFromString(const std::string& version_string) {
  // Extract driver version from the second number in a string like:
  // "OpenGL ES 2.0 V@6.0 AU@ (CL@2946718)"

  // Exclude first "2.0".
  size_t begin = version_string.find_first_of("0123456789");
  if (begin == std::string::npos)
    return "0";
  size_t end = version_string.find_first_not_of("01234567890.", begin);

  // Extract number of the form "%d.%d"
  begin = version_string.find_first_of("0123456789", end);
  if (begin == std::string::npos)
    return "0";
  end = version_string.find_first_not_of("01234567890.", begin);
  std::string sub_string;
  if (end != std::string::npos)
    sub_string = version_string.substr(begin, end - begin);
  else
    sub_string = version_string.substr(begin);
  std::vector<std::string> pieces;
  base::SplitString(sub_string, '.', &pieces);
  if (pieces.size() < 2)
    return "0";
  return pieces[0] + "." + pieces[1];
}

class ScopedRestoreNonOwnedEGLContext {
 public:
  ScopedRestoreNonOwnedEGLContext();
  ~ScopedRestoreNonOwnedEGLContext();

 private:
  EGLContext context_;
  EGLDisplay display_;
  EGLSurface draw_surface_;
  EGLSurface read_surface_;
};

ScopedRestoreNonOwnedEGLContext::ScopedRestoreNonOwnedEGLContext()
  : context_(EGL_NO_CONTEXT),
    display_(EGL_NO_DISPLAY),
    draw_surface_(EGL_NO_SURFACE),
    read_surface_(EGL_NO_SURFACE) {
  // This should only used to restore a context that is not created or owned by
  // Chromium native code, but created by Android system itself.
  DCHECK(!gfx::GLContext::GetCurrent());

  if (gfx::GLSurface::InitializeOneOff()) {
    context_ = eglGetCurrentContext();
    display_ = eglGetCurrentDisplay();
    draw_surface_ = eglGetCurrentSurface(EGL_DRAW);
    read_surface_ = eglGetCurrentSurface(EGL_READ);
  }
}

ScopedRestoreNonOwnedEGLContext::~ScopedRestoreNonOwnedEGLContext() {
  if (context_ == EGL_NO_CONTEXT || display_ == EGL_NO_DISPLAY ||
      draw_surface_ == EGL_NO_SURFACE || read_surface_ == EGL_NO_SURFACE) {
    return;
  }

  if (!eglMakeCurrent(display_, draw_surface_, read_surface_, context_)) {
    LOG(WARNING) << "Failed to restore EGL context";
  }
}

}

namespace gpu {

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  return CollectBasicGraphicsInfo(gpu_info);
}

GpuIDResult CollectGpuID(uint32* vendor_id, uint32* device_id) {
  DCHECK(vendor_id && device_id);
  *vendor_id = 0;
  *device_id = 0;
  return kGpuIDNotSupported;
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  gpu_info->can_lose_context = false;
  gpu_info->finalized = true;

  gpu_info->machine_model = base::android::BuildInfo::GetInstance()->model();

  // Create a short-lived context on the UI thread to collect the GL strings.
  // Make sure we restore the existing context if there is one.
  ScopedRestoreNonOwnedEGLContext restore_context;
  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectDriverInfoGL(GPUInfo* gpu_info) {
  gpu_info->driver_version = GetDriverVersionFromString(
      gpu_info->gl_version_string);
  gpu_info->gpu.vendor_string = gpu_info->gl_vendor;
  gpu_info->gpu.device_string = gpu_info->gl_renderer;
  return true;
}

void MergeGPUInfo(GPUInfo* basic_gpu_info,
                  const GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

bool DetermineActiveGPU(GPUInfo* gpu_info) {
  DCHECK(gpu_info);
  if (gpu_info->secondary_gpus.size() == 0)
    return true;
  // TODO(zmo): implement this when Android starts to support more
  // than one GPUs.
  return false;
}

}  // namespace gpu
