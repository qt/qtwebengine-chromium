/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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

#include "config.h"
#include "platform/graphics/GraphicsContext3D.h"

#include "platform/graphics/cpu/arm/GraphicsContext3DNEON.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/graphics/ImageObserver.h"

namespace WebCore {

namespace {

GraphicsContext3D::DataFormat getDataFormat(GC3Denum destinationFormat, GC3Denum destinationType)
{
    GraphicsContext3D::DataFormat dstFormat = GraphicsContext3D::DataFormatRGBA8;
    switch (destinationType) {
    case GL_UNSIGNED_BYTE:
        switch (destinationFormat) {
        case GL_RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB8;
            break;
        case GL_RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA8;
            break;
        case GL_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA8;
            break;
        case GL_LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR8;
            break;
        case GL_LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA8;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GL_UNSIGNED_SHORT_4_4_4_4:
        dstFormat = GraphicsContext3D::DataFormatRGBA4444;
        break;
    case GL_UNSIGNED_SHORT_5_5_5_1:
        dstFormat = GraphicsContext3D::DataFormatRGBA5551;
        break;
    case GL_UNSIGNED_SHORT_5_6_5:
        dstFormat = GraphicsContext3D::DataFormatRGB565;
        break;
    case GL_HALF_FLOAT_OES: // OES_texture_half_float
        switch (destinationFormat) {
        case GL_RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB16F;
            break;
        case GL_RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA16F;
            break;
        case GL_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA16F;
            break;
        case GL_LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR16F;
            break;
        case GL_LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA16F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    case GL_FLOAT: // OES_texture_float
        switch (destinationFormat) {
        case GL_RGB:
            dstFormat = GraphicsContext3D::DataFormatRGB32F;
            break;
        case GL_RGBA:
            dstFormat = GraphicsContext3D::DataFormatRGBA32F;
            break;
        case GL_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatA32F;
            break;
        case GL_LUMINANCE:
            dstFormat = GraphicsContext3D::DataFormatR32F;
            break;
        case GL_LUMINANCE_ALPHA:
            dstFormat = GraphicsContext3D::DataFormatRA32F;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return dstFormat;
}

// Following Float to Half-Float converion code is from the implementation of ftp://www.fox-toolkit.org/pub/fasthalffloatconversion.pdf,
// "Fast Half Float Conversions" by Jeroen van der Zijp, November 2008 (Revised September 2010).
// Specially, the basetable[512] and shifttable[512] are generated as follows:
/*
unsigned short basetable[512];
unsigned char shifttable[512];

void generatetables(){
    unsigned int i;
    int e;
    for (i = 0; i < 256; ++i){
        e = i - 127;
        if (e < -24){ // Very small numbers map to zero
            basetable[i | 0x000] = 0x0000;
            basetable[i | 0x100] = 0x8000;
            shifttable[i | 0x000] = 24;
            shifttable[i | 0x100] = 24;
        }
        else if (e < -14) { // Small numbers map to denorms
            basetable[i | 0x000] = (0x0400>>(-e-14));
            basetable[i | 0x100] = (0x0400>>(-e-14)) | 0x8000;
            shifttable[i | 0x000] = -e-1;
            shifttable[i | 0x100] = -e-1;
        }
        else if (e <= 15){ // Normal numbers just lose precision
            basetable[i | 0x000] = ((e+15)<<10);
            basetable[i| 0x100] = ((e+15)<<10) | 0x8000;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
        }
        else if (e<128){ // Large numbers map to Infinity
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 24;
            shifttable[i|0x100] = 24;
        }
        else { // Infinity and NaN's stay Infinity and NaN's
            basetable[i|0x000] = 0x7C00;
            basetable[i|0x100] = 0xFC00;
            shifttable[i|0x000] = 13;
            shifttable[i|0x100] = 13;
       }
    }
}
*/

unsigned short baseTable[512] = {
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      1,      2,      4,      8,      16,     32,     64,     128,    256,
512,    1024,   2048,   3072,   4096,   5120,   6144,   7168,   8192,   9216,   10240,  11264,  12288,  13312,  14336,  15360,
16384,  17408,  18432,  19456,  20480,  21504,  22528,  23552,  24576,  25600,  26624,  27648,  28672,  29696,  30720,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,  31744,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,  32768,
32768,  32768,  32768,  32768,  32768,  32768,  32768,  32769,  32770,  32772,  32776,  32784,  32800,  32832,  32896,  33024,
33280,  33792,  34816,  35840,  36864,  37888,  38912,  39936,  40960,  41984,  43008,  44032,  45056,  46080,  47104,  48128,
49152,  50176,  51200,  52224,  53248,  54272,  55296,  56320,  57344,  58368,  59392,  60416,  61440,  62464,  63488,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,
64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512,  64512
};

unsigned char shiftTable[512] = {
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     23,     22,     21,     20,     19,     18,     17,     16,     15,
14,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,
13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     13,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     23,     22,     21,     20,     19,     18,     17,     16,     15,
14,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,
13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     13,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,
24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     24,     13
};

unsigned short convertFloatToHalfFloat(float f)
{
    unsigned temp = *(reinterpret_cast<unsigned *>(&f));
    unsigned signexp = (temp >> 23) & 0x1ff;
    return baseTable[signexp] + ((temp & 0x007fffff) >> shiftTable[signexp]);
}

/* BEGIN CODE SHARED WITH MOZILLA FIREFOX */

// The following packing and unpacking routines are expressed in terms of function templates and inline functions to achieve generality and speedup.
// Explicit template specializations correspond to the cases that would occur.
// Some code are merged back from Mozilla code in http://mxr.mozilla.org/mozilla-central/source/content/canvas/src/WebGLTexelConversions.h

//----------------------------------------------------------------------
// Pixel unpacking routines.
template<int format, typename SourceType, typename DstType>
void unpack(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> void unpack<GraphicsContext3D::DataFormatRGB8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatBGR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        destination[3] = 0xFF;
        source += 3;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatARGB8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1];
        destination[1] = source[2];
        destination[2] = source[3];
        destination[3] = source[0];
        source += 4;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatABGR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        destination[1] = source[2];
        destination[2] = source[1];
        destination[3] = source[0];
        source += 4;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatBGRA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    const uint32_t* source32 = reinterpret_cast_ptr<const uint32_t*>(source);
    uint32_t* destination32 = reinterpret_cast_ptr<uint32_t*>(destination);
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint32_t bgra = source32[i];
#if CPU(BIG_ENDIAN)
        uint32_t brMask = 0xff00ff00;
        uint32_t gaMask = 0x00ff00ff;
#else
        uint32_t brMask = 0x00ff00ff;
        uint32_t gaMask = 0xff00ff00;
#endif
        uint32_t rgba = (((bgra >> 16) | (bgra << 16)) & brMask) | (bgra & gaMask);
        destination32[i] = rgba;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRGBA5551, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA5551ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 11;
        uint8_t g = (packedValue >> 6) & 0x1F;
        uint8_t b = (packedValue >> 1) & 0x1F;
        destination[0] = (r << 3) | (r & 0x7);
        destination[1] = (g << 3) | (g & 0x7);
        destination[2] = (b << 3) | (b & 0x7);
        destination[3] = (packedValue & 0x1) ? 0xFF : 0x0;
        source += 1;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRGBA4444, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGBA4444ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 12;
        uint8_t g = (packedValue >> 8) & 0x0F;
        uint8_t b = (packedValue >> 4) & 0x0F;
        uint8_t a = packedValue & 0x0F;
        destination[0] = r << 4 | r;
        destination[1] = g << 4 | g;
        destination[2] = b << 4 | b;
        destination[3] = a << 4 | a;
        source += 1;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRGB565, uint16_t, uint8_t>(const uint16_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::unpackOneRowOfRGB565ToRGBA8(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        uint16_t packedValue = source[0];
        uint8_t r = packedValue >> 11;
        uint8_t g = (packedValue >> 5) & 0x3F;
        uint8_t b = packedValue & 0x1F;
        destination[0] = (r << 3) | (r & 0x7);
        destination[1] = (g << 2) | (g & 0x3);
        destination[2] = (b << 3) | (b & 0x7);
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = 0xFF;
        source += 1;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = source[1];
        source += 2;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatAR8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1];
        destination[1] = source[1];
        destination[2] = source[1];
        destination[3] = source[0];
        source += 2;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatA8, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0x0;
        destination[1] = 0x0;
        destination[2] = 0x0;
        destination[3] = source[0];
        source += 1;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRGBA8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatBGRA8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[0] * scaleFactor;
        destination[3] = source[3] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatABGR8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3] * scaleFactor;
        destination[1] = source[2] * scaleFactor;
        destination[2] = source[1] * scaleFactor;
        destination[3] = source[0] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatARGB8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[1] * scaleFactor;
        destination[1] = source[2] * scaleFactor;
        destination[2] = source[3] * scaleFactor;
        destination[3] = source[0] * scaleFactor;
        source += 4;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRGB8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatBGR8, uint8_t, float>(const uint8_t* source, float* destination, unsigned pixelsPerRow)
{
    const float scaleFactor = 1.0f / 255.0f;
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[2] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[0] * scaleFactor;
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRGB32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = 1;
        source += 3;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatR32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = 1;
        source += 1;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatRA32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[0];
        destination[2] = source[0];
        destination[3] = source[1];
        source += 2;
        destination += 4;
    }
}

template<> void unpack<GraphicsContext3D::DataFormatA32F, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = 0;
        destination[1] = 0;
        destination[2] = 0;
        destination[3] = source[0];
        source += 1;
        destination += 4;
    }
}

//----------------------------------------------------------------------
// Pixel packing routines.
//

template<int format, int alphaOp, typename SourceType, typename DstType>
void pack(const SourceType*, DstType*, unsigned)
{
    ASSERT_NOT_REACHED();
}

template<> void pack<GraphicsContext3D::DataFormatA8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatR8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatRA8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        source += 4;
        destination += 3;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        source += 4;
        destination += 3;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatRGB8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        source += 4;
        destination += 3;
    }
}


template<> void pack<GraphicsContext3D::DataFormatRGBA8, GraphicsContext3D::AlphaDoNothing, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    memcpy(destination, source, pixelsPerRow * 4);
}

