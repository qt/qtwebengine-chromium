/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef FECustomFilter_h
#define FECustomFilter_h

#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/custom/CustomFilterConstants.h"
#include "platform/graphics/filters/custom/CustomFilterOperation.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class CustomFilterRenderer;
class CustomFilterValidatedProgram;
class GraphicsContext3D;
class IntSize;

class PLATFORM_EXPORT FECustomFilter : public FilterEffect {
public:
    static PassRefPtr<FECustomFilter> create(Filter*, PassRefPtr<GraphicsContext3D>, PassRefPtr<CustomFilterValidatedProgram>, const CustomFilterParameterList&,
        unsigned meshRows, unsigned meshColumns, CustomFilterMeshType);

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FECustomFilter(Filter*, PassRefPtr<GraphicsContext3D>, PassRefPtr<CustomFilterValidatedProgram>, const CustomFilterParameterList&,
        unsigned meshRows, unsigned meshColumns, CustomFilterMeshType);
    ~FECustomFilter();

    virtual void applySoftware() OVERRIDE;

    bool applyShader();
    void clearShaderResult();
    bool initializeContext();

    bool prepareForDrawing();

    void drawFilterMesh(Platform3DObject inputTexture);
    bool ensureInputTexture();
    void uploadInputTexture(Uint8ClampedArray* srcPixelArray);
    bool resizeContextIfNeeded(const IntSize&);
    bool resizeContext(const IntSize&);

    bool canUseMultisampleBuffers() const;
    bool createMultisampleBuffer();
    bool resizeMultisampleBuffers(const IntSize&);
    void resolveMultisampleBuffer();
    void deleteMultisampleRenderBuffers();

    bool ensureFrameBuffer();
    void deleteRenderBuffers();

    RefPtr<GraphicsContext3D> m_context;
    RefPtr<CustomFilterValidatedProgram> m_validatedProgram;
    RefPtr<CustomFilterRenderer> m_customFilterRenderer;
    IntSize m_contextSize;

    Platform3DObject m_inputTexture;
    Platform3DObject m_frameBuffer;
    Platform3DObject m_depthBuffer;
    Platform3DObject m_destTexture;

    bool m_triedMultisampleBuffer;
    Platform3DObject m_multisampleFrameBuffer;
    Platform3DObject m_multisampleRenderBuffer;
    Platform3DObject m_multisampleDepthBuffer;
};

} // namespace WebCore

#endif // FECustomFilter_h
