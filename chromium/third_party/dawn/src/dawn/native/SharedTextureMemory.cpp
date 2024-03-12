// Copyright 2023 The Dawn & Tint Authors
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

#include "dawn/native/SharedTextureMemory.h"

#include <utility>

#include "dawn/native/ChainUtils.h"
#include "dawn/native/Device.h"
#include "dawn/native/SharedFence.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

namespace {

class ErrorSharedTextureMemory : public SharedTextureMemoryBase {
  public:
    ErrorSharedTextureMemory(DeviceBase* device, const SharedTextureMemoryDescriptor* descriptor)
        : SharedTextureMemoryBase(device, descriptor, ObjectBase::kError) {}

    Ref<SharedTextureMemoryContents> CreateContents() override { DAWN_UNREACHABLE(); }
    ResultOrError<Ref<TextureBase>> CreateTextureImpl(
        const UnpackedPtr<TextureDescriptor>& descriptor) override {
        DAWN_UNREACHABLE();
    }
    MaybeError BeginAccessImpl(TextureBase* texture,
                               const UnpackedPtr<BeginAccessDescriptor>& descriptor) override {
        DAWN_UNREACHABLE();
    }
    ResultOrError<FenceAndSignalValue> EndAccessImpl(TextureBase* texture,
                                                     UnpackedPtr<EndAccessState>& state) override {
        DAWN_UNREACHABLE();
    }
};

}  // namespace

// static
Ref<SharedTextureMemoryBase> SharedTextureMemoryBase::MakeError(
    DeviceBase* device,
    const SharedTextureMemoryDescriptor* descriptor) {
    return AcquireRef(new ErrorSharedTextureMemory(device, descriptor));
}

SharedTextureMemoryBase::SharedTextureMemoryBase(DeviceBase* device,
                                                 const SharedTextureMemoryDescriptor* descriptor,
                                                 ObjectBase::ErrorTag tag)
    : ApiObjectBase(device, tag, descriptor->label),
      mProperties{
          nullptr,
          wgpu::TextureUsage::None,
          {0, 0, 0},
          wgpu::TextureFormat::Undefined,
      },
      mContents(new SharedTextureMemoryContents(GetWeakRef(this))) {}

SharedTextureMemoryBase::SharedTextureMemoryBase(DeviceBase* device,
                                                 const char* label,
                                                 const SharedTextureMemoryProperties& properties)
    : ApiObjectBase(device, label), mProperties(properties) {
    // Reify properties to ensure we don't expose capabilities not supported by the device.
    const Format& internalFormat = device->GetValidInternalFormat(mProperties.format);
    if (!internalFormat.supportsStorageUsage || internalFormat.IsMultiPlanar()) {
        mProperties.usage = mProperties.usage & ~wgpu::TextureUsage::StorageBinding;
    }
    if (!internalFormat.isRenderable || (internalFormat.IsMultiPlanar() &&
                                         !device->HasFeature(Feature::MultiPlanarRenderTargets))) {
        mProperties.usage = mProperties.usage & ~wgpu::TextureUsage::RenderAttachment;
    }
    if (internalFormat.IsMultiPlanar() &&
        !device->HasFeature(Feature::MultiPlanarFormatExtendedUsages)) {
        mProperties.usage = mProperties.usage & ~wgpu::TextureUsage::CopyDst;
    }

    GetObjectTrackingList()->Track(this);
}

ObjectType SharedTextureMemoryBase::GetType() const {
    return ObjectType::SharedTextureMemory;
}

void SharedTextureMemoryBase::DestroyImpl() {}

bool SharedTextureMemoryBase::HasWriteAccess() const {
    return mHasWriteAccess;
}

bool SharedTextureMemoryBase::HasExclusiveReadAccess() const {
    return mHasExclusiveReadAccess;
}

int SharedTextureMemoryBase::GetReadAccessCount() const {
    return mReadAccessCount;
}

void SharedTextureMemoryBase::Initialize() {
    DAWN_ASSERT(!IsError());
    mContents = CreateContents();
}

