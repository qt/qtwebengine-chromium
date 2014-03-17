/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/rendering/RenderView.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/frame/Frame.h"
#include "core/page/Page.h"
#include "core/rendering/ColumnInfo.h"
#include "core/rendering/CompositedLayerMapping.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderSelectionInfo.h"
#include "core/rendering/RenderWidget.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/custom/CustomFilterGlobalContext.h"

namespace WebCore {

RenderView::RenderView(Document* document)
    : RenderBlockFlow(document)
    , m_frameView(document->view())
    , m_selectionStart(0)
    , m_selectionEnd(0)
    , m_selectionStartPos(-1)
    , m_selectionEndPos(-1)
    , m_maximalOutlineSize(0)
    , m_pageLogicalHeight(0)
    , m_pageLogicalHeightChanged(false)
    , m_layoutState(0)
    , m_layoutStateDisableCount(0)
    , m_renderQuoteHead(0)
    , m_renderCounterCount(0)
{
    // init RenderObject attributes
    setInline(false);

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    setPreferredLogicalWidthsDirty(MarkOnlyThis);

    setPositionState(AbsolutePosition); // to 0,0 :)
}

RenderView::~RenderView()
{
}

bool RenderView::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool RenderView::hitTest(const HitTestRequest& request, const HitTestLocation& location, HitTestResult& result)
{
    // We have to recursively update layout/style here because otherwise, when the hit test recurses
    // into a child document, it could trigger a layout on the parent document, which can destroy RenderLayers
    // that are higher up in the call stack, leading to crashes.
    // Note that Document::updateLayout calls its parent's updateLayout.
    // FIXME: It should be the caller's responsibility to ensure an up-to-date layout.
    frameView()->updateLayoutAndStyleIfNeededRecursive();
    return layer()->hitTest(request, location, result);
}

void RenderView::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = (!shouldUsePrintingLayout() && m_frameView) ? LayoutUnit(viewLogicalHeight()) : logicalHeight;
}

void RenderView::updateLogicalWidth()
{
    if (!shouldUsePrintingLayout() && m_frameView)
        setLogicalWidth(viewLogicalWidth());
}

LayoutUnit RenderView::availableLogicalHeight(AvailableLogicalHeightType heightType) const
{
    // If we have columns, then the available logical height is reduced to the column height.
    if (hasColumns())
        return columnInfo()->columnHeight();
    return RenderBlock::availableLogicalHeight(heightType);
}

bool RenderView::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->isBox();
}

static bool dialogNeedsCentering(const RenderStyle* style)
{
    return style->position() == AbsolutePosition && style->hasAutoTopAndBottom();
}

void RenderView::positionDialog(RenderBox* box)
{
    HTMLDialogElement* dialog = toHTMLDialogElement(box->node());
    if (dialog->centeringMode() == HTMLDialogElement::NotCentered)
        return;
    if (dialog->centeringMode() == HTMLDialogElement::Centered) {
        if (dialogNeedsCentering(box->style()))
            box->setY(dialog->centeredPosition());
        return;
    }

    if (!dialogNeedsCentering(box->style())) {
        dialog->setNotCentered();
        return;
    }
    FrameView* frameView = document().view();
    int scrollTop = frameView->scrollOffset().height();
    int visibleHeight = frameView->visibleContentRect(ScrollableArea::IncludeScrollbars).height();
    LayoutUnit top = scrollTop;
    if (box->height() < visibleHeight)
        top += (visibleHeight - box->height()) / 2;
    box->setY(top);
    dialog->setCentered(top);
}

void RenderView::positionDialogs()
{
    TrackedRendererListHashSet* positionedDescendants = positionedObjects();
    if (!positionedDescendants)
        return;
    TrackedRendererListHashSet::iterator end = positionedDescendants->end();
    for (TrackedRendererListHashSet::iterator it = positionedDescendants->begin(); it != end; ++it) {
        RenderBox* box = *it;
        if (box->node() && box->node()->hasTagName(HTMLNames::dialogTag))
            positionDialog(box);
    }
}

void RenderView::layoutContent(const LayoutState& state)
{
    ASSERT(needsLayout());

    LayoutRectRecorder recorder(*this);
    RenderBlock::layout();

    if (RuntimeEnabledFeatures::dialogElementEnabled())
        positionDialogs();

    if (m_frameView->partialLayout().isStopping())
        return;

    if (hasRenderNamedFlowThreads())
        flowThreadController()->layoutRenderNamedFlowThreads();

#ifndef NDEBUG
    checkLayoutState(state);
#endif
}

#ifndef NDEBUG
void RenderView::checkLayoutState(const LayoutState& state)
{
    ASSERT(layoutDeltaMatches(LayoutSize()));
    ASSERT(!m_layoutStateDisableCount);
    ASSERT(m_layoutState == &state);
}
#endif

static RenderBox* enclosingSeamlessRenderer(const Document& doc)
{
    Element* ownerElement = doc.seamlessParentIFrame();
    if (!ownerElement)
        return 0;
    return ownerElement->renderBox();
}

