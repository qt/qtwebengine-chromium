// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"

using ::gfx::MockGLInterface;
using ::testing::Return;

namespace gpu {

class GPUInfoCollectorTest : public testing::Test {
 public:
  GPUInfoCollectorTest() {}
  virtual ~GPUInfoCollectorTest() { }

  virtual void SetUp() {
    // TODO(kbr): make this setup robust in the case where
    // GLSurface::InitializeOneOff() has already been called by
    // another unit test. http://crbug.com/100285
    gfx::InitializeGLBindings(gfx::kGLImplementationMockGL);
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
#if defined(OS_WIN)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0658;
    const char* driver_vendor = "";  // not implemented
    const char* driver_version = "";
    const char* shader_version = "1.40";
    const char* gl_version = "3.1";
    const char* gl_renderer = "Quadro FX 380/PCI/SSE2";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "3.1.0";
    const char* gl_shading_language_version = "1.40 NVIDIA via Cg compiler";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#elif defined(OS_MACOSX)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0640;
    const char* driver_vendor = "";  // not implemented
    const char* driver_version = "1.6.18";
    const char* shader_version = "1.20";
    const char* gl_version = "2.1";
    const char* gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "2.1 NVIDIA-1.6.18";
    const char* gl_shading_language_version = "1.20 ";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#else  // defined (OS_LINUX)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0658;
    const char* driver_vendor = "NVIDIA";
    const char* driver_version = "195.36.24";
    const char* shader_version = "1.50";
    const char* gl_version = "3.2";
    const char* gl_renderer = "Quadro FX 380/PCI/SSE2";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "3.2.0 NVIDIA 195.36.24";
    const char* gl_shading_language_version = "1.50 NVIDIA via Cg compiler";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#endif
    test_values_.gpu.vendor_id = vendor_id;
    test_values_.gpu.device_id = device_id;
    test_values_.driver_vendor = driver_vendor;
    test_values_.driver_version =driver_version;
    test_values_.pixel_shader_version = shader_version;
    test_values_.vertex_shader_version = shader_version;
    test_values_.gl_version = gl_version;
    test_values_.gl_renderer = gl_renderer;
    test_values_.gl_vendor = gl_vendor;
    test_values_.gl_version_string = gl_version_string;
    test_values_.gl_extensions = gl_extensions;
    test_values_.can_lose_context = false;

    EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_extensions)));
    EXPECT_CALL(*gl_, GetString(GL_SHADING_LANGUAGE_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_shading_language_version)));
    EXPECT_CALL(*gl_, GetString(GL_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_version_string)));
    EXPECT_CALL(*gl_, GetString(GL_VENDOR))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_vendor)));
    EXPECT_CALL(*gl_, GetString(GL_RENDERER))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_renderer)));
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

 public:
  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  GPUInfo test_values_;
};

// TODO(rlp): Test the vendor and device id collection if deemed necessary as
//            it involves several complicated mocks for each platform.

// TODO(kbr): re-enable these tests; see http://crbug.com/100285 .

TEST_F(GPUInfoCollectorTest, DISABLED_DriverVendorGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.driver_vendor,
            gpu_info.driver_vendor);
}

// Skip Windows because the driver version is obtained from bot registry.
#if !defined(OS_WIN)
TEST_F(GPUInfoCollectorTest, DISABLED_DriverVersionGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.driver_version,
            gpu_info.driver_version);
}
#endif

TEST_F(GPUInfoCollectorTest, DISABLED_PixelShaderVersionGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.pixel_shader_version,
            gpu_info.pixel_shader_version);
}

TEST_F(GPUInfoCollectorTest, DISABLED_VertexShaderVersionGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.vertex_shader_version,
            gpu_info.vertex_shader_version);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLVersionGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_version,
            gpu_info.gl_version);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLVersionStringGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_version_string,
            gpu_info.gl_version_string);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLRendererGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_renderer,
            gpu_info.gl_renderer);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLVendorGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_vendor,
            gpu_info.gl_vendor);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLExtensionsGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_extensions,
            gpu_info.gl_extensions);
}

#if defined(OS_LINUX)
TEST(GPUInfoCollectorUtilTest, DetermineActiveGPU) {
  const uint32 kIntelVendorID = 0x8086;
  const uint32 kIntelDeviceID = 0x0046;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = kIntelVendorID;
  intel_gpu.device_id = kIntelDeviceID;

  const uint32 kAMDVendorID = 0x1002;
  const uint32 kAMDDeviceID = 0x68c1;
  GPUInfo::GPUDevice amd_gpu;
  amd_gpu.vendor_id = kAMDVendorID;
  amd_gpu.device_id = kAMDDeviceID;

  // One GPU, do nothing.
  {
    GPUInfo gpu_info;
    gpu_info.gpu = amd_gpu;
    EXPECT_TRUE(DetermineActiveGPU(&gpu_info));
  }

  // Two GPUs, switched.
  {
    GPUInfo gpu_info;
    gpu_info.gpu = amd_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    gpu_info.gl_vendor = "Intel Open Source Technology Center";
    EXPECT_TRUE(DetermineActiveGPU(&gpu_info));
    EXPECT_EQ(kIntelVendorID, gpu_info.gpu.vendor_id);
    EXPECT_EQ(kIntelDeviceID, gpu_info.gpu.device_id);
    EXPECT_EQ(kAMDVendorID, gpu_info.secondary_gpus[0].vendor_id);
    EXPECT_EQ(kAMDDeviceID, gpu_info.secondary_gpus[0].device_id);
  }

  // Two GPUs, no switch necessary.
  {
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(amd_gpu);
    gpu_info.gl_vendor = "Intel Open Source Technology Center";
    EXPECT_TRUE(DetermineActiveGPU(&gpu_info));
    EXPECT_EQ(kIntelVendorID, gpu_info.gpu.vendor_id);
    EXPECT_EQ(kIntelDeviceID, gpu_info.gpu.device_id);
    EXPECT_EQ(kAMDVendorID, gpu_info.secondary_gpus[0].vendor_id);
    EXPECT_EQ(kAMDDeviceID, gpu_info.secondary_gpus[0].device_id);
  }

  // Two GPUs, empty GL_VENDOR string.
  {
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(amd_gpu);
    EXPECT_FALSE(DetermineActiveGPU(&gpu_info));
  }

  // Two GPUs, unhandled GL_VENDOR string.
  {
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(amd_gpu);
    gpu_info.gl_vendor = "nouveau";
    EXPECT_FALSE(DetermineActiveGPU(&gpu_info));
  }
}
#endif

}  // namespace gpu