void SharedTextureMemoryBase::APIGetProperties(SharedTextureMemoryProperties* properties) const {
    properties->usage = mProperties.usage;
    properties->size = mProperties.size;
    properties->format = mProperties.format;

    UnpackedPtr<SharedTextureMemoryProperties> unpacked;
    if (GetDevice()->ConsumedError(ValidateAndUnpack(properties), &unpacked,
                                   "calling %s.GetProperties", this)) {
        return;
    }
}

TextureBase* SharedTextureMemoryBase::APICreateTexture(const TextureDescriptor* descriptor) {
    Ref<TextureBase> result;

    // Provide the defaults if no descriptor is provided.
    TextureDescriptor defaultDescriptor;
    if (descriptor == nullptr) {
        defaultDescriptor = {};
        defaultDescriptor.format = mProperties.format;
        defaultDescriptor.size = mProperties.size;
        defaultDescriptor.usage = mProperties.usage;
        descriptor = &defaultDescriptor;
    }

    if (GetDevice()->ConsumedError(CreateTexture(descriptor), &result,
                                   InternalErrorType::OutOfMemory, "calling %s.CreateTexture(%s).",
                                   this, descriptor)) {
        result = TextureBase::MakeError(GetDevice(), descriptor);
    }
    return ReturnToAPI(std::move(result));
}

Ref<SharedTextureMemoryContents> SharedTextureMemoryBase::CreateContents() {
    return AcquireRef(new SharedTextureMemoryContents(GetWeakRef(this)));
}

ResultOrError<Ref<TextureBase>> SharedTextureMemoryBase::CreateTexture(
    const TextureDescriptor* rawDescriptor) {
    DAWN_TRY(GetDevice()->ValidateIsAlive());
    DAWN_TRY(GetDevice()->ValidateObject(this));

    UnpackedPtr<TextureDescriptor> descriptor;
    DAWN_TRY_ASSIGN(descriptor, ValidateAndUnpack(rawDescriptor));

    // Validate that there is one 2D, single-sampled subresource
    DAWN_INVALID_IF(descriptor->dimension != wgpu::TextureDimension::e2D,
                    "Texture dimension (%s) is not %s.", descriptor->dimension,
                    wgpu::TextureDimension::e2D);
    DAWN_INVALID_IF(descriptor->mipLevelCount != 1, "Mip level count (%u) is not 1.",
                    descriptor->mipLevelCount);
    DAWN_INVALID_IF(descriptor->size.depthOrArrayLayers != 1, "Array layer count (%u) is not 1.",
                    descriptor->size.depthOrArrayLayers);
    DAWN_INVALID_IF(descriptor->sampleCount != 1, "Sample count (%u) is not 1.",
                    descriptor->sampleCount);

    // Validate that the texture size exactly matches the shared texture memory's size.
    DAWN_INVALID_IF(
        (descriptor->size.width != mProperties.size.width) ||
            (descriptor->size.height != mProperties.size.height) ||
            (descriptor->size.depthOrArrayLayers != mProperties.size.depthOrArrayLayers),
        "SharedTextureMemory size (%s) doesn't match descriptor size (%s).", &mProperties.size,
        &descriptor->size);

    // Validate that the texture format exactly matches the shared texture memory's format.
    DAWN_INVALID_IF(descriptor->format != mProperties.format,
                    "SharedTextureMemory format (%s) doesn't match descriptor format (%s).",
                    mProperties.format, descriptor->format);

    // Validate the texture descriptor, and require its usage to be a subset of the shared texture
    // memory's usage.
    DAWN_TRY(ValidateTextureDescriptor(GetDevice(), descriptor, AllowMultiPlanarTextureFormat::Yes,
                                       mProperties.usage));

    Ref<TextureBase> texture;
    DAWN_TRY_ASSIGN(texture, CreateTextureImpl(descriptor));
    // Access is started on memory.BeginAccess.
    texture->SetHasAccess(false);
    return texture;
}

SharedTextureMemoryContents* SharedTextureMemoryBase::GetContents() const {
    return mContents.Get();
}

