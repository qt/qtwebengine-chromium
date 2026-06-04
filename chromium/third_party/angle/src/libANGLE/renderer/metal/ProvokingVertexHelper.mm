//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProvokingVertexHelper.mm:
//    Implements the class methods for ProvokingVertexHelper.
//

#include "libANGLE/renderer/metal/ProvokingVertexHelper.h"
#import <Foundation/Foundation.h>
#include "common/base/anglebase/numerics/checked_math.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/shaders/rewrite_indices_shared.h"

namespace rx
{

namespace
{
constexpr size_t kInitialIndexBufferSize = 0xFFFF;  // Initial 64k pool.
}
static inline uint32_t primCountForIndexCount(const uint fixIndexBufferKey,
                                              const GLsizei indexCount)
{
    const uint fixIndexBufferMode =
        (fixIndexBufferKey >> MtlFixIndexBufferKeyModeShift) & MtlFixIndexBufferKeyModeMask;

    switch (fixIndexBufferMode)
    {
        case MtlFixIndexBufferKeyPoints:
            return indexCount;
        case MtlFixIndexBufferKeyLines:
            return indexCount / 2;
        case MtlFixIndexBufferKeyLineStrip:
            // Prevent underflow with subtraction and avoid casting to a signed type
            return std::max(indexCount - 1, 0);
        case MtlFixIndexBufferKeyLineLoop:
            return indexCount;
        case MtlFixIndexBufferKeyTriangles:
            return indexCount / 3;
        case MtlFixIndexBufferKeyTriangleStrip:
            // Prevent underflow with subtraction and avoid casting to a signed type
            return std::max(indexCount - 2, 0);
        case MtlFixIndexBufferKeyTriangleFan:
            // Prevent underflow with subtraction and avoid casting to a signed type
            return std::max(indexCount - 2, 0);
        default:
            ASSERT(false);
            return 0;
    }
}

static inline bool indexCountForPrimCount(const uint fixIndexBufferKey,
                                          const uint32_t primCount,
                                          uint32_t *outIndexCount)
{

    const uint fixIndexBufferMode =
        (fixIndexBufferKey >> MtlFixIndexBufferKeyModeShift) & MtlFixIndexBufferKeyModeMask;

    uint32_t indicesPerPrimitive = 0;
    switch (fixIndexBufferMode)
    {
        case MtlFixIndexBufferKeyPoints:
            indicesPerPrimitive = 1;
            break;
        case MtlFixIndexBufferKeyLines:
        case MtlFixIndexBufferKeyLineStrip:
        case MtlFixIndexBufferKeyLineLoop:
            indicesPerPrimitive = 2;
            break;
        case MtlFixIndexBufferKeyTriangles:
        case MtlFixIndexBufferKeyTriangleStrip:
        case MtlFixIndexBufferKeyTriangleFan:
            indicesPerPrimitive = 3;
            break;
        default:
            UNREACHABLE();
            return false;
    }

    angle::CheckedNumeric<uint32_t> indexCount(primCount);
    indexCount *= indicesPerPrimitive;

    return indexCount.AssignIfValid(outIndexCount);
}

static inline gl::PrimitiveMode getNewPrimitiveMode(const uint fixIndexBufferKey)
{
    const uint fixIndexBufferMode =
        (fixIndexBufferKey >> MtlFixIndexBufferKeyModeShift) & MtlFixIndexBufferKeyModeMask;
    switch (fixIndexBufferMode)
    {
        case MtlFixIndexBufferKeyPoints:
            return gl::PrimitiveMode::Points;
        case MtlFixIndexBufferKeyLines:
            return gl::PrimitiveMode::Lines;
        case MtlFixIndexBufferKeyLineStrip:
            return gl::PrimitiveMode::Lines;
        case MtlFixIndexBufferKeyLineLoop:
            return gl::PrimitiveMode::Lines;
        case MtlFixIndexBufferKeyTriangles:
            return gl::PrimitiveMode::Triangles;
        case MtlFixIndexBufferKeyTriangleStrip:
            return gl::PrimitiveMode::Triangles;
        case MtlFixIndexBufferKeyTriangleFan:
            return gl::PrimitiveMode::Triangles;
        default:
            ASSERT(false);
            return gl::PrimitiveMode::InvalidEnum;
    }
}
ProvokingVertexHelper::ProvokingVertexHelper(ContextMtl *context) : mIndexBuffers(false)
{
    mIndexBuffers.initialize(context, kInitialIndexBufferSize, mtl::kIndexBufferOffsetAlignment, 0);
}

void ProvokingVertexHelper::onDestroy(ContextMtl *context)
{
    mIndexBuffers.destroy(context);
}

void ProvokingVertexHelper::releaseInFlightBuffers(ContextMtl *contextMtl)
{
    mIndexBuffers.releaseInFlightBuffers(contextMtl);
}

static uint buildIndexBufferKey(const mtl::ProvokingVertexComputePipelineDesc &pipelineDesc)
{
    uint indexBufferKey              = 0;
    gl::DrawElementsType elementType = (gl::DrawElementsType)pipelineDesc.elementType;
    bool doPrimPrestart              = pipelineDesc.primitiveRestartEnabled;
    gl::PrimitiveMode primMode       = pipelineDesc.primitiveMode;
    switch (elementType)
    {
        case gl::DrawElementsType::UnsignedShort:
            indexBufferKey |= MtlFixIndexBufferKeyUint16 << MtlFixIndexBufferKeyInShift;
            indexBufferKey |= MtlFixIndexBufferKeyUint16 << MtlFixIndexBufferKeyOutShift;
            break;
        case gl::DrawElementsType::UnsignedInt:
            indexBufferKey |= MtlFixIndexBufferKeyUint32 << MtlFixIndexBufferKeyInShift;
            indexBufferKey |= MtlFixIndexBufferKeyUint32 << MtlFixIndexBufferKeyOutShift;
            break;
        default:
            ASSERT(false);  // Index type should only be short or int.
            break;
    }
    indexBufferKey |= (uint)primMode << MtlFixIndexBufferKeyModeShift;
    indexBufferKey |= doPrimPrestart ? MtlFixIndexBufferKeyPrimRestart : 0;
    // We only rewrite indices if we're switching the provoking vertex mode.
    indexBufferKey |= MtlFixIndexBufferKeyProvokingVertexLast;
    return indexBufferKey;
}

angle::Result ProvokingVertexHelper::getComputePipleineState(
    ContextMtl *context,
    const mtl::ProvokingVertexComputePipelineDesc &desc,
    mtl::AutoObjCPtr<id<MTLComputePipelineState>> *outComputePipeline)
{
    auto iter = mComputeFunctions.find(desc);
    if (iter != mComputeFunctions.end())
    {
        return context->getPipelineCache().getComputePipeline(context, iter->second,
                                                              outComputePipeline);
    }

    id<MTLLibrary> provokingVertexLibrary = context->getDisplay()->getDefaultShadersLib();
    uint indexBufferKey                   = buildIndexBufferKey(desc);
    auto fcValues = mtl::adoptObjCObj([[MTLFunctionConstantValues alloc] init]);
    [fcValues setConstantValue:&indexBufferKey type:MTLDataTypeUInt withName:@"fixIndexBufferKey"];

    mtl::AutoObjCPtr<id<MTLFunction>> computeShader;
    if (desc.generateIndices)
    {
        ANGLE_TRY(CreateMslShader(context, provokingVertexLibrary, @"genIndexBuffer",
                                  fcValues.get(), &computeShader));
    }
    else
    {
        ANGLE_TRY(CreateMslShader(context, provokingVertexLibrary, @"fixIndexBuffer",
                                  fcValues.get(), &computeShader));
    }
    mComputeFunctions[desc] = computeShader;

    return context->getPipelineCache().getComputePipeline(context, computeShader,
                                                          outComputePipeline);
}

angle::Result ProvokingVertexHelper::prepareCommandEncoderForDescriptor(
    ContextMtl *context,
    mtl::ComputeCommandEncoder *encoder,
    mtl::ProvokingVertexComputePipelineDesc desc)
{
    mtl::AutoObjCPtr<id<MTLComputePipelineState>> pipelineState;
    ANGLE_TRY(getComputePipleineState(context, desc, &pipelineState));

    encoder->setComputePipelineState(pipelineState);

    return angle::Result::Continue;
}

angle::Result ProvokingVertexHelper::preconditionIndexBuffer(ContextMtl *context,
                                                             mtl::BufferRef indexBuffer,
                                                             GLsizei glCount,
                                                             size_t indexOffset,
                                                             bool primitiveRestartEnabled,
                                                             gl::PrimitiveMode primitiveMode,
                                                             gl::DrawElementsType elementsType,
                                                             uint32_t &outIndexCount,
                                                             size_t &outIndexOffset,
                                                             gl::PrimitiveMode &outPrimitiveMode,
                                                             mtl::BufferRef &outNewBuffer)
{
    // Get specialized program
    // Upload index buffer
    // dispatch per-primitive?
    mtl::ProvokingVertexComputePipelineDesc pipelineDesc;
    pipelineDesc.elementType             = (uint8_t)elementsType;
    pipelineDesc.primitiveMode           = primitiveMode;
    pipelineDesc.primitiveRestartEnabled = primitiveRestartEnabled;
    pipelineDesc.generateIndices         = false;
    uint indexBufferKey                  = buildIndexBufferKey(pipelineDesc);
    uint32_t primCount     = primCountForIndexCount(indexBufferKey, glCount);
    uint32_t newIndexCount = 0;
    ANGLE_CHECK_GL_MATH(context, indexCountForPrimCount(indexBufferKey, primCount, &newIndexCount));
    size_t indexSize   = gl::GetDrawElementsTypeSize(elementsType);
    size_t newOffset   = 0;
    mtl::BufferRef newBuffer;

    angle::CheckedNumeric<size_t> checkedBufferSize(newIndexCount);
    checkedBufferSize *= indexSize;
    checkedBufferSize += indexOffset;

    ANGLE_CHECK_GL_MATH(context, checkedBufferSize.IsValid());
    ANGLE_TRY(mIndexBuffers.allocate(context, checkedBufferSize.ValueOrDie(), nullptr, &newBuffer,
                                     &newOffset));
    auto threadsPerThreadgroup = MTLSizeMake(MIN(primCount, 64u), 1, 1);

    mtl::ComputeCommandEncoder *encoder =
        context->getComputeCommandEncoderWithoutEndingRenderEncoder();
    ANGLE_TRY(prepareCommandEncoderForDescriptor(context, encoder, pipelineDesc));
    encoder->setBuffer(indexBuffer, static_cast<uint32_t>(indexOffset), 0);
    encoder->setBufferForWrite(
        newBuffer, static_cast<uint32_t>(indexOffset) + static_cast<uint32_t>(newOffset), 1);
    encoder->setData(static_cast<uint>(glCount), 2);
    encoder->setData(primCount, 3);
    encoder->dispatch(
        MTLSizeMake((static_cast<NSUInteger>(primCount) + threadsPerThreadgroup.width - 1) /
                        threadsPerThreadgroup.width,
                    1, 1),
        threadsPerThreadgroup);
    outIndexCount    = static_cast<uint32_t>(newIndexCount);
    outIndexOffset   = newOffset;
    outPrimitiveMode = getNewPrimitiveMode(indexBufferKey);
    outNewBuffer     = newBuffer;
    return angle::Result::Continue;
}

angle::Result ProvokingVertexHelper::generateIndexBuffer(ContextMtl *context,
                                                         size_t first,
                                                         GLsizei glCount,
                                                         gl::PrimitiveMode primitiveMode,
                                                         gl::DrawElementsType elementsType,
                                                         uint32_t &outIndexCount,
                                                         size_t &outIndexOffset,
                                                         gl::PrimitiveMode &outPrimitiveMode,
                                                         mtl::BufferRef &outNewBuffer)
{
    // Get specialized program
    // Upload index buffer
    // dispatch per-primitive?
    mtl::ProvokingVertexComputePipelineDesc pipelineDesc;
    pipelineDesc.elementType             = (uint8_t)elementsType;
    pipelineDesc.primitiveMode           = primitiveMode;
    pipelineDesc.primitiveRestartEnabled = false;
    pipelineDesc.generateIndices         = true;
    uint indexBufferKey                  = buildIndexBufferKey(pipelineDesc);
    uint32_t primCount  = primCountForIndexCount(indexBufferKey, glCount);

    uint32_t newIndexCount = 0;
    ANGLE_CHECK_GL_MATH(context, indexCountForPrimCount(indexBufferKey, primCount, &newIndexCount));

    size_t indexSize      = gl::GetDrawElementsTypeSize(elementsType);
    size_t newIndexOffset = 0;
    mtl::BufferRef newBuffer;

    angle::CheckedNumeric<size_t> checkedBufferSize = newIndexCount;
    checkedBufferSize *= indexSize;

    ANGLE_CHECK_GL_MATH(context, checkedBufferSize.IsValid());
    ANGLE_TRY(mIndexBuffers.allocate(context, checkedBufferSize.ValueOrDie(), nullptr, &newBuffer,
                                     &newIndexOffset));
    uint firstVertexEncoded    = static_cast<uint>(first);
    uint indexOffsetEncoded    = static_cast<uint>(newIndexOffset);
    auto threadsPerThreadgroup = MTLSizeMake(MIN(primCount, 64u), 1, 1);

    mtl::ComputeCommandEncoder *encoder =
        context->getComputeCommandEncoderWithoutEndingRenderEncoder();
    ANGLE_TRY(prepareCommandEncoderForDescriptor(context, encoder, pipelineDesc));
    encoder->setBufferForWrite(newBuffer, indexOffsetEncoded, 1);
    encoder->setData(static_cast<uint>(glCount), 2);
    encoder->setData(primCount, 3);
    encoder->setData(firstVertexEncoded, 4);
    encoder->dispatch(
        MTLSizeMake((static_cast<NSUInteger>(primCount) + threadsPerThreadgroup.width - 1) /
                        threadsPerThreadgroup.width,
                    1, 1),
        threadsPerThreadgroup);
    outIndexCount    = static_cast<uint32_t>(newIndexCount);
    outIndexOffset   = newIndexOffset;
    outPrimitiveMode = getNewPrimitiveMode(indexBufferKey);
    outNewBuffer     = newBuffer;
    return angle::Result::Continue;
}

}  // namespace rx
