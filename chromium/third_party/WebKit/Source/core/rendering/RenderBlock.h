/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef RenderBlock_h
#define RenderBlock_h

#include "core/rendering/ColumnInfo.h"
#include "core/rendering/FloatingObjects.h"
#include "core/rendering/GapRects.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLineBoxList.h"
#include "core/rendering/RootInlineBox.h"
#include "core/rendering/shapes/ShapeInsideInfo.h"
#include "core/rendering/style/ShapeValue.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextRun.h"
#include "wtf/ListHashSet.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class BasicShape;
class BidiContext;
class InlineIterator;
class LayoutStateMaintainer;
class LineLayoutState;
class RenderInline;
class RenderText;

struct BidiRun;
struct PaintInfo;
class LineInfo;
class RenderRubyRun;
class TextLayout;
class WordMeasurement;

template <class Iterator, class Run> class BidiResolver;
template <class Run> class BidiRunList;
template <class Iterator> struct MidpointState;
typedef BidiResolver<InlineIterator, BidiRun> InlineBidiResolver;
typedef MidpointState<InlineIterator> LineMidpointState;
typedef WTF::ListHashSet<RenderBox*, 16> TrackedRendererListHashSet;
typedef WTF::HashMap<const RenderBlock*, OwnPtr<TrackedRendererListHashSet> > TrackedDescendantsMap;
typedef WTF::HashMap<const RenderBox*, OwnPtr<HashSet<RenderBlock*> > > TrackedContainerMap;
typedef Vector<WordMeasurement, 64> WordMeasurements;

enum CaretType { CursorCaret, DragCaret };
enum ContainingBlockState { NewContainingBlock, SameContainingBlock };

enum TextRunFlag {
    DefaultTextRunFlags = 0,
    RespectDirection = 1 << 0,
    RespectDirectionOverride = 1 << 1
};

typedef unsigned TextRunFlags;

class RenderBlock : public RenderBox {
public:
    friend class LineLayoutState;

protected:
    explicit RenderBlock(ContainerNode*);
    virtual ~RenderBlock();

public:
    RenderObject* firstChild() const { ASSERT(children() == virtualChildren()); return children()->firstChild(); }
    RenderObject* lastChild() const { ASSERT(children() == virtualChildren()); return children()->lastChild(); }

    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    bool beingDestroyed() const { return m_beingDestroyed; }

    // These two functions are overridden for inline-block.
    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const OVERRIDE FINAL;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const;

    LayoutUnit minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const;

    RenderLineBoxList* lineBoxes() { return &m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void deleteLineBoxTree();

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0);

    void insertPositionedObject(RenderBox*);
    static void removePositionedObject(RenderBox*);
    void removePositionedObjects(RenderBlock*, ContainingBlockState = SameContainingBlock);

    TrackedRendererListHashSet* positionedObjects() const;
    bool hasPositionedObjects() const
    {
        TrackedRendererListHashSet* objects = positionedObjects();
        return objects && !objects->isEmpty();
    }

    void addPercentHeightDescendant(RenderBox*);
    static void removePercentHeightDescendant(RenderBox*);
    TrackedRendererListHashSet* percentHeightDescendants() const;
    static bool hasPercentHeightContainerMap();
    static bool hasPercentHeightDescendant(RenderBox*);
    static void clearPercentHeightDescendantsFrom(RenderBox*);
    static void removePercentHeightDescendantIfNeeded(RenderBox*);

