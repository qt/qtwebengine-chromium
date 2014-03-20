/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGMarkerElement_h
#define SVGMarkerElement_h

#include "bindings/v8/ExceptionState.h"
#include "core/svg/SVGAnimatedAngle.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGAnimatedRect.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGFitToViewBox.h"

namespace WebCore {

enum SVGMarkerUnitsType {
    SVGMarkerUnitsUnknown = 0,
    SVGMarkerUnitsUserSpaceOnUse,
    SVGMarkerUnitsStrokeWidth
};

enum SVGMarkerOrientType {
    SVGMarkerOrientUnknown = 0,
    SVGMarkerOrientAuto,
    SVGMarkerOrientAngle
};

template<>
struct SVGPropertyTraits<SVGMarkerUnitsType> {
    static unsigned highestEnumValue() { return SVGMarkerUnitsStrokeWidth; }

    static String toString(SVGMarkerUnitsType type)
    {
        switch (type) {
        case SVGMarkerUnitsUnknown:
            return emptyString();
        case SVGMarkerUnitsUserSpaceOnUse:
            return "userSpaceOnUse";
        case SVGMarkerUnitsStrokeWidth:
            return "strokeWidth";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGMarkerUnitsType fromString(const String& value)
    {
        if (value == "userSpaceOnUse")
            return SVGMarkerUnitsUserSpaceOnUse;
        if (value == "strokeWidth")
            return SVGMarkerUnitsStrokeWidth;
        return SVGMarkerUnitsUnknown;
    }
};

template<>
struct SVGPropertyTraits<SVGMarkerOrientType> {
    static unsigned highestEnumValue() { return SVGMarkerOrientAngle; }

    // toString is not needed, synchronizeOrientType() handles this on its own.

    static SVGMarkerOrientType fromString(const String& value, SVGAngle& angle)
    {
        if (value == "auto")
            return SVGMarkerOrientAuto;

        TrackExceptionState exceptionState;
        angle.setValueAsString(value, exceptionState);
        if (!exceptionState.hadException())
            return SVGMarkerOrientAngle;
        return SVGMarkerOrientUnknown;
    }
};

class SVGMarkerElement FINAL : public SVGElement,
                               public SVGExternalResourcesRequired,
                               public SVGFitToViewBox {
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        SVG_MARKERUNITS_UNKNOWN = SVGMarkerUnitsUnknown,
        SVG_MARKERUNITS_USERSPACEONUSE = SVGMarkerUnitsUserSpaceOnUse,
        SVG_MARKERUNITS_STROKEWIDTH = SVGMarkerUnitsStrokeWidth
    };

    enum {
        SVG_MARKER_ORIENT_UNKNOWN = SVGMarkerOrientUnknown,
        SVG_MARKER_ORIENT_AUTO = SVGMarkerOrientAuto,
        SVG_MARKER_ORIENT_ANGLE = SVGMarkerOrientAngle
    };

    static PassRefPtr<SVGMarkerElement> create(Document&);

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

    void setOrientToAuto();
    void setOrientToAngle(const SVGAngle&);

    static const SVGPropertyInfo* orientTypePropertyInfo();

    // Custom 'orientType' property.
    static void synchronizeOrientType(SVGElement* contextElement);
    static PassRefPtr<SVGAnimatedProperty> lookupOrCreateOrientTypeWrapper(SVGElement* contextElement);
    SVGMarkerOrientType& orientTypeCurrentValue() const { return m_orientType.value; }
    SVGMarkerOrientType& orientTypeBaseValue() const { return m_orientType.value; }
    void setOrientTypeBaseValue(const SVGMarkerOrientType& type) { m_orientType.value = type; }
    PassRefPtr<SVGAnimatedEnumerationPropertyTearOff<SVGMarkerOrientType> > orientType();

private:
    explicit SVGMarkerElement(Document&);

    virtual bool needsPendingResourceHandling() const { return false; }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual RenderObject* createRenderer(RenderStyle*);
    virtual bool rendererIsNeeded(const RenderStyle&) { return true; }

    virtual bool selfHasRelativeLengths() const;

    void synchronizeOrientType();

    static const AtomicString& orientTypeIdentifier();
    static const AtomicString& orientAngleIdentifier();

    mutable SVGSynchronizableAnimatedProperty<SVGMarkerOrientType> m_orientType;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGMarkerElement)
        DECLARE_ANIMATED_LENGTH(RefX, refX)
        DECLARE_ANIMATED_LENGTH(RefY, refY)
        DECLARE_ANIMATED_LENGTH(MarkerWidth, markerWidth)
        DECLARE_ANIMATED_LENGTH(MarkerHeight, markerHeight)
        DECLARE_ANIMATED_ENUMERATION(MarkerUnits, markerUnits, SVGMarkerUnitsType)
        DECLARE_ANIMATED_ANGLE(OrientAngle, orientAngle)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
        DECLARE_ANIMATED_RECT(ViewBox, viewBox)
        DECLARE_ANIMATED_PRESERVEASPECTRATIO(PreserveAspectRatio, preserveAspectRatio)
    END_DECLARE_ANIMATED_PROPERTIES
};

DEFINE_NODE_TYPE_CASTS(SVGMarkerElement, hasTagName(SVGNames::markerTag));

}

#endif
