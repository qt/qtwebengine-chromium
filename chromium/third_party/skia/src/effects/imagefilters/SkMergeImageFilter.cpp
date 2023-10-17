/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/effects/SkImageFilters.h"

#include "include/core/SkFlattenable.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTypes.h"
#include "src/core/SkImageFilterTypes.h"
#include "src/core/SkImageFilter_Base.h"
#include "src/core/SkReadBuffer.h"
#include "src/effects/imagefilters/SkCropImageFilter.h"

#include <utility>

namespace {

class SkMergeImageFilter final : public SkImageFilter_Base {
public:
    SkMergeImageFilter(sk_sp<SkImageFilter>* const filters, int count)
            : SkImageFilter_Base(filters, count, nullptr) {}

    SkRect computeFastBounds(const SkRect&) const override;

    // No need to override flatten() since there's no additional state to write over base class.

private:
    friend void ::SkRegisterMergeImageFilterFlattenable();
    SK_FLATTENABLE_HOOKS(SkMergeImageFilter)

    MatrixCapability onGetCTMCapability() const override { return MatrixCapability::kComplex; }

    skif::FilterResult onFilterImage(const skif::Context& ctx) const override;

    skif::LayerSpace<SkIRect> onGetInputLayerBounds(
            const skif::Mapping&,
            const skif::LayerSpace<SkIRect>& desiredOutput,
            const skif::LayerSpace<SkIRect>& contentBounds) const override;

    skif::LayerSpace<SkIRect> onGetOutputLayerBounds(
            const skif::Mapping&,
            const skif::LayerSpace<SkIRect>& contentBounds) const override;
};

} // end namespace

sk_sp<SkImageFilter> SkImageFilters::Merge(sk_sp<SkImageFilter>* const filters, int count,
                                           const CropRect& cropRect) {
    sk_sp<SkImageFilter> filter{new SkMergeImageFilter(filters, count)};
    if (cropRect) {
        filter = SkMakeCropImageFilter(*cropRect, std::move(filter));
    }
    return filter;
}

void SkRegisterMergeImageFilterFlattenable() {
    SK_REGISTER_FLATTENABLE(SkMergeImageFilter);
    // TODO (michaelludwig) - Remove after grace period for SKPs to stop using old name
    SkFlattenable::Register("SkMergeImageFilterImpl", SkMergeImageFilter::CreateProc);
}

sk_sp<SkFlattenable> SkMergeImageFilter::CreateProc(SkReadBuffer& buffer) {
    Common common;
    if (!common.unflatten(buffer, -1) || !buffer.isValid()) {
        return nullptr;
    }
    return SkImageFilters::Merge(common.inputs(), common.inputCount(), common.cropRect());
}

///////////////////////////////////////////////////////////////////////////////

skif::FilterResult SkMergeImageFilter::onFilterImage(const skif::Context& ctx) const {
    const int inputCount = this->countInputs();
    skif::FilterResult::Builder builder{ctx};
    for (int i = 0; i < inputCount; ++i) {
        builder.add(this->getChildOutput(i, ctx));
    }
    return builder.merge();
}

skif::LayerSpace<SkIRect> SkMergeImageFilter::onGetInputLayerBounds(
        const skif::Mapping& mapping,
        const skif::LayerSpace<SkIRect>& desiredOutput,
        const skif::LayerSpace<SkIRect>& contentBounds) const {
    const int inputCount = this->countInputs();
    if (inputCount <= 0) {
        // A leaf, so no required input or recursion
        return skif::LayerSpace<SkIRect>::Empty();
    } else {
        // Union of all child input bounds so that one source image can provide for all of them.
        skif::LayerSpace<SkIRect> merged =
                this->getChildInputLayerBounds(0, mapping, desiredOutput, contentBounds);
        for (int i = 1; i < inputCount; ++i) {
            merged.join(this->getChildInputLayerBounds(i, mapping, desiredOutput, contentBounds));
        }
        return merged;
    }
}

skif::LayerSpace<SkIRect> SkMergeImageFilter::onGetOutputLayerBounds(
        const skif::Mapping& mapping,
        const skif::LayerSpace<SkIRect>& contentBounds) const {
    const int inputCount = this->countInputs();
    if (inputCount <= 0) {
        return skif::LayerSpace<SkIRect>::Empty(); // Transparent black
    } else {
        // Merge is src-over of all child outputs, so covers their union but no more
        skif::LayerSpace<SkIRect> merged =
                this->getChildOutputLayerBounds(0, mapping, contentBounds);
        for (int i = 1; i < inputCount; ++i) {
            merged.join(this->getChildOutputLayerBounds(i, mapping, contentBounds));
        }
        return merged;
    }
}

SkRect SkMergeImageFilter::computeFastBounds(const SkRect& rect) const {
    // The base computeFastBounds() implementation is the union of all fast bounds from children,
    // or 'rect' if there are none. For merge, zero children means zero output so only call the
    // base implementation when there are filters to merge.
    // TODO: When the bounds update is complete, this default implementation may go away and we
    // can move the union'ing logic here.
    if (this->countInputs() <= 0) {
        return SkRect::MakeEmpty();
    } else {
        return SkImageFilter_Base::computeFastBounds(rect);
    }
}
