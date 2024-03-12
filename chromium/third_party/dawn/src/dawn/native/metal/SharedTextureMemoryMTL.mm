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

#include "dawn/native/metal/SharedTextureMemoryMTL.h"

#include <CoreVideo/CVPixelBuffer.h>

#include "dawn/native/ChainUtils.h"
#include "dawn/native/metal/CommandRecordingContext.h"
#include "dawn/native/metal/DeviceMTL.h"
#include "dawn/native/metal/QueueMTL.h"
#include "dawn/native/metal/SharedFenceMTL.h"
#include "dawn/native/metal/TextureMTL.h"

namespace dawn::native::metal {

namespace {
ResultOrError<wgpu::TextureFormat> GetFormatEquivalentToIOSurfaceFormat(uint32_t format) {
    switch (format) {
        case kCVPixelFormatType_64RGBAHalf:
            return wgpu::TextureFormat::RGBA16Float;
        case kCVPixelFormatType_TwoComponent16Half:
            return wgpu::TextureFormat::RG16Float;
        case kCVPixelFormatType_OneComponent16Half:
            return wgpu::TextureFormat::R16Float;
        case kCVPixelFormatType_ARGB2101010LEPacked:
            return wgpu::TextureFormat::RGB10A2Unorm;
        case kCVPixelFormatType_32RGBA:
            return wgpu::TextureFormat::RGBA8Unorm;
        case kCVPixelFormatType_32BGRA:
            return wgpu::TextureFormat::BGRA8Unorm;
        case kCVPixelFormatType_TwoComponent8:
            return wgpu::TextureFormat::RG8Unorm;
        case kCVPixelFormatType_OneComponent8:
            return wgpu::TextureFormat::R8Unorm;
        case kCVPixelFormatType_TwoComponent16:
            return wgpu::TextureFormat::RG16Unorm;
        case kCVPixelFormatType_OneComponent16:
            return wgpu::TextureFormat::R16Unorm;
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            return wgpu::TextureFormat::R8BG8Biplanar420Unorm;
        case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
            return wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm;
        case kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar:
            return wgpu::TextureFormat::R8BG8A8Triplanar420Unorm;
        default:
            return DAWN_VALIDATION_ERROR("Unsupported IOSurface format (%x).", format);
    }
}

}  // anonymous namespace

// static
ResultOrError<Ref<SharedTextureMemory>> SharedTextureMemory::Create(
    Device* device,
    const char* label,
    const SharedTextureMemoryIOSurfaceDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->ioSurface == nullptr, "IOSurface is missing.");

    IOSurfaceRef ioSurface = static_cast<IOSurfaceRef>(descriptor->ioSurface);
    wgpu::TextureFormat format;
    DAWN_TRY_ASSIGN(format,
                    GetFormatEquivalentToIOSurfaceFormat(IOSurfaceGetPixelFormat(ioSurface)));

    size_t width = IOSurfaceGetWidth(ioSurface);
    size_t height = IOSurfaceGetHeight(ioSurface);

    const CombinedLimits& limits = device->GetLimits();

    DAWN_INVALID_IF(width > limits.v1.maxTextureDimension2D,
                    "IOSurface width (%u) exceeds maxTextureDimension2D (%u).", width,
                    limits.v1.maxTextureDimension2D);
    DAWN_INVALID_IF(height > limits.v1.maxTextureDimension2D,
                    "IOSurface height (%u) exceeds maxTextureDimension2D (%u).", height,
                    limits.v1.maxTextureDimension2D);

    // IO surfaces support the following usages (the SharedTextureMemory frontend strips
    // out any usages that are not supported by `format`).
    const wgpu::TextureUsage kIOSurfaceSupportedUsages =
        wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::StorageBinding |
        wgpu::TextureUsage::RenderAttachment;

    SharedTextureMemoryProperties properties;
    properties.usage = kIOSurfaceSupportedUsages;
    properties.format = format;
    properties.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    auto result = AcquireRef(new SharedTextureMemory(device, label, properties, ioSurface));
    result->Initialize();
    return result;
}

SharedTextureMemory::SharedTextureMemory(Device* device,
                                         const char* label,
                                         const SharedTextureMemoryProperties& properties,
                                         IOSurfaceRef ioSurface)
    : SharedTextureMemoryBase(device, label, properties), mIOSurface(ioSurface) {}

void SharedTextureMemory::DestroyImpl() {
    SharedTextureMemoryBase::DestroyImpl();
    mIOSurface = nullptr;
}

IOSurfaceRef SharedTextureMemory::GetIOSurface() const {
    return mIOSurface.Get();
}

ResultOrError<Ref<TextureBase>> SharedTextureMemory::CreateTextureImpl(
    const UnpackedPtr<TextureDescriptor>& descriptor) {
    return Texture::CreateFromSharedTextureMemory(this, descriptor);
}

MaybeError SharedTextureMemory::BeginAccessImpl(
    TextureBase* texture,
    const UnpackedPtr<BeginAccessDescriptor>& descriptor) {
    DAWN_TRY(descriptor.ValidateSubset<>());
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        SharedFenceBase* fence = descriptor->fences[i];

        SharedFenceExportInfo exportInfo;
        DAWN_TRY(fence->ExportInfo(&exportInfo));
        switch (exportInfo.type) {
            case wgpu::SharedFenceType::MTLSharedEvent:
                DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceMTLSharedEvent),
                                "Required feature (%s) for %s is missing.",
                                wgpu::FeatureName::SharedFenceMTLSharedEvent,
                                wgpu::SharedFenceType::MTLSharedEvent);
                break;
            default:
                return DAWN_VALIDATION_ERROR("Unsupported fence type %s.", exportInfo.type);
        }
    }
    return {};
}

ResultOrError<FenceAndSignalValue> SharedTextureMemory::EndAccessImpl(
    TextureBase* texture,
    UnpackedPtr<EndAccessState>& state) {
    DAWN_TRY(state.ValidateSubset<>());
    DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceMTLSharedEvent),
                    "Required feature (%s) is missing.",
                    wgpu::FeatureName::SharedFenceMTLSharedEvent);

    if (@available(macOS 10.14, iOS 12.0, *)) {
        ExternalImageIOSurfaceEndAccessDescriptor oldEndAccessDesc;
        ToBackend(GetDevice()->GetQueue())->ExportLastSignaledEvent(&oldEndAccessDesc);

        SharedFenceMTLSharedEventDescriptor newDesc;
        newDesc.sharedEvent = oldEndAccessDesc.sharedEvent;

        Ref<SharedFence> fence;
        DAWN_TRY_ASSIGN(fence, SharedFence::Create(ToBackend(GetDevice()),
                                                   "Internal MTLSharedEvent", &newDesc));

        return FenceAndSignalValue{
            std::move(fence),
            static_cast<uint64_t>(texture->GetSharedTextureMemoryContents()->GetLastUsageSerial())};
    }
    DAWN_UNREACHABLE();
}

}  // namespace dawn::native::metal
