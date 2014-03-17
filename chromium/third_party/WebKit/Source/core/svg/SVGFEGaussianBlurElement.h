/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGFEGaussianBlurElement_h
#define SVGFEGaussianBlurElement_h

#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEGaussianBlur.h"

namespace WebCore {

class SVGFEGaussianBlurElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEGaussianBlurElement> create(Document&);

    void setStdDeviation(float stdDeviationX, float stdDeviationY);

private:
    explicit SVGFEGaussianBlurElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*);

    static const AtomicString& stdDeviationXIdentifier();
    static const AtomicString& stdDeviationYIdentifier();

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFEGaussianBlurElement)
        DECLARE_ANIMATED_STRING(In1, in1)
        DECLARE_ANIMATED_NUMBER(StdDeviationX, stdDeviationX)
        DECLARE_ANIMATED_NUMBER(StdDeviationY, stdDeviationY)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore

#endif