void RenderView::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    // Seamless iframes are considered part of an enclosing render flow thread from the parent document. This is necessary for them to look
    // up regions in the parent document during layout.
    if (newChild && !newChild->isRenderFlowThread()) {
        RenderBox* seamlessBox = enclosingSeamlessRenderer(document());
        if (seamlessBox && seamlessBox->flowThreadContainingBlock())
            newChild->setFlowThreadState(seamlessBox->flowThreadState());
    }
    RenderBlock::addChild(newChild, beforeChild);
}

bool RenderView::initializeLayoutState(LayoutState& state)
{
    bool isSeamlessAncestorInFlowThread = false;

    // FIXME: May be better to push a clip and avoid issuing offscreen repaints.
    state.m_clipped = false;

    // Check the writing mode of the seamless ancestor. It has to match our document's writing mode, or we won't inherit any
    // pagination information.
    RenderBox* seamlessAncestor = enclosingSeamlessRenderer(document());
    LayoutState* seamlessLayoutState = seamlessAncestor ? seamlessAncestor->view()->layoutState() : 0;
    bool shouldInheritPagination = seamlessLayoutState && !m_pageLogicalHeight && seamlessAncestor->style()->writingMode() == style()->writingMode();

    state.m_pageLogicalHeight = shouldInheritPagination ? seamlessLayoutState->m_pageLogicalHeight : m_pageLogicalHeight;
    state.m_pageLogicalHeightChanged = shouldInheritPagination ? seamlessLayoutState->m_pageLogicalHeightChanged : m_pageLogicalHeightChanged;
    state.m_isPaginated = state.m_pageLogicalHeight;
    if (state.m_isPaginated && shouldInheritPagination) {
        // Set up the correct pagination offset. We can use a negative offset in order to push the top of the RenderView into its correct place
        // on a page. We can take the iframe's offset from the logical top of the first page and make the negative into the pagination offset within the child
        // view.
        bool isFlipped = seamlessAncestor->style()->isFlippedBlocksWritingMode();
        LayoutSize layoutOffset = seamlessLayoutState->layoutOffset();
        LayoutSize iFrameOffset(layoutOffset.width() + seamlessAncestor->x() + (!isFlipped ? seamlessAncestor->borderLeft() + seamlessAncestor->paddingLeft() :
            seamlessAncestor->borderRight() + seamlessAncestor->paddingRight()),
            layoutOffset.height() + seamlessAncestor->y() + (!isFlipped ? seamlessAncestor->borderTop() + seamlessAncestor->paddingTop() :
            seamlessAncestor->borderBottom() + seamlessAncestor->paddingBottom()));

        LayoutSize offsetDelta = seamlessLayoutState->m_pageOffset - iFrameOffset;
        state.m_pageOffset = offsetDelta;

        // Set the current render flow thread to point to our ancestor. This will allow the seamless document to locate the correct
        // regions when doing a layout.
        if (seamlessAncestor->flowThreadContainingBlock()) {
            flowThreadController()->setCurrentRenderFlowThread(seamlessAncestor->view()->flowThreadController()->currentRenderFlowThread());
            isSeamlessAncestorInFlowThread = true;
        }
    }

    // FIXME: We need to make line grids and exclusions work with seamless iframes as well here. Basically all layout state information needs
    // to propagate here and not just pagination information.
    return isSeamlessAncestorInFlowThread;
}

// The algorithm below assumes this is a full layout. In case there are previously computed values for regions, supplemental steps are taken
// to ensure the results are the same as those obtained from a full layout (i.e. the auto-height regions from all the flows are marked as needing
// layout).
// 1. The flows are laid out from the outer flow to the inner flow. This successfully computes the outer non-auto-height regions size so the
// inner flows have the necessary information to correctly fragment the content.
// 2. The flows are laid out from the inner flow to the outer flow. After an inner flow is laid out it goes into the constrained layout phase
// and marks the auto-height regions they need layout. This means the outer flows will relayout if they depend on regions with auto-height regions
// belonging to inner flows. This step will correctly set the computedAutoHeight for the auto-height regions. It's possible for non-auto-height
// regions to relayout if they depend on auto-height regions. This will invalidate the inner flow threads and mark them as needing layout.
// 3. The last step is to do one last layout if there are pathological dependencies between non-auto-height regions and auto-height regions
// as detected in the previous step.
void RenderView::layoutContentInAutoLogicalHeightRegions(const LayoutState& state)
{
    if (!m_frameView->partialLayout().isStopping()) {
        // Disable partial layout for any two-pass layout algorithm.
        m_frameView->partialLayout().reset();
    }

    // We need to invalidate all the flows with auto-height regions if one such flow needs layout.
    // If none is found we do a layout a check back again afterwards.
    if (!flowThreadController()->updateFlowThreadsNeedingLayout()) {
        // Do a first layout of the content. In some cases more layouts are not needed (e.g. only flows with non-auto-height regions have changed).
        layoutContent(state);

        // If we find no named flow needing a two step layout after the first layout, exit early.
        // Otherwise, initiate the two step layout algorithm and recompute all the flows.
        if (!flowThreadController()->updateFlowThreadsNeedingTwoStepLayout())
            return;
    }

    // Layout to recompute all the named flows with auto-height regions.
    layoutContent(state);

    // Propagate the computed auto-height values upwards.
    // Non-auto-height regions may invalidate the flow thread because they depended on auto-height regions, but that's ok.
    flowThreadController()->updateFlowThreadsIntoConstrainedPhase();

    // Do one last layout that should update the auto-height regions found in the main flow
    // and solve pathological dependencies between regions (e.g. a non-auto-height region depending
    // on an auto-height one).
    if (needsLayout())
        layoutContent(state);
}

