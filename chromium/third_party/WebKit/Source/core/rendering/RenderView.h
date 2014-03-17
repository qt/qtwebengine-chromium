/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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
 *
 */

#ifndef RenderView_h
#define RenderView_h

#include "core/frame/FrameView.h"
#include "core/rendering/LayoutIndicator.h"
#include "core/rendering/LayoutState.h"
#include "core/rendering/RenderBlockFlow.h"
#include "platform/PODFreeListArena.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class CustomFilterGlobalContext;
class FlowThreadController;
class RenderLayerCompositor;
class RenderQuote;
class RenderWidget;

// The root of the render tree, corresponding to the CSS initial containing block.
// It's dimensions match that of the logical viewport (which may be different from
// the visible viewport in fixed-layout mode), and it is always at position (0,0)
// relative to the document (and so isn't necessarily in view).
class RenderView FINAL : public RenderBlockFlow {
public:
    explicit RenderView(Document*);
    virtual ~RenderView();

    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    virtual const char* renderName() const OVERRIDE { return "RenderView"; }

    virtual bool isRenderView() const OVERRIDE { return true; }

    virtual bool requiresLayer() const OVERRIDE { return true; }

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const OVERRIDE;

    virtual void layout() OVERRIDE;
    virtual void updateLogicalWidth() OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;

    virtual bool supportsPartialLayout() const OVERRIDE { return true; }

    virtual LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const OVERRIDE;

    // The same as the FrameView's layoutHeight/layoutWidth but with null check guards.
    int viewHeight(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion = ScrollableArea::ExcludeScrollbars) const;
    int viewWidth(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion = ScrollableArea::ExcludeScrollbars) const;
    int viewLogicalWidth(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion = ScrollableArea::ExcludeScrollbars) const
    {
        return style()->isHorizontalWritingMode() ? viewWidth(scrollbarInclusion) : viewHeight(scrollbarInclusion);
    }
    int viewLogicalHeight(ScrollableArea::IncludeScrollbarsInRect scrollbarInclusion = ScrollableArea::ExcludeScrollbars) const;

    float zoomFactor() const;

    FrameView* frameView() const { return m_frameView; }

    virtual void computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect&, bool fixed = false) const OVERRIDE;
    void repaintViewRectangle(const LayoutRect&) const;
    // Repaint the view, and all composited layers that intersect the given absolute rectangle.
    // FIXME: ideally we'd never have to do this, if all repaints are container-relative.
    void repaintRectangleInViewAndCompositedLayers(const LayoutRect&);
    void repaintViewAndCompositedLayers();

    virtual void paint(PaintInfo&, const LayoutPoint&);
    virtual void paintBoxDecorations(PaintInfo&, const LayoutPoint&) OVERRIDE;

    enum SelectionRepaintMode { RepaintNewXOROld, RepaintNewMinusOld, RepaintNothing };
    void setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos, SelectionRepaintMode = RepaintNewXOROld);
    void getSelection(RenderObject*& startRenderer, int& startOffset, RenderObject*& endRenderer, int& endOffset) const;
    void clearSelection();
    RenderObject* selectionStart() const { return m_selectionStart; }
    RenderObject* selectionEnd() const { return m_selectionEnd; }
    IntRect selectionBounds(bool clipToVisibleContent = true) const;
    void selectionStartEnd(int& startPos, int& endPos) const;
    void repaintSelection() const;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const;

    void setMaximalOutlineSize(int o);
    int maximalOutlineSize() const { return m_maximalOutlineSize; }

    void setOldMaximalOutlineSize(int o) { m_oldMaximalOutlineSize = o; }
    int oldMaximalOutlineSize() const { return m_oldMaximalOutlineSize; }

    virtual LayoutRect viewRect() const OVERRIDE;

    void updateWidgetPositions();
    void addWidget(RenderWidget*);
    void removeWidget(RenderWidget*);

    // layoutDelta is used transiently during layout to store how far an object has moved from its
    // last layout location, in order to repaint correctly.
    // If we're doing a full repaint m_layoutState will be 0, but in that case layoutDelta doesn't matter.
    LayoutSize layoutDelta() const
    {
        return m_layoutState ? m_layoutState->m_layoutDelta : LayoutSize();
    }
    void addLayoutDelta(const LayoutSize& delta)
    {
        if (m_layoutState) {
            m_layoutState->m_layoutDelta += delta;
#if !ASSERT_DISABLED
            m_layoutState->m_layoutDeltaXSaturated |= m_layoutState->m_layoutDelta.width() == LayoutUnit::max() || m_layoutState->m_layoutDelta.width() == LayoutUnit::min();
            m_layoutState->m_layoutDeltaYSaturated |= m_layoutState->m_layoutDelta.height() == LayoutUnit::max() || m_layoutState->m_layoutDelta.height() == LayoutUnit::min();
#endif
        }
    }

