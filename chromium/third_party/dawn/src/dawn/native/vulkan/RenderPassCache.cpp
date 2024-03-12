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

#include "dawn/native/vulkan/RenderPassCache.h"

#include "dawn/common/BitSetIterator.h"
#include "dawn/common/Enumerator.h"
#include "dawn/common/HashUtils.h"
#include "dawn/common/Range.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/TextureVk.h"
#include "dawn/native/vulkan/VulkanError.h"

namespace dawn::native::vulkan {

namespace {
VkAttachmentLoadOp VulkanAttachmentLoadOp(wgpu::LoadOp op) {
    switch (op) {
        case wgpu::LoadOp::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case wgpu::LoadOp::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case wgpu::LoadOp::Undefined:
            DAWN_UNREACHABLE();
            break;
    }
    DAWN_UNREACHABLE();
}

VkAttachmentStoreOp VulkanAttachmentStoreOp(wgpu::StoreOp op) {
    // TODO(crbug.com/dawn/485): return STORE_OP_STORE_NONE_QCOM if the device has required
    // extension.
    switch (op) {
        case wgpu::StoreOp::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case wgpu::StoreOp::Discard:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case wgpu::StoreOp::Undefined:
            DAWN_UNREACHABLE();
            break;
    }
    DAWN_UNREACHABLE();
}
}  // anonymous namespace

// RenderPassCacheQuery

void RenderPassCacheQuery::SetColor(ColorAttachmentIndex index,
                                    wgpu::TextureFormat format,
                                    wgpu::LoadOp loadOp,
                                    wgpu::StoreOp storeOp,
                                    bool hasResolveTarget) {
    colorMask.set(index);
    colorFormats[index] = format;
    colorLoadOp[index] = loadOp;
    colorStoreOp[index] = storeOp;
    resolveTargetMask[index] = hasResolveTarget;
}

void RenderPassCacheQuery::SetDepthStencil(wgpu::TextureFormat format,
                                           wgpu::LoadOp depthLoadOpIn,
                                           wgpu::StoreOp depthStoreOpIn,
                                           bool depthReadOnlyIn,
                                           wgpu::LoadOp stencilLoadOpIn,
                                           wgpu::StoreOp stencilStoreOpIn,
                                           bool stencilReadOnlyIn) {
    hasDepthStencil = true;
    depthStencilFormat = format;
    depthLoadOp = depthLoadOpIn;
    depthStoreOp = depthStoreOpIn;
    depthReadOnly = depthReadOnlyIn;
    stencilLoadOp = stencilLoadOpIn;
    stencilStoreOp = stencilStoreOpIn;
    stencilReadOnly = stencilReadOnlyIn;
}

void RenderPassCacheQuery::SetSampleCount(uint32_t sampleCountIn) {
    sampleCount = sampleCountIn;
}

// RenderPassCache

RenderPassCache::RenderPassCache(Device* device) : mDevice(device) {}

RenderPassCache::~RenderPassCache() {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto [_, renderPass] : mCache) {
        mDevice->fn.DestroyRenderPass(mDevice->GetVkDevice(), renderPass, nullptr);
    }

    mCache.clear();
}

ResultOrError<VkRenderPass> RenderPassCache::GetRenderPass(const RenderPassCacheQuery& query) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mCache.find(query);
    if (it != mCache.end()) {
        return VkRenderPass(it->second);
    }

    VkRenderPass renderPass;
    DAWN_TRY_ASSIGN(renderPass, CreateRenderPassForQuery(query));
    mCache.emplace(query, renderPass);
    return renderPass;
}

