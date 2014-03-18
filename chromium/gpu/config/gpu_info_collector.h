// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_INFO_COLLECTOR_H_
#define GPU_CONFIG_GPU_INFO_COLLECTOR_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"

namespace gpu {

enum GpuIDResult {
  kGpuIDFailure,
  kGpuIDSuccess,
  kGpuIDNotSupported
};

// Collect GPU vendor_id and device ID.
GPU_EXPORT GpuIDResult CollectGpuID(uint32* vendor_id, uint32* device_id);

// Collects basic GPU info without creating a GL/DirectX context (and without
// the danger of crashing), including vendor_id and device_id.
// This is called at browser process startup time.
// The subset each platform collects may be different.
GPU_EXPORT bool CollectBasicGraphicsInfo(GPUInfo* gpu_info);

// Create a GL/DirectX context and collect related info.
// This is called at GPU process startup time.
// Returns true on success.
GPU_EXPORT bool CollectContextGraphicsInfo(GPUInfo* gpu_info);

#if defined(OS_WIN)
// Collect the DirectX Disagnostics information about the attached displays.
GPU_EXPORT bool GetDxDiagnostics(DxDiagNode* output);
#endif  // OS_WIN

// Create a GL context and collect GL strings and versions.
GPU_EXPORT bool CollectGraphicsInfoGL(GPUInfo* gpu_info);

// Each platform stores the driver version on the GL_VERSION string differently
GPU_EXPORT bool CollectDriverInfoGL(GPUInfo* gpu_info);

// Merge GPUInfo from CollectContextGraphicsInfo into basic GPUInfo.
// This is platform specific, depending on which info are collected at which
// stage.
GPU_EXPORT void MergeGPUInfo(GPUInfo* basic_gpu_info,
                             const GPUInfo& context_gpu_info);

// MergeGPUInfo() when GL driver is used.
GPU_EXPORT void MergeGPUInfoGL(GPUInfo* basic_gpu_info,
                               const GPUInfo& context_gpu_info);

// If multiple GPUs are detected, use GL_VENDOR string to determine which GPU
// is currently active.
// |gpu_info| is expected to be the merged GPUInfo after full info collection.
// Upon return, |gpu_info->gpu| will contain the active GPU, assuming the
// platform supports it. Return false if it's not supported.
GPU_EXPORT bool DetermineActiveGPU(GPUInfo* gpu_info);

// Advanced Micro Devices has interesting configurations on laptops were
// there are two videocards that can alternatively a given process output.
enum AMDVideoCardType {
  UNKNOWN,
  STANDALONE,
  INTEGRATED,
  SWITCHABLE
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_COLLECTOR_H_