#if !ASSERT_DISABLED
    bool layoutDeltaMatches(const LayoutSize& delta)
    {
        if (!m_layoutState)
            return false;
        return (delta.width() == m_layoutState->m_layoutDelta.width() || m_layoutState->m_layoutDeltaXSaturated) && (delta.height() == m_layoutState->m_layoutDelta.height() || m_layoutState->m_layoutDeltaYSaturated);
    }
#endif

    bool doingFullRepaint() const { return m_frameView->needsFullRepaint(); }

    // Subtree push/pop
    void pushLayoutState(RenderObject*);
    void popLayoutState(RenderObject*) { return popLayoutState(); } // Just doing this to keep popLayoutState() private and to make the subtree calls symmetrical.

    bool shouldDisableLayoutStateForSubtree(RenderObject*) const;

    // Returns true if layoutState should be used for its cached offset and clip.
    bool layoutStateEnabled() const { return m_layoutStateDisableCount == 0 && m_layoutState; }
    LayoutState* layoutState() const { return m_layoutState; }

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&);

    LayoutUnit pageLogicalHeight() const { return m_pageLogicalHeight; }
    void setPageLogicalHeight(LayoutUnit height)
    {
        if (m_pageLogicalHeight != height) {
            m_pageLogicalHeight = height;
            m_pageLogicalHeightChanged = true;
        }
    }

    // Notification that this view moved into or out of a native window.
    void setIsInWindow(bool);

    RenderLayerCompositor* compositor();
    bool usesCompositing() const;

    CustomFilterGlobalContext* customFilterGlobalContext();

    IntRect unscaledDocumentRect() const;
    LayoutRect backgroundRect(RenderBox* backgroundRenderer) const;

    IntRect documentRect() const;

    // Renderer that paints the root background has background-images which all have background-attachment: fixed.
    bool rootBackgroundIsEntirelyFixed() const;

    bool hasRenderNamedFlowThreads() const;
    bool checkTwoPassLayoutForAutoHeightRegions() const;
    FlowThreadController* flowThreadController();

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    IntervalArena* intervalArena();

    void setRenderQuoteHead(RenderQuote* head) { m_renderQuoteHead = head; }
    RenderQuote* renderQuoteHead() const { return m_renderQuoteHead; }

    // FIXME: This is a work around because the current implementation of counters
    // requires walking the entire tree repeatedly and most pages don't actually use either
    // feature so we shouldn't take the performance hit when not needed. Long term we should
    // rewrite the counter and quotes code.
    void addRenderCounter() { m_renderCounterCount++; }
    void removeRenderCounter() { ASSERT(m_renderCounterCount > 0); m_renderCounterCount--; }
    bool hasRenderCounters() { return m_renderCounterCount; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) OVERRIDE;

    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const OVERRIDE FINAL;

    LayoutUnit viewportPercentageWidth(float percentage) const;
    LayoutUnit viewportPercentageHeight(float percentage) const;
    LayoutUnit viewportPercentageMin(float percentage) const;
    LayoutUnit viewportPercentageMax(float percentage) const;

private:
    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const OVERRIDE;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const OVERRIDE;
    virtual void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const;
    virtual bool requiresColumns(int desiredColumnCount) const OVERRIDE;
    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const OVERRIDE;

    bool initializeLayoutState(LayoutState&);

    virtual void calcColumnWidth() OVERRIDE;
    virtual ColumnInfo::PaginationUnit paginationUnit() const OVERRIDE;

    bool shouldRepaint(const LayoutRect&) const;

    // These functions may only be accessed by LayoutStateMaintainer.
    bool pushLayoutState(RenderBox* renderer, const LayoutSize& offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false, ColumnInfo* colInfo = 0)
    {
        // We push LayoutState even if layoutState is disabled because it stores layoutDelta too.
        if (!doingFullRepaint() || m_layoutState->isPaginated() || renderer->hasColumns() || renderer->flowThreadContainingBlock()
            || m_layoutState->lineGrid() || (renderer->style()->lineGrid() != RenderStyle::initialLineGrid() && renderer->isRenderBlockFlow())
            || (renderer->isRenderBlock() && toRenderBlock(renderer)->shapeInsideInfo())
            || (m_layoutState->shapeInsideInfo() && renderer->isRenderBlock() && !toRenderBlock(renderer)->allowsShapeInsideInfoSharing(m_layoutState->shapeInsideInfo()->owner()))
            ) {
            pushLayoutStateForCurrentFlowThread(renderer);
            m_layoutState = new LayoutState(m_layoutState, renderer, offset, pageHeight, pageHeightChanged, colInfo);
            return true;
        }
        return false;
    }

    void popLayoutState()
    {
        LayoutState* state = m_layoutState;
        m_layoutState = state->m_next;
        delete state;
        popLayoutStateForCurrentFlowThread();
    }

    // Suspends the LayoutState optimization. Used under transforms that cannot be represented by
    // LayoutState (common in SVG) and when manipulating the render tree during layout in ways
    // that can trigger repaint of a non-child (e.g. when a list item moves its list marker around).
    // Note that even when disabled, LayoutState is still used to store layoutDelta.
    // These functions may only be accessed by LayoutStateMaintainer or LayoutStateDisabler.
    void disableLayoutState() { m_layoutStateDisableCount++; }
    void enableLayoutState() { ASSERT(m_layoutStateDisableCount > 0); m_layoutStateDisableCount--; }

    void layoutContent(const LayoutState&);
    void layoutContentInAutoLogicalHeightRegions(const LayoutState&);
