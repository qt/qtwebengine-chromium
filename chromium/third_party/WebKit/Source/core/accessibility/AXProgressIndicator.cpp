/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "core/accessibility/AXProgressIndicator.h"

#include "core/html/HTMLProgressElement.h"
#include "core/rendering/RenderProgress.h"
#include "platform/FloatConversion.h"

namespace WebCore {

using namespace HTMLNames;

AXProgressIndicator::AXProgressIndicator(RenderProgress* renderer)
    : AXRenderObject(renderer)
{
}

PassRefPtr<AXProgressIndicator> AXProgressIndicator::create(RenderProgress* renderer)
{
    return adoptRef(new AXProgressIndicator(renderer));
}

bool AXProgressIndicator::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

float AXProgressIndicator::valueForRange() const
{
    if (hasAttribute(aria_valuenowAttr))
        return getAttribute(aria_valuenowAttr).toFloat();

    if (element()->position() >= 0)
        return narrowPrecisionToFloat(element()->value());
    // Indeterminate progress bar should return 0.
    return 0.0f;
}

float AXProgressIndicator::maxValueForRange() const
{
    if (hasAttribute(aria_valuemaxAttr))
        return getAttribute(aria_valuemaxAttr).toFloat();

    return narrowPrecisionToFloat(element()->max());
}

float AXProgressIndicator::minValueForRange() const
{
    if (hasAttribute(aria_valueminAttr))
        return getAttribute(aria_valueminAttr).toFloat();

    return 0.0f;
}

HTMLProgressElement* AXProgressIndicator::element() const
{
    return toRenderProgress(m_renderer)->progressElement();
}


} // namespace WebCore