void RenderView::layout()
{
    if (!document().paginated())
        setPageLogicalHeight(0);

    if (shouldUsePrintingLayout())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = logicalWidth();

    SubtreeLayoutScope layoutScope(this);

    // Use calcWidth/Height to get the new width/height, since this will take the full page zoom factor into account.
    bool relayoutChildren = !shouldUsePrintingLayout() && (!m_frameView || width() != viewWidth() || height() != viewHeight());
    if (relayoutChildren) {
        layoutScope.setChildNeedsLayout(this);
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if (child->isSVGRoot())
                continue;

            if ((child->isBox() && toRenderBox(child)->hasRelativeLogicalHeight())
                    || child->style()->logicalHeight().isPercent()
                    || child->style()->logicalMinHeight().isPercent()
                    || child->style()->logicalMaxHeight().isPercent()
                    || child->style()->logicalHeight().isViewportPercentage()
                    || child->style()->logicalMinHeight().isViewportPercentage()
                    || child->style()->logicalMaxHeight().isViewportPercentage())
                layoutScope.setChildNeedsLayout(child);
        }

        if (document().svgExtensions())
            document().accessSVGExtensions()->invalidateSVGRootsWithRelativeLengthDescendents(&layoutScope);
    }

    ASSERT(!m_layoutState);
    if (!needsLayout())
        return;

    LayoutState state;
    bool isSeamlessAncestorInFlowThread = initializeLayoutState(state);

    m_pageLogicalHeightChanged = false;
    m_layoutState = &state;

    if (checkTwoPassLayoutForAutoHeightRegions())
        layoutContentInAutoLogicalHeightRegions(state);
    else
        layoutContent(state);

    if (m_frameView->partialLayout().isStopping()) {
        m_layoutState = 0;
        return;
    }

#ifndef NDEBUG
    checkLayoutState(state);
#endif
    m_layoutState = 0;
    clearNeedsLayout();

    if (isSeamlessAncestorInFlowThread)
        flowThreadController()->setCurrentRenderFlowThread(0);
}

void RenderView::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    ASSERT_UNUSED(wasFixed, !wasFixed || *wasFixed == static_cast<bool>(mode & IsFixed));

    if (!repaintContainer && mode & UseTransforms && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        transformState.applyTransform(t);
    }

    if (mode & IsFixed && m_frameView)
        transformState.move(m_frameView->scrollOffsetForFixedPosition());

    if (repaintContainer == this)
        return;

    if (mode & TraverseDocumentBoundaries) {
        if (RenderObject* parentDocRenderer = frame()->ownerRenderer()) {
            transformState.move(-frame()->view()->scrollOffset());
            if (parentDocRenderer->isBox())
                transformState.move(toLayoutSize(toRenderBox(parentDocRenderer)->contentBoxRect().location()));
            parentDocRenderer->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
            return;
        }
    }

    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(repaintContainer, !repaintContainer);
}

const RenderObject* RenderView::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    LayoutSize offsetForFixedPosition;
    LayoutSize offset;
    RenderObject* container = 0;

    if (m_frameView)
        offsetForFixedPosition = m_frameView->scrollOffsetForFixedPosition();

    if (geometryMap.mapCoordinatesFlags() & TraverseDocumentBoundaries) {
        if (RenderPart* parentDocRenderer = frame()->ownerRenderer()) {
            offset = -m_frameView->scrollOffset();
            offset += toLayoutSize(parentDocRenderer->contentBoxRect().location());
            container = parentDocRenderer;
        }
    }

    // If a container was specified, and was not 0 or the RenderView, then we
    // should have found it by now unless we're traversing to a parent document.
    ASSERT_ARG(ancestorToStopAt, !ancestorToStopAt || ancestorToStopAt == this || container);

    if ((!ancestorToStopAt || container) && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, LayoutSize(), t);
        geometryMap.push(this, t, false, false, false, true, offsetForFixedPosition);
    } else {
        geometryMap.push(this, offset, false, false, false, false, offsetForFixedPosition);
    }

    return container;
}

void RenderView::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    if (mode & IsFixed && m_frameView)
        transformState.move(m_frameView->scrollOffsetForFixedPosition());

    if (mode & UseTransforms && shouldUseTransformFromContainer(0)) {
        TransformationMatrix t;
        getTransformFromContainer(0, LayoutSize(), t);
        transformState.applyTransform(t);
    }
}

void RenderView::computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint&) const
{
    // Record the entire size of the contents of the frame. Note that we don't just
    // use the viewport size (containing block) here because we want to ensure this includes
    // all children (so we can avoid walking them explicitly).
    rects.append(LayoutRect(LayoutPoint::zero(), frameView()->contentsSize()));
}

bool RenderView::requiresColumns(int desiredColumnCount) const
{
    if (m_frameView)
        return m_frameView->pagination().mode != Pagination::Unpaginated;

    return RenderBlock::requiresColumns(desiredColumnCount);
}

