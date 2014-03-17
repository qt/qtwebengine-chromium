/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Uint8ClampedArray.h"
#include "wtf/Vector.h"

class SkCanvas;

namespace WebCore {

class DrawingBuffer;
class GraphicsContext3D;
class Image;
class IntPoint;
class IntRect;

enum Multiply {
    Premultiplied,
    Unmultiplied
};

enum BackingStoreCopy {
    CopyBackingStore, // Guarantee subsequent draws don't affect the copy.
    DontCopyBackingStore // Subsequent draws may affect the copy.
};

enum ScaleBehavior {
    Scaled,
    Unscaled
};

class PLATFORM_EXPORT ImageBuffer {
    WTF_MAKE_NONCOPYABLE(ImageBuffer); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ImageBuffer> create(const IntSize&, OpacityMode = NonOpaque);
    static PassOwnPtr<ImageBuffer> create(PassOwnPtr<ImageBufferSurface>);

    ~ImageBuffer();

    const IntSize& size() const { return m_surface->size(); }
    bool isAccelerated() const { return m_surface->isAccelerated(); }

    GraphicsContext* context() const;

    const SkBitmap& bitmap() const;

    PassRefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, ScaleBehavior = Scaled) const;
    // Give hints on the faster copyImage Mode, return DontCopyBackingStore if it supports the DontCopyBackingStore behavior
    // or return CopyBackingStore if it doesn't.
    static BackingStoreCopy fastCopyImageMode();

    PassRefPtr<Uint8ClampedArray> getUnmultipliedImageData(const IntRect&) const;
    PassRefPtr<Uint8ClampedArray> getPremultipliedImageData(const IntRect&) const;

    void putByteArray(Multiply multiplied, Uint8ClampedArray*, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint);

    String toDataURL(const String& mimeType, const double* quality = 0) const;
    AffineTransform baseTransform() const { return AffineTransform(); }
    void transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace);
    blink::WebLayer* platformLayer() const;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, UNSIGNED_BYTE type and level 0, as specified in
    // Extensions3D::canUseCopyTextureCHROMIUM().
    bool copyToPlatformTexture(GraphicsContext3D&, Platform3DObject, GC3Denum, GC3Denum, GC3Dint, bool, bool);

    Platform3DObject getBackingTexture();
    bool copyRenderingResultsFromDrawingBuffer(DrawingBuffer*);

    void flush();

private:
    ImageBuffer(PassOwnPtr<ImageBufferSurface>);
    bool isValid() const;

    void draw(GraphicsContext*, const FloatRect&, const FloatRect& = FloatRect(0, 0, -1, -1), CompositeOperator = CompositeSourceOver, blink::WebBlendMode = blink::WebBlendModeNormal, bool useLowQualityScale = false);
    void drawPattern(GraphicsContext*, const FloatRect&, const FloatSize&, const FloatPoint&, CompositeOperator, const FloatRect&, blink::WebBlendMode, const IntSize& repeatSpacing = IntSize());
    static PassRefPtr<SkColorFilter> createColorSpaceFilter(ColorSpace srcColorSpace, ColorSpace dstColorSpace);

    friend class GraphicsContext;
    friend class GeneratedImage;
    friend class CrossfadeGeneratedImage;
    friend class GradientGeneratedImage;
    friend class SkiaImageFilterBuilder;

    OwnPtr<ImageBufferSurface> m_surface;
    OwnPtr<GraphicsContext> m_context;
};

struct ImageDataBuffer {
    ImageDataBuffer(const IntSize& size, PassRefPtr<Uint8ClampedArray> data) : m_size(size), m_data(data) { }
    IntSize size() const { return m_size; }
    unsigned char* data() const { return m_data->data(); }

    IntSize m_size;
    RefPtr<Uint8ClampedArray> m_data;
};

String PLATFORM_EXPORT ImageDataToDataURL(const ImageDataBuffer&, const String& mimeType, const double* quality);

} // namespace WebCore

#endif // ImageBuffer_h
