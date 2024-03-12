// Copyright 2021 The Dawn & Tint Authors
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

#include "dawn/wire/client/Adapter.h"

#include <memory>
#include <string>

#include "dawn/common/Log.h"
#include "dawn/wire/client/Client.h"

namespace dawn::wire::client {
namespace {

class RequestDeviceEvent : public TrackedEvent {
  public:
    static constexpr EventType kType = EventType::RequestDevice;

    RequestDeviceEvent(const WGPURequestDeviceCallbackInfo& callbackInfo, Device* device)
        : TrackedEvent(callbackInfo.mode),
          mCallback(callbackInfo.callback),
          mUserdata(callbackInfo.userdata),
          mDevice(device) {}

    EventType GetType() override { return kType; }

    WireResult ReadyHook(FutureID futureID,
                         WGPURequestDeviceStatus status,
                         const char* message,
                         const WGPUSupportedLimits* limits,
                         uint32_t featuresCount,
                         const WGPUFeatureName* features) {
        DAWN_ASSERT(mDevice != nullptr);
        mStatus = status;
        if (message != nullptr) {
            mMessage = message;
        }
        if (status == WGPURequestDeviceStatus_Success) {
            mDevice->SetLimits(limits);
            mDevice->SetFeatures(features, featuresCount);
        }
        return WireResult::Success;
    }

  private:
    void CompleteImpl(FutureID futureID, EventCompletionType completionType) override {
        if (completionType == EventCompletionType::Shutdown) {
            mStatus = WGPURequestDeviceStatus_Unknown;
            mMessage = "GPU connection lost";
        }
        if (mStatus != WGPURequestDeviceStatus_Success && mDevice != nullptr) {
            // If there was an error, we may need to reclaim the device allocation, otherwise the
            // device is returned to the user who owns it.
            mDevice->GetClient()->Free(mDevice);
            mDevice = nullptr;
        }
        if (mCallback) {
            mCallback(mStatus, ToAPI(mDevice), mMessage ? mMessage->c_str() : nullptr, mUserdata);
        }
    }

    WGPURequestDeviceCallback mCallback;
    void* mUserdata;

    // Note that the message is optional because we want to return nullptr when it wasn't set
    // instead of a pointer to an empty string.
    WGPURequestDeviceStatus mStatus;
    std::optional<std::string> mMessage;