template<> void pack<GraphicsContext3D::DataFormatRGBA8, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatRGBA8, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint8_t>(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        destination[0] = sourceR;
        destination[1] = sourceG;
        destination[2] = sourceB;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA4444, GraphicsContext3D::AlphaDoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort4444(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF0) << 8)
                        | ((source[1] & 0xF0) << 4)
                        | (source[2] & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA4444, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF0) << 8)
                        | ((sourceG & 0xF0) << 4)
                        | (sourceB & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatRGBA4444, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF0) << 8)
                        | ((sourceG & 0xF0) << 4)
                        | (sourceB & 0xF0)
                        | (source[3] >> 4));
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA5551, GraphicsContext3D::AlphaDoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort5551(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF8) << 8)
                        | ((source[1] & 0xF8) << 3)
                        | ((source[2] & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA5551, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xF8) << 3)
                        | ((sourceB & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatRGBA5551, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xF8) << 3)
                        | ((sourceB & 0xF8) >> 2)
                        | (source[3] >> 7));
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB565, GraphicsContext3D::AlphaDoNothing, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
#if HAVE(ARM_NEON_INTRINSICS)
    SIMD::packOneRowOfRGBA8ToUnsignedShort565(source, destination, pixelsPerRow);
#endif
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        *destination = (((source[0] & 0xF8) << 8)
                        | ((source[1] & 0xFC) << 3)
                        | ((source[2] & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB565, GraphicsContext3D::AlphaDoPremultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] / 255.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xFC) << 3)
                        | ((sourceB & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

// FIXME: this routine is lossy and must be removed.
template<> void pack<GraphicsContext3D::DataFormatRGB565, GraphicsContext3D::AlphaDoUnmultiply, uint8_t, uint16_t>(const uint8_t* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 255.0f / source[3] : 1.0f;
        uint8_t sourceR = static_cast<uint8_t>(static_cast<float>(source[0]) * scaleFactor);
        uint8_t sourceG = static_cast<uint8_t>(static_cast<float>(source[1]) * scaleFactor);
        uint8_t sourceB = static_cast<uint8_t>(static_cast<float>(source[2]) * scaleFactor);
        *destination = (((sourceR & 0xF8) << 8)
                        | ((sourceG & 0xFC) << 3)
                        | ((sourceB & 0xF8) >> 3));
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        source += 4;
        destination += 3;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        source += 4;
        destination += 3;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        source += 4;
        destination += 3;
    }
}

// Used only during RGBA8 or BGRA8 -> floating-point uploads.
template<> void pack<GraphicsContext3D::DataFormatRGBA32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    memcpy(destination, source, pixelsPerRow * 4 * sizeof(float));
}