    void setHasMarkupTruncation(bool b) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
    void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }

    bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
    bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }

    bool hasMarginBeforeQuirk(const RenderBox* child) const;
    bool hasMarginAfterQuirk(const RenderBox* child) const;

    RootInlineBox* createAndAppendRootInlineBox();

    void markShapeInsideDescendantsForLayout();
    void markPositionedObjectsForLayout();
    // FIXME: Do we really need this to be virtual? It's just so we can call this on
    // RenderBoxes without needed to check whether they're RenderBlocks first.
    virtual void markForPaginationRelayoutIfNeeded(SubtreeLayoutScope&) OVERRIDE FINAL;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all of the line layout code has been moved out of RenderBlock
    virtual bool containsFloats() const { return false; }

    // Versions that can compute line offsets with the region and page offset passed in. Used for speed to avoid having to
    // compute the region all over again when you already know it.
    LayoutUnit availableLogicalWidthForLineInRegion(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return max<LayoutUnit>(0, logicalRightOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight)
            - logicalLeftOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight));
    }
    LayoutUnit logicalRightOffsetForLineInRegion(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(region), shouldIndentText, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLineInRegion(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(region), shouldIndentText, logicalHeight);
    }
    LayoutUnit startOffsetForLineInRegion(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return style()->isLeftToRightDirection() ? logicalLeftOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight);
    }
    LayoutUnit endOffsetForLineInRegion(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return !style()->isLeftToRightDirection() ? logicalLeftOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight);
    }

    LayoutUnit availableLogicalWidthForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return availableLogicalWidthForLineInRegion(position, shouldIndentText, regionAtBlockOffset(position), logicalHeight);
    }
    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(position), shouldIndentText, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(position), shouldIndentText, logicalHeight);
    }
    LayoutUnit pixelSnappedLogicalLeftOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return roundToInt(logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight));
    }
    LayoutUnit pixelSnappedLogicalRightOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        // FIXME: Multicolumn layouts break carrying over subpixel values to the logical right offset because the lines may be shifted
        // by a subpixel value for all but the first column. This can lead to the actual pixel snapped width of the column being off
        // by one pixel when rendered versus layed out, which can result in the line being clipped. For now, we have to floor.
        // https://bugs.webkit.org/show_bug.cgi?id=105461
        return floorToInt(logicalRightOffsetForLine(position, shouldIndentText, logicalHeight));
    }
    LayoutUnit startOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return style()->isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, logicalHeight);
    }
    LayoutUnit endOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return !style()->isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, logicalHeight);
    }

    LayoutUnit textIndentOffset() const;

    virtual PositionWithAffinity positionForPoint(const LayoutPoint&) OVERRIDE;

    // Block flows subclass availableWidth to handle multi column layout (shrinking the width available to children when laying out.)
    virtual LayoutUnit availableLogicalWidth() const OVERRIDE FINAL;

    LayoutPoint flipForWritingModeIncludingColumns(const LayoutPoint&) const;
    void adjustStartEdgeForWritingModeIncludingColumns(LayoutRect&) const;

    LayoutUnit blockDirectionOffset(const LayoutSize& offsetFromBlock) const;
    LayoutUnit inlineDirectionOffset(const LayoutSize& offsetFromBlock) const;

    RootInlineBox* firstRootBox() const { return static_cast<RootInlineBox*>(firstLineBox()); }
    RootInlineBox* lastRootBox() const { return static_cast<RootInlineBox*>(lastLineBox()); }

    GapRects selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer);
    LayoutRect logicalLeftSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                       RenderObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const PaintInfo*);
    LayoutRect logicalRightSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                        RenderObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const PaintInfo*);
    void getSelectionGapInfo(SelectionState, bool& leftGap, bool& rightGap);
    RenderBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const;

    LayoutRect logicalRectToPhysicalRect(const LayoutPoint& physicalPosition, const LayoutRect& logicalRect);

    // Helper methods for computing line counts and heights for line counts.
    RootInlineBox* lineAtIndex(int) const;
    int lineCount(const RootInlineBox* = 0, bool* = 0) const;
    int heightForLineCount(int);
    void clearTruncation();

    void adjustRectForColumns(LayoutRect&) const;
    virtual void adjustForColumns(LayoutSize&, const LayoutPoint&) const OVERRIDE FINAL;
    void adjustForColumnRect(LayoutSize& offset, const LayoutPoint& locationInContainer) const;

    void addContinuationWithOutline(RenderInline*);
    bool paintsContinuationOutline(RenderInline*);

    virtual RenderBoxModelObject* virtualContinuation() const OVERRIDE FINAL { return continuation(); }
    bool isAnonymousBlockContinuation() const { return continuation() && isAnonymousBlock(); }
    RenderInline* inlineElementContinuation() const;
    RenderBlock* blockElementContinuation() const;

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    static RenderBlock* createAnonymousWithParentRendererAndDisplay(const RenderObject*, EDisplay = BLOCK);
    static RenderBlockFlow* createAnonymousColumnsWithParentRenderer(const RenderObject*);
    static RenderBlockFlow* createAnonymousColumnSpanWithParentRenderer(const RenderObject*);
    RenderBlock* createAnonymousBlock(EDisplay display = BLOCK) const { return createAnonymousWithParentRendererAndDisplay(this, display); }
    RenderBlockFlow* createAnonymousColumnsBlock() const { return createAnonymousColumnsWithParentRenderer(this); }
    RenderBlockFlow* createAnonymousColumnSpanBlock() const { return createAnonymousColumnSpanWithParentRenderer(this); }

    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const OVERRIDE;

    ColumnInfo* columnInfo() const;
    int columnGap() const;

    void updateColumnInfoFromStyle(RenderStyle*);

    // These two functions take the ColumnInfo* to avoid repeated lookups of the info in the global HashMap.
    unsigned columnCount(ColumnInfo*) const;
    LayoutRect columnRectAt(ColumnInfo*, unsigned) const;

    LayoutUnit paginationStrut() const { return m_rareData ? m_rareData->m_paginationStrut : LayoutUnit(); }
    void setPaginationStrut(LayoutUnit);

    bool shouldBreakAtLineToAvoidWidow() const { return m_rareData && m_rareData->m_lineBreakToAvoidWidow >= 0; }
    void clearShouldBreakAtLineToAvoidWidow() const;
    int lineBreakToAvoidWidow() const { return m_rareData ? m_rareData->m_lineBreakToAvoidWidow : -1; }
    void setBreakAtLineToAvoidWidow(int);
    void clearDidBreakAtLineToAvoidWidow();
    void setDidBreakAtLineToAvoidWidow();
    bool didBreakAtLineToAvoidWidow() const { return m_rareData && m_rareData->m_didBreakAtLineToAvoidWidow; }

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset() const { return m_rareData ? m_rareData->m_pageLogicalOffset : LayoutUnit(); }
    void setPageLogicalOffset(LayoutUnit);

    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    enum ApplyLayoutDeltaMode { ApplyLayoutDelta, DoNotApplyLayoutDelta };
    LayoutUnit logicalWidthForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->width() : child->height(); }
    LayoutUnit logicalHeightForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->height() : child->width(); }
    LayoutUnit logicalTopForChild(const RenderBox* child) const { return isHorizontalWritingMode() ? child->y() : child->x(); }
    void setLogicalLeftForChild(RenderBox* child, LayoutUnit logicalLeft, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    void setLogicalTopForChild(RenderBox* child, LayoutUnit logicalTop, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    LayoutUnit marginBeforeForChild(const RenderBoxModelObject* child) const { return child->marginBefore(style()); }
    LayoutUnit marginAfterForChild(const RenderBoxModelObject* child) const { return child->marginAfter(style()); }
    LayoutUnit marginStartForChild(const RenderBoxModelObject* child) const { return child->marginStart(style()); }
    LayoutUnit marginEndForChild(const RenderBoxModelObject* child) const { return child->marginEnd(style()); }
    void setMarginStartForChild(RenderBox* child, LayoutUnit value) const { child->setMarginStart(value, style()); }
    void setMarginEndForChild(RenderBox* child, LayoutUnit value) const { child->setMarginEnd(value, style()); }
    void setMarginBeforeForChild(RenderBox* child, LayoutUnit value) const { child->setMarginBefore(value, style()); }
    void setMarginAfterForChild(RenderBox* child, LayoutUnit value) const { child->setMarginAfter(value, style()); }
    LayoutUnit collapsedMarginBeforeForChild(const RenderBox* child) const;
    LayoutUnit collapsedMarginAfterForChild(const RenderBox* child) const;

    virtual void updateFirstLetter();

    virtual void scrollbarsChanged(bool /*horizontalScrollbarChanged*/, bool /*verticalScrollbarChanged*/) { };

    LayoutUnit logicalLeftOffsetForContent(RenderRegion*) const;
    LayoutUnit logicalRightOffsetForContent(RenderRegion*) const;
    LayoutUnit availableLogicalWidthForContent(RenderRegion* region) const
    {
        return max<LayoutUnit>(0, logicalRightOffsetForContent(region) - logicalLeftOffsetForContent(region));
    }
    LayoutUnit startOffsetForContent(RenderRegion* region) const
    {
        return style()->isLeftToRightDirection() ? logicalLeftOffsetForContent(region) : logicalWidth() - logicalRightOffsetForContent(region);
    }
    LayoutUnit endOffsetForContent(RenderRegion* region) const
    {
        return !style()->isLeftToRightDirection() ? logicalLeftOffsetForContent(region) : logicalWidth() - logicalRightOffsetForContent(region);
    }
    LayoutUnit logicalLeftOffsetForContent(LayoutUnit blockOffset) const
    {
        return logicalLeftOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit logicalRightOffsetForContent(LayoutUnit blockOffset) const
    {
        return logicalRightOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit availableLogicalWidthForContent(LayoutUnit blockOffset) const
    {
        return availableLogicalWidthForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit startOffsetForContent(LayoutUnit blockOffset) const
    {
        return startOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit endOffsetForContent(LayoutUnit blockOffset) const
    {
        return endOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit logicalLeftOffsetForContent() const { return isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }
    LayoutUnit logicalRightOffsetForContent() const { return logicalLeftOffsetForContent() + availableLogicalWidth(); }
    LayoutUnit startOffsetForContent() const { return style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }
    LayoutUnit endOffsetForContent() const { return !style()->isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }

    LayoutUnit computeStartPositionDeltaForChildAvoidingFloats(const RenderBox* child, LayoutUnit childMarginStart, RenderRegion* = 0);

#ifndef NDEBUG
    void checkPositionedObjectsNeedLayout();
    void showLineTreeAndMark(const InlineBox* = 0, const char* = 0, const InlineBox* = 0, const char* = 0, const RenderObject* = 0) const;
#endif

    ShapeInsideInfo* ensureShapeInsideInfo()
    {
        if (!m_rareData || !m_rareData->m_shapeInsideInfo)
            setShapeInsideInfo(ShapeInsideInfo::createInfo(this));
        return m_rareData->m_shapeInsideInfo.get();
    }
    ShapeInsideInfo* shapeInsideInfo() const
    {
        return m_rareData && m_rareData->m_shapeInsideInfo && ShapeInsideInfo::isEnabledFor(this) ? m_rareData->m_shapeInsideInfo.get() : 0;
    }
    void setShapeInsideInfo(PassOwnPtr<ShapeInsideInfo> value)
    {
        if (!m_rareData)
            m_rareData = adoptPtr(new RenderBlockRareData());
        m_rareData->m_shapeInsideInfo = value;
    }
    ShapeInsideInfo* layoutShapeInsideInfo() const;
    bool allowsShapeInsideInfoSharing(const RenderBlock* other) const;
    LayoutSize logicalOffsetFromShapeAncestorContainer(const RenderBlock* container) const;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) OVERRIDE;

    // inline-block elements paint all phases atomically. This function ensures that. Certain other elements
    // (grid items, flex items) require this behavior as well, and this function exists as a helper for them.
    // It is expected that the caller will call this function independent of the value of paintInfo.phase.
    static void paintAsInlineBlock(RenderObject*, PaintInfo&, const LayoutPoint&);
protected:
    virtual void willBeDestroyed();

    void dirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope&);

    virtual void layout();
    virtual void didLayout(ResourceLoadPriorityOptimizer&);
    virtual void didScroll(ResourceLoadPriorityOptimizer&);
    void updateStyleImageLoadingPriorities(ResourceLoadPriorityOptimizer&);

    void layoutPositionedObjects(bool relayoutChildren, bool fixedPositionObjectsOnly = false);
    void markFixedPositionObjectForLayoutIfNeeded(RenderObject* child, SubtreeLayoutScope&);

    LayoutUnit marginIntrinsicLogicalWidthForChild(RenderBox* child) const;

    virtual bool supportsPartialLayout() const OVERRIDE { return true; };

    virtual void paint(PaintInfo&, const LayoutPoint&);
    virtual void paintObject(PaintInfo&, const LayoutPoint&);
    virtual void paintChildren(PaintInfo&, const LayoutPoint&);
    void paintChild(RenderBox*, PaintInfo&, const LayoutPoint&);
    void paintChildAsInlineBlock(RenderBox*, PaintInfo&, const LayoutPoint&);

    LayoutUnit logicalRightOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalRightOffsetForLine(logicalRightFloatOffsetForLine(logicalTop, fixedOffset, logicalHeight), applyTextIndent);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalLeftOffsetForLine(logicalLeftFloatOffsetForLine(logicalTop, fixedOffset, logicalHeight), applyTextIndent);
    }

    virtual void adjustInlineDirectionLineBounds(int /* expansionOpportunityCount */, float& /* logicalLeft */, float& /* logicalWidth */) const { }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;
    void adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    virtual int firstLineBoxBaseline() const;
    virtual int inlineBlockBaseline(LineDirectionMode) const OVERRIDE;
    int lastLineBoxBaseline(LineDirectionMode) const;

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&);

    // Delay update scrollbar until finishDelayRepaint() will be
    // called. This function is used when a flexbox is laying out its
    // descendant. If multiple calls are made to startDelayRepaint(),
    // finishDelayRepaint() will do nothing until finishDelayRepaint()
    // is called the same number of times.
    static void startDelayUpdateScrollInfo();
    static void finishDelayUpdateScrollInfo();

    void updateScrollInfoAfterLayout();

    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual bool hasLineIfEmpty() const;

    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

    void setDesiredColumnCountAndWidth(int, LayoutUnit);

public:
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool = false);
protected:
    virtual void addOverflowFromChildren();
    void addOverflowFromPositionedObjects();
    void addOverflowFromBlockChildren();
    void addVisualOverflowFromTheme();

    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) OVERRIDE;

    virtual void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const OVERRIDE;

    bool updateRegionsAndShapesLogicalSize(RenderFlowThread*);
    void computeRegionRangeForBlock(RenderFlowThread*);

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox*);

    virtual void checkForPaginationLogicalHeightChange(LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged, bool& hasSpecifiedPageLogicalHeight);

    virtual bool isInlineBlockOrInlineTable() const OVERRIDE FINAL { return isInline() && isReplaced(); }

