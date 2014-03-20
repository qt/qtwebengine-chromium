/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/ImageBuffer.h"

#include "platform/MIMETypeRegistry.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Extensions3D.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContext3D.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/SharedGraphicsContext3D.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-encoders/skia/JPEGImageEncoder.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"
#include "platform/image-encoders/skia/WEBPImageEncoder.h"
#include "public/platform/Platform.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"
#include "wtf/MathExtras.h"
#include "wtf/text/Base64.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace WebCore {

PassOwnPtr<ImageBuffer> ImageBuffer::create(PassOwnPtr<ImageBufferSurface> surface)
{
    if (!surface->isValid())
        return nullptr;
    return adoptPtr(new ImageBuffer(surface));
}

PassOwnPtr<ImageBuffer> ImageBuffer::create(const IntSize& size, OpacityMode opacityMode)
{
    OwnPtr<ImageBufferSurface> surface = adoptPtr(new UnacceleratedImageBufferSurface(size, opacityMode));
    if (!surface->isValid())
        return nullptr;
    return adoptPtr(new ImageBuffer(surface.release()));
}

ImageBuffer::ImageBuffer(PassOwnPtr<ImageBufferSurface> surface)
    : m_surface(surface)
{
    if (m_surface->canvas()) {
        m_context = adoptPtr(new GraphicsContext(m_surface->canvas()));
        m_context->setCertainlyOpaque(m_surface->opacityMode() == Opaque);
        m_context->setAccelerated(m_surface->isAccelerated());
    }
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    m_surface->willUse();
    ASSERT(m_context.get());
    return m_context.get();
}

const SkBitmap& ImageBuffer::bitmap() const
{
    m_surface->willUse();
    return m_surface->bitmap();
}

bool ImageBuffer::isValid() const
{
    return m_surface->isValid();
}

