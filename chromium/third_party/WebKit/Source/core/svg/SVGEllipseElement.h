/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGEllipseElement_h
#define SVGEllipseElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGGeometryElement.h"

namespace WebCore {

class SVGEllipseElement FINAL : public SVGGeometryElement,
                                public SVGExternalResourcesRequired {
public:
    static PassRefPtr<SVGEllipseElement> create(Document&);

private:
    explicit SVGEllipseElement(Document&);

    virtual bool isValid() const { return SVGTests::isValid(); }
    virtual bool supportsFocus() const OVERRIDE { return hasFocusEventListeners(); }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool selfHasRelativeLengths() const;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGEllipseElement)
        DECLARE_ANIMATED_LENGTH(Cx, cx)
        DECLARE_ANIMATED_LENGTH(Cy, cy)
        DECLARE_ANIMATED_LENGTH(Rx, rx)
        DECLARE_ANIMATED_LENGTH(Ry, ry)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

DEFINE_NODE_TYPE_CASTS(SVGEllipseElement, hasTagName(SVGNames::ellipseTag));

} // namespace WebCore

#endif