#ifndef NDEBUG
    void checkLayoutState(const LayoutState&);
#endif

    void positionDialog(RenderBox*);
    void positionDialogs();

    size_t getRetainedWidgets(Vector<RenderWidget*>&);
    void releaseWidgets(Vector<RenderWidget*>&);

    void pushLayoutStateForCurrentFlowThread(const RenderObject*);
    void popLayoutStateForCurrentFlowThread();

    friend class LayoutStateMaintainer;
    friend class LayoutStateDisabler;

    bool shouldUsePrintingLayout() const;

    FrameView* m_frameView;

    RenderObject* m_selectionStart;
    RenderObject* m_selectionEnd;

    int m_selectionStartPos;
    int m_selectionEndPos;

    int m_maximalOutlineSize; // Used to apply a fudge factor to dirty-rect checks on blocks/tables.
    int m_oldMaximalOutlineSize; // The fudge factor from the previous layout.

    typedef HashSet<RenderWidget*> RenderWidgetSet;
    RenderWidgetSet m_widgets;

    LayoutUnit m_pageLogicalHeight;
    bool m_pageLogicalHeightChanged;
    LayoutState* m_layoutState;
    unsigned m_layoutStateDisableCount;
    OwnPtr<RenderLayerCompositor> m_compositor;
    OwnPtr<CustomFilterGlobalContext> m_customFilterGlobalContext;
    OwnPtr<FlowThreadController> m_flowThreadController;
    RefPtr<IntervalArena> m_intervalArena;

    RenderQuote* m_renderQuoteHead;
    unsigned m_renderCounterCount;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderView, isRenderView());

// Stack-based class to assist with LayoutState push/pop
class LayoutStateMaintainer {
    WTF_MAKE_NONCOPYABLE(LayoutStateMaintainer);
public:
    // ctor to push now
    LayoutStateMaintainer(RenderView* view, RenderBox* root, LayoutSize offset, bool disableState = false, LayoutUnit pageHeight = 0, bool pageHeightChanged = false, ColumnInfo* colInfo = 0)
        : m_view(view)
        , m_disabled(disableState)
        , m_didStart(false)
        , m_didEnd(false)
        , m_didCreateLayoutState(false)
    {
        push(root, offset, pageHeight, pageHeightChanged, colInfo);
    }

    // ctor to maybe push later
    LayoutStateMaintainer(RenderView* view)
        : m_view(view)
        , m_disabled(false)
        , m_didStart(false)
        , m_didEnd(false)
        , m_didCreateLayoutState(false)
    {
    }

    ~LayoutStateMaintainer()
    {
        ASSERT(m_didStart == m_didEnd);   // if this fires, it means that someone did a push(), but forgot to pop().
    }

    void push(RenderBox* root, LayoutSize offset, LayoutUnit pageHeight = 0, bool pageHeightChanged = false, ColumnInfo* colInfo = 0)
    {
        ASSERT(!m_didStart);
        // We push state even if disabled, because we still need to store layoutDelta
        m_didCreateLayoutState = m_view->pushLayoutState(root, offset, pageHeight, pageHeightChanged, colInfo);
        if (m_disabled && m_didCreateLayoutState)
            m_view->disableLayoutState();
        m_didStart = true;
    }

    void pop()
    {
        if (m_didStart) {
            ASSERT(!m_didEnd);
            if (m_didCreateLayoutState) {
                m_view->popLayoutState();
                if (m_disabled)
                    m_view->enableLayoutState();
            }

            m_didEnd = true;
        }
    }

    bool didPush() const { return m_didStart; }

private:
    RenderView* m_view;
    bool m_disabled : 1;        // true if the offset and clip part of layoutState is disabled
    bool m_didStart : 1;        // true if we did a push or disable
    bool m_didEnd : 1;          // true if we popped or re-enabled
    bool m_didCreateLayoutState : 1; // true if we actually made a layout state.
};

class LayoutStateDisabler {
    WTF_MAKE_NONCOPYABLE(LayoutStateDisabler);
public:
    LayoutStateDisabler(RenderView* view)
        : m_view(view)
    {
        if (m_view)
            m_view->disableLayoutState();
    }

    ~LayoutStateDisabler()
    {
        if (m_view)
            m_view->enableLayoutState();
    }
private:
    RenderView* m_view;
};

class FragmentationDisabler {
    WTF_MAKE_NONCOPYABLE(FragmentationDisabler);
public:
    FragmentationDisabler(RenderObject* root);
    ~FragmentationDisabler();
private:
    RenderObject* m_root;
    RenderObject::FlowThreadState m_flowThreadState;
    bool m_fragmenting;
#ifndef NDEBUG
    LayoutState* m_layoutState;
#endif
};

} // namespace WebCore

#endif // RenderView_h
