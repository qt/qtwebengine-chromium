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

#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"

#include <string>
#include <utility>

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/Forward.h"
#include "dawn/native/d3d11/PhysicalDeviceD3D11.h"
#include "dawn/native/d3d11/PipelineLayoutD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

ScopedCommandRecordingContext::ScopedCommandRecordingContext(
    CommandRecordingContext* commandContext)
    : mCommandContext(commandContext), mD3D11Multithread(mCommandContext->mD3D11Multithread) {
    DAWN_ASSERT(!mCommandContext->mScopedAccessed);
    mCommandContext->mScopedAccessed = true;

    if (mD3D11Multithread) {
        mD3D11Multithread->Enter();
    }
}

ScopedCommandRecordingContext::~ScopedCommandRecordingContext() {
    DAWN_ASSERT(mCommandContext->mScopedAccessed);
    mCommandContext->mScopedAccessed = false;

    if (mD3D11Multithread) {
        mD3D11Multithread->Leave();
    }
}

Device* ScopedCommandRecordingContext::GetDevice() const {
    return mCommandContext->mDevice.Get();
}

void ScopedCommandRecordingContext::UpdateSubresource(ID3D11Resource* pDstResource,
                                                      UINT DstSubresource,
                                                      const D3D11_BOX* pDstBox,
                                                      const void* pSrcData,
                                                      UINT SrcRowPitch,
                                                      UINT SrcDepthPitch) const {
    mCommandContext->mD3D11DeviceContext4->UpdateSubresource(pDstResource, DstSubresource, pDstBox,
                                                             pSrcData, SrcRowPitch, SrcDepthPitch);
}

void ScopedCommandRecordingContext::CopyResource(ID3D11Resource* pDstResource,
                                                 ID3D11Resource* pSrcResource) const {
    mCommandContext->mD3D11DeviceContext4->CopyResource(pDstResource, pSrcResource);
}

void ScopedCommandRecordingContext::CopySubresourceRegion(ID3D11Resource* pDstResource,
                                                          UINT DstSubresource,
                                                          UINT DstX,
                                                          UINT DstY,
                                                          UINT DstZ,
                                                          ID3D11Resource* pSrcResource,
                                                          UINT SrcSubresource,
                                                          const D3D11_BOX* pSrcBox) const {
    mCommandContext->mD3D11DeviceContext4->CopySubresourceRegion(
        pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}

void ScopedCommandRecordingContext::ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView,
                                                          const FLOAT ColorRGBA[4]) const {
    mCommandContext->mD3D11DeviceContext4->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}

void ScopedCommandRecordingContext::ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView,
                                                          UINT ClearFlags,
                                                          FLOAT Depth,
                                                          UINT8 Stencil) const {
    mCommandContext->mD3D11DeviceContext4->ClearDepthStencilView(pDepthStencilView, ClearFlags,
                                                                 Depth, Stencil);
}

HRESULT ScopedCommandRecordingContext::Map(ID3D11Resource* pResource,
                                           UINT Subresource,
                                           D3D11_MAP MapType,
                                           UINT MapFlags,
                                           D3D11_MAPPED_SUBRESOURCE* pMappedResource) const {
    return mCommandContext->mD3D11DeviceContext4->Map(pResource, Subresource, MapType, MapFlags,
                                                      pMappedResource);
}

void ScopedCommandRecordingContext::Unmap(ID3D11Resource* pResource, UINT Subresource) const {
    mCommandContext->mD3D11DeviceContext4->Unmap(pResource, Subresource);
}

HRESULT ScopedCommandRecordingContext::Signal(ID3D11Fence* pFence, UINT64 Value) const {
    return mCommandContext->mD3D11DeviceContext4->Signal(pFence, Value);
}

HRESULT ScopedCommandRecordingContext::Wait(ID3D11Fence* pFence, UINT64 Value) const {
    return mCommandContext->mD3D11DeviceContext4->Wait(pFence, Value);
}

void ScopedCommandRecordingContext::WriteUniformBuffer(uint32_t offset, uint32_t element) const {
    DAWN_ASSERT(offset < CommandRecordingContext::kMaxNumBuiltinElements);
    if (mCommandContext->mUniformBufferData[offset] != element) {
        mCommandContext->mUniformBufferData[offset] = element;
        mCommandContext->mUniformBufferDirty = true;
    }
}

MaybeError ScopedCommandRecordingContext::FlushUniformBuffer() const {
    if (mCommandContext->mUniformBufferDirty) {
        DAWN_TRY(mCommandContext->mUniformBuffer->Write(
            this, 0, mCommandContext->mUniformBufferData.data(),
            mCommandContext->mUniformBufferData.size() * sizeof(uint32_t)));
        mCommandContext->mUniformBufferDirty = false;
    }
    return {};
}

ScopedSwapStateCommandRecordingContext::ScopedSwapStateCommandRecordingContext(
    CommandRecordingContext* commandContext)
    : ScopedCommandRecordingContext(commandContext),
      mSwapContextState(
          ToBackend(mCommandContext->mDevice->GetPhysicalDevice())->IsSharedD3D11Device()) {
    if (mSwapContextState) {
        mCommandContext->mD3D11DeviceContext4->SwapDeviceContextState(
            mCommandContext->mD3D11DeviceContextState.Get(), &mPreviousState);
    }
}

