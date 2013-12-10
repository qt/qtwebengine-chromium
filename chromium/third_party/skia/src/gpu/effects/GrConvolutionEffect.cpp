/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrConvolutionEffect.h"
#include "gl/GrGLEffect.h"
#include "gl/GrGLEffectMatrix.h"
#include "gl/GrGLSL.h"
#include "gl/GrGLTexture.h"
#include "GrTBackendEffectFactory.h"

// For brevity
typedef GrGLUniformManager::UniformHandle UniformHandle;

class GrGLConvolutionEffect : public GrGLEffect {
public:
    GrGLConvolutionEffect(const GrBackendEffectFactory&, const GrDrawEffect&);

    virtual void emitCode(GrGLShaderBuilder*,
                          const GrDrawEffect&,
                          EffectKey,
                          const char* outputColor,
                          const char* inputColor,
                          const TextureSamplerArray&) SK_OVERRIDE;

    virtual void setData(const GrGLUniformManager& uman, const GrDrawEffect&) SK_OVERRIDE;

    static inline EffectKey GenKey(const GrDrawEffect&, const GrGLCaps&);

private:
    int width() const { return Gr1DKernelEffect::WidthFromRadius(fRadius); }
    bool useBounds() const { return fUseBounds; }
    Gr1DKernelEffect::Direction direction() const { return fDirection; }

    int                 fRadius;
    bool                fUseBounds;
    Gr1DKernelEffect::Direction    fDirection;
    UniformHandle       fKernelUni;
    UniformHandle       fImageIncrementUni;
    UniformHandle       fBoundsUni;
    GrGLEffectMatrix    fEffectMatrix;

    typedef GrGLEffect INHERITED;
};

GrGLConvolutionEffect::GrGLConvolutionEffect(const GrBackendEffectFactory& factory,
                                             const GrDrawEffect& drawEffect)
    : INHERITED(factory)
    , fEffectMatrix(drawEffect.castEffect<GrConvolutionEffect>().coordsType()) {
    const GrConvolutionEffect& c = drawEffect.castEffect<GrConvolutionEffect>();
    fRadius = c.radius();
    fUseBounds = c.useBounds();
    fDirection = c.direction();
}

void GrGLConvolutionEffect::emitCode(GrGLShaderBuilder* builder,
                                     const GrDrawEffect&,
                                     EffectKey key,
                                     const char* outputColor,
                                     const char* inputColor,
                                     const TextureSamplerArray& samplers) {
    SkString coords;
    fEffectMatrix.emitCodeMakeFSCoords2D(builder, key, &coords);
    fImageIncrementUni = builder->addUniform(GrGLShaderBuilder::kFragment_Visibility,
                                             kVec2f_GrSLType, "ImageIncrement");
    if (this->useBounds()) {
        fBoundsUni = builder->addUniform(GrGLShaderBuilder::kFragment_Visibility,
                                         kVec2f_GrSLType, "Bounds");
    }
    fKernelUni = builder->addUniformArray(GrGLShaderBuilder::kFragment_Visibility,
                                          kFloat_GrSLType, "Kernel", this->width());

    builder->fsCodeAppendf("\t\t%s = vec4(0, 0, 0, 0);\n", outputColor);

    int width = this->width();
    const GrGLShaderVar& kernel = builder->getUniformVariable(fKernelUni);
    const char* imgInc = builder->getUniformCStr(fImageIncrementUni);

    builder->fsCodeAppendf("\t\tvec2 coord = %s - %d.0 * %s;\n", coords.c_str(), fRadius, imgInc);

    // Manually unroll loop because some drivers don't; yields 20-30% speedup.
    for (int i = 0; i < width; i++) {
        SkString index;
        SkString kernelIndex;
        index.appendS32(i);
        kernel.appendArrayAccess(index.c_str(), &kernelIndex);
        builder->fsCodeAppendf("\t\t%s += ", outputColor);
        builder->fsAppendTextureLookup(samplers[0], "coord");
        if (this->useBounds()) {
            const char* bounds = builder->getUniformCStr(fBoundsUni);
            const char* component = this->direction() == Gr1DKernelEffect::kY_Direction ? "y" : "x";
            builder->fsCodeAppendf(" * float(coord.%s >= %s.x && coord.%s <= %s.y)",
                component, bounds, component, bounds);
        }
        builder->fsCodeAppendf(" * %s;\n", kernelIndex.c_str());
        builder->fsCodeAppendf("\t\tcoord += %s;\n", imgInc);
    }

    SkString modulate;
    GrGLSLMulVarBy4f(&modulate, 2, outputColor, inputColor);
    builder->fsCodeAppend(modulate.c_str());
}

