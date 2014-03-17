/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "platform/graphics/filters/FEColorMatrix.h"

#include "SkColorFilterImageFilter.h"
#include "SkColorMatrixFilter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/text/TextStream.h"
#include "wtf/MathExtras.h"
#include "wtf/Uint8ClampedArray.h"

namespace WebCore {

FEColorMatrix::FEColorMatrix(Filter* filter, ColorMatrixType type, const Vector<float>& values)
    : FilterEffect(filter)
    , m_type(type)
    , m_values(values)
{
}

PassRefPtr<FEColorMatrix> FEColorMatrix::create(Filter* filter, ColorMatrixType type, const Vector<float>& values)
{
    return adoptRef(new FEColorMatrix(filter, type, values));
}

ColorMatrixType FEColorMatrix::type() const
{
    return m_type;
}

bool FEColorMatrix::setType(ColorMatrixType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

const Vector<float>& FEColorMatrix::values() const
{
    return m_values;
}

bool FEColorMatrix::setValues(const Vector<float> &values)
{
    if (m_values == values)
        return false;
    m_values = values;
    return true;
}

inline void matrix(float& red, float& green, float& blue, float& alpha, const Vector<float>& values)
{
    float r = values[0] * red + values[1] * green + values[2] * blue + values[3] * alpha + values[4] * 255;
    float g = values[5] * red + values[6] * green + values[7] * blue + values[8] * alpha + values[9] * 255;
    float b = values[10] * red + values[11] * green + values[12] * blue + values[13] * alpha + values[14] * 255;
    float a = values[15] * red + values[16] * green + values[17] * blue + values[18] * alpha + values[19] * 255;

    red = r;
    green = g;
    blue = b;
    alpha = a;
}

inline void saturateAndHueRotate(float& red, float& green, float& blue, const float* components)
{
    float r = red * components[0] + green * components[1] + blue * components[2];
    float g = red * components[3] + green * components[4] + blue * components[5];
    float b = red * components[6] + green * components[7] + blue * components[8];

    red = r;
    green = g;
    blue = b;
}

inline void luminance(float& red, float& green, float& blue, float& alpha)
{
    alpha = 0.2125 * red + 0.7154 * green + 0.0721 * blue;
    red = 0;
    green = 0;
    blue = 0;
}

template<ColorMatrixType filterType>
void effectType(Uint8ClampedArray* pixelArray, const Vector<float>& values)
{
    unsigned pixelArrayLength = pixelArray->length();
    float components[9];

    if (filterType == FECOLORMATRIX_TYPE_SATURATE)
        FEColorMatrix::calculateSaturateComponents(components, values[0]);
    else if (filterType == FECOLORMATRIX_TYPE_HUEROTATE)
        FEColorMatrix::calculateHueRotateComponents(components, values[0]);

    for (unsigned pixelByteOffset = 0; pixelByteOffset < pixelArrayLength; pixelByteOffset += 4) {
        float red = pixelArray->item(pixelByteOffset);
        float green = pixelArray->item(pixelByteOffset + 1);
        float blue = pixelArray->item(pixelByteOffset + 2);
        float alpha = pixelArray->item(pixelByteOffset + 3);

        switch (filterType) {
        case FECOLORMATRIX_TYPE_MATRIX:
            matrix(red, green, blue, alpha, values);
            break;
        case FECOLORMATRIX_TYPE_SATURATE:
        case FECOLORMATRIX_TYPE_HUEROTATE:
            saturateAndHueRotate(red, green, blue, components);
            break;
        case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
            luminance(red, green, blue, alpha);
            break;
        }

        pixelArray->set(pixelByteOffset, red);
        pixelArray->set(pixelByteOffset + 1, green);
        pixelArray->set(pixelByteOffset + 2, blue);
        pixelArray->set(pixelByteOffset + 3, alpha);
    }
}

void FEColorMatrix::applySoftware()
{
    FilterEffect* in = inputEffect(0);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    resultImage->context()->drawImageBuffer(in->asImageBuffer(), drawingRegionOfInputImage(in->absolutePaintRect()));

    IntRect imageRect(IntPoint(), absolutePaintRect().size());
    RefPtr<Uint8ClampedArray> pixelArray = resultImage->getUnmultipliedImageData(imageRect);

    switch (m_type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        effectType<FECOLORMATRIX_TYPE_MATRIX>(pixelArray.get(), m_values);
        break;
    case FECOLORMATRIX_TYPE_SATURATE:
        effectType<FECOLORMATRIX_TYPE_SATURATE>(pixelArray.get(), m_values);
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        effectType<FECOLORMATRIX_TYPE_HUEROTATE>(pixelArray.get(), m_values);
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        effectType<FECOLORMATRIX_TYPE_LUMINANCETOALPHA>(pixelArray.get(), m_values);
        setIsAlphaImage(true);
        break;
    }

    resultImage->putByteArray(Unmultiplied, pixelArray.get(), imageRect.size(), imageRect, IntPoint());
}

static void saturateMatrix(float s, SkScalar matrix[20])
{
    matrix[0] = 0.213f + 0.787f * s;
    matrix[1] = 0.715f - 0.715f * s;
    matrix[2] = 0.072f - 0.072f * s;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - 0.213f * s;
    matrix[6] = 0.715f + 0.285f * s;
    matrix[7] = 0.072f - 0.072f * s;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - 0.213f * s;
    matrix[11] = 0.715f - 0.715f * s;
    matrix[12] = 0.072f + 0.928f * s;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = 0;
    matrix[18] = 1;
    matrix[19] = 0;
}

static void hueRotateMatrix(float hue, SkScalar matrix[20])
{
    float cosHue = cosf(hue * piFloat / 180);
    float sinHue = sinf(hue * piFloat / 180);
    matrix[0] = 0.213f + cosHue * 0.787f - sinHue * 0.213f;
    matrix[1] = 0.715f - cosHue * 0.715f - sinHue * 0.715f;
    matrix[2] = 0.072f - cosHue * 0.072f + sinHue * 0.928f;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - cosHue * 0.213f + sinHue * 0.143f;
    matrix[6] = 0.715f + cosHue * 0.285f + sinHue * 0.140f;
    matrix[7] = 0.072f - cosHue * 0.072f - sinHue * 0.283f;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - cosHue * 0.213f - sinHue * 0.787f;
    matrix[11] = 0.715f - cosHue * 0.715f + sinHue * 0.715f;
    matrix[12] = 0.072f + cosHue * 0.928f + sinHue * 0.072f;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = 0;
    matrix[18] = 1;
    matrix[19] = 0;
}

static void luminanceToAlphaMatrix(SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[15] = 0.2125f;
    matrix[16] = 0.7154f;
    matrix[17] = 0.0721f;
}

static SkColorFilter* createColorFilter(ColorMatrixType type, const float* values)
{
    SkScalar matrix[20];
    switch (type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        for (int i = 0; i < 20; ++i)
            matrix[i] = values[i];

        matrix[4] *= SkScalar(255);
        matrix[9] *= SkScalar(255);
        matrix[14] *= SkScalar(255);
        matrix[19] *= SkScalar(255);
        break;
    case FECOLORMATRIX_TYPE_SATURATE:
        saturateMatrix(values[0], matrix);
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        hueRotateMatrix(values[0], matrix);
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        luminanceToAlphaMatrix(matrix);
        break;
    }
    return new SkColorMatrixFilter(matrix);
}

bool FEColorMatrix::applySkia()
{
    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    FilterEffect* in = inputEffect(0);

    SkRect drawingRegion = drawingRegionOfInputImage(in->absolutePaintRect());

    SkAutoTUnref<SkColorFilter> filter(createColorFilter(m_type, m_values.data()));

    RefPtr<Image> image = in->asImageBuffer()->copyImage(DontCopyBackingStore);
    RefPtr<NativeImageSkia> nativeImage = image->nativeImageForCurrentFrame();
    if (!nativeImage)
        return false;

    SkPaint paint;
    paint.setColorFilter(filter);
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    resultImage->context()->drawBitmap(nativeImage->bitmap(), drawingRegion.fLeft, drawingRegion.fTop, &paint);
    return true;
}

PassRefPtr<SkImageFilter> FEColorMatrix::createImageFilter(SkiaImageFilterBuilder* builder)
{
    RefPtr<SkImageFilter> input(builder->build(inputEffect(0), operatingColorSpace()));
    SkAutoTUnref<SkColorFilter> filter(createColorFilter(m_type, m_values.data()));
    SkImageFilter::CropRect rect = getCropRect(builder->cropOffset());
    return adoptRef(SkColorFilterImageFilter::Create(filter, input.get(), &rect));
}

static TextStream& operator<<(TextStream& ts, const ColorMatrixType& type)
{
    switch (type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        ts << "MATRIX";
        break;
    case FECOLORMATRIX_TYPE_SATURATE:
        ts << "SATURATE";
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        ts << "HUEROTATE";
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        ts << "LUMINANCETOALPHA";
        break;
    }
    return ts;
}

TextStream& FEColorMatrix::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feColorMatrix";
    FilterEffect::externalRepresentation(ts);
    ts << " type=\"" << m_type << "\"";
    if (!m_values.isEmpty()) {
        ts << " values=\"";
        Vector<float>::const_iterator ptr = m_values.begin();
        const Vector<float>::const_iterator end = m_values.end();
        while (ptr < end) {
            ts << *ptr;
            ++ptr;
            if (ptr < end)
                ts << " ";
        }
        ts << "\"";
    }
    ts << "]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore
