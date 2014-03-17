/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "config.h"
#include "core/rendering/svg/SVGPathData.h"

#include "SVGNames.h"
#include "core/svg/SVGCircleElement.h"
#include "core/svg/SVGEllipseElement.h"
#include "core/svg/SVGLineElement.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathUtilities.h"
#include "core/svg/SVGPolygonElement.h"
#include "core/svg/SVGPolylineElement.h"
#include "core/svg/SVGRectElement.h"
#include "platform/graphics/Path.h"
#include "wtf/HashMap.h"

namespace WebCore {

static void updatePathFromCircleElement(SVGElement* element, Path& path)
{
    SVGCircleElement* circle = toSVGCircleElement(element);

    SVGLengthContext lengthContext(element);
    float r = circle->rCurrentValue().value(lengthContext);
    if (r > 0)
        path.addEllipse(FloatRect(circle->cxCurrentValue().value(lengthContext) - r, circle->cyCurrentValue().value(lengthContext) - r, r * 2, r * 2));
}

static void updatePathFromEllipseElement(SVGElement* element, Path& path)
{
    SVGEllipseElement* ellipse = toSVGEllipseElement(element);

    SVGLengthContext lengthContext(element);
    float rx = ellipse->rxCurrentValue().value(lengthContext);
    if (rx <= 0)
        return;
    float ry = ellipse->ryCurrentValue().value(lengthContext);
    if (ry <= 0)
        return;
    path.addEllipse(FloatRect(ellipse->cxCurrentValue().value(lengthContext) - rx, ellipse->cyCurrentValue().value(lengthContext) - ry, rx * 2, ry * 2));
}

static void updatePathFromLineElement(SVGElement* element, Path& path)
{
    SVGLineElement* line = toSVGLineElement(element);

    SVGLengthContext lengthContext(element);
    path.moveTo(FloatPoint(line->x1CurrentValue().value(lengthContext), line->y1CurrentValue().value(lengthContext)));
    path.addLineTo(FloatPoint(line->x2CurrentValue().value(lengthContext), line->y2CurrentValue().value(lengthContext)));
}

static void updatePathFromPathElement(SVGElement* element, Path& path)
{
    buildPathFromByteStream(toSVGPathElement(element)->pathByteStream(), path);
}

static void updatePathFromPolygonElement(SVGElement* element, Path& path)
{
    SVGPointList& points = toSVGPolygonElement(element)->pointsCurrentValue();
    if (points.isEmpty())
        return;

    path.moveTo(points.first());

    unsigned size = points.size();
    for (unsigned i = 1; i < size; ++i)
        path.addLineTo(points.at(i));

    path.closeSubpath();
}

static void updatePathFromPolylineElement(SVGElement* element, Path& path)
{
    SVGPointList& points = toSVGPolylineElement(element)->pointsCurrentValue();
    if (points.isEmpty())
        return;

    path.moveTo(points.first());

    unsigned size = points.size();
    for (unsigned i = 1; i < size; ++i)
        path.addLineTo(points.at(i));
}

static void updatePathFromRectElement(SVGElement* element, Path& path)
{
    SVGRectElement* rect = toSVGRectElement(element);

    SVGLengthContext lengthContext(element);
    float width = rect->widthCurrentValue().value(lengthContext);
    if (width <= 0)
        return;
    float height = rect->heightCurrentValue().value(lengthContext);
    if (height <= 0)
        return;
    float x = rect->xCurrentValue().value(lengthContext);
    float y = rect->yCurrentValue().value(lengthContext);
    bool hasRx = rect->rxCurrentValue().value(lengthContext) > 0;
    bool hasRy = rect->ryCurrentValue().value(lengthContext) > 0;
    if (hasRx || hasRy) {
        float rx = rect->rxCurrentValue().value(lengthContext);
        float ry = rect->ryCurrentValue().value(lengthContext);
        if (!hasRx)
            rx = ry;
        else if (!hasRy)
            ry = rx;

        path.addRoundedRect(FloatRect(x, y, width, height), FloatSize(rx, ry));
        return;
    }

    path.addRect(FloatRect(x, y, width, height));
}

void updatePathFromGraphicsElement(SVGElement* element, Path& path)
{
    ASSERT(element);
    ASSERT(path.isEmpty());

    typedef void (*PathUpdateFunction)(SVGElement*, Path&);
    static HashMap<StringImpl*, PathUpdateFunction>* map = 0;
    if (!map) {
        map = new HashMap<StringImpl*, PathUpdateFunction>;
        map->set(SVGNames::circleTag.localName().impl(), updatePathFromCircleElement);
        map->set(SVGNames::ellipseTag.localName().impl(), updatePathFromEllipseElement);
        map->set(SVGNames::lineTag.localName().impl(), updatePathFromLineElement);
        map->set(SVGNames::pathTag.localName().impl(), updatePathFromPathElement);
        map->set(SVGNames::polygonTag.localName().impl(), updatePathFromPolygonElement);
        map->set(SVGNames::polylineTag.localName().impl(), updatePathFromPolylineElement);
        map->set(SVGNames::rectTag.localName().impl(), updatePathFromRectElement);
    }

    if (PathUpdateFunction pathUpdateFunction = map->get(element->localName().impl()))
        (*pathUpdateFunction)(element, path);
}

} // namespace WebCore
