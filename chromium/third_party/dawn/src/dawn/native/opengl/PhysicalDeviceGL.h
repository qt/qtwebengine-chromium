// Copyright 2022 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_DAWN_NATIVE_OPENGL_PHYSICALDEVICEGL_H_
#define SRC_DAWN_NATIVE_OPENGL_PHYSICALDEVICEGL_H_

#include "dawn/native/PhysicalDevice.h"
#include "dawn/native/opengl/EGLFunctions.h"
#include "dawn/native/opengl/OpenGLFunctions.h"

namespace dawn::native::opengl {

class PhysicalDevice : public PhysicalDeviceBase {
  public:
    static ResultOrError<Ref<PhysicalDevice>> Create(InstanceBase* instance,
                                                     wgpu::BackendType backendType,
                                                     void* (*getProc)(const char*),
                                                     EGLDisplay display);

    ~PhysicalDevice() override = default;

    // PhysicalDeviceBase Implementation
    bool SupportsExternalImages() const override;
    bool SupportsFeatureLevel(FeatureLevel featureLevel) const override;

  private:
    PhysicalDevice(InstanceBase* instance, wgpu::BackendType backendType, EGLDisplay display);
    MaybeError InitializeGLFunctions(void* (*getProc)(const char*));

    MaybeError InitializeImpl() override;
    void InitializeSupportedFeaturesImpl() override;
    MaybeError InitializeSupportedLimitsImpl(CombinedLimits* limits) override;

    FeatureValidationResult ValidateFeatureSupportedWithTogglesImpl(
        wgpu::FeatureName feature,
        const TogglesState& toggles) const override;

    void SetupBackendAdapterToggles(TogglesState* adapterToggles) const override;
    void SetupBackendDeviceToggles(TogglesState* deviceToggles) const override;
    ResultOrError<Ref<DeviceBase>> CreateDeviceImpl(AdapterBase* adapter,
                                                    const UnpackedPtr<DeviceDescriptor>& descriptor,
                                                    const TogglesState& deviceToggles) override;

    void PopulateBackendProperties(UnpackedPtr<AdapterProperties>& properties) const override;

    OpenGLFunctions mFunctions;
    EGLDisplay mDisplay;
    EGLFunctions mEGLFunctions;
};

}  // namespace dawn::native::opengl

#endif  // SRC_DAWN_NATIVE_OPENGL_PHYSICALDEVICEGL_H_