template<> void pack<GraphicsContext3D::DataFormatRGBA32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[1] * scaleFactor;
        destination[2] = source[2] * scaleFactor;
        destination[3] = source[3];
        source += 4;
        destination += 4;
    }
}

template<> void pack<GraphicsContext3D::DataFormatA32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[3];
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA32F, GraphicsContext3D::AlphaDoNothing, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        destination[0] = source[0];
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA32F, GraphicsContext3D::AlphaDoPremultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA32F, GraphicsContext3D::AlphaDoUnmultiply, float, float>(const float* source, float* destination, unsigned pixelsPerRow)
{
    for (unsigned int i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = source[0] * scaleFactor;
        destination[1] = source[3];
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        destination[2] = convertFloatToHalfFloat(source[2]);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGBA16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        destination[3] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 4;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[1]);
        destination[2] = convertFloatToHalfFloat(source[2]);
        source += 4;
        destination += 3;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        source += 4;
        destination += 3;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRGB16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[1] * scaleFactor);
        destination[2] = convertFloatToHalfFloat(source[2] * scaleFactor);
        source += 4;
        destination += 3;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatRA16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        destination[1] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 2;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[0]);
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR16F, GraphicsContext3D::AlphaDoPremultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3];
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatR16F, GraphicsContext3D::AlphaDoUnmultiply, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        float scaleFactor = source[3] ? 1.0f / source[3] : 1.0f;
        destination[0] = convertFloatToHalfFloat(source[0] * scaleFactor);
        source += 4;
        destination += 1;
    }
}