void RenderView::calcColumnWidth()
{
    int columnWidth = contentLogicalWidth();
    if (m_frameView && style()->hasInlineColumnAxis()) {
        if (int pageLength = m_frameView->pagination().pageLength)
            columnWidth = pageLength;
    }
    setDesiredColumnCountAndWidth(1, columnWidth);
}

ColumnInfo::PaginationUnit RenderView::paginationUnit() const
{
    if (m_frameView)
        return m_frameView->pagination().behavesLikeColumns ? ColumnInfo::Column : ColumnInfo::Page;

    return ColumnInfo::Page;
}

void RenderView::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!needsLayout());
    // RenderViews should never be called to paint with an offset not on device pixels.
    ASSERT(LayoutPoint(IntPoint(paintOffset.x(), paintOffset.y())) == paintOffset);

    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, this);

    // This avoids painting garbage between columns if there is a column gap.
    if (m_frameView && m_frameView->pagination().mode != Pagination::Unpaginated)
        paintInfo.context->fillRect(paintInfo.rect, m_frameView->baseBackgroundColor());

    paintObject(paintInfo, paintOffset);
}

static inline bool rendererObscuresBackground(RenderObject* rootObject)
{
    if (!rootObject)
        return false;

    RenderStyle* style = rootObject->style();
    if (style->visibility() != VISIBLE
        || style->opacity() != 1
        || style->hasTransform())
        return false;

    if (rootObject->compositingState() == PaintsIntoOwnBacking)
        return false;

    const RenderObject* rootRenderer = rootObject->rendererForRootBackground();
    if (rootRenderer->style()->backgroundClip() == TextFillBox)
        return false;

    return true;
}

void RenderView::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint&)
{
    // Check to see if we are enclosed by a layer that requires complex painting rules.  If so, we cannot blit
    // when scrolling, and we need to use slow repaints.  Examples of layers that require this are transparent layers,
    // layers with reflections, or transformed layers.
    // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being inside
    // a transform, transparency layer, etc.
    Element* elt;
    for (elt = document().ownerElement(); view() && elt && elt->renderer(); elt = elt->document().ownerElement()) {
        RenderLayer* layer = elt->renderer()->enclosingLayer();
        if (layer->cannotBlitToWindow()) {
            frameView()->setCannotBlitToWindow();
            break;
        }

        if (layer->enclosingCompositingLayerForRepaint()) {
            frameView()->setCannotBlitToWindow();
            break;
        }
    }

    if (document().ownerElement() || !view())
        return;

    if (paintInfo.skipRootBackground())
        return;

    bool rootFillsViewport = false;
    bool rootObscuresBackground = false;
    Node* documentElement = document().documentElement();
    if (RenderObject* rootRenderer = documentElement ? documentElement->renderer() : 0) {
        // The document element's renderer is currently forced to be a block, but may not always be.
        RenderBox* rootBox = rootRenderer->isBox() ? toRenderBox(rootRenderer) : 0;
        rootFillsViewport = rootBox && !rootBox->x() && !rootBox->y() && rootBox->width() >= width() && rootBox->height() >= height();
        rootObscuresBackground = rendererObscuresBackground(rootRenderer);
    }

    Page* page = document().page();
    float pageScaleFactor = page ? page->pageScaleFactor() : 1;

    // If painting will entirely fill the view, no need to fill the background.
    if (rootFillsViewport && rootObscuresBackground && pageScaleFactor >= 1)
        return;

    // This code typically only executes if the root element's visibility has been set to hidden,
    // if there is a transform on the <html>, or if there is a page scale factor less than 1.
    // Only fill with the base background color (typically white) if we're the root document,
    // since iframes/frames with no background in the child document should show the parent's background.
    if (frameView()->isTransparent()) // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being transparent.
        frameView()->setCannotBlitToWindow(); // The parent must show behind the child.
    else {
        Color baseColor = frameView()->baseBackgroundColor();
        if (baseColor.alpha()) {
            CompositeOperator previousOperator = paintInfo.context->compositeOperation();
            paintInfo.context->setCompositeOperation(CompositeCopy);
            paintInfo.context->fillRect(paintInfo.rect, baseColor);
            paintInfo.context->setCompositeOperation(previousOperator);
        } else {
            paintInfo.context->clearRect(paintInfo.rect);
        }
    }
}

bool RenderView::shouldRepaint(const LayoutRect& rect) const
{
    if (document().printing())
        return false;
    return m_frameView && !rect.isEmpty();
}

void RenderView::repaintViewRectangle(const LayoutRect& ur) const
{
    if (!shouldRepaint(ur))
        return;

    // We always just invalidate the root view, since we could be an iframe that is clipped out
    // or even invisible.
    Element* elt = document().ownerElement();
    if (!elt)
        m_frameView->repaintContentRectangle(pixelSnappedIntRect(ur));
    else if (RenderBox* obj = elt->renderBox()) {
        LayoutRect vr = viewRect();
        LayoutRect r = intersection(ur, vr);

        // Subtract out the contentsX and contentsY offsets to get our coords within the viewing
        // rectangle.
        r.moveBy(-vr.location());

        // FIXME: Hardcoded offsets here are not good.
        r.moveBy(obj->contentBoxRect().location());
        obj->repaintRectangle(r);
    }
}

