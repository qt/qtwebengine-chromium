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

#include "dawn/wire/client/Instance.h"

#include <memory>
#include <string>
#include <utility>

#include "dawn/common/Log.h"
#include "dawn/common/WGSLFeatureMapping.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/EventManager.h"
#include "partition_alloc/pointers/raw_ptr.h"
#include "tint/lang/wgsl/features/language_feature.h"
#include "tint/lang/wgsl/features/status.h"

namespace dawn::wire::client {
namespace {

class RequestAdapterEvent : public TrackedEvent {
  public:
    static constexpr EventType kType = EventType::RequestAdapter;

    RequestAdapterEvent(const WGPURequestAdapterCallbackInfo& callbackInfo, Adapter* adapter)
        : TrackedEvent(callbackInfo.mode),
          mCallback(callbackInfo.callback),
          mUserdata(callbackInfo.userdata),
          mAdapter(adapter) {}

    EventType GetType() override { return kType; }

    WireResult ReadyHook(FutureID futureID,
                         WGPURequestAdapterStatus status,
                         const char* message,
                         const WGPUAdapterProperties* properties,
                         const WGPUSupportedLimits* limits,
                         uint32_t featuresCount,
                         const WGPUFeatureName* features) {
        DAWN_ASSERT(mAdapter != nullptr);
        mStatus = status;
        if (message != nullptr) {
            mMessage = message;
        }
        if (status == WGPURequestAdapterStatus_Success) {
            mAdapter->SetProperties(properties);
            mAdapter->SetLimits(limits);
            mAdapter->SetFeatures(features, featuresCount);
        }
        return WireResult::Success;
    }

  private:
    void CompleteImpl(FutureID futureID, EventCompletionType completionType) override {
        if (completionType == EventCompletionType::Shutdown) {
            mStatus = WGPURequestAdapterStatus_Unknown;
            mMessage = "GPU connection lost";
        }
        if (mStatus != WGPURequestAdapterStatus_Success && mAdapter != nullptr) {
            // If there was an error, we may need to reclaim the adapter allocation, otherwise the
            // adapter is returned to the user who owns it.
            mAdapter->GetClient()->Free(mAdapter.get());
            mAdapter = nullptr;
        }
        if (mCallback) {
            mCallback(mStatus, ToAPI(mAdapter), mMessage ? mMessage->c_str() : nullptr, mUserdata);
        }
    }

    WGPURequestAdapterCallback mCallback;
    // TODO(https://crbug.com/dawn/2345): Investigate `DanglingUntriaged` in dawn/wire.
    raw_ptr<void, DanglingUntriaged> mUserdata;

    // Note that the message is optional because we want to return nullptr when it wasn't set
    // instead of a pointer to an empty string.
    WGPURequestAdapterStatus mStatus;
    std::optional<std::string> mMessage;