template<> void pack<GraphicsContext3D::DataFormatA16F, GraphicsContext3D::AlphaDoNothing, float, uint16_t>(const float* source, uint16_t* destination, unsigned pixelsPerRow)
{
    for (unsigned i = 0; i < pixelsPerRow; ++i) {
        destination[0] = convertFloatToHalfFloat(source[3]);
        source += 4;
        destination += 1;
    }
}

bool HasAlpha(int format)
{
    return format == GraphicsContext3D::DataFormatA8
        || format == GraphicsContext3D::DataFormatA16F
        || format == GraphicsContext3D::DataFormatA32F
        || format == GraphicsContext3D::DataFormatRA8
        || format == GraphicsContext3D::DataFormatAR8
        || format == GraphicsContext3D::DataFormatRA16F
        || format == GraphicsContext3D::DataFormatRA32F
        || format == GraphicsContext3D::DataFormatRGBA8
        || format == GraphicsContext3D::DataFormatBGRA8
        || format == GraphicsContext3D::DataFormatARGB8
        || format == GraphicsContext3D::DataFormatABGR8
        || format == GraphicsContext3D::DataFormatRGBA16F
        || format == GraphicsContext3D::DataFormatRGBA32F
        || format == GraphicsContext3D::DataFormatRGBA4444
        || format == GraphicsContext3D::DataFormatRGBA5551;
}

