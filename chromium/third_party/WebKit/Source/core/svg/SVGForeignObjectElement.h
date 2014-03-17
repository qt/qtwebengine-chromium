/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#ifndef SVGForeignObjectElement_h
#define SVGForeignObjectElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

class SVGForeignObjectElement FINAL : public SVGGraphicsElement,
                                      public SVGExternalResourcesRequired {
public:
    static PassRefPtr<SVGForeignObjectElement> create(Document&);

private:
    explicit SVGForeignObjectElement(Document&);

    virtual bool isValid() const { return SVGTests::isValid(); }
    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual bool childShouldCreateRenderer(const Node& child) const;
    virtual RenderObject* createRenderer(RenderStyle*);

    virtual bool selfHasRelativeLengths() const;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGForeignObjectElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

DEFINE_NODE_TYPE_CASTS(SVGForeignObjectElement, hasTagName(SVGNames::foreignObjectTag));

} // namespace WebCore

#endif
