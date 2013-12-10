/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGZoomAndPan_h
#define SVGZoomAndPan_h

#include "SVGNames.h"
#include "core/dom/QualifiedName.h"
#include "wtf/HashSet.h"

namespace WebCore {

enum SVGZoomAndPanType {
    SVGZoomAndPanUnknown = 0,
    SVGZoomAndPanDisable,
    SVGZoomAndPanMagnify
};

class SVGZoomAndPan {
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        SVG_ZOOMANDPAN_UNKNOWN = SVGZoomAndPanUnknown,
        SVG_ZOOMANDPAN_DISABLE = SVGZoomAndPanDisable,
        SVG_ZOOMANDPAN_MAGNIFY = SVGZoomAndPanMagnify
    };

    static bool isKnownAttribute(const QualifiedName&);
    static void addSupportedAttributes(HashSet<QualifiedName>&);

    static SVGZoomAndPanType parseFromNumber(unsigned short number)
    {
        if (!number || number > SVGZoomAndPanMagnify)
            return SVGZoomAndPanUnknown;
        return static_cast<SVGZoomAndPanType>(number);
    }

    static bool parseZoomAndPan(const LChar*& start, const LChar* end, SVGZoomAndPanType&);
    static bool parseZoomAndPan(const UChar*& start, const UChar* end, SVGZoomAndPanType&);

    template<class SVGElementTarget>
    static bool parseAttribute(SVGElementTarget* target, const QualifiedName& name, const AtomicString& value)
    {
        ASSERT(target);
        if (name == SVGNames::zoomAndPanAttr) {
            SVGZoomAndPanType zoomAndPan = SVGZoomAndPanUnknown;
            if (!value.isEmpty()) {
                if (value.is8Bit()) {
                    const LChar* start = value.characters8();
                    parseZoomAndPan(start, start + value.length(), zoomAndPan);
                } else {
                    const UChar* start = value.characters16();
                    parseZoomAndPan(start, start + value.length(), zoomAndPan);
                }
            }
            target->setZoomAndPan(zoomAndPan);
            return true;
        }

        return false;
    }

    SVGZoomAndPanType zoomAndPan() const { return SVGZoomAndPanUnknown; }

    // These methods only exist to allow us to compile V8/JSSVGZoomAndPan.*.
    // These are never called, and thus ASSERT_NOT_REACHED.
    void ref();
    void deref();
    void setZoomAndPan(unsigned short);
};

} // namespace WebCore

#endif // SVGZoomAndPan_h