bool HasColor(int format)
{
    return format == GraphicsContext3D::DataFormatRGBA8
        || format == GraphicsContext3D::DataFormatRGBA16F
        || format == GraphicsContext3D::DataFormatRGBA32F
        || format == GraphicsContext3D::DataFormatRGB8
        || format == GraphicsContext3D::DataFormatRGB16F
        || format == GraphicsContext3D::DataFormatRGB32F
        || format == GraphicsContext3D::DataFormatBGR8
        || format == GraphicsContext3D::DataFormatBGRA8
        || format == GraphicsContext3D::DataFormatARGB8
        || format == GraphicsContext3D::DataFormatABGR8
        || format == GraphicsContext3D::DataFormatRGBA5551
        || format == GraphicsContext3D::DataFormatRGBA4444
        || format == GraphicsContext3D::DataFormatRGB565
        || format == GraphicsContext3D::DataFormatR8
        || format == GraphicsContext3D::DataFormatR16F
        || format == GraphicsContext3D::DataFormatR32F
        || format == GraphicsContext3D::DataFormatRA8
        || format == GraphicsContext3D::DataFormatRA16F
        || format == GraphicsContext3D::DataFormatRA32F
        || format == GraphicsContext3D::DataFormatAR8;
}

template<int Format>
struct IsFloatFormat {
    static const bool Value =
        Format == GraphicsContext3D::DataFormatRGBA32F
        || Format == GraphicsContext3D::DataFormatRGB32F
        || Format == GraphicsContext3D::DataFormatRA32F
        || Format == GraphicsContext3D::DataFormatR32F
        || Format == GraphicsContext3D::DataFormatA32F;
};

template<int Format>
struct IsHalfFloatFormat {
    static const bool Value =
        Format == GraphicsContext3D::DataFormatRGBA16F
        || Format == GraphicsContext3D::DataFormatRGB16F
        || Format == GraphicsContext3D::DataFormatRA16F
        || Format == GraphicsContext3D::DataFormatR16F
        || Format == GraphicsContext3D::DataFormatA16F;
};

template<int Format>
struct Is16bppFormat {
    static const bool Value =
        Format == GraphicsContext3D::DataFormatRGBA5551
        || Format == GraphicsContext3D::DataFormatRGBA4444
        || Format == GraphicsContext3D::DataFormatRGB565;
};

template<int Format, bool IsFloat = IsFloatFormat<Format>::Value, bool IsHalfFloat = IsHalfFloatFormat<Format>::Value, bool Is16bpp = Is16bppFormat<Format>::Value>
struct DataTypeForFormat {
    typedef uint8_t Type;
};

template<int Format>
struct DataTypeForFormat<Format, true, false, false> {
    typedef float Type;
};

template<int Format>
struct DataTypeForFormat<Format, false, true, false> {
    typedef uint16_t Type;
};

template<int Format>
struct DataTypeForFormat<Format, false, false, true> {
    typedef uint16_t Type;
};

template<int Format>
struct IntermediateFormat {
    static const int Value = (IsFloatFormat<Format>::Value || IsHalfFloatFormat<Format>::Value) ? GraphicsContext3D::DataFormatRGBA32F : GraphicsContext3D::DataFormatRGBA8;
};

