/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
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

#ifndef SVGSVGElement_h
#define SVGSVGElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGAnimatedRect.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGZoomAndPan.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class SVGAngle;
class SVGMatrix;
class SVGTransform;
class SVGViewSpec;
class SVGViewElement;
class SMILTimeContainer;

class SVGSVGElement FINAL : public SVGGraphicsElement,
                            public SVGExternalResourcesRequired,
                            public SVGFitToViewBox,
                            public SVGZoomAndPan {
public:
    static PassRefPtr<SVGSVGElement> create(Document&);

    using SVGGraphicsElement::ref;
    using SVGGraphicsElement::deref;

    virtual bool isValid() const { return SVGTests::isValid(); }
    virtual bool supportsFocus() const OVERRIDE { return hasFocusEventListeners(); }

    // 'SVGSVGElement' functions
    const AtomicString& contentScriptType() const;
    void setContentScriptType(const AtomicString& type);

    const AtomicString& contentStyleType() const;
    void setContentStyleType(const AtomicString& type);

    SVGRect viewport() const;

    float pixelUnitToMillimeterX() const;
    float pixelUnitToMillimeterY() const;
    float screenPixelToMillimeterX() const;
    float screenPixelToMillimeterY() const;

    bool useCurrentView() const { return m_useCurrentView; }
    SVGViewSpec* currentView();

    enum ConsiderCSSMode {
        RespectCSSProperties,
        IgnoreCSSProperties
    };

    // RenderSVGRoot wants to query the intrinsic size, by only examining the width/height attributes.
    Length intrinsicWidth(ConsiderCSSMode = RespectCSSProperties) const;
    Length intrinsicHeight(ConsiderCSSMode = RespectCSSProperties) const;
    FloatSize currentViewportSize() const;
    SVGRect currentViewBoxRect() const;

    float currentScale() const;
    void setCurrentScale(float scale);

    SVGPoint& currentTranslate() { return m_translation; }
    void setCurrentTranslate(const FloatPoint&);

    // Only used from the bindings.
    void updateCurrentTranslate();

    SMILTimeContainer* timeContainer() const { return m_timeContainer.get(); }

    void pauseAnimations();
    void unpauseAnimations();
    bool animationsPaused() const;

    float getCurrentTime() const;
    void setCurrentTime(float seconds);

    unsigned suspendRedraw(unsigned maxWaitMilliseconds);
    void unsuspendRedraw(unsigned suspendHandleId);
    void unsuspendRedrawAll();
    void forceRedraw();

    PassRefPtr<NodeList> getIntersectionList(const SVGRect&, SVGElement* referenceElement) const;
    PassRefPtr<NodeList> getEnclosureList(const SVGRect&, SVGElement* referenceElement) const;
    bool checkIntersection(SVGElement*, const SVGRect&) const;
    bool checkEnclosure(SVGElement*, const SVGRect&) const;
    void deselectAll();

    static float createSVGNumber();
    static SVGLength createSVGLength();
    static SVGAngle createSVGAngle();
    static SVGPoint createSVGPoint();
    static SVGMatrix createSVGMatrix();
    static SVGRect createSVGRect();
    static SVGTransform createSVGTransform();
    static SVGTransform createSVGTransformFromMatrix(const SVGMatrix&);

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

    void setupInitialView(const String& fragmentIdentifier, Element* anchorNode);

    Element* getElementById(const AtomicString&) const;

    bool widthAttributeEstablishesViewport() const;
    bool heightAttributeEstablishesViewport() const;

    SVGZoomAndPanType zoomAndPan() const { return m_zoomAndPan; }
    void setZoomAndPan(unsigned short zoomAndPan) { m_zoomAndPan = SVGZoomAndPan::parseFromNumber(zoomAndPan); }

    bool hasEmptyViewBox() const { return viewBoxCurrentValue().isValid() && viewBoxCurrentValue().isEmpty(); }

private:
    explicit SVGSVGElement(Document&);
    virtual ~SVGSVGElement();

    virtual bool isSVGSVGElement() const OVERRIDE { return true; }

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*);

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool selfHasRelativeLengths() const;

    void inheritViewAttributes(SVGViewElement*);

    enum CollectIntersectionOrEnclosure {
        CollectIntersectionList,
        CollectEnclosureList
    };

    PassRefPtr<NodeList> collectIntersectionOrEnclosureList(const SVGRect&, SVGElement*, CollectIntersectionOrEnclosure) const;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGSVGElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
        DECLARE_ANIMATED_RECT(ViewBox, viewBox)
        DECLARE_ANIMATED_PRESERVEASPECTRATIO(PreserveAspectRatio, preserveAspectRatio)
    END_DECLARE_ANIMATED_PROPERTIES

    virtual AffineTransform localCoordinateSpaceTransform(SVGElement::CTMScope) const;

    bool m_useCurrentView;
    SVGZoomAndPanType m_zoomAndPan;
    RefPtr<SMILTimeContainer> m_timeContainer;
    SVGPoint m_translation;
    RefPtr<SVGViewSpec> m_viewSpec;
    WeakPtrFactory<SVGSVGElement> m_weakFactory;
};

inline bool isSVGSVGElement(const Node& node)
{
    return node.isSVGElement() && toSVGElement(node).isSVGSVGElement();
}

DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(SVGSVGElement);

} // namespace WebCore

#endif