    // The device is created when we call RequestDevice(F). It is guaranteed to be alive
    // throughout the duration of a RequestDeviceEvent because the Event essentially takes
    // ownership of it until either an error occurs at which point the Event cleans it up, or it
    // returns the device to the user who then takes ownership as the Event goes away.
    Device* mDevice = nullptr;
};

}  // anonymous namespace

Adapter::~Adapter() {
    mRequestDeviceRequests.CloseAll([](RequestDeviceData* request) {
        request->callback(WGPURequestDeviceStatus_Unknown, nullptr,
                          "Adapter destroyed before callback", request->userdata);
    });
}

void Adapter::CancelCallbacksForDisconnect() {
    mRequestDeviceRequests.CloseAll([](RequestDeviceData* request) {
        request->callback(WGPURequestDeviceStatus_Unknown, nullptr, "GPU connection lost",
                          request->userdata);
    });
}

bool Adapter::GetLimits(WGPUSupportedLimits* limits) const {
    return mLimitsAndFeatures.GetLimits(limits);
}

bool Adapter::HasFeature(WGPUFeatureName feature) const {
    return mLimitsAndFeatures.HasFeature(feature);
}

size_t Adapter::EnumerateFeatures(WGPUFeatureName* features) const {
    return mLimitsAndFeatures.EnumerateFeatures(features);
}

void Adapter::SetLimits(const WGPUSupportedLimits* limits) {
    return mLimitsAndFeatures.SetLimits(limits);
}

void Adapter::SetFeatures(const WGPUFeatureName* features, uint32_t featuresCount) {
    return mLimitsAndFeatures.SetFeatures(features, featuresCount);
}

void Adapter::SetProperties(const WGPUAdapterProperties* properties) {
    mProperties = *properties;
    mProperties.nextInChain = nullptr;

    // Loop through the chained struct.
    WGPUChainedStructOut* chain = properties->nextInChain;
    while (chain != nullptr) {
        switch (chain->sType) {
            case WGPUSType_AdapterPropertiesMemoryHeaps: {
                // Make a copy of the heap info in `mMemoryHeapInfo`.
                const auto* memoryHeapProperties =
                    reinterpret_cast<const WGPUAdapterPropertiesMemoryHeaps*>(chain);
                mMemoryHeapInfo = {
                    memoryHeapProperties->heapInfo,
                    memoryHeapProperties->heapInfo + memoryHeapProperties->heapCount};
                break;
            }
            case WGPUSType_AdapterPropertiesD3D: {
                auto* d3dProperties = reinterpret_cast<WGPUAdapterPropertiesD3D*>(chain);
                mD3DProperties.shaderModel = d3dProperties->shaderModel;
                break;
            }
            default:
                DAWN_UNREACHABLE();
                break;
        }
        chain = chain->next;
    }
}

void Adapter::GetProperties(WGPUAdapterProperties* properties) const {
    // Loop through the chained struct.
    WGPUChainedStructOut* chain = properties->nextInChain;
    while (chain != nullptr) {
        switch (chain->sType) {
            case WGPUSType_AdapterPropertiesMemoryHeaps: {
                // Copy `mMemoryHeapInfo` into a new allocation.
                auto* memoryHeapProperties =
                    reinterpret_cast<WGPUAdapterPropertiesMemoryHeaps*>(chain);
                size_t heapCount = mMemoryHeapInfo.size();
                auto* heapInfo = new WGPUMemoryHeapInfo[heapCount];
                memcpy(heapInfo, mMemoryHeapInfo.data(), sizeof(WGPUMemoryHeapInfo) * heapCount);
                // Write out the pointer and count to the heap properties out-struct.
                memoryHeapProperties->heapCount = heapCount;
                memoryHeapProperties->heapInfo = heapInfo;
                break;
            }
            case WGPUSType_AdapterPropertiesD3D: {
                auto* d3dProperties = reinterpret_cast<WGPUAdapterPropertiesD3D*>(chain);
                d3dProperties->shaderModel = mD3DProperties.shaderModel;
                break;
            }
            default:
                break;
        }
        chain = chain->next;
    }

    *properties = mProperties;

    // Get lengths, with null terminators.
    size_t vendorNameCLen = strlen(mProperties.vendorName) + 1;
    size_t architectureCLen = strlen(mProperties.architecture) + 1;
    size_t nameCLen = strlen(mProperties.name) + 1;
    size_t driverDescriptionCLen = strlen(mProperties.driverDescription) + 1;

    // Allocate space for all strings.
    char* ptr = new char[vendorNameCLen + architectureCLen + nameCLen + driverDescriptionCLen];

    properties->vendorName = ptr;
    memcpy(ptr, mProperties.vendorName, vendorNameCLen);
    ptr += vendorNameCLen;

    properties->architecture = ptr;
    memcpy(ptr, mProperties.architecture, architectureCLen);
    ptr += architectureCLen;

    properties->name = ptr;
    memcpy(ptr, mProperties.name, nameCLen);
    ptr += nameCLen;

    properties->driverDescription = ptr;
    memcpy(ptr, mProperties.driverDescription, driverDescriptionCLen);
    ptr += driverDescriptionCLen;
}

void ClientAdapterPropertiesFreeMembers(WGPUAdapterProperties properties) {
    // This single delete is enough because everything is a single allocation.
    delete[] properties.vendorName;
}

void ClientAdapterPropertiesMemoryHeapsFreeMembers(
    WGPUAdapterPropertiesMemoryHeaps memoryHeapProperties) {
    delete[] memoryHeapProperties.heapInfo;
}

void Adapter::RequestDevice(const WGPUDeviceDescriptor* descriptor,
                            WGPURequestDeviceCallback callback,
                            void* userdata) {
    WGPURequestDeviceCallbackInfo callbackInfo = {};
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = callback;
    callbackInfo.userdata = userdata;
    RequestDeviceF(descriptor, callbackInfo);
}

WGPUFuture Adapter::RequestDeviceF(const WGPUDeviceDescriptor* descriptor,
                                   const WGPURequestDeviceCallbackInfo& callbackInfo) {
    Client* client = GetClient();
    Device* device = client->Make<Device>(GetEventManagerHandle(), descriptor);
    auto [futureIDInternal, tracked] =
        GetEventManager().TrackEvent(std::make_unique<RequestDeviceEvent>(callbackInfo, device));
    if (!tracked) {
        return {futureIDInternal};
    }

    // Ensure the device lost callback isn't serialized as part of the command, as it cannot be
    // passed between processes.
    WGPUDeviceDescriptor wireDescriptor = {};
    if (descriptor) {
        wireDescriptor = *descriptor;
        wireDescriptor.deviceLostCallback = nullptr;
        wireDescriptor.deviceLostUserdata = nullptr;
    }

    AdapterRequestDeviceCmd cmd;
    cmd.adapterId = GetWireId();
    cmd.eventManagerHandle = GetEventManagerHandle();
    cmd.future = {futureIDInternal};
    cmd.deviceObjectHandle = device->GetWireHandle();
    cmd.descriptor = &wireDescriptor;

    client->SerializeCommand(cmd);
    return {futureIDInternal};
}

bool Client::DoAdapterRequestDeviceCallback(ObjectHandle eventManager,
                                            WGPUFuture future,
                                            WGPURequestDeviceStatus status,
                                            const char* message,
                                            const WGPUSupportedLimits* limits,
                                            uint32_t featuresCount,
                                            const WGPUFeatureName* features) {
    return GetEventManager(eventManager)
               .SetFutureReady<RequestDeviceEvent>(future.id, status, message, limits,
                                                   featuresCount, features) == WireResult::Success;
}

WGPUInstance Adapter::GetInstance() const {
    dawn::ErrorLog() << "adapter.GetInstance not supported with dawn_wire.";
    return nullptr;
}

WGPUDevice Adapter::CreateDevice(const WGPUDeviceDescriptor*) {
    dawn::ErrorLog() << "adapter.CreateDevice not supported with dawn_wire.";
    return nullptr;
}

}  // namespace dawn::wire::client
