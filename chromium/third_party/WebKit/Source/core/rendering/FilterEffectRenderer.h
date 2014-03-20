/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilterEffectRenderer_h
#define FilterEffectRenderer_h

#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRectExtent.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class ShaderResource;
class CustomFilterProgram;
class Document;
class GraphicsContext;
class RenderLayer;
class RenderObject;

class FilterEffectRendererHelper {
public:
    FilterEffectRendererHelper(bool haveFilterEffect)
        : m_savedGraphicsContext(0)
        , m_renderLayer(0)
        , m_haveFilterEffect(haveFilterEffect)
    {
    }

    bool haveFilterEffect() const { return m_haveFilterEffect; }
    bool hasStartedFilterEffect() const { return m_savedGraphicsContext; }

    bool prepareFilterEffect(RenderLayer*, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect);
    GraphicsContext* beginFilterEffect(GraphicsContext* oldContext);
    GraphicsContext* applyFilterEffect();

    const LayoutRect& repaintRect() const { return m_repaintRect; }
private:
    GraphicsContext* m_savedGraphicsContext;
    RenderLayer* m_renderLayer;

    LayoutRect m_repaintRect;
    bool m_haveFilterEffect;
};

class FilterEffectRenderer : public Filter
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<FilterEffectRenderer> create()
    {
        return adoptRef(new FilterEffectRenderer());
    }

    void setSourceImageRect(const FloatRect& sourceImageRect)
    {
        m_sourceDrawingRegion = sourceImageRect;
        m_graphicsBufferAttached = false;
    }
    virtual FloatRect sourceImageRect() const { return m_sourceDrawingRegion; }

    GraphicsContext* inputContext();
    ImageBuffer* output() const { return lastEffect()->asImageBuffer(); }

    bool build(RenderObject* renderer, const FilterOperations&);
    bool updateBackingStoreRect(const FloatRect& filterRect);
    void allocateBackingStoreIfNeeded();
    void clearIntermediateResults();
    void apply();

    IntRect outputRect() const { return lastEffect()->hasResult() ? lastEffect()->absolutePaintRect() : IntRect(); }

    bool hasFilterThatMovesPixels() const { return m_hasFilterThatMovesPixels; }
    LayoutRect computeSourceImageRectForDirtyRect(const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect);

    bool hasCustomShaderFilter() const { return m_hasCustomShaderFilter; }
    PassRefPtr<FilterEffect> lastEffect() const
    {
        return m_lastEffect;
    }
private:

    FilterEffectRenderer();
    virtual ~FilterEffectRenderer();

    FloatRect m_sourceDrawingRegion;

    RefPtr<SourceGraphic> m_sourceGraphic;
    RefPtr<FilterEffect> m_lastEffect;

    IntRectExtent m_outsets;

    bool m_graphicsBufferAttached;
    bool m_hasFilterThatMovesPixels;
    bool m_hasCustomShaderFilter;
};

} // namespace WebCore


#endif // FilterEffectRenderer_h
