// Copyright 2018 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_NATIVE_ADAPTER_H_
#define SRC_DAWN_NATIVE_ADAPTER_H_

#include <vector>

#include "dawn/common/Ref.h"
#include "dawn/common/RefCounted.h"
#include "dawn/native/DawnNative.h"
#include "dawn/native/PhysicalDevice.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

class DeviceBase;
class TogglesState;
struct SupportedLimits;

class AdapterBase : public RefCounted {
  public:
    AdapterBase(Ref<PhysicalDeviceBase> physicalDevice,
                FeatureLevel featureLevel,
                const TogglesState& requiredAdapterToggles,
                wgpu::PowerPreference powerPreference);
    ~AdapterBase() override;

    // WebGPU API
    InstanceBase* APIGetInstance() const;
    bool APIGetLimits(SupportedLimits* limits) const;
    void APIGetProperties(AdapterProperties* properties) const;
    bool APIHasFeature(wgpu::FeatureName feature) const;
    size_t APIEnumerateFeatures(wgpu::FeatureName* features) const;
    void APIRequestDevice(const DeviceDescriptor* descriptor,
                          WGPURequestDeviceCallback callback,
                          void* userdata);
    Future APIRequestDeviceF(const DeviceDescriptor* descriptor,
                             const RequestDeviceCallbackInfo& callbackInfo);
    DeviceBase* APICreateDevice(const DeviceDescriptor* descriptor = nullptr);
    ResultOrError<Ref<DeviceBase>> CreateDevice(const DeviceDescriptor* rawDescriptor);

    void SetUseTieredLimits(bool useTieredLimits);

    FeaturesSet GetSupportedFeatures() const;

    // Return the underlying PhysicalDevice.
    PhysicalDeviceBase* GetPhysicalDevice();

    // Get the actual toggles state of the adapter.
    const TogglesState& GetTogglesState() const;

    FeatureLevel GetFeatureLevel() const;

  private:
    Ref<PhysicalDeviceBase> mPhysicalDevice;
    FeatureLevel mFeatureLevel;
    bool mUseTieredLimits = false;

    // Supported features under adapter toggles.
    FeaturesSet mSupportedFeatures;

    // Adapter toggles state.
    TogglesState mTogglesState;

    wgpu::PowerPreference mPowerPreference;
};

std::vector<Ref<AdapterBase>> SortAdapters(std::vector<Ref<AdapterBase>> adapters,
                                           const RequestAdapterOptions* options);

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_ADAPTER_H_