MaybeError SharedTextureMemoryBase::ValidateTextureCreatedFromSelf(TextureBase* texture) {
    auto* contents = texture->GetSharedTextureMemoryContents();
    DAWN_INVALID_IF(contents == nullptr, "%s was not created from %s.", texture, this);

    auto* sharedTextureMemory =
        texture->GetSharedTextureMemoryContents()->GetSharedTextureMemory().Promote().Get();
    DAWN_INVALID_IF(sharedTextureMemory != this, "%s created from %s cannot be used with %s.",
                    texture, sharedTextureMemory, this);
    return {};
}

bool SharedTextureMemoryBase::APIBeginAccess(TextureBase* texture,
                                             const BeginAccessDescriptor* descriptor) {
    if (GetDevice()->ConsumedError(BeginAccess(texture, descriptor), "calling %s.BeginAccess(%s).",
                                   this, texture)) {
        return false;
    }
    return true;
}

bool SharedTextureMemoryBase::APIIsDeviceLost() {
    return GetDevice()->IsLost();
}

MaybeError SharedTextureMemoryBase::BeginAccess(TextureBase* texture,
                                                const BeginAccessDescriptor* rawDescriptor) {
    DAWN_TRY(GetDevice()->ValidateIsAlive());
    DAWN_TRY(GetDevice()->ValidateObject(texture));

    UnpackedPtr<BeginAccessDescriptor> descriptor;
    DAWN_TRY_ASSIGN(descriptor, ValidateAndUnpack(rawDescriptor));

    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        DAWN_TRY(GetDevice()->ValidateObject(descriptor->fences[i]));
    }

    DAWN_TRY(ValidateTextureCreatedFromSelf(texture));

    DAWN_INVALID_IF(texture->GetFormat().IsMultiPlanar() && !descriptor->initialized,
                    "%s with multiplanar format (%s) must be initialized.", texture,
                    texture->GetFormat().format);

    DAWN_INVALID_IF(texture->IsDestroyed(), "%s has been destroyed.", texture);
    DAWN_INVALID_IF(texture->HasAccess(), "%s is already used to access %s.", texture, this);

    DAWN_INVALID_IF(mHasWriteAccess, "%s is currently accessed for writing.", this);
    DAWN_INVALID_IF(mHasExclusiveReadAccess, "%s is currently accessed for exclusive reading.",
                    this);

    if (texture->IsReadOnly()) {
        if (descriptor->concurrentRead) {
            DAWN_INVALID_IF(!descriptor->initialized, "Concurrent reading an uninitialized %s.",
                            texture);
            ++mReadAccessCount;
        } else {
            DAWN_INVALID_IF(
                mReadAccessCount != 0,
                "Exclusive read access used while %s is currently accessed for reading.", this);
            mHasExclusiveReadAccess = true;
        }
    } else {
        DAWN_INVALID_IF(descriptor->concurrentRead, "Concurrent reading read-write %s.", texture);
        DAWN_INVALID_IF(mReadAccessCount != 0,
                        "Read-Write access used while %s is currently accessed for reading.", this);
        mHasWriteAccess = true;
    }

    DAWN_TRY(BeginAccessImpl(texture, descriptor));

    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        mContents->mPendingFences->push_back(
            {descriptor->fences[i], descriptor->signaledValues[i]});
    }

    DAWN_ASSERT(!texture->IsError());
    texture->SetHasAccess(true);
    texture->SetIsSubresourceContentInitialized(descriptor->initialized,
                                                texture->GetAllSubresources());
    return {};
}

bool SharedTextureMemoryBase::APIEndAccess(TextureBase* texture, EndAccessState* state) {
    bool didEnd = false;
    DAWN_UNUSED(GetDevice()->ConsumedError(EndAccess(texture, state, &didEnd),
                                           "calling %s.EndAccess(%s).", this, texture));
    return didEnd;
}