static SkBitmap deepSkBitmapCopy(const SkBitmap& bitmap)
{
    SkBitmap tmp;
    if (!bitmap.deepCopyTo(&tmp, bitmap.config()))
        bitmap.copyTo(&tmp, bitmap.config());

    return tmp;
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior) const
{
    if (!isValid())
        return BitmapImage::create(NativeImageSkia::create());

    const SkBitmap& bitmap = m_surface->bitmap();
    return BitmapImage::create(NativeImageSkia::create(copyBehavior == CopyBackingStore ? deepSkBitmapCopy(bitmap) : bitmap));
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

blink::WebLayer* ImageBuffer::platformLayer() const
{
    return m_surface->layer();
}

bool ImageBuffer::copyToPlatformTexture(GraphicsContext3D& context, Platform3DObject texture, GC3Denum internalFormat, GC3Denum destType, GC3Dint level, bool premultiplyAlpha, bool flipY)
{
    if (!m_surface->isAccelerated() || !platformLayer() || !isValid())
        return false;

    if (!context.makeContextCurrent())
        return false;

    Extensions3D* extensions = context.extensions();
    if (!extensions->supports("GL_CHROMIUM_copy_texture") || !extensions->supports("GL_CHROMIUM_flipy")
        || !extensions->canUseCopyTextureCHROMIUM(internalFormat, destType, level))
        return false;

    // The canvas is stored in a premultiplied format, so unpremultiply if necessary.
    context.pixelStorei(Extensions3D::UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM, !premultiplyAlpha);

    // The canvas is stored in an inverted position, so the flip semantics are reversed.
    context.pixelStorei(Extensions3D::UNPACK_FLIP_Y_CHROMIUM, !flipY);
    extensions->copyTextureCHROMIUM(GL_TEXTURE_2D, getBackingTexture(), texture, level, internalFormat, destType);

    context.pixelStorei(Extensions3D::UNPACK_FLIP_Y_CHROMIUM, false);
    context.pixelStorei(Extensions3D::UNPACK_UNPREMULTIPLY_ALPHA_CHROMIUM, false);
    context.flush();
    return true;
}

static bool drawNeedsCopy(GraphicsContext* src, GraphicsContext* dst)
{
    ASSERT(dst);
    return (src == dst);
}

Platform3DObject ImageBuffer::getBackingTexture()
{
    return m_surface->getBackingTexture();
}

bool ImageBuffer::copyRenderingResultsFromDrawingBuffer(DrawingBuffer* drawingBuffer)
{
    if (!drawingBuffer)
        return false;
    RefPtr<GraphicsContext3D> context3D = SharedGraphicsContext3D::get();
    Platform3DObject tex = m_surface->getBackingTexture();
    if (!context3D || !tex)
        return false;

    return drawingBuffer->copyToPlatformTexture(*(context3D.get()), tex, GL_RGBA,
        GL_UNSIGNED_BYTE, 0, true, false);
}

void ImageBuffer::draw(GraphicsContext* context, const FloatRect& destRect, const FloatRect& srcRect,
    CompositeOperator op, blink::WebBlendMode blendMode, bool useLowQualityScale)
{
    if (!isValid())
        return;

    const SkBitmap& bitmap = m_surface->bitmap();
    RefPtr<Image> image = BitmapImage::create(NativeImageSkia::create(drawNeedsCopy(m_context.get(), context) ? deepSkBitmapCopy(bitmap) : bitmap));
    context->drawImage(image.get(), destRect, srcRect, op, blendMode, DoNotRespectImageOrientation, useLowQualityScale);
}

void ImageBuffer::flush()
{
    if (m_surface->canvas()) {
        m_surface->canvas()->flush();
    }
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const FloatSize& scale,
    const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect, blink::WebBlendMode blendMode, const IntSize& repeatSpacing)
{
    if (!isValid())
        return;

    const SkBitmap& bitmap = m_surface->bitmap();
    RefPtr<Image> image = BitmapImage::create(NativeImageSkia::create(drawNeedsCopy(m_context.get(), context) ? deepSkBitmapCopy(bitmap) : bitmap));
    image->drawPattern(context, srcRect, scale, phase, op, destRect, blendMode, repeatSpacing);
}

static const Vector<uint8_t>& getLinearRgbLUT()
{
    DEFINE_STATIC_LOCAL(Vector<uint8_t>, linearRgbLUT, ());
    if (linearRgbLUT.isEmpty()) {
        linearRgbLUT.reserveCapacity(256);
        for (unsigned i = 0; i < 256; i++) {
            float color = i  / 255.0f;
            color = (color <= 0.04045f ? color / 12.92f : pow((color + 0.055f) / 1.055f, 2.4f));
            color = std::max(0.0f, color);
            color = std::min(1.0f, color);
            linearRgbLUT.append(static_cast<uint8_t>(round(color * 255)));
        }
    }
    return linearRgbLUT;
}

static const Vector<uint8_t>& getDeviceRgbLUT()
{
    DEFINE_STATIC_LOCAL(Vector<uint8_t>, deviceRgbLUT, ());
    if (deviceRgbLUT.isEmpty()) {
        deviceRgbLUT.reserveCapacity(256);
        for (unsigned i = 0; i < 256; i++) {
            float color = i / 255.0f;
            color = (powf(color, 1.0f / 2.4f) * 1.055f) - 0.055f;
            color = std::max(0.0f, color);
            color = std::min(1.0f, color);
            deviceRgbLUT.append(static_cast<uint8_t>(round(color * 255)));
        }
    }
    return deviceRgbLUT;
}

void ImageBuffer::transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace)
{
    if (srcColorSpace == dstColorSpace)
        return;

    // only sRGB <-> linearRGB are supported at the moment
    if ((srcColorSpace != ColorSpaceLinearRGB && srcColorSpace != ColorSpaceDeviceRGB)
        || (dstColorSpace != ColorSpaceLinearRGB && dstColorSpace != ColorSpaceDeviceRGB))
        return;

    // FIXME: Disable color space conversions on accelerated canvases (for now).
    if (context()->isAccelerated() || !isValid())
        return;

    const SkBitmap& bitmap = m_surface->bitmap();
    if (bitmap.isNull())
        return;

    const Vector<uint8_t>& lookUpTable = dstColorSpace == ColorSpaceLinearRGB ?
        getLinearRgbLUT() : getDeviceRgbLUT();

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    IntSize size = m_surface->size();
    SkAutoLockPixels bitmapLock(bitmap);
    for (int y = 0; y < size.height(); ++y) {
        uint32_t* srcRow = bitmap.getAddr32(0, y);
        for (int x = 0; x < size.width(); ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            srcRow[x] = SkPreMultiplyARGB(
                SkColorGetA(color),
                lookUpTable[SkColorGetR(color)],
                lookUpTable[SkColorGetG(color)],
                lookUpTable[SkColorGetB(color)]);
        }
    }
}