void GrGLConvolutionEffect::setData(const GrGLUniformManager& uman,
                                    const GrDrawEffect& drawEffect) {
    const GrConvolutionEffect& conv = drawEffect.castEffect<GrConvolutionEffect>();
    GrTexture& texture = *conv.texture(0);
    // the code we generated was for a specific kernel radius
    SkASSERT(conv.radius() == fRadius);
    float imageIncrement[2] = { 0 };
    float ySign = texture.origin() != kTopLeft_GrSurfaceOrigin ? 1.0f : -1.0f;
    switch (conv.direction()) {
        case Gr1DKernelEffect::kX_Direction:
            imageIncrement[0] = 1.0f / texture.width();
            break;
        case Gr1DKernelEffect::kY_Direction:
            imageIncrement[1] = ySign / texture.height();
            break;
        default:
            GrCrash("Unknown filter direction.");
    }
    uman.set2fv(fImageIncrementUni, 0, 1, imageIncrement);
    if (conv.useBounds()) {
        const float* bounds = conv.bounds();
        if (Gr1DKernelEffect::kY_Direction == conv.direction() &&
            texture.origin() != kTopLeft_GrSurfaceOrigin) {
            uman.set2f(fBoundsUni, 1.0f - bounds[1], 1.0f - bounds[0]);
        } else {
            uman.set2f(fBoundsUni, bounds[0], bounds[1]);
        }
    }
    uman.set1fv(fKernelUni, 0, this->width(), conv.kernel());
    fEffectMatrix.setData(uman, conv.getMatrix(), drawEffect, conv.texture(0));
}

GrGLEffect::EffectKey GrGLConvolutionEffect::GenKey(const GrDrawEffect& drawEffect,
                                                    const GrGLCaps&) {
    const GrConvolutionEffect& conv = drawEffect.castEffect<GrConvolutionEffect>();
    EffectKey key = conv.radius();
    key <<= 2;
    if (conv.useBounds()) {
        key |= 0x2;
        key |= GrConvolutionEffect::kY_Direction == conv.direction() ? 0x1 : 0x0;
    }
    key <<= GrGLEffectMatrix::kKeyBits;
    EffectKey matrixKey = GrGLEffectMatrix::GenKey(conv.getMatrix(),
                                                   drawEffect,
                                                   conv.coordsType(),
                                                   conv.texture(0));
    return key | matrixKey;
}

///////////////////////////////////////////////////////////////////////////////

GrConvolutionEffect::GrConvolutionEffect(GrTexture* texture,
                                         Direction direction,
                                         int radius,
                                         const float* kernel,
                                         bool useBounds,
                                         float bounds[2])
    : Gr1DKernelEffect(texture, direction, radius), fUseBounds(useBounds) {
    SkASSERT(radius <= kMaxKernelRadius);
    SkASSERT(NULL != kernel);
    int width = this->width();
    for (int i = 0; i < width; i++) {
        fKernel[i] = kernel[i];
    }
    memcpy(fBounds, bounds, sizeof(fBounds));
}

GrConvolutionEffect::GrConvolutionEffect(GrTexture* texture,
                                         Direction direction,
                                         int radius,
                                         float gaussianSigma,
                                         bool useBounds,
                                         float bounds[2])
    : Gr1DKernelEffect(texture, direction, radius), fUseBounds(useBounds) {
    SkASSERT(radius <= kMaxKernelRadius);
    int width = this->width();

    float sum = 0.0f;
    float denom = 1.0f / (2.0f * gaussianSigma * gaussianSigma);
    for (int i = 0; i < width; ++i) {
        float x = static_cast<float>(i - this->radius());
        // Note that the constant term (1/(sqrt(2*pi*sigma^2)) of the Gaussian
        // is dropped here, since we renormalize the kernel below.
        fKernel[i] = sk_float_exp(- x * x * denom);
        sum += fKernel[i];
    }
    // Normalize the kernel
    float scale = 1.0f / sum;
    for (int i = 0; i < width; ++i) {
        fKernel[i] *= scale;
    }
    memcpy(fBounds, bounds, sizeof(fBounds));
}

GrConvolutionEffect::~GrConvolutionEffect() {
}

const GrBackendEffectFactory& GrConvolutionEffect::getFactory() const {
    return GrTBackendEffectFactory<GrConvolutionEffect>::getInstance();
}

bool GrConvolutionEffect::onIsEqual(const GrEffect& sBase) const {
    const GrConvolutionEffect& s = CastEffect<GrConvolutionEffect>(sBase);
    return (this->texture(0) == s.texture(0) &&
            this->radius() == s.radius() &&
            this->direction() == s.direction() &&
            this->useBounds() == s.useBounds() &&
            0 == memcmp(fBounds, s.fBounds, sizeof(fBounds)) &&
            0 == memcmp(fKernel, s.fKernel, this->width() * sizeof(float)));
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_EFFECT_TEST(GrConvolutionEffect);

GrEffectRef* GrConvolutionEffect::TestCreate(SkRandom* random,
                                             GrContext*,
                                             const GrDrawTargetCaps&,
                                             GrTexture* textures[]) {
    int texIdx = random->nextBool() ? GrEffectUnitTest::kSkiaPMTextureIdx :
                                      GrEffectUnitTest::kAlphaTextureIdx;
    Direction dir = random->nextBool() ? kX_Direction : kY_Direction;
    int radius = random->nextRangeU(1, kMaxKernelRadius);
    float kernel[kMaxKernelWidth];
    for (size_t i = 0; i < SK_ARRAY_COUNT(kernel); ++i) {
        kernel[i] = random->nextSScalar1();
    }
    float bounds[2];
    for (size_t i = 0; i < SK_ARRAY_COUNT(bounds); ++i) {
        bounds[i] = random->nextF();
    }

    bool useBounds = random->nextBool();
    return GrConvolutionEffect::Create(textures[texIdx],
                                       dir,
                                       radius,
                                       kernel,
                                       useBounds,
                                       bounds);
}