ScopedSwapStateCommandRecordingContext::~ScopedSwapStateCommandRecordingContext() {
    if (mSwapContextState) {
        mCommandContext->mD3D11DeviceContext4->SwapDeviceContextState(mPreviousState.Get(),
                                                                      nullptr);
    }
}

ID3D11Device* ScopedSwapStateCommandRecordingContext::GetD3D11Device() const {
    return mCommandContext->mD3D11Device.Get();
}

ID3D11DeviceContext4* ScopedSwapStateCommandRecordingContext::GetD3D11DeviceContext4() const {
    return mCommandContext->mD3D11DeviceContext4.Get();
}

ID3DUserDefinedAnnotation* ScopedSwapStateCommandRecordingContext::GetD3DUserDefinedAnnotation()
    const {
    return mCommandContext->mD3DUserDefinedAnnotation.Get();
}

Buffer* ScopedSwapStateCommandRecordingContext::GetUniformBuffer() const {
    return mCommandContext->mUniformBuffer.Get();
}

MaybeError CommandRecordingContext::Initialize(Device* device) {
    DAWN_ASSERT(!IsOpen());
    DAWN_ASSERT(device);
    mDevice = device;
    mNeedsSubmit = false;

    ID3D11Device5* d3d11Device = device->GetD3D11Device5();

    if (ToBackend(device->GetPhysicalDevice())->IsSharedD3D11Device()) {
        const D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        DAWN_TRY(CheckHRESULT(
            d3d11Device->CreateDeviceContextState(
                /*Flags=*/0, featureLevels, std::size(featureLevels), D3D11_SDK_VERSION,
                __uuidof(ID3D11Device5), nullptr, &mD3D11DeviceContextState),
            "D3D11: create device context state"));
    }

    ComPtr<ID3D11DeviceContext> d3d11DeviceContext;
    device->GetD3D11Device()->GetImmediateContext(&d3d11DeviceContext);

    ComPtr<ID3D11DeviceContext4> d3d11DeviceContext4;
    DAWN_TRY(CheckHRESULT(d3d11DeviceContext.As(&d3d11DeviceContext4),
                          "D3D11 querying immediate context for ID3D11DeviceContext4 interface"));

    DAWN_TRY(
        CheckHRESULT(d3d11DeviceContext4.As(&mD3DUserDefinedAnnotation),
                     "D3D11 querying immediate context for ID3DUserDefinedAnnotation interface"));

    if (device->HasFeature(Feature::D3D11MultithreadProtected)) {
        DAWN_TRY(CheckHRESULT(d3d11DeviceContext.As(&mD3D11Multithread),
                              "D3D11 querying immediate context for ID3D11Multithread interface"));
        mD3D11Multithread->SetMultithreadProtected(TRUE);
    }

    mD3D11Device = d3d11Device;
    mD3D11DeviceContext4 = std::move(d3d11DeviceContext4);
    mIsOpen = true;

    // Create a uniform buffer for built in variables.
    BufferDescriptor descriptor;
    descriptor.size = sizeof(uint32_t) * kMaxNumBuiltinElements;
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    descriptor.mappedAtCreation = false;
    descriptor.label = "builtin uniform buffer";

    Ref<BufferBase> uniformBuffer;
    {
        // Lock the device to protect the clearing of the built-in uniform buffer.
        auto deviceLock(device->GetScopedLock());
        DAWN_TRY_ASSIGN(uniformBuffer, device->CreateBuffer(&descriptor));
    }
    mUniformBuffer = ToBackend(std::move(uniformBuffer));

    // Always bind the uniform buffer to the reserved slot for all pipelines.
    // This buffer will be updated with the correct values before each draw or dispatch call.
    ID3D11Buffer* bufferPtr = mUniformBuffer->GetD3D11ConstantBuffer();
    mD3D11DeviceContext4->VSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                               &bufferPtr);
    mD3D11DeviceContext4->CSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                               &bufferPtr);

    return {};
}

MaybeError CommandRecordingContext::ExecuteCommandList() {
    // Consider using deferred DeviceContext.
    mNeedsSubmit = false;
    return {};
}

void CommandRecordingContext::Release() {
    if (mIsOpen) {
        DAWN_ASSERT(mDevice->IsLockedByCurrentThreadIfNeeded());
        mIsOpen = false;
        mNeedsSubmit = false;
        mUniformBuffer = nullptr;
        mDevice = nullptr;
        ID3D11Buffer* nullBuffer = nullptr;
        mD3D11DeviceContext4->VSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &nullBuffer);
        mD3D11DeviceContext4->CSSetConstantBuffers(PipelineLayout::kReservedConstantBufferSlot, 1,
                                                   &nullBuffer);
        mD3D11DeviceContextState = nullptr;
        mD3D11DeviceContext4 = nullptr;
        mD3D11Device = nullptr;
    }
}

}  // namespace dawn::native::d3d11