MaybeError SharedTextureMemoryBase::EndAccess(TextureBase* texture,
                                              EndAccessState* state,
                                              bool* didEnd) {
    DAWN_TRY(GetDevice()->ValidateObject(texture));
    DAWN_TRY(ValidateTextureCreatedFromSelf(texture));

    DAWN_INVALID_IF(!texture->HasAccess(), "%s is not currently being accessed.", texture);

    if (texture->IsReadOnly()) {
        DAWN_ASSERT(!mHasWriteAccess);
        if (mHasExclusiveReadAccess) {
            DAWN_ASSERT(mReadAccessCount == 0);
            mHasExclusiveReadAccess = false;
        } else {
            DAWN_ASSERT(!mHasExclusiveReadAccess);
            --mReadAccessCount;
        }
    } else {
        DAWN_ASSERT(mHasWriteAccess);
        DAWN_ASSERT(!mHasExclusiveReadAccess);
        DAWN_ASSERT(mReadAccessCount == 0);
        mHasWriteAccess = false;
    }

    PendingFenceList fenceList;
    mContents->AcquirePendingFences(&fenceList);

    DAWN_ASSERT(!texture->IsError());
    texture->SetHasAccess(false);

    *didEnd = true;

    // Call the error-generating part of the EndAccess implementation. This is separated out because
    // writing the output state must happen regardless of whether or not EndAccessInternal
    // succeeds.
    MaybeError err;
    {
        ResultOrError<FenceAndSignalValue> result = EndAccessInternal(texture, state);
        if (result.IsSuccess()) {
            fenceList->push_back(result.AcquireSuccess());
        } else {
            err = result.AcquireError();
        }
    }

    // Copy the fences to the output state.
    if (size_t fenceCount = fenceList->size()) {
        auto* fences = new SharedFenceBase*[fenceCount];
        uint64_t* signaledValues = new uint64_t[fenceCount];
        for (size_t i = 0; i < fenceCount; ++i) {
            fences[i] = ReturnToAPI(std::move(fenceList[i].object));
            signaledValues[i] = fenceList[i].signaledValue;
        }

        state->fenceCount = fenceCount;
        state->fences = fences;
        state->signaledValues = signaledValues;
    } else {
        state->fenceCount = 0;
        state->fences = nullptr;
        state->signaledValues = nullptr;
    }
    state->initialized = texture->IsSubresourceContentInitialized(texture->GetAllSubresources());
    return err;
}

ResultOrError<FenceAndSignalValue> SharedTextureMemoryBase::EndAccessInternal(
    TextureBase* texture,
    EndAccessState* rawState) {
    UnpackedPtr<EndAccessState> state;
    DAWN_TRY_ASSIGN(state, ValidateAndUnpack(rawState));
    return EndAccessImpl(texture, state);
}

// SharedTextureMemoryContents

SharedTextureMemoryContents::SharedTextureMemoryContents(
    WeakRef<SharedTextureMemoryBase> sharedTextureMemory)
    : mSharedTextureMemory(std::move(sharedTextureMemory)) {}

const WeakRef<SharedTextureMemoryBase>& SharedTextureMemoryContents::GetSharedTextureMemory()
    const {
    return mSharedTextureMemory;
}

void SharedTextureMemoryContents::AcquirePendingFences(PendingFenceList* fences) {
    *fences = mPendingFences;
    mPendingFences->clear();
}

void SharedTextureMemoryContents::SetLastUsageSerial(ExecutionSerial lastUsageSerial) {
    mLastUsageSerial = lastUsageSerial;
}

ExecutionSerial SharedTextureMemoryContents::GetLastUsageSerial() const {
    return mLastUsageSerial;
}

void APISharedTextureMemoryEndAccessStateFreeMembers(WGPUSharedTextureMemoryEndAccessState cState) {
    auto* state = reinterpret_cast<SharedTextureMemoryBase::EndAccessState*>(&cState);
    for (size_t i = 0; i < state->fenceCount; ++i) {
        state->fences[i]->APIRelease();
    }
    delete[] state->fences;
    delete[] state->signaledValues;
}

}  // namespace dawn::native