    // The adapter is created when we call RequestAdapter(F). It is guaranteed to be alive
    // throughout the duration of a RequestAdapterEvent because the Event essentially takes
    // ownership of it until either an error occurs at which point the Event cleans it up, or it
    // returns the adapter to the user who then takes ownership as the Event goes away.
    // TODO(https://crbug.com/dawn/2345): Investigate `DanglingUntriaged` in dawn/wire.
    raw_ptr<Adapter, DanglingUntriaged> mAdapter = nullptr;
};

WGPUWGSLFeatureName ToWGPUFeature(tint::wgsl::LanguageFeature f) {
    switch (f) {
#define CASE(WgslName, WgpuName)                \
    case tint::wgsl::LanguageFeature::WgslName: \
        return WGPUWGSLFeatureName_##WgpuName;
        DAWN_FOREACH_WGSL_FEATURE(CASE)
#undef CASE
    }
}

}  // anonymous namespace

// Free-standing API functions

WGPUBool ClientGetInstanceFeatures(WGPUInstanceFeatures* features) {
    if (features->nextInChain != nullptr) {
        return false;
    }

    features->timedWaitAnyEnable = false;
    features->timedWaitAnyMaxCount = kTimedWaitAnyMaxCountDefault;
    return true;
}

WGPUInstance ClientCreateInstance(WGPUInstanceDescriptor const* descriptor) {
    DAWN_UNREACHABLE();
    return nullptr;
}

// Instance

Instance::Instance(const ObjectBaseParams& params) : ObjectWithEventsBase(params, params.handle) {}

Instance::~Instance() {
    GetEventManager().TransitionTo(EventManager::State::InstanceDropped);
}

WireResult Instance::Initialize(const WGPUInstanceDescriptor* descriptor) {
    if (descriptor == nullptr) {
        return WireResult::Success;
    }

    if (descriptor->features.timedWaitAnyEnable) {
        dawn::ErrorLog() << "Wire client instance doesn't support timedWaitAnyEnable = true";
        return WireResult::FatalError;
    }
    if (descriptor->features.timedWaitAnyMaxCount > 0) {
        dawn::ErrorLog() << "Wire client instance doesn't support non-zero timedWaitAnyMaxCount";
        return WireResult::FatalError;
    }

    const WGPUDawnWireWGSLControl* wgslControl = nullptr;
    const WGPUDawnWGSLBlocklist* wgslBlocklist = nullptr;
    for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain != nullptr;
         chain = chain->next) {
        switch (chain->sType) {
            case WGPUSType_DawnWireWGSLControl:
                wgslControl = reinterpret_cast<const WGPUDawnWireWGSLControl*>(chain);
                break;
            case WGPUSType_DawnWGSLBlocklist:
                wgslBlocklist = reinterpret_cast<const WGPUDawnWGSLBlocklist*>(chain);
                break;
            default:
                dawn::ErrorLog() << "Wire client instance doesn't support InstanceDescriptor "
                                    "extension structure with sType ("
                                 << chain->sType << ")";
                return WireResult::FatalError;
        }
    }

    GatherWGSLFeatures(wgslControl, wgslBlocklist);

    return WireResult::Success;
}

void Instance::RequestAdapter(const WGPURequestAdapterOptions* options,
                              WGPURequestAdapterCallback callback,
                              void* userdata) {
    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = callback;
    callbackInfo.userdata = userdata;
    RequestAdapterF(options, callbackInfo);
}

WGPUFuture Instance::RequestAdapterF(const WGPURequestAdapterOptions* options,
                                     const WGPURequestAdapterCallbackInfo& callbackInfo) {
    Client* client = GetClient();
    Adapter* adapter = client->Make<Adapter>(GetEventManagerHandle());
    auto [futureIDInternal, tracked] =
        GetEventManager().TrackEvent(std::make_unique<RequestAdapterEvent>(callbackInfo, adapter));
    if (!tracked) {
        return {futureIDInternal};
    }

    InstanceRequestAdapterCmd cmd;
    cmd.instanceId = GetWireId();
    cmd.eventManagerHandle = GetEventManagerHandle();
    cmd.future = {futureIDInternal};
    cmd.adapterObjectHandle = adapter->GetWireHandle();
    cmd.options = options;

    client->SerializeCommand(cmd);
    return {futureIDInternal};
}

bool Client::DoInstanceRequestAdapterCallback(ObjectHandle eventManager,
                                              WGPUFuture future,
                                              WGPURequestAdapterStatus status,
                                              const char* message,
                                              const WGPUAdapterProperties* properties,
                                              const WGPUSupportedLimits* limits,
                                              uint32_t featuresCount,
                                              const WGPUFeatureName* features) {
    return GetEventManager(eventManager)
               .SetFutureReady<RequestAdapterEvent>(future.id, status, message, properties, limits,
                                                    featuresCount, features) == WireResult::Success;
}