unsigned TexelBytesForFormat(GraphicsContext3D::DataFormat format)
{
    switch (format) {
    case GraphicsContext3D::DataFormatR8:
    case GraphicsContext3D::DataFormatA8:
        return 1;
    case GraphicsContext3D::DataFormatRA8:
    case GraphicsContext3D::DataFormatAR8:
    case GraphicsContext3D::DataFormatRGBA5551:
    case GraphicsContext3D::DataFormatRGBA4444:
    case GraphicsContext3D::DataFormatRGB565:
    case GraphicsContext3D::DataFormatA16F:
    case GraphicsContext3D::DataFormatR16F:
        return 2;
    case GraphicsContext3D::DataFormatRGB8:
    case GraphicsContext3D::DataFormatBGR8:
        return 3;
    case GraphicsContext3D::DataFormatRGBA8:
    case GraphicsContext3D::DataFormatARGB8:
    case GraphicsContext3D::DataFormatABGR8:
    case GraphicsContext3D::DataFormatBGRA8:
    case GraphicsContext3D::DataFormatR32F:
    case GraphicsContext3D::DataFormatA32F:
    case GraphicsContext3D::DataFormatRA16F:
        return 4;
    case GraphicsContext3D::DataFormatRGB16F:
        return 6;
    case GraphicsContext3D::DataFormatRA32F:
    case GraphicsContext3D::DataFormatRGBA16F:
        return 8;
    case GraphicsContext3D::DataFormatRGB32F:
        return 12;
    case GraphicsContext3D::DataFormatRGBA32F:
        return 16;
    default:
        return 0;
    }
}

/* END CODE SHARED WITH MOZILLA FIREFOX */

class FormatConverter {
public:
    FormatConverter(unsigned width, unsigned height,
        const void* srcStart, void* dstStart, int srcStride, int dstStride)
        : m_width(width), m_height(height), m_srcStart(srcStart), m_dstStart(dstStart), m_srcStride(srcStride), m_dstStride(dstStride), m_success(false)
    {
        const unsigned MaxNumberOfComponents = 4;
        const unsigned MaxBytesPerComponent  = 4;
        m_unpackedIntermediateSrcData = adoptArrayPtr(new uint8_t[m_width * MaxNumberOfComponents *MaxBytesPerComponent]);
        ASSERT(m_unpackedIntermediateSrcData.get());
    }

    void convert(GraphicsContext3D::DataFormat srcFormat, GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp);
    bool Success() const { return m_success; }

private:
    template<GraphicsContext3D::DataFormat SrcFormat>
    void convert(GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp);

    template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat>
    void convert(GraphicsContext3D::AlphaOp);

    template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat, GraphicsContext3D::AlphaOp alphaOp>
    void convert();

    const unsigned m_width, m_height;
    const void* const m_srcStart;
    void* const m_dstStart;
    const int m_srcStride, m_dstStride;
    bool m_success;
    OwnPtr<uint8_t[]> m_unpackedIntermediateSrcData;
};

void FormatConverter::convert(GraphicsContext3D::DataFormat srcFormat, GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_SRCFORMAT(SrcFormat) \
    case SrcFormat: \
        return convert<SrcFormat>(dstFormat, alphaOp);

        switch (srcFormat) {
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatR32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatA32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRA32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGB8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatBGR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGB565)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGB32F)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatARGB8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatABGR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatAR8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatBGRA8)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA5551)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA4444)
            FORMATCONVERTER_CASE_SRCFORMAT(GraphicsContext3D::DataFormatRGBA32F)
        default:
            ASSERT_NOT_REACHED();
        }
#undef FORMATCONVERTER_CASE_SRCFORMAT
}

template<GraphicsContext3D::DataFormat SrcFormat>
void FormatConverter::convert(GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_DSTFORMAT(DstFormat) \
    case DstFormat: \
        return convert<SrcFormat, DstFormat>(alphaOp);

        switch (dstFormat) {
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatR8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatR16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatR32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatA32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRA32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB565)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGB32F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA8)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA5551)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA4444)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA16F)
            FORMATCONVERTER_CASE_DSTFORMAT(GraphicsContext3D::DataFormatRGBA32F)
        default:
            ASSERT_NOT_REACHED();
        }

#undef FORMATCONVERTER_CASE_DSTFORMAT
}

template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat>
void FormatConverter::convert(GraphicsContext3D::AlphaOp alphaOp)
{
#define FORMATCONVERTER_CASE_ALPHAOP(alphaOp) \
    case alphaOp: \
        return convert<SrcFormat, DstFormat, alphaOp>();

        switch (alphaOp) {
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContext3D::AlphaDoNothing)
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContext3D::AlphaDoPremultiply)
            FORMATCONVERTER_CASE_ALPHAOP(GraphicsContext3D::AlphaDoUnmultiply)
        default:
            ASSERT_NOT_REACHED();
        }
