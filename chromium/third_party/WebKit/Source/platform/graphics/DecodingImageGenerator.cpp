/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/DecodingImageGenerator.h"

#include "SkData.h"
#include "SkImageInfo.h"
#include "platform/PlatformInstrumentation.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/ImageFrameGenerator.h"

namespace WebCore {

DecodingImageGenerator::DecodingImageGenerator(PassRefPtr<ImageFrameGenerator> frameGenerator, const SkImageInfo& info, size_t index)
    : m_frameGenerator(frameGenerator)
    , m_imageInfo(info)
    , m_frameIndex(index)
    , m_generationId(0)
{
}

DecodingImageGenerator::~DecodingImageGenerator()
{
}

SkData* DecodingImageGenerator::refEncodedData()
{
    // FIXME: If the image has been clipped or scaled, do not return the original
    // encoded data, since on playback it will not be known how the clipping/scaling
    // was done.
    RefPtr<SharedBuffer> buffer = 0;
    bool allDataReceived = false;
    m_frameGenerator->copyData(&buffer, &allDataReceived);
    if (buffer && allDataReceived) {
        return SkData::NewWithCopy(buffer->data(), buffer->size());
    }
    return 0;
}

bool DecodingImageGenerator::getInfo(SkImageInfo* info)
{
    *info = m_imageInfo;
    return true;
}

bool DecodingImageGenerator::getPixels(const SkImageInfo& info, void* pixels, size_t rowBytes)
{
    TRACE_EVENT1("webkit", "DecodingImageGenerator::getPixels", "index", static_cast<int>(m_frameIndex));

    // Implementation doesn't support scaling yet so make sure we're not given
    // a different size.
    ASSERT(info.fWidth == m_imageInfo.fWidth);
    ASSERT(info.fHeight == m_imageInfo.fHeight);
    ASSERT(info.fColorType == m_imageInfo.fColorType);
    ASSERT(info.fAlphaType == m_imageInfo.fAlphaType);
    PlatformInstrumentation::willDecodeLazyPixelRef(m_generationId);
    bool decoded = m_frameGenerator->decodeAndScale(m_imageInfo, m_frameIndex, pixels, rowBytes);
    PlatformInstrumentation::didDecodeLazyPixelRef(m_generationId);
    return decoded;
}

} // namespace blink
