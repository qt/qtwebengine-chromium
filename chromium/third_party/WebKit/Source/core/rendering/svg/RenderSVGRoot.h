/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef RenderSVGRoot_h
#define RenderSVGRoot_h

#include "core/rendering/RenderReplaced.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "platform/geometry/FloatRect.h"

namespace WebCore {

class AffineTransform;
class SVGElement;

class RenderSVGRoot FINAL : public RenderReplaced {
public:
    explicit RenderSVGRoot(SVGElement*);
    virtual ~RenderSVGRoot();

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    virtual void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio, bool& isPercentageIntrinsicSize) const;

    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    virtual void setNeedsBoundariesUpdate() { m_needsBoundariesOrTransformUpdate = true; }
    virtual bool needsBoundariesUpdate() OVERRIDE { return m_needsBoundariesOrTransformUpdate; }
    virtual void setNeedsTransformUpdate() { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; }

    virtual bool hasRelativeDimensions() const OVERRIDE;
    virtual bool hasRelativeIntrinsicLogicalWidth() const OVERRIDE;
    virtual bool hasRelativeLogicalHeight() const OVERRIDE;

    // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS box coordinates.
    const AffineTransform& localToBorderBoxTransform() const { return m_localToBorderBoxTransform; }

private:
    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }

    virtual bool isSVGRoot() const { return true; }
    virtual const char* renderName() const { return "RenderSVGRoot"; }

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const OVERRIDE;
    virtual LayoutUnit computeReplacedLogicalHeight() const;
    virtual void layout();
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&);

    virtual void willBeDestroyed();
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject*) OVERRIDE;

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

    virtual const AffineTransform& localToParentTransform() const;

    virtual FloatRect objectBoundingBox() const { return m_objectBoundingBox; }
    virtual FloatRect strokeBoundingBox() const { return m_strokeBoundingBox; }
    virtual FloatRect repaintRectInLocalCoordinates() const { return m_repaintBoundingBox; }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const OVERRIDE;
    virtual void computeFloatRectForRepaint(const RenderLayerModelObject* repaintContainer, FloatRect& repaintRect, bool fixed) const OVERRIDE;

    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const OVERRIDE;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const OVERRIDE;

    virtual bool canBeSelectionLeaf() const { return false; }
    virtual bool canHaveChildren() const { return true; }

    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    RenderObjectChildList m_children;
    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGRoot, isSVGRoot());

} // namespace WebCore

#endif // RenderSVGRoot_h
