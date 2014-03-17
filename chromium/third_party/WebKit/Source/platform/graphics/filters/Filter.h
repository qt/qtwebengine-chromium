/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#ifndef Filter_h
#define Filter_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class FilterEffect;

class PLATFORM_EXPORT Filter : public RefCounted<Filter> {
public:
    Filter(const AffineTransform& absoluteTransform) : m_isAccelerated(false), m_absoluteTransform(absoluteTransform) { }
    virtual ~Filter() { }

    void setSourceImage(PassOwnPtr<ImageBuffer> sourceImage) { m_sourceImage = sourceImage; }
    ImageBuffer* sourceImage() { return m_sourceImage.get(); }

    FloatSize filterResolution() const { return m_filterResolution; }
    void setFilterResolution(const FloatSize& filterResolution) { m_filterResolution = filterResolution; }

    const AffineTransform& absoluteTransform() const { return m_absoluteTransform; }
    void setAbsoluteTransform(const AffineTransform& absoluteTransform) { m_absoluteTransform = absoluteTransform; }
    FloatPoint mapAbsolutePointToLocalPoint(const FloatPoint& point) const { return m_absoluteTransform.inverse().mapPoint(point); }

    bool isAccelerated() const { return m_isAccelerated; }
    void setIsAccelerated(bool isAccelerated) { m_isAccelerated = isAccelerated; }

    virtual float applyHorizontalScale(float value) const
    {
        float filterRegionScale = absoluteFilterRegion().isEmpty() || filterRegion().isEmpty() ?
            1.0f : absoluteFilterRegion().width() / filterRegion().width();
        return value * m_filterResolution.width() * filterRegionScale;
    }
    virtual float applyVerticalScale(float value) const
    {
        float filterRegionScale = absoluteFilterRegion().isEmpty() || filterRegion().isEmpty() ?
            1.0f : absoluteFilterRegion().height() / filterRegion().height();
        return value * m_filterResolution.height() * filterRegionScale;
    }

    virtual FloatRect sourceImageRect() const = 0;

    FloatRect absoluteFilterRegion() const { return m_absoluteFilterRegion; }
    void setAbsoluteFilterRegion(const FloatRect& rect) { m_absoluteFilterRegion = rect; }

    FloatRect filterRegion() const { return m_filterRegion; }
    void setFilterRegion(const FloatRect& rect) { m_filterRegion = rect; }

private:
    OwnPtr<ImageBuffer> m_sourceImage;
    FloatSize m_filterResolution;
    bool m_isAccelerated;
    AffineTransform m_absoluteTransform;
    FloatRect m_absoluteFilterRegion;
    FloatRect m_filterRegion;
};

} // namespace WebCore

#endif // Filter_h