private:
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit) const { return fixedOffset; }
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit) const { return fixedOffset; }
    LayoutUnit adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const;
    LayoutUnit adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const;

    void computeShapeSize();
    void updateRegionsAndShapesAfterChildLayout(RenderFlowThread*, bool);
    void updateShapeInsideInfoAfterStyleChange(const ShapeValue*, const ShapeValue* oldShape);
    void relayoutShapeDescendantIfMoved(RenderBlock* child, LayoutSize offset);

    virtual RenderObjectChildList* virtualChildren() OVERRIDE FINAL { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const OVERRIDE FINAL { return children(); }

    virtual const char* renderName() const;

    virtual bool isRenderBlock() const OVERRIDE FINAL { return true; }

    void makeChildrenNonInline(RenderObject* insertionPoint = 0);
    virtual void removeLeftoverAnonymousBlock(RenderBlock* child);

    static void collapseAnonymousBlockChild(RenderBlock* parent, RenderBlock* child);
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void moveAllChildrenIncludingFloatsTo(RenderBlock* toBlock, bool fullRemoveInsert) { moveAllChildrenTo(toBlock, fullRemoveInsert); }

    virtual void dirtyLinesFromChangedChild(RenderObject* child) OVERRIDE FINAL { m_lineBoxes.dirtyLinesFromChangedChild(this, child); }

    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild);
    void addChildToAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild);

    void addChildIgnoringAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild = 0);

    virtual bool isSelfCollapsingBlock() const OVERRIDE FINAL;

    void insertIntoTrackedRendererMaps(RenderBox* descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);
    static void removeFromTrackedRendererMaps(RenderBox* descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);

    virtual RootInlineBox* createRootInlineBox() { return 0; } // Subclassed by RenderBlockFlow, SVG and Ruby.

    // Called to lay out the legend for a fieldset or the ruby text of a ruby run.
    virtual RenderObject* layoutSpecialExcludedChild(bool /*relayoutChildren*/, SubtreeLayoutScope&) { return 0; }

    void createFirstLetterRenderer(RenderObject* firstLetterBlock, RenderObject* currentChild, unsigned length);
    void updateFirstLetterStyle(RenderObject* firstLetterBlock, RenderObject* firstLetterContainer);

    Node* nodeForHitTest() const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool) { }
    void paintContents(PaintInfo&, const LayoutPoint&);
    void paintColumnContents(PaintInfo&, const LayoutPoint&, bool paintFloats = false);
    void paintColumnRules(PaintInfo&, const LayoutPoint&);
    void paintSelection(PaintInfo&, const LayoutPoint&);
    void paintCaret(PaintInfo&, const LayoutPoint&, CaretType);

    bool hasCaret() const { return hasCaret(CursorCaret) || hasCaret(DragCaret); }
    bool hasCaret(CaretType) const;

    virtual bool avoidsFloats() const;

    bool hitTestColumns(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    bool hitTestContents(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset);

    // FIXME: Make this method const so we can remove the const_cast in computeIntrinsicLogicalWidths.
    void computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth);
    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const;

    virtual LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const OVERRIDE FINAL;
    virtual RenderStyle* outlineStyleForRepaint() const OVERRIDE FINAL;

    virtual RenderObject* hoverAncestor() const OVERRIDE FINAL;
    virtual void updateDragState(bool dragOn) OVERRIDE FINAL;
    virtual void childBecameNonInline(RenderObject* child) OVERRIDE FINAL;

    virtual LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool /*clipToVisibleContent*/) OVERRIDE FINAL
    {
        return selectionGapRectsForRepaint(repaintContainer);
    }
    virtual bool shouldPaintSelectionGaps() const OVERRIDE FINAL;
    bool isSelectionRoot() const;
    GapRects selectionGaps(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                           LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo* = 0);
    GapRects blockSelectionGaps(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo*);
    LayoutRect blockSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                 LayoutUnit lastLogicalTop, LayoutUnit lastLogicalLeft, LayoutUnit lastLogicalRight, LayoutUnit logicalBottom, const PaintInfo*);
    LayoutUnit logicalLeftSelectionOffset(RenderBlock* rootBlock, LayoutUnit position);
    LayoutUnit logicalRightSelectionOffset(RenderBlock* rootBlock, LayoutUnit position);

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void clipOutFloatingObjects(RenderBlock*, const PaintInfo*, const LayoutPoint&, const LayoutSize&) { };

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const;

    LayoutUnit desiredColumnWidth() const;

    void paintContinuationOutlines(PaintInfo&, const LayoutPoint&);

    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = 0) OVERRIDE FINAL;

    void adjustPointToColumnContents(LayoutPoint&) const;

    void fitBorderToLinesIfNeeded(); // Shrink the box in which the border paints if border-fit is set.
    virtual void adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const; // Helper function for borderFitAdjust

    void markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest = 0);

    Position positionForBox(InlineBox*, bool start = true) const;
    PositionWithAffinity positionForPointWithInlineChildren(const LayoutPoint&);

    virtual void calcColumnWidth();
    void makeChildrenAnonymousColumnBlocks(RenderObject* beforeChild, RenderBlockFlow* newBlockBox, RenderObject* newChild);

    bool expandsToEncloseOverhangingFloats() const;

    void splitBlocks(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                     RenderObject* beforeChild, RenderBoxModelObject* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderBoxModelObject* oldCont);
    RenderBlock* clone() const;
    RenderBlock* continuationBefore(RenderObject* beforeChild);
    RenderBlockFlow* containingColumnsBlock(bool allowAnonymousColumnBlock = true);
    RenderBlockFlow* columnsBlockForSpanningElement(RenderObject* newChild);

    // End helper functions and structs used by layoutBlockChildren.

