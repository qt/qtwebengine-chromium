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

#ifndef SVGCursorElement_h
#define SVGCursorElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedString.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGTests.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

class SVGCursorElement FINAL : public SVGElement,
                               public SVGTests,
                               public SVGExternalResourcesRequired,
                               public SVGURIReference {
public:
    static PassRefPtr<SVGCursorElement> create(Document&);

    virtual ~SVGCursorElement();

    void addClient(SVGElement*);
    void removeClient(SVGElement*);
    void removeReferencedElement(SVGElement*);

private:
    explicit SVGCursorElement(Document&);

    virtual bool isValid() const { return SVGTests::isValid(); }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return false; }

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGCursorElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    // SVGTests
    virtual void synchronizeRequiredFeatures() { SVGTests::synchronizeRequiredFeatures(this); }
    virtual void synchronizeRequiredExtensions() { SVGTests::synchronizeRequiredExtensions(this); }
    virtual void synchronizeSystemLanguage() { SVGTests::synchronizeSystemLanguage(this); }

    HashSet<SVGElement*> m_clients;
};

} // namespace WebCore

#endif
