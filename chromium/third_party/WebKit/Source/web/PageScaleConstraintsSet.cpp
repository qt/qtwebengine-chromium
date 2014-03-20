/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "PageScaleConstraintsSet.h"

#include "wtf/Assertions.h"

using namespace WebCore;

namespace blink {

static const float defaultMinimumScale = 0.25f;
static const float defaultMaximumScale = 5.0f;

PageScaleConstraintsSet::PageScaleConstraintsSet()
    : m_lastContentsWidth(0)
    , m_needsReset(false)
    , m_constraintsDirty(false)
{
    m_finalConstraints = defaultConstraints();
}

PageScaleConstraints PageScaleConstraintsSet::defaultConstraints() const
{
    return PageScaleConstraints(-1, defaultMinimumScale, defaultMaximumScale);
}

void PageScaleConstraintsSet::updatePageDefinedConstraints(const ViewportDescription& description, IntSize viewSize)
{
    m_pageDefinedConstraints = description.resolve(viewSize);

    m_constraintsDirty = true;
}

void PageScaleConstraintsSet::setUserAgentConstraints(const PageScaleConstraints& userAgentConstraints)
{
    m_userAgentConstraints = userAgentConstraints;
    m_constraintsDirty = true;
}

PageScaleConstraints PageScaleConstraintsSet::computeConstraintsStack() const
{
    PageScaleConstraints constraints = defaultConstraints();
    constraints.overrideWith(m_pageDefinedConstraints);
    constraints.overrideWith(m_userAgentConstraints);
    return constraints;
}

void PageScaleConstraintsSet::computeFinalConstraints()
{
    m_finalConstraints = computeConstraintsStack();

    m_constraintsDirty = false;
}

void PageScaleConstraintsSet::adjustFinalConstraintsToContentsSize(IntSize viewSize, IntSize contentsSize, int nonOverlayScrollbarWidth)
{
    m_finalConstraints.fitToContentsWidth(contentsSize.width(), viewSize.width() - nonOverlayScrollbarWidth);
}

void PageScaleConstraintsSet::setNeedsReset(bool needsReset)
{
    m_needsReset = needsReset;
    if (needsReset)
        m_constraintsDirty = true;
}

void PageScaleConstraintsSet::didChangeContentsSize(IntSize contentsSize, float pageScaleFactor)
{
    // If a large fixed-width element expanded the size of the document late in
    // loading and our initial scale is not set (or set to be less than the last
    // minimum scale), reset the page scale factor to the new initial scale.
    if (contentsSize.width() > m_lastContentsWidth
        && pageScaleFactor == finalConstraints().minimumScale
        && computeConstraintsStack().initialScale < finalConstraints().minimumScale)
        setNeedsReset(true);

    m_constraintsDirty = true;
    m_lastContentsWidth = contentsSize.width();
}

static float computeDeprecatedTargetDensityDPIFactor(const ViewportDescription& description, float deviceScaleFactor)
{
    if (description.deprecatedTargetDensityDPI == ViewportDescription::ValueDeviceDPI)
        return 1.0f / deviceScaleFactor;

    float targetDPI = -1.0f;
    if (description.deprecatedTargetDensityDPI == ViewportDescription::ValueLowDPI)
        targetDPI = 120.0f;
    else if (description.deprecatedTargetDensityDPI == ViewportDescription::ValueMediumDPI)
        targetDPI = 160.0f;
    else if (description.deprecatedTargetDensityDPI == ViewportDescription::ValueHighDPI)
        targetDPI = 240.0f;
    else if (description.deprecatedTargetDensityDPI != ViewportDescription::ValueAuto)
        targetDPI = description.deprecatedTargetDensityDPI;
    return targetDPI > 0 ? 160.0f / targetDPI : 1.0f;
}

static float getLayoutWidthForNonWideViewport(const FloatSize& deviceSize, float initialScale)
{
    return initialScale == -1 ? deviceSize.width() : deviceSize.width() / initialScale;
}

static float computeHeightByAspectRatio(float width, const FloatSize& deviceSize)
{
    return width * (deviceSize.height() / deviceSize.width());
}

void PageScaleConstraintsSet::adjustForAndroidWebViewQuirks(const ViewportDescription& description, IntSize viewSize, int layoutFallbackWidth, float deviceScaleFactor, bool supportTargetDensityDPI, bool wideViewportQuirkEnabled, bool useWideViewport, bool loadWithOverviewMode, bool nonUserScalableQuirkEnabled)
{
    if (!supportTargetDensityDPI && !wideViewportQuirkEnabled && loadWithOverviewMode && !nonUserScalableQuirkEnabled)
        return;

    const float oldInitialScale = m_pageDefinedConstraints.initialScale;
    if (!loadWithOverviewMode) {
        bool resetInitialScale = false;
        if (description.zoom == -1) {
            if (description.maxWidth.isAuto() || description.maxWidth.type() == ExtendToZoom)
                resetInitialScale = true;
            if (useWideViewport || description.maxWidth == Length(100, ViewportPercentageWidth))
                resetInitialScale = true;
        }
        if (resetInitialScale)
            m_pageDefinedConstraints.initialScale = 1.0f;
    }

    float adjustedLayoutSizeWidth = m_pageDefinedConstraints.layoutSize.width();
    float adjustedLayoutSizeHeight = m_pageDefinedConstraints.layoutSize.height();
    float targetDensityDPIFactor = 1.0f;

    if (supportTargetDensityDPI) {
        targetDensityDPIFactor = computeDeprecatedTargetDensityDPIFactor(description, deviceScaleFactor);
        if (m_pageDefinedConstraints.initialScale != -1)
            m_pageDefinedConstraints.initialScale *= targetDensityDPIFactor;
        if (m_pageDefinedConstraints.minimumScale != -1)
            m_pageDefinedConstraints.minimumScale *= targetDensityDPIFactor;
        if (m_pageDefinedConstraints.maximumScale != -1)
            m_pageDefinedConstraints.maximumScale *= targetDensityDPIFactor;
        if (wideViewportQuirkEnabled && (!useWideViewport || description.maxWidth == Length(100, ViewportPercentageWidth))) {
            adjustedLayoutSizeWidth /= targetDensityDPIFactor;
            adjustedLayoutSizeHeight /= targetDensityDPIFactor;
        }
    }

    if (wideViewportQuirkEnabled) {
        if (useWideViewport && (description.maxWidth.isAuto() || description.maxWidth.type() == ExtendToZoom) && description.zoom != 1.0f) {
            adjustedLayoutSizeWidth = layoutFallbackWidth;
            adjustedLayoutSizeHeight = computeHeightByAspectRatio(adjustedLayoutSizeWidth, viewSize);
        } else if (!useWideViewport) {
            const float nonWideScale = description.zoom < 1 && !description.maxWidth.isViewportPercentage() ? -1 : oldInitialScale;
            adjustedLayoutSizeWidth = getLayoutWidthForNonWideViewport(viewSize, nonWideScale) / targetDensityDPIFactor;
            float newInitialScale = targetDensityDPIFactor;
            if (m_userAgentConstraints.initialScale != -1 && (description.maxWidth == Length(100, ViewportPercentageWidth) || ((description.maxWidth.isAuto() || description.maxWidth.type() == ExtendToZoom) && description.zoom == -1))) {
                adjustedLayoutSizeWidth /= m_userAgentConstraints.initialScale;
                newInitialScale = m_userAgentConstraints.initialScale;
            }
            adjustedLayoutSizeHeight = computeHeightByAspectRatio(adjustedLayoutSizeWidth, viewSize);
            if (description.zoom < 1) {
                m_pageDefinedConstraints.initialScale = newInitialScale;
                if (m_pageDefinedConstraints.minimumScale != -1)
                    m_pageDefinedConstraints.minimumScale = std::min<float>(m_pageDefinedConstraints.minimumScale, m_pageDefinedConstraints.initialScale);
                if (m_pageDefinedConstraints.maximumScale != -1)
                    m_pageDefinedConstraints.maximumScale = std::max<float>(m_pageDefinedConstraints.maximumScale, m_pageDefinedConstraints.initialScale);
            }
        }
    }

    if (nonUserScalableQuirkEnabled && !description.userZoom) {
        m_pageDefinedConstraints.initialScale = targetDensityDPIFactor;
        m_pageDefinedConstraints.minimumScale = m_pageDefinedConstraints.initialScale;
        m_pageDefinedConstraints.maximumScale = m_pageDefinedConstraints.initialScale;
        if (description.maxWidth.isAuto() || description.maxWidth.type() == ExtendToZoom || description.maxWidth == Length(100, ViewportPercentageWidth)) {
            adjustedLayoutSizeWidth = viewSize.width() / targetDensityDPIFactor;
            adjustedLayoutSizeHeight = computeHeightByAspectRatio(adjustedLayoutSizeWidth, viewSize);
        }
    }

    m_pageDefinedConstraints.layoutSize.setWidth(adjustedLayoutSizeWidth);
    m_pageDefinedConstraints.layoutSize.setHeight(adjustedLayoutSizeHeight);
}

} // namespace WebCore