#undef FORMATCONVERTER_CASE_ALPHAOP
}

template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat, GraphicsContext3D::AlphaOp alphaOp>
void FormatConverter::convert()
{
    // Many instantiations of this template function will never be entered, so we try
    // to return immediately in these cases to avoid the compiler to generate useless code.
    if (SrcFormat == DstFormat && alphaOp == GraphicsContext3D::AlphaDoNothing) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!IsFloatFormat<DstFormat>::Value && IsFloatFormat<SrcFormat>::Value) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Only textures uploaded from DOM elements or ImageData can allow DstFormat != SrcFormat.
    const bool srcFormatComesFromDOMElementOrImageData = GraphicsContext3D::srcFormatComeFromDOMElementOrImageData(SrcFormat);
    if (!srcFormatComesFromDOMElementOrImageData && SrcFormat != DstFormat) {
        ASSERT_NOT_REACHED();
        return;
    }
    // Likewise, only textures uploaded from DOM elements or ImageData can possibly have to be unpremultiplied.
    if (!srcFormatComesFromDOMElementOrImageData && alphaOp == GraphicsContext3D::AlphaDoUnmultiply) {
        ASSERT_NOT_REACHED();
        return;
    }
    if ((!HasAlpha(SrcFormat) || !HasColor(SrcFormat) || !HasColor(DstFormat)) && alphaOp != GraphicsContext3D::AlphaDoNothing) {
        ASSERT_NOT_REACHED();
        return;
    }

    typedef typename DataTypeForFormat<SrcFormat>::Type SrcType;
    typedef typename DataTypeForFormat<DstFormat>::Type DstType;
    const int IntermediateSrcFormat = IntermediateFormat<DstFormat>::Value;
    typedef typename DataTypeForFormat<IntermediateSrcFormat>::Type IntermediateSrcType;
    const ptrdiff_t srcStrideInElements = m_srcStride / sizeof(SrcType);
    const ptrdiff_t dstStrideInElements = m_dstStride / sizeof(DstType);
    const bool trivialUnpack = (SrcFormat == GraphicsContext3D::DataFormatRGBA8 && !IsFloatFormat<DstFormat>::Value && !IsHalfFloatFormat<DstFormat>::Value) || SrcFormat == GraphicsContext3D::DataFormatRGBA32F;
    const bool trivialPack = (DstFormat == GraphicsContext3D::DataFormatRGBA8 || DstFormat == GraphicsContext3D::DataFormatRGBA32F) && alphaOp == GraphicsContext3D::AlphaDoNothing && m_dstStride > 0;
    ASSERT(!trivialUnpack || !trivialPack);

    const SrcType *srcRowStart = static_cast<const SrcType*>(m_srcStart);
    DstType* dstRowStart = static_cast<DstType*>(m_dstStart);
    if (!trivialUnpack && trivialPack) {
        for (size_t i = 0; i < m_height; ++i) {
            unpack<SrcFormat>(srcRowStart, dstRowStart, m_width);
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }
    } else if (!trivialUnpack && !trivialPack) {
        for (size_t i = 0; i < m_height; ++i) {
            unpack<SrcFormat>(srcRowStart, reinterpret_cast<IntermediateSrcType*>(m_unpackedIntermediateSrcData.get()), m_width);
            pack<DstFormat, alphaOp>(reinterpret_cast<IntermediateSrcType*>(m_unpackedIntermediateSrcData.get()), dstRowStart, m_width);
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }
    } else {
        for (size_t i = 0; i < m_height; ++i) {
            pack<DstFormat, alphaOp>(srcRowStart, dstRowStart, m_width);
            srcRowStart += srcStrideInElements;
            dstRowStart += dstStrideInElements;
        }
    }
    m_success = true;
    return;
}

} // anonymous namespace

