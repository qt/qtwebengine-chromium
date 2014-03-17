
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkTransparentShader.h"
#include "SkColorPriv.h"
#include "SkString.h"

bool SkTransparentShader::setContext(const SkBitmap& device,
                                     const SkPaint& paint,
                                     const SkMatrix& matrix) {
    fDevice = &device;
    fAlpha = paint.getAlpha();

    return this->INHERITED::setContext(device, paint, matrix);
}

uint32_t SkTransparentShader::getFlags() {
    uint32_t flags = this->INHERITED::getFlags();

    switch (fDevice->config()) {
        case SkBitmap::kRGB_565_Config:
            flags |= kHasSpan16_Flag;
            if (fAlpha == 255)
                flags |= kOpaqueAlpha_Flag;
            break;
        case SkBitmap::kARGB_8888_Config:
            if (fAlpha == 255 && fDevice->isOpaque())
                flags |= kOpaqueAlpha_Flag;
            break;
        default:
            break;
    }
    return flags;
}

void SkTransparentShader::shadeSpan(int x, int y, SkPMColor span[], int count) {
    unsigned scale = SkAlpha255To256(fAlpha);

    switch (fDevice->config()) {
        case SkBitmap::kARGB_8888_Config:
            if (scale == 256) {
                SkPMColor* src = fDevice->getAddr32(x, y);
                if (src != span) {
                    memcpy(span, src, count * sizeof(SkPMColor));
                }
            } else {
                const SkPMColor* src = fDevice->getAddr32(x, y);
                for (int i = count - 1; i >= 0; --i) {
                    span[i] = SkAlphaMulQ(src[i], scale);
                }
            }
            break;
        case SkBitmap::kRGB_565_Config: {
            const uint16_t* src = fDevice->getAddr16(x, y);
            if (scale == 256) {
                for (int i = count - 1; i >= 0; --i) {
                    span[i] = SkPixel16ToPixel32(src[i]);
                }
            } else {
                unsigned alpha = fAlpha;
                for (int i = count - 1; i >= 0; --i) {
                    uint16_t c = src[i];
                    unsigned r = SkPacked16ToR32(c);
                    unsigned g = SkPacked16ToG32(c);
                    unsigned b = SkPacked16ToB32(c);

                    span[i] = SkPackARGB32( alpha,
                                            SkAlphaMul(r, scale),
                                            SkAlphaMul(g, scale),
                                            SkAlphaMul(b, scale));
                }
            }
            break;
        }
        case SkBitmap::kIndex8_Config:
            SkDEBUGFAIL("index8 not supported as a destination device");
            break;
        case SkBitmap::kA8_Config: {
            const uint8_t* src = fDevice->getAddr8(x, y);
            if (scale == 256) {
                for (int i = count - 1; i >= 0; --i) {
                    span[i] = SkPackARGB32(src[i], 0, 0, 0);
                }
            } else {
                for (int i = count - 1; i >= 0; --i) {
                    span[i] = SkPackARGB32(SkAlphaMul(src[i], scale), 0, 0, 0);
                }
            }
            break;
        }
        default:    // to avoid warnings
            break;
    }
}

void SkTransparentShader::shadeSpan16(int x, int y, uint16_t span[], int count) {
    SkASSERT(fDevice->config() == SkBitmap::kRGB_565_Config);

    uint16_t* src = fDevice->getAddr16(x, y);
    if (src != span) {
        memcpy(span, src, count << 1);
    }
}

#ifdef SK_DEVELOPER
void SkTransparentShader::toString(SkString* str) const {
    str->append("SkTransparentShader: (");

    this->INHERITED::toString(str);

    str->append(")");
}
#endif
