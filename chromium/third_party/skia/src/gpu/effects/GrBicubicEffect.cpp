#include "GrBicubicEffect.h"

#define DS(x) SkDoubleToScalar(x)

const SkScalar GrBicubicEffect::gMitchellCoefficients[16] = {
    DS( 1.0 / 18.0), DS(-9.0 / 18.0), DS( 15.0 / 18.0), DS( -7.0 / 18.0),
    DS(16.0 / 18.0), DS( 0.0 / 18.0), DS(-36.0 / 18.0), DS( 21.0 / 18.0),
    DS( 1.0 / 18.0), DS( 9.0 / 18.0), DS( 27.0 / 18.0), DS(-21.0 / 18.0),
    DS( 0.0 / 18.0), DS( 0.0 / 18.0), DS( -6.0 / 18.0), DS(  7.0 / 18.0),
};


class GrGLBicubicEffect : public GrGLEffect {
public:
    GrGLBicubicEffect(const GrBackendEffectFactory& factory,
                      const GrDrawEffect&);
    virtual void emitCode(GrGLShaderBuilder*,
                          const GrDrawEffect&,
                          EffectKey,
                          const char* outputColor,
                          const char* inputColor,
                          const TransformedCoordsArray&,
                          const TextureSamplerArray&) SK_OVERRIDE;

    virtual void setData(const GrGLUniformManager&, const GrDrawEffect&) SK_OVERRIDE;

private:
    typedef GrGLUniformManager::UniformHandle        UniformHandle;

    UniformHandle       fCoefficientsUni;
    UniformHandle       fImageIncrementUni;

    typedef GrGLEffect INHERITED;
};

GrGLBicubicEffect::GrGLBicubicEffect(const GrBackendEffectFactory& factory, const GrDrawEffect&)
    : INHERITED(factory) {
}

void GrGLBicubicEffect::emitCode(GrGLShaderBuilder* builder,
                                 const GrDrawEffect&,
                                 EffectKey key,
                                 const char* outputColor,
                                 const char* inputColor,
                                 const TransformedCoordsArray& coords,
                                 const TextureSamplerArray& samplers) {
    sk_ignore_unused_variable(inputColor);

    SkString coords2D = builder->ensureFSCoords2D(coords, 0);
    fCoefficientsUni = builder->addUniform(GrGLShaderBuilder::kFragment_Visibility,
                                           kMat44f_GrSLType, "Coefficients");
    fImageIncrementUni = builder->addUniform(GrGLShaderBuilder::kFragment_Visibility,
                                             kVec2f_GrSLType, "ImageIncrement");

    const char* imgInc = builder->getUniformCStr(fImageIncrementUni);
    const char* coeff = builder->getUniformCStr(fCoefficientsUni);

    SkString cubicBlendName;

    static const GrGLShaderVar gCubicBlendArgs[] = {
        GrGLShaderVar("coefficients",  kMat44f_GrSLType),
        GrGLShaderVar("t",             kFloat_GrSLType),
        GrGLShaderVar("c0",            kVec4f_GrSLType),
        GrGLShaderVar("c1",            kVec4f_GrSLType),
        GrGLShaderVar("c2",            kVec4f_GrSLType),
        GrGLShaderVar("c3",            kVec4f_GrSLType),
    };
    builder->fsEmitFunction(kVec4f_GrSLType,
                            "cubicBlend",
                            SK_ARRAY_COUNT(gCubicBlendArgs),
                            gCubicBlendArgs,
                            "\tvec4 ts = vec4(1.0, t, t * t, t * t * t);\n"
                            "\tvec4 c = coefficients * ts;\n"
                            "\treturn c.x * c0 + c.y * c1 + c.z * c2 + c.w * c3;\n",
                            &cubicBlendName);
    builder->fsCodeAppendf("\tvec2 coord = %s - %s * vec2(0.5);\n", coords2D.c_str(), imgInc);
    // We unnormalize the coord in order to determine our fractional offset (f) within the texel
    // We then snap coord to a texel center and renormalize. The snap prevents cases where the
    // starting coords are near a texel boundary and accumulations of imgInc would cause us to skip/
    // double hit a texel.
    builder->fsCodeAppendf("\tcoord /= %s;\n", imgInc);
    builder->fsCodeAppend("\tvec2 f = fract(coord);\n");
    builder->fsCodeAppendf("\tcoord = (coord - f + vec2(0.5)) * %s;\n", imgInc);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            SkString coord;
            coord.printf("coord + %s * vec2(%d, %d)", imgInc, x - 1, y - 1);
            builder->fsCodeAppendf("\tvec4 s%d%d = ", x, y);
            builder->fsAppendTextureLookup(samplers[0], coord.c_str());
            builder->fsCodeAppend(";\n");
        }
        builder->fsCodeAppendf("\tvec4 s%d = %s(%s, f.x, s0%d, s1%d, s2%d, s3%d);\n", y, cubicBlendName.c_str(), coeff, y, y, y, y);
    }
    builder->fsCodeAppendf("\t%s = %s(%s, f.y, s0, s1, s2, s3);\n", outputColor, cubicBlendName.c_str(), coeff);
}

void GrGLBicubicEffect::setData(const GrGLUniformManager& uman,
                                const GrDrawEffect& drawEffect) {
    const GrBicubicEffect& effect = drawEffect.castEffect<GrBicubicEffect>();
    GrTexture& texture = *effect.texture(0);
    float imageIncrement[2];
    imageIncrement[0] = 1.0f / texture.width();
    imageIncrement[1] = 1.0f / texture.height();
    uman.set2fv(fImageIncrementUni, 1, imageIncrement);
    uman.setMatrix4f(fCoefficientsUni, effect.coefficients());
}

GrBicubicEffect::GrBicubicEffect(GrTexture* texture,
                                 const SkScalar coefficients[16],
                                 const SkMatrix &matrix,
                                 const SkShader::TileMode tileModes[2])
  : INHERITED(texture, matrix, GrTextureParams(tileModes, GrTextureParams::kNone_FilterMode)) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            // Convert from row-major scalars to column-major floats.
            fCoefficients[x * 4 + y] = SkScalarToFloat(coefficients[y * 4 + x]);
        }
    }
    this->setWillNotUseInputColor();
}

GrBicubicEffect::~GrBicubicEffect() {
}

const GrBackendEffectFactory& GrBicubicEffect::getFactory() const {
    return GrTBackendEffectFactory<GrBicubicEffect>::getInstance();
}

bool GrBicubicEffect::onIsEqual(const GrEffect& sBase) const {
    const GrBicubicEffect& s = CastEffect<GrBicubicEffect>(sBase);
    return this->textureAccess(0) == s.textureAccess(0) &&
           !memcmp(fCoefficients, s.coefficients(), 16);
}

void GrBicubicEffect::getConstantColorComponents(GrColor* color, uint32_t* validFlags) const {
    // FIXME:  Perhaps we can do better.
    *validFlags = 0;
    return;
}

GR_DEFINE_EFFECT_TEST(GrBicubicEffect);

GrEffectRef* GrBicubicEffect::TestCreate(SkRandom* random,
                                         GrContext* context,
                                         const GrDrawTargetCaps&,
                                         GrTexture* textures[]) {
    int texIdx = random->nextBool() ? GrEffectUnitTest::kSkiaPMTextureIdx :
                                      GrEffectUnitTest::kAlphaTextureIdx;
    SkScalar coefficients[16];
    for (int i = 0; i < 16; i++) {
        coefficients[i] = random->nextSScalar1();
    }
    return GrBicubicEffect::Create(textures[texIdx], coefficients);
}