ResultOrError<VkRenderPass> RenderPassCache::CreateRenderPassForQuery(
    const RenderPassCacheQuery& query) const {
    // The Vulkan subpasses want to know the layout of the attachments with VkAttachmentRef.
    // Precompute them as they must be pointer-chained in VkSubpassDescription.
    // Note that both colorAttachmentRefs and resolveAttachmentRefs can be sparse with holes
    // filled with VK_ATTACHMENT_UNUSED.
    PerColorAttachment<VkAttachmentReference> colorAttachmentRefs;
    PerColorAttachment<VkAttachmentReference> resolveAttachmentRefs;
    VkAttachmentReference depthStencilAttachmentRef;

    for (auto i : Range(kMaxColorAttachmentsTyped)) {
        colorAttachmentRefs[i].attachment = VK_ATTACHMENT_UNUSED;
        resolveAttachmentRefs[i].attachment = VK_ATTACHMENT_UNUSED;
        // The Khronos Vulkan validation layer will complain if not set
        colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        resolveAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Contains the attachment description that will be chained in the create info
    // The order of all attachments in attachmentDescs is "color-depthstencil-resolve".
    constexpr uint8_t kMaxAttachmentCount = kMaxColorAttachments * 2 + 1;
    std::array<VkAttachmentDescription, kMaxAttachmentCount> attachmentDescs = {};

    VkSampleCountFlagBits vkSampleCount = VulkanSampleCount(query.sampleCount);

    uint32_t attachmentCount = 0;
    ColorAttachmentIndex highestColorAttachmentIndexPlusOne(static_cast<uint8_t>(0));
    for (auto i : IterateBitSet(query.colorMask)) {
        auto& attachmentRef = colorAttachmentRefs[i];
        auto& attachmentDesc = attachmentDescs[attachmentCount];

        attachmentRef.attachment = attachmentCount;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachmentDesc.flags = 0;
        attachmentDesc.format = VulkanImageFormat(mDevice, query.colorFormats[i]);
        attachmentDesc.samples = vkSampleCount;
        attachmentDesc.loadOp = VulkanAttachmentLoadOp(query.colorLoadOp[i]);
        attachmentDesc.storeOp = VulkanAttachmentStoreOp(query.colorStoreOp[i]);
        attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachmentCount++;
        highestColorAttachmentIndexPlusOne =
            ColorAttachmentIndex(static_cast<uint8_t>(static_cast<uint8_t>(i) + 1u));
    }

    VkAttachmentReference* depthStencilAttachment = nullptr;
    if (query.hasDepthStencil) {
        const Format& dsFormat = mDevice->GetValidInternalFormat(query.depthStencilFormat);

        depthStencilAttachment = &depthStencilAttachmentRef;
        depthStencilAttachmentRef.attachment = attachmentCount;
        depthStencilAttachmentRef.layout = VulkanImageLayoutForDepthStencilAttachment(
            dsFormat, query.depthReadOnly, query.stencilReadOnly);

        // Build the attachment descriptor.
        auto& attachmentDesc = attachmentDescs[attachmentCount];
        attachmentDesc.flags = 0;
        attachmentDesc.format = VulkanImageFormat(mDevice, dsFormat.format);
        attachmentDesc.samples = vkSampleCount;

        attachmentDesc.loadOp = VulkanAttachmentLoadOp(query.depthLoadOp);
        attachmentDesc.storeOp = VulkanAttachmentStoreOp(query.depthStoreOp);
        attachmentDesc.stencilLoadOp = VulkanAttachmentLoadOp(query.stencilLoadOp);
        attachmentDesc.stencilStoreOp = VulkanAttachmentStoreOp(query.stencilStoreOp);

        // There is only one subpass, so it is safe to set both initialLayout and finalLayout to
        // the only subpass's layout.
        attachmentDesc.initialLayout = depthStencilAttachmentRef.layout;
        attachmentDesc.finalLayout = depthStencilAttachmentRef.layout;

        attachmentCount++;
    }

    uint32_t resolveAttachmentCount = 0;
    for (auto i : IterateBitSet(query.resolveTargetMask)) {
        auto& attachmentRef = resolveAttachmentRefs[i];
        auto& attachmentDesc = attachmentDescs[attachmentCount];

        attachmentRef.attachment = attachmentCount;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachmentDesc.flags = 0;
        attachmentDesc.format = VulkanImageFormat(mDevice, query.colorFormats[i]);
        attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachmentCount++;
        resolveAttachmentCount++;
    }

    // Create the VkSubpassDescription that will be chained in the VkRenderPassCreateInfo
    VkSubpassDescription subpassDesc;
    subpassDesc.flags = 0;
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.inputAttachmentCount = 0;
    subpassDesc.pInputAttachments = nullptr;
    subpassDesc.colorAttachmentCount = static_cast<uint8_t>(highestColorAttachmentIndexPlusOne);
    subpassDesc.pColorAttachments = colorAttachmentRefs.data();

    // Qualcomm GPUs have a driver bug on some devices where passing a zero-length array to the
    // resolveAttachments causes a VK_ERROR_OUT_OF_HOST_MEMORY. nullptr must be passed instead.
    if (resolveAttachmentCount) {
        subpassDesc.pResolveAttachments = resolveAttachmentRefs.data();
    } else {
        subpassDesc.pResolveAttachments = nullptr;
    }

    subpassDesc.pDepthStencilAttachment = depthStencilAttachment;
    subpassDesc.preserveAttachmentCount = 0;
    subpassDesc.pPreserveAttachments = nullptr;

    // Chain everything in VkRenderPassCreateInfo
    VkRenderPassCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.attachmentCount = attachmentCount;
    createInfo.pAttachments = attachmentDescs.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDesc;
    createInfo.dependencyCount = 0;
    createInfo.pDependencies = nullptr;

    // Create the render pass from the zillion parameters
    VkRenderPass renderPass;
    DAWN_TRY(CheckVkSuccess(
        mDevice->fn.CreateRenderPass(mDevice->GetVkDevice(), &createInfo, nullptr, &*renderPass),
        "CreateRenderPass"));
    return renderPass;
}

// RenderPassCache

// If you change these, remember to also update StreamImplVk.cpp

size_t RenderPassCache::CacheFuncs::operator()(const RenderPassCacheQuery& query) const {
    size_t hash = Hash(query.colorMask);

    HashCombine(&hash, Hash(query.resolveTargetMask));

    for (auto i : IterateBitSet(query.colorMask)) {
        HashCombine(&hash, query.colorFormats[i], query.colorLoadOp[i], query.colorStoreOp[i]);
    }

    HashCombine(&hash, query.hasDepthStencil);
    if (query.hasDepthStencil) {
        HashCombine(&hash, query.depthStencilFormat, query.depthLoadOp, query.depthStoreOp,
                    query.depthReadOnly, query.stencilLoadOp, query.stencilStoreOp,
                    query.stencilReadOnly);
    }

    HashCombine(&hash, query.sampleCount);

    return hash;
}

bool RenderPassCache::CacheFuncs::operator()(const RenderPassCacheQuery& a,
                                             const RenderPassCacheQuery& b) const {
    if (a.colorMask != b.colorMask) {
        return false;
    }

    if (a.resolveTargetMask != b.resolveTargetMask) {
        return false;
    }

    if (a.sampleCount != b.sampleCount) {
        return false;
    }

    for (auto i : IterateBitSet(a.colorMask)) {
        if ((a.colorFormats[i] != b.colorFormats[i]) || (a.colorLoadOp[i] != b.colorLoadOp[i]) ||
            (a.colorStoreOp[i] != b.colorStoreOp[i])) {
            return false;
        }
    }

    if (a.hasDepthStencil != b.hasDepthStencil) {
        return false;
    }

    if (a.hasDepthStencil) {
        if ((a.depthStencilFormat != b.depthStencilFormat) || (a.depthLoadOp != b.depthLoadOp) ||
            (a.stencilLoadOp != b.stencilLoadOp) || (a.depthStoreOp != b.depthStoreOp) ||
            (a.depthReadOnly != b.depthReadOnly) || (a.stencilStoreOp != b.stencilStoreOp) ||
            (a.stencilReadOnly != b.stencilReadOnly)) {
            return false;
        }
    }

    return true;
}
}  // namespace dawn::native::vulkan