void RenderView::repaintRectangleInViewAndCompositedLayers(const LayoutRect& ur)
{
    if (!shouldRepaint(ur))
        return;

    repaintViewRectangle(ur);

    if (compositor()->inCompositingMode()) {
        IntRect repaintRect = pixelSnappedIntRect(ur);
        compositor()->repaintCompositedLayers(&repaintRect);
    }
}

void RenderView::repaintViewAndCompositedLayers()
{
    repaint();

    if (compositor()->inCompositingMode())
        compositor()->repaintCompositedLayers();
}

void RenderView::computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect& rect, bool fixed) const
{
    // If a container was specified, and was not 0 or the RenderView,
    // then we should have found it by now.
    ASSERT_ARG(repaintContainer, !repaintContainer || repaintContainer == this);

    if (document().printing())
        return;

    if (style()->isFlippedBlocksWritingMode()) {
        // We have to flip by hand since the view's logical height has not been determined.  We
        // can use the viewport width and height.
        if (style()->isHorizontalWritingMode())
            rect.setY(viewHeight() - rect.maxY());
        else
            rect.setX(viewWidth() - rect.maxX());
    }

    if (fixed && m_frameView)
        rect.move(m_frameView->scrollOffsetForFixedPosition());

    // Apply our transform if we have one (because of full page zooming).
    if (!repaintContainer && layer() && layer()->transform())
        rect = layer()->transform()->mapRect(rect);
}

void RenderView::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(pixelSnappedIntRect(accumulatedOffset, layer()->size()));
}

void RenderView::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (wasFixed)
        *wasFixed = false;
    quads.append(FloatRect(FloatPoint(), layer()->size()));
}

static RenderObject* rendererAfterPosition(RenderObject* object, unsigned offset)
{
    if (!object)
        return 0;

    RenderObject* child = object->childAt(offset);
    return child ? child : object->nextInPreOrderAfterChildren();
}

IntRect RenderView::selectionBounds(bool clipToVisibleContent) const
{
    typedef HashMap<RenderObject*, OwnPtr<RenderSelectionInfo> > SelectionMap;
    SelectionMap selectedObjects;

    RenderObject* os = m_selectionStart;
    RenderObject* stop = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            selectedObjects.set(os, adoptPtr(new RenderSelectionInfo(os, clipToVisibleContent)));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isRenderView()) {
                OwnPtr<RenderSelectionInfo>& blockInfo = selectedObjects.add(cb, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = adoptPtr(new RenderSelectionInfo(cb, clipToVisibleContent));
                cb = cb->containingBlock();
            }
        }

        os = os->nextInPreOrder();
    }

    // Now create a single bounding box rect that encloses the whole selection.
    LayoutRect selRect;
    SelectionMap::iterator end = selectedObjects.end();
    for (SelectionMap::iterator i = selectedObjects.begin(); i != end; ++i) {
        RenderSelectionInfo* info = i->value.get();
        // RenderSelectionInfo::rect() is in the coordinates of the repaintContainer, so map to page coordinates.
        LayoutRect currRect = info->rect();
        if (RenderLayerModelObject* repaintContainer = info->repaintContainer()) {
            FloatQuad absQuad = repaintContainer->localToAbsoluteQuad(FloatRect(currRect));
            currRect = absQuad.enclosingBoundingBox();
        }
        selRect.unite(currRect);
    }
    return pixelSnappedIntRect(selRect);
}

void RenderView::repaintSelection() const
{
    HashSet<RenderBlock*> processedBlocks;

    RenderObject* end = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    for (RenderObject* o = m_selectionStart; o && o != end; o = o->nextInPreOrder()) {
        if (!o->canBeSelectionLeaf() && o != m_selectionStart && o != m_selectionEnd)
            continue;
        if (o->selectionState() == SelectionNone)
            continue;

        RenderSelectionInfo(o, true).repaint();

        // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
        for (RenderBlock* block = o->containingBlock(); block && !block->isRenderView(); block = block->containingBlock()) {
            if (!processedBlocks.add(block).isNewEntry)
                break;
            RenderSelectionInfo(block, true).repaint();
        }
    }
}

// Compositing layer dimensions take outline size into account, so we have to recompute layer
// bounds when it changes.
// FIXME: This is ugly; it would be nice to have a better way to do this.
void RenderView::setMaximalOutlineSize(int o)
{
    if (o != m_maximalOutlineSize) {
        m_maximalOutlineSize = o;

        // maximalOutlineSize affects compositing layer dimensions.
        compositor()->setCompositingLayersNeedRebuild();    // FIXME: this really just needs to be a geometry update.
    }
}

// When exploring the RenderTree looking for the nodes involved in the Selection, sometimes it's
// required to change the traversing direction because the "start" position is below the "end" one.
static inline RenderObject* getNextOrPrevRenderObjectBasedOnDirection(const RenderObject* o, const RenderObject* stop, bool& continueExploring, bool& exploringBackwards)
{
    RenderObject* next;
    if (exploringBackwards) {
        next = o->previousInPreOrder();
        continueExploring = next && !(next)->isRenderView();
    } else {
        next = o->nextInPreOrder();
        continueExploring = next && next != stop;
        exploringBackwards = !next && (next != stop);
        if (exploringBackwards) {
            next = stop->previousInPreOrder();
            continueExploring = next && !next->isRenderView();
        }
    }

    return next;
}