void Instance::ProcessEvents() {
    GetEventManager().ProcessPollEvents();

    // TODO(crbug.com/dawn/1987): The responsibility of ProcessEvents here is a bit mixed. It both
    // processes events coming in from the server, and also prompts the server to check for and
    // forward over new events - which won't be received until *after* this client-side
    // ProcessEvents completes.
    //
    // Fixing this nicely probably requires the server to more self-sufficiently
    // forward the events, which is half of making the wire fully invisible to use (which we might
    // like to do, someday, but not soon). This is easy for immediate events (like requestDevice)
    // and thread-driven events (async pipeline creation), but harder for queue fences where we have
    // to wait on the backend and then trigger Dawn code to forward the event.
    //
    // In the meantime, we could maybe do this on client->server flush to keep this concern in the
    // wire instead of in the API itself, but otherwise it's not significantly better so we just
    // keep it here for now for backward compatibility.
    InstanceProcessEventsCmd cmd;
    cmd.self = ToAPI(this);
    GetClient()->SerializeCommand(cmd);
}

WGPUWaitStatus Instance::WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS) {
    return GetEventManager().WaitAny(count, infos, timeoutNS);
}

void Instance::GatherWGSLFeatures(const WGPUDawnWireWGSLControl* wgslControl,
                                  const WGPUDawnWGSLBlocklist* wgslBlocklist) {
    WGPUDawnWireWGSLControl defaultWgslControl{};
    if (wgslControl == nullptr) {
        wgslControl = &defaultWgslControl;
    }

    for (auto wgslFeature : tint::wgsl::kAllLanguageFeatures) {
        // Skip over testing features if we don't have the toggle to expose them.
        if (!wgslControl->enableTesting) {
            switch (wgslFeature) {
                case tint::wgsl::LanguageFeature::kChromiumTestingUnimplemented:
                case tint::wgsl::LanguageFeature::kChromiumTestingUnsafeExperimental:
                case tint::wgsl::LanguageFeature::kChromiumTestingExperimental:
                case tint::wgsl::LanguageFeature::kChromiumTestingShippedWithKillswitch:
                case tint::wgsl::LanguageFeature::kChromiumTestingShipped:
                    continue;
                default:
                    break;
            }
        }

        // Expose the feature depending on its status and wgslControl.
        bool enable = false;
        switch (tint::wgsl::GetLanguageFeatureStatus(wgslFeature)) {
            case tint::wgsl::FeatureStatus::kUnknown:
            case tint::wgsl::FeatureStatus::kUnimplemented:
                enable = false;
                break;

            case tint::wgsl::FeatureStatus::kUnsafeExperimental:
                enable = wgslControl->enableUnsafe;
                break;
            case tint::wgsl::FeatureStatus::kExperimental:
                enable = wgslControl->enableExperimental;
                break;

            case tint::wgsl::FeatureStatus::kShippedWithKillswitch:
            case tint::wgsl::FeatureStatus::kShipped:
                enable = true;
                break;
        }

        if (enable) {
            mWGSLFeatures.emplace(ToWGPUFeature(wgslFeature));
        }
    }

    // Remove blocklisted features.
    if (wgslBlocklist != nullptr) {
        for (size_t i = 0; i < wgslBlocklist->blocklistedFeatureCount; i++) {
            const char* name = wgslBlocklist->blocklistedFeatures[i];
            tint::wgsl::LanguageFeature tintFeature = tint::wgsl::ParseLanguageFeature(name);
            WGPUWGSLFeatureName feature = ToWGPUFeature(tintFeature);

            // Ignore unknown features in the blocklist.
            if (feature == WGPUWGSLFeatureName_Undefined) {
                continue;
            }

            mWGSLFeatures.erase(feature);
        }
    }
}

bool Instance::HasWGSLLanguageFeature(WGPUWGSLFeatureName feature) const {
    return mWGSLFeatures.contains(feature);
}

size_t Instance::EnumerateWGSLLanguageFeatures(WGPUWGSLFeatureName* features) const {
    if (features != nullptr) {
        for (WGPUWGSLFeatureName f : mWGSLFeatures) {
            *features = f;
            ++features;
        }
    }
    return mWGSLFeatures.size();
}

}  // namespace dawn::wire::client
