/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2012 Motorola Mobility, Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "platform/LengthFunctions.h"

#include "platform/LayoutUnit.h"
#include "platform/Length.h"

namespace WebCore {

// This method is over-ridden in core/css/CSSLengthFunctions.cpp.
// Any changes here most likely also need to be applied there.
float floatValueForLength(const Length& length, float maximumValue)
{
    switch (length.type()) {
    case Fixed:
        return length.getFloatValue();
    case Percent:
        return static_cast<float>(maximumValue * length.percent() / 100.0f);
    case FillAvailable:
    case Auto:
        return static_cast<float>(maximumValue);
    case Calculated:
        return length.nonNanCalculatedValue(maximumValue);
    case ViewportPercentageWidth:
    case ViewportPercentageHeight:
    case ViewportPercentageMin:
    case ViewportPercentageMax:
        return 0;
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FitContent:
    case ExtendToZoom:
    case Undefined:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