protected:
    void determineLogicalLeftPositionForChild(RenderBox* child, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);

    // Pagination routines.
    bool relayoutToAvoidWidows(LayoutStateMaintainer&);

    // Returns the logicalOffset at the top of the next page. If the offset passed in is already at the top of the current page,
    // then nextPageLogicalTop with ExcludePageBoundary will still move to the top of the next page. nextPageLogicalTop with
    // IncludePageBoundary set will not.
    //
    // For a page height of 800px, the first rule will return 800 if the value passed in is 0. The second rule will simply return 0.
    enum PageBoundaryRule { ExcludePageBoundary, IncludePageBoundary };
    LayoutUnit nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule = ExcludePageBoundary) const;
    bool hasNextPage(LayoutUnit logicalOffset, PageBoundaryRule = ExcludePageBoundary) const;

    virtual ColumnInfo::PaginationUnit paginationUnit() const;

public:
    LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const;
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit offset) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule = IncludePageBoundary) const;

protected:
    bool pushToNextPageWithMinimumLogicalHeight(LayoutUnit& adjustment, LayoutUnit logicalOffset, LayoutUnit minimumLogicalHeight) const;

    // A page break is required at some offset due to space shortage in the current fragmentainer.
    void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage);

    // Update minimum page height required to avoid fragmentation where it shouldn't occur (inside
    // unbreakable content, between orphans and widows, etc.). This will be used as a hint to the
    // column balancer to help set a good minimum column height.
    void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight);

    LayoutUnit adjustForUnsplittableChild(RenderBox* child, LayoutUnit logicalOffset, bool includeMargins = false); // If the child is unsplittable and can't fit on the current page, return the top of the next page/column.
    void adjustLinePositionForPagination(RootInlineBox*, LayoutUnit& deltaOffset, RenderFlowThread*); // Computes a deltaOffset value that put a line at the top of the next page if it doesn't fit on the current page.
    void updateRegionForLine(RootInlineBox*) const;

    // Adjust from painting offsets to the local coords of this renderer
    void offsetForContents(LayoutPoint&) const;

    // This function is called to test a line box that has moved in the block direction to see if it has ended up in a new
    // region/page/column that has a different available line width than the old one. Used to know when you have to dirty a
    // line, i.e., that it can't be re-used.
    bool lineWidthForPaginatedLineChanged(RootInlineBox*, LayoutUnit lineDelta, RenderFlowThread*) const;

    bool logicalWidthChangedInRegions(RenderFlowThread*) const;

    virtual bool requiresColumns(int desiredColumnCount) const;

    virtual bool updateLogicalWidthAndColumnWidth();

    virtual bool canCollapseAnonymousBlockChild() const { return true; }

