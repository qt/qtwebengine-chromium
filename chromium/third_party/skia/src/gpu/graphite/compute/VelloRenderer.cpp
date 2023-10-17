/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/compute/VelloRenderer.h"

#include "include/core/SkPath.h"
#include "include/core/SkTypes.h"
#include "include/gpu/graphite/Recorder.h"
#include "src/core/SkGeometry.h"
#include "src/core/SkPathPriv.h"
#include "src/gpu/graphite/BufferManager.h"
#include "src/gpu/graphite/Caps.h"
#include "src/gpu/graphite/DrawParams.h"
#include "src/gpu/graphite/Log.h"
#include "src/gpu/graphite/PipelineData.h"
#include "src/gpu/graphite/RecorderPriv.h"
#include "src/gpu/graphite/TextureProxy.h"
#include "src/gpu/graphite/UniformManager.h"
#include "src/gpu/graphite/compute/DispatchGroup.h"

#include <algorithm>

namespace skgpu::graphite {
namespace {

::rust::Slice<uint8_t> to_slice(void* ptr, size_t size) {
    return {static_cast<uint8_t*>(ptr), size};
}

vello_cpp::Affine to_vello_affine(const SkMatrix& m) {
    // Vello currently doesn't support perspective scaling and the encoding only accepts a 2x3
    // affine transform matrix.
    return {{m.get(0), m.get(3), m.get(1), m.get(4), m.get(2), m.get(5)}};
}

vello_cpp::Point to_vello_point(const SkPoint& p) { return {p.x(), p.y()}; }

vello_cpp::Color to_vello_color(const SkColor4f& color) {
    SkColor c = color.toSkColor();
    return {
            static_cast<uint8_t>(SkColorGetR(c)),
            static_cast<uint8_t>(SkColorGetG(c)),
            static_cast<uint8_t>(SkColorGetB(c)),
            static_cast<uint8_t>(SkColorGetA(c)),
    };
}

WorkgroupSize to_wg_size(const vello_cpp::WorkgroupSize& src) {
    return WorkgroupSize(src.x, src.y, src.z);
}

vello_cpp::Fill to_fill_type(SkPathFillType fillType) {
    switch (fillType) {
        case SkPathFillType::kWinding:
            return vello_cpp::Fill::NonZero;
        case SkPathFillType::kEvenOdd:
            return vello_cpp::Fill::EvenOdd;
        default:
            // TODO(b/238756757): vello doesn't define fill types for kInverseWinding and
            // kInverseEvenOdd. This should be updated to support those cases.
            SkDebugf("fill type not supported by vello\n");
            break;
    }
    return vello_cpp::Fill::NonZero;
}

class PathIter : public vello_cpp::PathIterator {
public:
    PathIter(const SkPath& path, const Transform& t)
            : fIterate(path), fIter(fIterate.begin()), fTransform(t) {}

    bool next_element(vello_cpp::PathElement* outElem) override {
        if (fConicQuadIdx < fConicConverter.countQuads()) {
            SkASSERT(fConicQuads != nullptr);
            outElem->verb = vello_cpp::PathVerb::QuadTo;
            int pointIdx = fConicQuadIdx * 2;
            outElem->points[0] = to_vello_point(fConicQuads[pointIdx]);
            outElem->points[1] = to_vello_point(fConicQuads[pointIdx + 1]);
            outElem->points[2] = to_vello_point(fConicQuads[pointIdx + 2]);
            fConicQuadIdx++;
            return true;
        }

        if (fIter == fIterate.end()) {
            return false;
        }

        SkASSERT(outElem);
        auto [verb, points, weights] = *fIter;
        fIter++;

        switch (verb) {
            case SkPathVerb::kMove:
                outElem->verb = vello_cpp::PathVerb::MoveTo;
                outElem->points[0] = to_vello_point(points[0]);
                break;
            case SkPathVerb::kLine:
                outElem->verb = vello_cpp::PathVerb::LineTo;
                outElem->points[0] = to_vello_point(points[0]);
                outElem->points[1] = to_vello_point(points[1]);
                break;
            case SkPathVerb::kConic:
                // The vello encoding API doesn't handle conic sections. Approximate it with
                // quadratic Béziers.
                SkASSERT(fConicQuadIdx >= fConicConverter.countQuads());  // No other conic->quad
                                                                          // conversions should be
                                                                          // in progress
                fConicQuads = fConicConverter.computeQuads(
                        points, *weights, 0.25 / fTransform.maxScaleFactor());
                outElem->verb = vello_cpp::PathVerb::QuadTo;
                outElem->points[0] = to_vello_point(fConicQuads[0]);
                outElem->points[1] = to_vello_point(fConicQuads[1]);
                outElem->points[2] = to_vello_point(fConicQuads[2]);

                // The next call to `next_element` will yield the next quad in the list (at index 1)
                // if `fConicConverter` contains more than 1 quad.
                fConicQuadIdx = 1;
                break;
            case SkPathVerb::kQuad:
                outElem->verb = vello_cpp::PathVerb::QuadTo;
                outElem->points[0] = to_vello_point(points[0]);
                outElem->points[1] = to_vello_point(points[1]);
                outElem->points[2] = to_vello_point(points[2]);
                break;
            case SkPathVerb::kCubic:
                outElem->verb = vello_cpp::PathVerb::CurveTo;
                outElem->points[0] = to_vello_point(points[0]);
                outElem->points[1] = to_vello_point(points[1]);
                outElem->points[2] = to_vello_point(points[2]);
                outElem->points[3] = to_vello_point(points[3]);
                break;
            case SkPathVerb::kClose:
                outElem->verb = vello_cpp::PathVerb::Close;
                break;
        }

        return true;
    }

private:
    SkPathPriv::Iterate fIterate;
    SkPathPriv::RangeIter fIter;