PassRefPtr<SkColorFilter> ImageBuffer::createColorSpaceFilter(ColorSpace srcColorSpace,
    ColorSpace dstColorSpace)
{
    if ((srcColorSpace == dstColorSpace)
        || (srcColorSpace != ColorSpaceLinearRGB && srcColorSpace != ColorSpaceDeviceRGB)
        || (dstColorSpace != ColorSpaceLinearRGB && dstColorSpace != ColorSpaceDeviceRGB))
        return 0;

    const uint8_t* lut = 0;
    if (dstColorSpace == ColorSpaceLinearRGB)
        lut = &getLinearRgbLUT()[0];
    else if (dstColorSpace == ColorSpaceDeviceRGB)
        lut = &getDeviceRgbLUT()[0];
    else
        return 0;

    return adoptRef(SkTableColorFilter::CreateARGB(0, lut, lut, lut));
}

template <Multiply multiplied>
PassRefPtr<Uint8ClampedArray> getImageData(const IntRect& rect, GraphicsContext* context, const IntSize& size)
{
    float area = 4.0f * rect.width() * rect.height();
    if (area > static_cast<float>(std::numeric_limits<int>::max()))
        return 0;

    RefPtr<Uint8ClampedArray> result = Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4);

    unsigned char* data = result->data();

    if (rect.x() < 0
        || rect.y() < 0
        || rect.maxX() > size.width()
        || rect.maxY() > size.height())
        result->zeroFill();

    unsigned destBytesPerRow = 4 * rect.width();
    SkBitmap destBitmap;
    destBitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), destBytesPerRow);
    destBitmap.setPixels(data);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    context->readPixels(&destBitmap, rect.x(), rect.y(), config8888);
    return result.release();
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    if (!isValid())
        return Uint8ClampedArray::create(rect.width() * rect.height() * 4);
    return getImageData<Unmultiplied>(rect, context(), m_surface->size());
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    if (!isValid())
        return Uint8ClampedArray::create(rect.width() * rect.height() * 4);
    return getImageData<Premultiplied>(rect, context(), m_surface->size());
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    if (!isValid())
        return;

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < m_surface->size().width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.maxX());

    int endX = destPoint.x() + sourceRect.maxX();
    ASSERT(endX <= m_surface->size().width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < m_surface->size().height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.maxY());

    int endY = destPoint.y() + sourceRect.maxY();
    ASSERT(endY <= m_surface->size().height());
    int numRows = endY - destY;

    unsigned srcBytesPerRow = 4 * sourceSize.width();
    SkBitmap srcBitmap;
    srcBitmap.setConfig(SkBitmap::kARGB_8888_Config, numColumns, numRows, srcBytesPerRow);
    srcBitmap.setPixels(source->data() + originY * srcBytesPerRow + originX * 4);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    context()->writePixels(srcBitmap, destX, destY, config8888);
}

template <typename T>
static bool encodeImage(T& source, const String& mimeType, const double* quality, Vector<char>* output)
{
    Vector<unsigned char>* encodedImage = reinterpret_cast<Vector<unsigned char>*>(output);

    if (mimeType == "image/jpeg") {
        int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!JPEGImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
    } else if (mimeType == "image/webp") {
        int compressionQuality = WEBPImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!WEBPImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
    } else {
        if (!PNGImageEncoder::encode(source, encodedImage))
            return false;
        ASSERT(mimeType == "image/png");
    }

    return true;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!isValid() || !encodeImage(m_surface->bitmap(), mimeType, quality, &encodedImage))
        return "data:,";
    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

String ImageDataToDataURL(const ImageDataBuffer& imageData, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!encodeImage(imageData, mimeType, quality, &encodedImage))
        return "data:,";

    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

} // namespace WebCore