bool GraphicsContext3D::packImageData(
    Image* image,
    const void* pixels,
    GC3Denum format,
    GC3Denum type,
    bool flipY,
    AlphaOp alphaOp,
    DataFormat sourceFormat,
    unsigned width,
    unsigned height,
    unsigned sourceUnpackAlignment,
    Vector<uint8_t>& data)
{
    if (!pixels)
        return false;

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GL_NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(reinterpret_cast<const uint8_t*>(pixels), sourceFormat, width, height, sourceUnpackAlignment, format, type, alphaOp, data.data(), flipY))
        return false;
    if (ImageObserver *observer = image->imageObserver())
        observer->didDraw(image);
    return true;
}

bool GraphicsContext3D::extractImageData(
    const uint8_t* imageData,
    const IntSize& imageDataSize,
    GC3Denum format,
    GC3Denum type,
    bool flipY,
    bool premultiplyAlpha,
    Vector<uint8_t>& data)
{
    if (!imageData)
        return false;
    int width = imageDataSize.width();
    int height = imageDataSize.height();

    unsigned packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GL_NO_ERROR)
        return false;
    data.resize(packedSize);

    if (!packPixels(imageData, DataFormatRGBA8, width, height, 0, format, type, premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing, data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContext3D::extractTextureData(
    unsigned width,
    unsigned height,
    GC3Denum format, GC3Denum type,
    unsigned unpackAlignment,
    bool flipY, bool premultiplyAlpha,
    const void* pixels,
    Vector<uint8_t>& data)
{
    // Assumes format, type, etc. have already been validated.
    DataFormat sourceDataFormat = getDataFormat(format, type);

    // Resize the output buffer.
    unsigned int componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return false;
    unsigned bytesPerPixel = componentsPerPixel * bytesPerComponent;
    data.resize(width * height * bytesPerPixel);

    if (!packPixels(static_cast<const uint8_t*>(pixels), sourceDataFormat, width, height, unpackAlignment, format, type, (premultiplyAlpha ? AlphaDoPremultiply : AlphaDoNothing), data.data(), flipY))
        return false;

    return true;
}

bool GraphicsContext3D::packPixels(
    const uint8_t* sourceData,
    DataFormat sourceDataFormat,
    unsigned width,
    unsigned height,
    unsigned sourceUnpackAlignment,
    unsigned destinationFormat,
    unsigned destinationType,
    AlphaOp alphaOp,
    void* destinationData,
    bool flipY)
{
    int validSrc = width * TexelBytesForFormat(sourceDataFormat);
    int remainder = sourceUnpackAlignment ? (validSrc % sourceUnpackAlignment) : 0;
    int srcStride = remainder ? (validSrc + sourceUnpackAlignment - remainder) : validSrc;

    DataFormat dstDataFormat = getDataFormat(destinationFormat, destinationType);
    int dstStride = width * TexelBytesForFormat(dstDataFormat);
    if (flipY) {
        destinationData = static_cast<uint8_t*>(destinationData) + dstStride*(height - 1);
        dstStride = -dstStride;
    }
    if (!HasAlpha(sourceDataFormat) || !HasColor(sourceDataFormat) || !HasColor(dstDataFormat))
        alphaOp = AlphaDoNothing;

    if (sourceDataFormat == dstDataFormat && alphaOp == AlphaDoNothing) {
        const uint8_t* ptr = sourceData;
        const uint8_t* ptrEnd = sourceData + srcStride * height;
        unsigned rowSize = (dstStride > 0) ? dstStride: -dstStride;
        uint8_t* dst = static_cast<uint8_t*>(destinationData);
        while (ptr < ptrEnd) {
            memcpy(dst, ptr, rowSize);
            ptr += srcStride;
            dst += dstStride;
        }
        return true;
    }

    FormatConverter converter(width, height, sourceData, destinationData, srcStride, dstStride);
    converter.convert(sourceDataFormat, dstDataFormat, alphaOp);
    if (!converter.Success())
        return false;
    return true;
}

} // namespace WebCore