void RenderView::setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode blockRepaintMode)
{
    // This code makes no assumptions as to if the rendering tree is up to date or not
    // and will not try to update it. Currently clearSelection calls this
    // (intentionally) without updating the rendering tree as it doesn't care.
    // Other callers may want to force recalc style before calling this.

    // Make sure both our start and end objects are defined.
    // Check www.msnbc.com and try clicking around to find the case where this happened.
    if ((start && !end) || (end && !start))
        return;

    // Just return if the selection hasn't changed.
    if (m_selectionStart == start && m_selectionStartPos == startPos &&
        m_selectionEnd == end && m_selectionEndPos == endPos)
        return;

    // Record the old selected objects.  These will be used later
    // when we compare against the new selected objects.
    int oldStartPos = m_selectionStartPos;
    int oldEndPos = m_selectionEndPos;

    // Objects each have a single selection rect to examine.
    typedef HashMap<RenderObject*, OwnPtr<RenderSelectionInfo> > SelectedObjectMap;
    SelectedObjectMap oldSelectedObjects;
    SelectedObjectMap newSelectedObjects;

    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.
    typedef HashMap<RenderBlock*, OwnPtr<RenderBlockSelectionInfo> > SelectedBlockMap;
    SelectedBlockMap oldSelectedBlocks;
    SelectedBlockMap newSelectedBlocks;

    RenderObject* os = m_selectionStart;
    RenderObject* stop = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    bool exploringBackwards = false;
    bool continueExploring = os && (os != stop);
    while (continueExploring) {
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps.  They must be examined as well.
            oldSelectedObjects.set(os, adoptPtr(new RenderSelectionInfo(os, true)));
            if (blockRepaintMode == RepaintNewXOROld) {
                RenderBlock* cb = os->containingBlock();
                while (cb && !cb->isRenderView()) {
                    OwnPtr<RenderBlockSelectionInfo>& blockInfo = oldSelectedBlocks.add(cb, nullptr).iterator->value;
                    if (blockInfo)
                        break;
                    blockInfo = adoptPtr(new RenderBlockSelectionInfo(cb));
                    cb = cb->containingBlock();
                }
            }
        }

        os = getNextOrPrevRenderObjectBasedOnDirection(os, stop, continueExploring, exploringBackwards);
    }

    // Now clear the selection.
    SelectedObjectMap::iterator oldObjectsEnd = oldSelectedObjects.end();
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i)
        i->key->setSelectionStateIfNeeded(SelectionNone);

    // set selection start and end
    m_selectionStart = start;
    m_selectionStartPos = startPos;
    m_selectionEnd = end;
    m_selectionEndPos = endPos;

    // Update the selection status of all objects between m_selectionStart and m_selectionEnd
    if (start && start == end)
        start->setSelectionStateIfNeeded(SelectionBoth);
    else {
        if (start)
            start->setSelectionStateIfNeeded(SelectionStart);
        if (end)
            end->setSelectionStateIfNeeded(SelectionEnd);
    }

    RenderObject* o = start;
    stop = rendererAfterPosition(end, endPos);

    while (o && o != stop) {
        if (o != start && o != end && o->canBeSelectionLeaf())
            o->setSelectionStateIfNeeded(SelectionInside);
        o = o->nextInPreOrder();
    }

    if (blockRepaintMode != RepaintNothing)
        layer()->clearBlockSelectionGapsBounds();

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    o = start;
    exploringBackwards = false;
    continueExploring = o && (o != stop);
    while (continueExploring) {
        if ((o->canBeSelectionLeaf() || o == start || o == end) && o->selectionState() != SelectionNone) {
            newSelectedObjects.set(o, adoptPtr(new RenderSelectionInfo(o, true)));
            RenderBlock* cb = o->containingBlock();
            while (cb && !cb->isRenderView()) {
                OwnPtr<RenderBlockSelectionInfo>& blockInfo = newSelectedBlocks.add(cb, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = adoptPtr(new RenderBlockSelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }

        o = getNextOrPrevRenderObjectBasedOnDirection(o, stop, continueExploring, exploringBackwards);
    }

    if (!m_frameView || blockRepaintMode == RepaintNothing)
        return;

    FrameView::DeferredRepaintScope deferRepaints(*m_frameView);

    // Have any of the old selected objects changed compared to the new selection?
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i) {
        RenderObject* obj = i->key;
        RenderSelectionInfo* newInfo = newSelectedObjects.get(obj);
        RenderSelectionInfo* oldInfo = i->value.get();
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state() ||
            (m_selectionStart == obj && oldStartPos != m_selectionStartPos) ||
            (m_selectionEnd == obj && oldEndPos != m_selectionEndPos)) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedObjects.remove(obj);
            }
        }
    }

    // Any new objects that remain were not found in the old objects dict, and so they need to be updated.
    SelectedObjectMap::iterator newObjectsEnd = newSelectedObjects.end();
    for (SelectedObjectMap::iterator i = newSelectedObjects.begin(); i != newObjectsEnd; ++i)
        i->value->repaint();

    // Have any of the old blocks changed?
    SelectedBlockMap::iterator oldBlocksEnd = oldSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = oldSelectedBlocks.begin(); i != oldBlocksEnd; ++i) {
        RenderBlock* block = i->key;
        RenderBlockSelectionInfo* newInfo = newSelectedBlocks.get(block);
        RenderBlockSelectionInfo* oldInfo = i->value.get();
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedBlocks.remove(block);
            }
        }
    }

    // Any new blocks that remain were not found in the old blocks dict, and so they need to be updated.
    SelectedBlockMap::iterator newBlocksEnd = newSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = newSelectedBlocks.begin(); i != newBlocksEnd; ++i)
        i->value->repaint();
}

