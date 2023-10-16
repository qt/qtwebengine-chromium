/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkBlendModeBlender_DEFINED
#define SkBlendModeBlender_DEFINED

#include "src/core/SkBlenderBase.h"

#include <memory>

class SkBlendModeBlender : public SkBlenderBase {
public:
    SkBlendModeBlender(SkBlendMode mode) : fMode(mode) {}

    SK_FLATTENABLE_HOOKS(SkBlendModeBlender)

private:
    using INHERITED = SkBlenderBase;

    std::optional<SkBlendMode> asBlendMode() const final { return fMode; }

#if defined(SK_GANESH)
    std::unique_ptr<GrFragmentProcessor> asFragmentProcessor(
            std::unique_ptr<GrFragmentProcessor> srcFP,
            std::unique_ptr<GrFragmentProcessor> dstFP,
            const GrFPArgs& fpArgs) const override;
#endif

#if defined(SK_GRAPHITE)
    void addToKey(const skgpu::graphite::KeyContext&,
                  skgpu::graphite::PaintParamsKeyBuilder*,
                  skgpu::graphite::PipelineDataGatherer*) const override;
#endif

    void flatten(SkWriteBuffer& buffer) const override;

    bool onAppendStages(const SkStageRec& rec) const override;

#if defined(SK_ENABLE_SKVM)
    skvm::Color onProgram(skvm::Builder* p, skvm::Color src, skvm::Color dst,
                          const SkColorInfo& colorInfo, skvm::Uniforms* uniforms,
                          SkArenaAlloc* alloc) const override;
#endif

    SkBlendMode fMode;
};

#endif
