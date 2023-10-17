/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkLocalMatrixShader_DEFINED
#define SkLocalMatrixShader_DEFINED

#include "include/core/SkFlattenable.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkShader.h"
#include "include/core/SkTypes.h"
#include "src/shaders/SkShaderBase.h"

#include <type_traits>
#include <utility>

class SkArenaAlloc;
class SkImage;
class SkReadBuffer;
class SkWriteBuffer;
enum class SkTileMode;
struct SkStageRec;

class SkLocalMatrixShader final : public SkShaderBase {
public:
    template <typename T, typename... Args>
    static std::enable_if_t<std::is_base_of_v<SkShader, T>, sk_sp<SkShader>>
    MakeWrapped(const SkMatrix* localMatrix, Args&&... args) {
        auto t = sk_make_sp<T>(std::forward<Args>(args)...);
        if (!localMatrix || localMatrix->isIdentity()) {
            return std::move(t);
        }
        return sk_make_sp<SkLocalMatrixShader>(sk_sp<SkShader>(std::move(t)), *localMatrix);
    }

    SkLocalMatrixShader(sk_sp<SkShader> wrapped, const SkMatrix& localMatrix)
            : fLocalMatrix(localMatrix), fWrappedShader(std::move(wrapped)) {}

    GradientType asGradient(GradientInfo* info, SkMatrix* localMatrix) const override;
    ShaderType type() const override { return ShaderType::kLocalMatrix; }

#if defined(SK_GRAPHITE)
    void addToKey(const skgpu::graphite::KeyContext&,
                  skgpu::graphite::PaintParamsKeyBuilder*,
                  skgpu::graphite::PipelineDataGatherer*) const override;
#endif

    sk_sp<SkShader> makeAsALocalMatrixShader(SkMatrix* localMatrix) const override {
        if (localMatrix) {
            *localMatrix = fLocalMatrix;
        }
        return fWrappedShader;
    }

    const SkMatrix& localMatrix() const { return fLocalMatrix; }
    sk_sp<SkShader> wrappedShader() const { return fWrappedShader; }

protected:
    void flatten(SkWriteBuffer&) const override;

#ifdef SK_ENABLE_LEGACY_SHADERCONTEXT
    Context* onMakeContext(const ContextRec&, SkArenaAlloc*) const override;
#endif

    SkImage* onIsAImage(SkMatrix* matrix, SkTileMode* mode) const override;

    bool appendStages(const SkStageRec&, const SkShaders::MatrixRec&) const override;

#if defined(SK_ENABLE_SKVM)
    skvm::Color program(skvm::Builder*,
                        skvm::Coord device,
                        skvm::Coord local,
                        skvm::Color paint,
                        const SkShaders::MatrixRec&,
                        const SkColorInfo& dst,
                        skvm::Uniforms* uniforms,
                        SkArenaAlloc*) const override;
#endif

private:
    SK_FLATTENABLE_HOOKS(SkLocalMatrixShader)

    SkMatrix fLocalMatrix;
    sk_sp<SkShader> fWrappedShader;
};

/**
 *  Replaces the CTM when used. Created to support clipShaders, which have to be evaluated
 *  using the CTM that was present at the time they were specified (which may be different
 *  from the CTM at the time something is drawn through the clip.
 */
class SkCTMShader final : public SkShaderBase {
public:
    SkCTMShader(sk_sp<SkShader> proxy, const SkMatrix& ctm);

    GradientType asGradient(GradientInfo* info, SkMatrix* localMatrix) const override;

    ShaderType type() const override { return ShaderType::kCTM; }

    const SkMatrix& ctm() const { return fCTM; }
    sk_sp<SkShader> proxyShader() const { return fProxyShader; }

protected:
    void flatten(SkWriteBuffer&) const override { SkASSERT(false); }

    bool appendStages(const SkStageRec& rec, const SkShaders::MatrixRec&) const override;

#if defined(SK_ENABLE_SKVM)
    skvm::Color program(skvm::Builder* p,
                        skvm::Coord device,
                        skvm::Coord local,
                        skvm::Color paint,
                        const SkShaders::MatrixRec& mRec,
                        const SkColorInfo& dst,
                        skvm::Uniforms* uniforms,
                        SkArenaAlloc* alloc) const override;
#endif

private:
    SK_FLATTENABLE_HOOKS(SkCTMShader)

    sk_sp<SkShader> fProxyShader;
    SkMatrix fCTM;
};

#endif
