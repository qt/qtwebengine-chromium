/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012-2013 Intel Corporation. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/dom/ViewportDescription.h"

using namespace std;

namespace WebCore {

static const float& compareIgnoringAuto(const float& value1, const float& value2, const float& (*compare) (const float&, const float&))
{
    if (value1 == ViewportDescription::ValueAuto)
        return value2;

    if (value2 == ViewportDescription::ValueAuto)
        return value1;

    return compare(value1, value2);
}

float ViewportDescription::resolveViewportLength(const Length& length, const FloatSize& initialViewportSize, Direction direction)
{
    if (length.isAuto())
        return ViewportDescription::ValueAuto;

    if (length.isFixed())
        return length.getFloatValue();

    if (length.type() == ExtendToZoom)
        return ViewportDescription::ValueExtendToZoom;

    if ((length.type() == Percent && direction == Horizontal) || length.type() == ViewportPercentageWidth)
        return initialViewportSize.width() * length.getFloatValue() / 100.0f;

    if ((length.type() == Percent && direction == Vertical) || length.type() == ViewportPercentageHeight)
        return initialViewportSize.height() * length.getFloatValue() / 100.0f;

    if (length.type() == ViewportPercentageMin)
        return min(initialViewportSize.width(), initialViewportSize.height()) * length.viewportPercentageLength() / 100.0f;

    if (length.type() == ViewportPercentageMax)
        return max(initialViewportSize.width(), initialViewportSize.height()) * length.viewportPercentageLength() / 100.0f;

    ASSERT_NOT_REACHED();
    return ViewportDescription::ValueAuto;
}

PageScaleConstraints ViewportDescription::resolve(const FloatSize& initialViewportSize) const
{
    float resultWidth = ValueAuto;
    float resultMaxWidth = resolveViewportLength(maxWidth, initialViewportSize, Horizontal);
    float resultMinWidth = resolveViewportLength(minWidth, initialViewportSize, Horizontal);
    float resultHeight = ValueAuto;
    float resultMaxHeight = resolveViewportLength(maxHeight, initialViewportSize, Vertical);
    float resultMinHeight = resolveViewportLength(minHeight, initialViewportSize, Vertical);

    float resultZoom = zoom;
    float resultMinZoom = minZoom;
    float resultMaxZoom = maxZoom;
    float resultUserZoom = userZoom;

    // 1. Resolve min-zoom and max-zoom values.
    if (resultMinZoom != ViewportDescription::ValueAuto && resultMaxZoom != ViewportDescription::ValueAuto)
        resultMaxZoom = max(resultMinZoom, resultMaxZoom);

    // 2. Constrain zoom value to the [min-zoom, max-zoom] range.
    if (resultZoom != ViewportDescription::ValueAuto)
        resultZoom = compareIgnoringAuto(resultMinZoom, compareIgnoringAuto(resultMaxZoom, resultZoom, min), max);

    float extendZoom = compareIgnoringAuto(resultZoom, resultMaxZoom, min);

    // 3. Resolve non-"auto" lengths to pixel lengths.
    if (extendZoom == ViewportDescription::ValueAuto) {
        if (resultMaxWidth == ViewportDescription::ValueExtendToZoom)
            resultMaxWidth = ViewportDescription::ValueAuto;

        if (resultMaxHeight == ViewportDescription::ValueExtendToZoom)
            resultMaxHeight = ViewportDescription::ValueAuto;

        if (resultMinWidth == ViewportDescription::ValueExtendToZoom)
            resultMinWidth = resultMaxWidth;

        if (resultMinHeight == ViewportDescription::ValueExtendToZoom)
            resultMinHeight = resultMaxHeight;
    } else {
        float extendWidth = initialViewportSize.width() / extendZoom;
        float extendHeight = initialViewportSize.height() / extendZoom;

        if (resultMaxWidth == ViewportDescription::ValueExtendToZoom)
            resultMaxWidth = extendWidth;

        if (resultMaxHeight == ViewportDescription::ValueExtendToZoom)
            resultMaxHeight = extendHeight;

        if (resultMinWidth == ViewportDescription::ValueExtendToZoom)
            resultMinWidth = compareIgnoringAuto(extendWidth, resultMaxWidth, max);

        if (resultMinHeight == ViewportDescription::ValueExtendToZoom)
            resultMinHeight = compareIgnoringAuto(extendHeight, resultMaxHeight, max);
    }

    // 4. Resolve initial width from min/max descriptors.
    if (resultMinWidth != ViewportDescription::ValueAuto || resultMaxWidth != ViewportDescription::ValueAuto)
        resultWidth = compareIgnoringAuto(resultMinWidth, compareIgnoringAuto(resultMaxWidth, initialViewportSize.width(), min), max);

    // 5. Resolve initial height from min/max descriptors.
    if (resultMinHeight != ViewportDescription::ValueAuto || resultMaxHeight != ViewportDescription::ValueAuto)
        resultHeight = compareIgnoringAuto(resultMinHeight, compareIgnoringAuto(resultMaxHeight, initialViewportSize.height(), min), max);

    // 6-7. Resolve width value.
    if (resultWidth == ViewportDescription::ValueAuto) {
        if (resultHeight == ViewportDescription::ValueAuto || !initialViewportSize.height())
            resultWidth = initialViewportSize.width();
        else
            resultWidth = resultHeight * (initialViewportSize.width() / initialViewportSize.height());
    }

    // 8. Resolve height value.
    if (resultHeight == ViewportDescription::ValueAuto) {
        if (!initialViewportSize.width())
            resultHeight = initialViewportSize.height();
        else
            resultHeight = resultWidth * initialViewportSize.height() / initialViewportSize.width();
    }

    // Resolve initial-scale value.
    if (resultZoom == ViewportDescription::ValueAuto) {
        if (resultWidth != ViewportDescription::ValueAuto && resultWidth > 0)
            resultZoom = initialViewportSize.width() / resultWidth;
        if (resultHeight != ViewportDescription::ValueAuto && resultHeight > 0) {
            // if 'auto', the initial-scale will be negative here and thus ignored.
            resultZoom = max<float>(resultZoom, initialViewportSize.height() / resultHeight);
        }
    }

    // If user-scalable = no, lock the min/max scale to the computed initial
    // scale.
    if (!resultUserZoom)
        resultMinZoom = resultMaxZoom = resultZoom;

    // Only set initialScale to a value if it was explicitly set.
    if (zoom == ViewportDescription::ValueAuto)
        resultZoom = ViewportDescription::ValueAuto;

    PageScaleConstraints result;
    result.minimumScale = resultMinZoom;
    result.maximumScale = resultMaxZoom;
    result.initialScale = resultZoom;
    result.layoutSize.setWidth(resultWidth);
    result.layoutSize.setHeight(resultHeight);
    return result;
}

} // namespace WebCore