public:
    virtual LayoutUnit offsetFromLogicalTopOfFirstPage() const OVERRIDE FINAL;
    RenderRegion* regionAtBlockOffset(LayoutUnit) const;
    RenderRegion* clampToStartAndEndRegions(RenderRegion*) const;

public:

    // Allocated only when some of these fields have non-default values
    struct RenderBlockRareData {
        WTF_MAKE_NONCOPYABLE(RenderBlockRareData); WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderBlockRareData()
            : m_paginationStrut(0)
            , m_pageLogicalOffset(0)
            , m_lineBreakToAvoidWidow(-1)
            , m_didBreakAtLineToAvoidWidow(false)
        {
        }

        LayoutUnit m_paginationStrut;
        LayoutUnit m_pageLogicalOffset;

        int m_lineBreakToAvoidWidow;
        OwnPtr<ShapeInsideInfo> m_shapeInsideInfo;
        bool m_didBreakAtLineToAvoidWidow : 1;
     };

protected:
    OwnPtr<RenderBlockRareData> m_rareData;

    RenderObjectChildList m_children;
    RenderLineBoxList m_lineBoxes;   // All of the root line boxes created for this block flow.  For example, <div>Hello<br>world.</div> will have two total lines for the <div>.

    mutable signed m_lineHeight : 27;
    unsigned m_hasMarginBeforeQuirk : 1; // Note these quirk values can't be put in RenderBlockRareData since they are set too frequently.
    unsigned m_hasMarginAfterQuirk : 1;
    unsigned m_beingDestroyed : 1;
    unsigned m_hasMarkupTruncation : 1;
    unsigned m_hasBorderOrPaddingLogicalWidthChanged : 1;

    // RenderRubyBase objects need to be able to split and merge, moving their children around
    // (calling moveChildTo, moveAllChildrenTo, and makeChildrenNonInline).
    friend class RenderRubyBase;
    // FIXME-BLOCKFLOW: Remove this when the line layout stuff has all moved out of RenderBlock
    friend class LineBreaker;

    // FIXME: This is temporary as we move code that accesses block flow
    // member variables out of RenderBlock and into RenderBlockFlow.
    friend class RenderBlockFlow;
};


inline bool RenderBlock::allowsShapeInsideInfoSharing(const RenderBlock* other) const
{
    if (!other)
        return false;
    for (const RenderBlock* current = this; current && current != other && !current->isRenderFlowThread(); current = current->containingBlock()) {
        if (current->isInline() || current->isFloating())
            return false;
        if (current->parent() != current->containingBlock())
            return false;
    }
    return true;
}

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderBlock, isRenderBlock());

} // namespace WebCore

#endif // RenderBlock_h