    // Variables used to track conic to quadratic spline conversion. `fTransform` is used to
    // determine the subpixel error tolerance in device coordinate space.
    const Transform& fTransform;
    SkAutoConicToQuads fConicConverter;
    const SkPoint* fConicQuads = nullptr;
    int fConicQuadIdx = 0;
};

}  // namespace

VelloScene::VelloScene() : fEncoding(vello_cpp::new_encoding()) {}

void VelloScene::reset() {
    fEncoding->reset();
}

void VelloScene::solidFill(const SkPath& shape,
                           const SkColor4f& fillColor,
                           const SkPathFillType fillType,
                           const Transform& t) {
    PathIter iter(shape, t);
    fEncoding->fill(to_fill_type(fillType),
                    to_vello_affine(t),
                    {vello_cpp::BrushKind::Solid, {to_vello_color(fillColor)}},
                    iter);
}

void VelloScene::solidStroke(const SkPath& shape,
                             const SkColor4f& fillColor,
                             float width,
                             const Transform& t) {
    // Vello currently only supports round stroke styles
    PathIter iter(shape, t);
    fEncoding->stroke({width},
                      to_vello_affine(t),
                      {vello_cpp::BrushKind::Solid, {to_vello_color(fillColor)}},
                      iter);
}

void VelloScene::pushClipLayer(const SkPath& shape, const Transform& t) {
    PathIter iter(shape, t);
    fEncoding->begin_clip(to_vello_affine(t), iter);
    SkDEBUGCODE(fLayers++;)
}

void VelloScene::popClipLayer() {
    SkASSERT(fLayers > 0);
    fEncoding->end_clip();
    SkDEBUGCODE(fLayers--;)
}

VelloRenderer::VelloRenderer(const Caps* caps) {
    fGradientImage = TextureProxy::Make(caps,
                                        {1, 1},
                                        kRGBA_8888_SkColorType,
                                        skgpu::Mipmapped::kNo,
                                        skgpu::Protected::kNo,
                                        skgpu::Renderable::kNo,
                                        skgpu::Budgeted::kYes);
    fImageAtlas = TextureProxy::Make(caps,
                                     {1, 1},
                                     kRGBA_8888_SkColorType,
                                     skgpu::Mipmapped::kNo,
                                     skgpu::Protected::kNo,
                                     skgpu::Renderable::kNo,
                                     skgpu::Budgeted::kYes);
}

VelloRenderer::~VelloRenderer() = default;

std::unique_ptr<DispatchGroup> VelloRenderer::renderScene(const RenderParams& params,
                                                          const VelloScene& scene,
                                                          sk_sp<TextureProxy> target,
                                                          Recorder* recorder) const {
    SkASSERT(target);

    if (scene.fEncoding->is_empty()) {
        return nullptr;
    }

    if (params.fWidth == 0 || params.fHeight == 0) {
        return nullptr;
    }

    // TODO: validate that the pixel format is kRGBA_8888_SkColorType.
    // Clamp the draw region to the target texture dimensions.
    const SkISize dims = target->dimensions();
    if (dims.isEmpty() || dims.fWidth < 0 || dims.fHeight < 0) {
        SKGPU_LOG_W("VelloRenderer: cannot render to an empty target");
        return nullptr;
    }

    SkASSERT(scene.fLayers == 0);  // Begin/end clips must be matched.
    auto config = scene.fEncoding->prepare_render(
            std::min(params.fWidth, static_cast<uint32_t>(dims.fWidth)),
            std::min(params.fHeight, static_cast<uint32_t>(dims.fHeight)),
            to_vello_color(params.fBaseColor));
    auto dispatchInfo = config->workgroup_counts();
    auto bufferSizes = config->buffer_sizes();

    // TODO(b/279955342): VelloComputeSteps do not perform any per-draw computation. Instead all
    // buffer and global dispatch sizes get computed upfront from the scene encoding
    // (in `prepare_render()` above).
    //
    // We should have an alternate appendStep() interface for ComputeSteps that implement per-draw
    // logic for batched processing (e.g. geometry dispatches that offload CPU-side RenderStep
    // logic). DispatchGroup::Builder can operate over pre-computed data without requiring a
    // DrawParams parameter and we should remove the need for a `placeholder`.
    DrawParams placeholder(Transform::Identity(), {}, {}, DrawOrder({}), nullptr);
    DispatchGroup::Builder builder(recorder);

    // In total there are 25 resources that are used across the full pipeline stages. The sizes of
    // these resources depend on the encoded scene. We allocate all of them and assign them
    // directly to the builder here instead of delegating the logic to the ComputeSteps.
    DrawBufferManager* bufMgr = recorder->priv().drawBufferManager();

    size_t uboSize = config->config_uniform_buffer_size();
    auto [uboPtr, configBuf] = bufMgr->getUniformPointer(uboSize);
    if (!config->write_config_uniform_buffer(to_slice(uboPtr, uboSize))) {
        return nullptr;
    }

    size_t sceneSize = config->scene_buffer_size();
    auto [scenePtr, sceneBuf] = bufMgr->getStoragePointer(sceneSize);
    if (!config->write_scene_buffer(to_slice(scenePtr, sceneSize))) {
        return nullptr;
    }

    // TODO(b/285189802): The default sizes for the bump buffers (~97MB) exceed Graphite's resource
    // budget if multiple passes are necessary per frame (250MB, see ResouceCache.h). We shrink
    // them by half here as a crude reduction which seems to be enough for a 4k x 4k atlas render
    // even in dense situations (e.g. paris-30k). We need to come up with a better approach
    // to accurately predict the sizes for these buffers based on the scene encoding and our
    // resource budget.
    //
    // The following numbers amount to ~48MB
    const size_t bin_data_size = bufferSizes.bin_data / 2;
    const size_t tiles_size = bufferSizes.tiles / 2;
    const size_t segments_size = bufferSizes.segments / 2;
    const size_t ptcl_size = bufferSizes.ptcl / 2;

    // See the comments in VelloComputeSteps.h for an explanation of the logic here.

    builder.assignSharedBuffer(configBuf, kVelloSlot_ConfigUniform);
    builder.assignSharedBuffer(sceneBuf, kVelloSlot_Scene);

    // path_reduce
    auto pathtagReduceOutput = bufMgr->getStorage(bufferSizes.path_reduced);
    auto tagmonoid = bufMgr->getStorage(bufferSizes.path_monoids);
    builder.assignSharedBuffer(pathtagReduceOutput, kVelloSlot_PathtagReduceOutput);
    builder.assignSharedBuffer(tagmonoid, kVelloSlot_TagMonoid);
    builder.appendStep(&fPathtagReduce, placeholder, 0, to_wg_size(dispatchInfo.path_reduce));

    // If the input is too large to be fully processed by a single workgroup then a second reduce
    // step and two scan steps are necessary. Otherwise one reduce+scan pair is sufficient.
    //
    // In either case, the result is `tagmonoids`.
    if (dispatchInfo.use_large_path_scan) {
        builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.path_reduced2),
                                   kVelloSlot_LargePathtagReduceSecondPassOutput);
        builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.path_reduced_scan),
                                   kVelloSlot_LargePathtagScanFirstPassOutput);
        builder.appendStep(&fPathtagReduce2, placeholder, 0, to_wg_size(dispatchInfo.path_reduce2));
        builder.appendStep(&fPathtagScan1, placeholder, 0, to_wg_size(dispatchInfo.path_scan1));
        builder.appendStep(&fPathtagScanLarge, placeholder, 0, to_wg_size(dispatchInfo.path_scan));
    } else {
        builder.appendStep(&fPathtagScanSmall, placeholder, 0, to_wg_size(dispatchInfo.path_scan));
    }

    // bbox_clear
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.path_bboxes), kVelloSlot_PathBBoxes);
    builder.appendStep(&fBboxClear, placeholder, 0, to_wg_size(dispatchInfo.bbox_clear));

    // pathseg
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.cubics), kVelloSlot_Cubics);
    builder.appendStep(&fPathseg, placeholder, 0, to_wg_size(dispatchInfo.path_seg));

    // draw_reduce
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.draw_reduced),
                               kVelloSlot_DrawReduceOutput);
    builder.appendStep(&fDrawReduce, placeholder, 0, to_wg_size(dispatchInfo.draw_reduce));

    // draw_leaf
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.draw_monoids), kVelloSlot_DrawMonoid);
    builder.assignSharedBuffer(bufMgr->getStorage(bin_data_size), kVelloSlot_InfoBinData);
    // A clip input buffer must still get bound even if the encoding doesn't contain any clips
    builder.assignSharedBuffer(bufMgr->getStorage(std::max(1u, bufferSizes.clip_inps)),
                               kVelloSlot_ClipInput);
    builder.appendStep(&fDrawLeaf, placeholder, 0, to_wg_size(dispatchInfo.draw_leaf));

    // clip_reduce, clip_leaf
    // The clip bbox buffer is always an input to the binning stage, even when the encoding doesn't
    // contain any clips
    builder.assignSharedBuffer(bufMgr->getStorage(std::max(1u, bufferSizes.clip_bboxes)),
                               kVelloSlot_ClipBBoxes);
    WorkgroupSize clipReduceWgCount = to_wg_size(dispatchInfo.clip_reduce);
    WorkgroupSize clipLeafWgCount = to_wg_size(dispatchInfo.clip_leaf);
    bool doClipReduce = clipReduceWgCount.scalarSize() > 0u;
    bool doClipLeaf = clipLeafWgCount.scalarSize() > 0u;
    if (doClipReduce || doClipLeaf) {
        builder.assignSharedBuffer(bufMgr->getStorage(std::max(1u, bufferSizes.clip_bics)),
                                   kVelloSlot_ClipBicyclic);
        builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.clip_els),
                                   kVelloSlot_ClipElement);
        if (doClipReduce) {
            builder.appendStep(&fClipReduce, placeholder, 0, clipReduceWgCount);
        }
        if (doClipLeaf) {
            builder.appendStep(&fClipLeaf, placeholder, 0, clipLeafWgCount);
        }
    }

    // binning
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.draw_bboxes), kVelloSlot_DrawBBoxes);
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.bump_alloc, ClearBuffer::kYes),
                               kVelloSlot_BumpAlloc);
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.bin_headers), kVelloSlot_BinHeader);
    builder.appendStep(&fBinning, placeholder, 0, to_wg_size(dispatchInfo.binning));

    // tile_alloc
    builder.assignSharedBuffer(bufMgr->getStorage(bufferSizes.paths), kVelloSlot_Path);
    builder.assignSharedBuffer(bufMgr->getStorage(tiles_size), kVelloSlot_Tile);
    builder.appendStep(&fTileAlloc, placeholder, 0, to_wg_size(dispatchInfo.tile_alloc));

    // path_coarse
    builder.assignSharedBuffer(bufMgr->getStorage(segments_size), kVelloSlot_Segments);
    builder.appendStep(&fPathCoarseFull, placeholder, 0, to_wg_size(dispatchInfo.path_coarse));

    // backdrop
    builder.appendStep(&fBackdropDyn, placeholder, 0, to_wg_size(dispatchInfo.backdrop));

    // coarse
    builder.assignSharedBuffer(bufMgr->getStorage(ptcl_size), kVelloSlot_PTCL);
    builder.appendStep(&fCoarse, placeholder, 0, to_wg_size(dispatchInfo.coarse));

    // fine
    builder.assignSharedTexture(fImageAtlas, kVelloSlot_ImageAtlas);
    builder.assignSharedTexture(fGradientImage, kVelloSlot_GradientImage);
    builder.assignSharedTexture(std::move(target), kVelloSlot_OutputImage);
    builder.appendStep(&fFine, placeholder, 0, to_wg_size(dispatchInfo.fine));

    return builder.finalize();
}

}  // namespace skgpu::graphite