void RenderView::getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const
{
    startRenderer = m_selectionStart;
    startOffset = m_selectionStartPos;
    endRenderer = m_selectionEnd;
    endOffset = m_selectionEndPos;
}

void RenderView::clearSelection()
{
    layer()->repaintBlockSelectionGaps();
    setSelection(0, -1, 0, -1, RepaintNewMinusOld);
}

void RenderView::selectionStartEnd(int& startPos, int& endPos) const
{
    startPos = m_selectionStartPos;
    endPos = m_selectionEndPos;
}

bool RenderView::shouldUsePrintingLayout() const
{
    if (!document().printing() || !m_frameView)
        return false;
    return m_frameView->frame().shouldUsePrintingLayout();
}

size_t RenderView::getRetainedWidgets(Vector<RenderWidget*>& renderWidgets)
{
    size_t size = m_widgets.size();

    renderWidgets.reserveCapacity(size);

    RenderWidgetSet::const_iterator end = m_widgets.end();
    for (RenderWidgetSet::const_iterator it = m_widgets.begin(); it != end; ++it) {
        renderWidgets.uncheckedAppend(*it);
        (*it)->ref();
    }

    return size;
}

void RenderView::releaseWidgets(Vector<RenderWidget*>& renderWidgets)
{
    size_t size = renderWidgets.size();

    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->deref();
}

void RenderView::updateWidgetPositions()
{
    // updateWidgetPosition() can possibly cause layout to be re-entered (via plug-ins running
    // scripts in response to NPP_SetWindow, for example), so we need to keep the Widgets
    // alive during enumeration.

    Vector<RenderWidget*> renderWidgets;
    size_t size = getRetainedWidgets(renderWidgets);

    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->updateWidgetPosition();

    for (size_t i = 0; i < size; ++i)
        renderWidgets[i]->widgetPositionsUpdated();

    releaseWidgets(renderWidgets);
}

void RenderView::addWidget(RenderWidget* o)
{
    m_widgets.add(o);
}

void RenderView::removeWidget(RenderWidget* o)
{
    m_widgets.remove(o);
}

LayoutRect RenderView::viewRect() const
{
    if (shouldUsePrintingLayout())
        return LayoutRect(LayoutPoint(), size());
    if (m_frameView)
        return m_frameView->visibleContentRect();
    return LayoutRect();
}

IntRect RenderView::unscaledDocumentRect() const
{
    LayoutRect overflowRect(layoutOverflowRect());
    flipForWritingMode(overflowRect);
    return pixelSnappedIntRect(overflowRect);
}

bool RenderView::rootBackgroundIsEntirelyFixed() const
{
    RenderObject* rootObject = document().documentElement() ? document().documentElement()->renderer() : 0;
    if (!rootObject)
        return false;

    RenderObject* rootRenderer = rootObject->rendererForRootBackground();
    return rootRenderer->hasEntirelyFixedBackground();
}

LayoutRect RenderView::backgroundRect(RenderBox* backgroundRenderer) const
{
    if (!hasColumns())
        return unscaledDocumentRect();

    ColumnInfo* columnInfo = this->columnInfo();
    LayoutRect backgroundRect(0, 0, columnInfo->desiredColumnWidth(), columnInfo->columnHeight() * columnInfo->columnCount());
    if (!isHorizontalWritingMode())
        backgroundRect = backgroundRect.transposedRect();
    backgroundRenderer->flipForWritingMode(backgroundRect);

    return backgroundRect;
}

IntRect RenderView::documentRect() const
{
    FloatRect overflowRect(unscaledDocumentRect());
    if (hasTransform())
        overflowRect = layer()->currentTransform().mapRect(overflowRect);
    return IntRect(overflowRect);
}

int RenderView::viewHeight(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion) const
{
    int height = 0;
    if (!shouldUsePrintingLayout() && m_frameView)
        height = m_frameView->layoutSize(scrollbarInclusion).height();

    return height;
}

int RenderView::viewWidth(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion) const
{
    int width = 0;
    if (!shouldUsePrintingLayout() && m_frameView)
        width = m_frameView->layoutSize(scrollbarInclusion).width();

    return width;
}

int RenderView::viewLogicalHeight(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion) const
{
    int height = style()->isHorizontalWritingMode() ? viewHeight(scrollbarInclusion) : viewWidth(scrollbarInclusion);

    if (hasColumns() && !style()->hasInlineColumnAxis()) {
        if (int pageLength = m_frameView->pagination().pageLength)
            height = pageLength;
    }

    return height;
}

float RenderView::zoomFactor() const
{
    return m_frameView->frame().pageZoomFactor();
}

void RenderView::pushLayoutState(RenderObject* root)
{
    ASSERT(m_layoutStateDisableCount == 0);
    ASSERT(m_layoutState == 0);

    pushLayoutStateForCurrentFlowThread(root);
    m_layoutState = new LayoutState(root);
}

bool RenderView::shouldDisableLayoutStateForSubtree(RenderObject* renderer) const
{
    RenderObject* o = renderer;
    while (o) {
        if (o->hasColumns() || o->hasTransform() || o->hasReflection())
            return true;
        o = o->container();
    }
    return false;
}

void RenderView::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = document().documentElement();
    if (node) {
        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);

        LayoutPoint adjustedPoint = point;
        offsetForContents(adjustedPoint);

        result.setLocalPoint(adjustedPoint);
    }
}

bool RenderView::usesCompositing() const
{
    return m_compositor && m_compositor->inCompositingMode();
}

RenderLayerCompositor* RenderView::compositor()
{
    if (!m_compositor)
        m_compositor = adoptPtr(new RenderLayerCompositor(this));

    return m_compositor.get();
}

void RenderView::setIsInWindow(bool isInWindow)
{
    if (m_compositor)
        m_compositor->setIsInWindow(isInWindow);
}

CustomFilterGlobalContext* RenderView::customFilterGlobalContext()
{
    if (!m_customFilterGlobalContext)
        m_customFilterGlobalContext = adoptPtr(new CustomFilterGlobalContext());
    return m_customFilterGlobalContext.get();
}

void RenderView::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (hasRenderNamedFlowThreads())
        flowThreadController()->styleDidChange();
}

bool RenderView::hasRenderNamedFlowThreads() const
{
    return m_flowThreadController && m_flowThreadController->hasRenderNamedFlowThreads();
}

bool RenderView::checkTwoPassLayoutForAutoHeightRegions() const
{
    return hasRenderNamedFlowThreads() && m_flowThreadController->hasFlowThreadsWithAutoLogicalHeightRegions();
}

FlowThreadController* RenderView::flowThreadController()
{
    if (!m_flowThreadController)
        m_flowThreadController = FlowThreadController::create(this);

    return m_flowThreadController.get();
}

void RenderView::pushLayoutStateForCurrentFlowThread(const RenderObject* object)
{
    if (!m_flowThreadController)
        return;

    RenderFlowThread* currentFlowThread = m_flowThreadController->currentRenderFlowThread();
    if (!currentFlowThread)
        return;

    currentFlowThread->pushFlowThreadLayoutState(object);
}

void RenderView::popLayoutStateForCurrentFlowThread()
{
    if (!m_flowThreadController)
        return;

    RenderFlowThread* currentFlowThread = m_flowThreadController->currentRenderFlowThread();
    if (!currentFlowThread)
        return;

    currentFlowThread->popFlowThreadLayoutState();
}

IntervalArena* RenderView::intervalArena()
{
    if (!m_intervalArena)
        m_intervalArena = IntervalArena::create();
    return m_intervalArena.get();
}

bool RenderView::backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const
{
    // FIXME: Remove this main frame check. Same concept applies to subframes too.
    if (!m_frameView || !m_frameView->isMainFrame())
        return false;

    return m_frameView->hasOpaqueBackground();
}

LayoutUnit RenderView::viewportPercentageWidth(float percentage) const
{
    return viewLogicalWidth(ScrollableArea::IncludeScrollbars) * percentage / 100.f;
}

LayoutUnit RenderView::viewportPercentageHeight(float percentage) const
{
    return viewLogicalHeight(ScrollableArea::IncludeScrollbars) * percentage / 100.f;
}

LayoutUnit RenderView::viewportPercentageMin(float percentage) const
{
    return std::min(viewLogicalWidth(ScrollableArea::IncludeScrollbars), viewLogicalHeight(ScrollableArea::IncludeScrollbars))
        * percentage / 100.f;
}

LayoutUnit RenderView::viewportPercentageMax(float percentage) const
{
    return std::max(viewLogicalWidth(ScrollableArea::IncludeScrollbars), viewLogicalHeight(ScrollableArea::IncludeScrollbars))
        * percentage / 100.f;
}

FragmentationDisabler::FragmentationDisabler(RenderObject* root)
{
    RenderView* renderView = root->view();
    ASSERT(renderView);

    LayoutState* layoutState = renderView->layoutState();

    m_root = root;
    m_fragmenting = layoutState && layoutState->isPaginated();
    m_flowThreadState = m_root->flowThreadState();
#ifndef NDEBUG
    m_layoutState = layoutState;
#endif

    if (layoutState)
        layoutState->m_isPaginated = false;

    if (m_flowThreadState != RenderObject::NotInsideFlowThread)
        m_root->setFlowThreadStateIncludingDescendants(RenderObject::NotInsideFlowThread);
}

FragmentationDisabler::~FragmentationDisabler()
{
    RenderView* renderView = m_root->view();
    ASSERT(renderView);

    LayoutState* layoutState = renderView->layoutState();
#ifndef NDEBUG
    ASSERT(m_layoutState == layoutState);
#endif

    if (layoutState)
        layoutState->m_isPaginated = m_fragmenting;

    if (m_flowThreadState != RenderObject::NotInsideFlowThread)
        m_root->setFlowThreadStateIncludingDescendants(m_flowThreadState);
}

} // namespace WebCore
