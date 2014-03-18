/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderBlockFlow_h
#define RenderBlockFlow_h

#include "core/rendering/FloatingObjects.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/style/RenderStyleConstants.h"

namespace WebCore {

class MarginInfo;
class LineBreaker;
class LineWidth;
class RenderNamedFlowFragment;

class RenderBlockFlow : public RenderBlock {
public:
    explicit RenderBlockFlow(ContainerNode*);
    virtual ~RenderBlockFlow();

    static RenderBlockFlow* createAnonymous(Document*);
    RenderBlockFlow* createAnonymousBlockFlow() const;

    virtual bool isRenderBlockFlow() const OVERRIDE FINAL { return true; }

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) OVERRIDE;

    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false) OVERRIDE;
    virtual void deleteLineBoxTree() OVERRIDE FINAL;

    void markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove = 0, bool inLayout = true);
    void markSiblingsWithFloatsForLayout(RenderBox* floatToRemove = 0);

    virtual bool containsFloats() const OVERRIDE FINAL { return m_floatingObjects && !m_floatingObjects->set().isEmpty(); }
    bool containsFloat(RenderBox*) const;

    void removeFloatingObjects();

    bool generatesLineBoxesForInlineChild(RenderObject*);

    LayoutUnit logicalTopForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->y() : floatingObject->x(); }
    LayoutUnit logicalBottomForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->maxY() : floatingObject->maxX(); }
    LayoutUnit logicalLeftForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->x() : floatingObject->y(); }
    LayoutUnit logicalRightForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->maxX() : floatingObject->maxY(); }
    LayoutUnit logicalWidthForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->width() : floatingObject->height(); }
    LayoutUnit logicalHeightForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->height() : floatingObject->width(); }
    LayoutSize logicalSizeForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? LayoutSize(floatingObject->width(), floatingObject->height()) : LayoutSize(floatingObject->height(), floatingObject->width()); }

    int pixelSnappedLogicalTopForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedY() : floatingObject->frameRect().pixelSnappedX(); }
    int pixelSnappedLogicalBottomForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedMaxY() : floatingObject->frameRect().pixelSnappedMaxX(); }
    int pixelSnappedLogicalLeftForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedX() : floatingObject->frameRect().pixelSnappedY(); }
    int pixelSnappedLogicalRightForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedMaxX() : floatingObject->frameRect().pixelSnappedMaxY(); }

    void setLogicalTopForFloat(FloatingObject* floatingObject, LayoutUnit logicalTop)
    {
        if (isHorizontalWritingMode())
            floatingObject->setY(logicalTop);
        else
            floatingObject->setX(logicalTop);
    }
    void setLogicalLeftForFloat(FloatingObject* floatingObject, LayoutUnit logicalLeft)
    {
        if (isHorizontalWritingMode())
            floatingObject->setX(logicalLeft);
        else
            floatingObject->setY(logicalLeft);
    }
    void setLogicalHeightForFloat(FloatingObject* floatingObject, LayoutUnit logicalHeight)
    {
        if (isHorizontalWritingMode())
            floatingObject->setHeight(logicalHeight);
        else
            floatingObject->setWidth(logicalHeight);
    }
    void setLogicalWidthForFloat(FloatingObject* floatingObject, LayoutUnit logicalWidth)
    {
        if (isHorizontalWritingMode())
            floatingObject->setWidth(logicalWidth);
        else
            floatingObject->setHeight(logicalWidth);
    }

    LayoutUnit startAlignedOffsetForLine(LayoutUnit position, bool shouldIndentText);

    void setStaticInlinePositionForChild(RenderBox*, LayoutUnit blockOffset, LayoutUnit inlinePosition);
    void updateStaticInlinePositionForChild(RenderBox*, LayoutUnit logicalTop);

    static bool shouldSkipCreatingRunsForObject(RenderObject* obj)
    {
        return obj->isFloating() || (obj->isOutOfFlowPositioned() && !obj->style()->isOriginalDisplayInlineType() && !obj->container()->isRenderInline());
    }

    static TextRun constructTextRun(RenderObject* context, const Font&, const String&, RenderStyle*,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion, TextRunFlags = DefaultTextRunFlags);

    static TextRun constructTextRun(RenderObject* context, const Font&, const RenderText*, RenderStyle*,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    static TextRun constructTextRun(RenderObject* context, const Font&, const RenderText*, unsigned offset, unsigned length, RenderStyle*,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    static TextRun constructTextRun(RenderObject* context, const Font&, const RenderText*, unsigned offset, RenderStyle*,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    static TextRun constructTextRun(RenderObject* context, const Font&, const LChar* characters, int length, RenderStyle*,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    static TextRun constructTextRun(RenderObject* context, const Font&, const UChar* characters, int length, RenderStyle*,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    RootInlineBox* lineGridBox() const { return m_rareData ? m_rareData->m_lineGridBox : 0; }
    void setLineGridBox(RootInlineBox* box)
    {
        RenderBlockFlow::RenderBlockFlowRareData& rareData = ensureRareData();
        if (rareData.m_lineGridBox)
            rareData.m_lineGridBox->destroy();
        rareData.m_lineGridBox = box;
    }
    void layoutLineGridBox();

    void addOverflowFromInlineChildren();

    GapRects inlineSelectionGaps(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const PaintInfo*);
protected:
    // Only used by RenderSVGText, which explicitly overrides RenderBlock::layoutBlock(), do NOT use for anything else.
    void forceLayoutInlineChildren()
    {
        LayoutUnit repaintLogicalTop = 0;
        LayoutUnit repaintLogicalBottom = 0;
        rebuildFloatsFromIntruding();
        layoutInlineChildren(true, repaintLogicalTop, repaintLogicalBottom);
    }

    void createFloatingObjects();

    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle) OVERRIDE;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    void addOverflowFromFloats();

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeDestroyed() OVERRIDE;
private:
    void layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom, SubtreeLayoutScope&);
    void layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    void layoutBlockChild(RenderBox* child, MarginInfo&, LayoutUnit& previousFloatLogicalBottom, LayoutUnit& maxFloatLogicalBottom);
    void adjustPositionedBlock(RenderBox* child, const MarginInfo&);
    void adjustFloatingBlock(const MarginInfo&);

    void rebuildFloatsFromIntruding();

    LayoutPoint flipFloatForWritingModeForChild(const FloatingObject*, const LayoutPoint&) const;

    LayoutUnit xPositionForFloatIncludingMargin(const FloatingObject* child) const
    {
        if (isHorizontalWritingMode())
            return child->x() + child->renderer()->marginLeft();

        return child->x() + marginBeforeForChild(child->renderer());
    }

    LayoutUnit yPositionForFloatIncludingMargin(const FloatingObject* child) const
    {
        if (isHorizontalWritingMode())
            return child->y() + marginBeforeForChild(child->renderer());

        return child->y() + child->renderer()->marginTop();
    }

    LayoutPoint computeLogicalLocationForFloat(const FloatingObject*, LayoutUnit logicalTopOffset) const;

    FloatingObject* insertFloatingObject(RenderBox*);
    void removeFloatingObject(RenderBox*);
    void removeFloatingObjectsBelow(FloatingObject*, int logicalOffset);

    // Called from lineWidth, to position the floats added in the last line.
    // Returns true if and only if it has positioned any floats.
    bool positionNewFloats();

    LayoutUnit getClearDelta(RenderBox* child, LayoutUnit yPos);

    bool hasOverhangingFloats() { return parent() && !hasColumns() && containsFloats() && lowestFloatLogicalBottom() > logicalHeight(); }
    bool hasOverhangingFloat(RenderBox*);
    void addIntrudingFloats(RenderBlockFlow* prev, LayoutUnit xoffset, LayoutUnit yoffset);
    LayoutUnit addOverhangingFloats(RenderBlockFlow* child, bool makeChildPaintOtherFloats);

    LayoutUnit lowestFloatLogicalBottom(FloatingObject::Type = FloatingObject::FloatLeftRight) const;
    LayoutUnit nextFloatLogicalBottomBelow(LayoutUnit, ShapeOutsideFloatOffsetMode = ShapeOutsideFloatMarginBoxOffset) const;

    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset) OVERRIDE FINAL;

    virtual void moveAllChildrenIncludingFloatsTo(RenderBlock* toBlock, bool fullRemoveInsert) OVERRIDE;
    virtual void repaintOverhangingFloats(bool paintAllDescendants) OVERRIDE FINAL;
    virtual void repaintOverflow() OVERRIDE;
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool preservePhase = false) OVERRIDE FINAL;
    virtual void clipOutFloatingObjects(RenderBlock*, const PaintInfo*, const LayoutPoint&, const LayoutSize&) OVERRIDE;
    void clearFloats(EClear);

    virtual LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const OVERRIDE;
    virtual LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const OVERRIDE;

    LayoutUnit logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const;
    LayoutUnit logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const;

    virtual void adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const OVERRIDE; // Helper function for borderFitAdjust

    virtual RootInlineBox* createRootInlineBox() OVERRIDE;

    void updateLogicalWidthForAlignment(const ETextAlign&, const RootInlineBox*, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount);
    virtual bool relayoutForPagination(bool hasSpecifiedPageLogicalHeight, LayoutUnit pageLogicalHeight, LayoutStateMaintainer&);
public:
    struct FloatWithRect {
        FloatWithRect(RenderBox* f)
            : object(f)
            , rect(LayoutRect(f->x() - f->marginLeft(), f->y() - f->marginTop(), f->width() + f->marginWidth(), f->height() + f->marginHeight()))
            , everHadLayout(f->everHadLayout())
        {
        }

        RenderBox* object;
        LayoutRect rect;
        bool everHadLayout;
    };

    class MarginValues {
    public:
        MarginValues(LayoutUnit beforePos, LayoutUnit beforeNeg, LayoutUnit afterPos, LayoutUnit afterNeg)
            : m_positiveMarginBefore(beforePos)
            , m_negativeMarginBefore(beforeNeg)
            , m_positiveMarginAfter(afterPos)
            , m_negativeMarginAfter(afterNeg)
        { }

        LayoutUnit positiveMarginBefore() const { return m_positiveMarginBefore; }
        LayoutUnit negativeMarginBefore() const { return m_negativeMarginBefore; }
        LayoutUnit positiveMarginAfter() const { return m_positiveMarginAfter; }
        LayoutUnit negativeMarginAfter() const { return m_negativeMarginAfter; }

        void setPositiveMarginBefore(LayoutUnit pos) { m_positiveMarginBefore = pos; }
        void setNegativeMarginBefore(LayoutUnit neg) { m_negativeMarginBefore = neg; }
        void setPositiveMarginAfter(LayoutUnit pos) { m_positiveMarginAfter = pos; }
        void setNegativeMarginAfter(LayoutUnit neg) { m_negativeMarginAfter = neg; }

    private:
        LayoutUnit m_positiveMarginBefore;
        LayoutUnit m_negativeMarginBefore;
        LayoutUnit m_positiveMarginAfter;
        LayoutUnit m_negativeMarginAfter;
    };
    MarginValues marginValuesForChild(RenderBox* child) const;

    virtual void updateLogicalHeight() OVERRIDE;

    // Allocated only when some of these fields have non-default values
    struct RenderBlockFlowRareData {
        WTF_MAKE_NONCOPYABLE(RenderBlockFlowRareData); WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderBlockFlowRareData(const RenderBlockFlow* block)
            : m_margins(positiveMarginBeforeDefault(block), negativeMarginBeforeDefault(block), positiveMarginAfterDefault(block), negativeMarginAfterDefault(block))
            , m_lineGridBox(0)
            , m_discardMarginBefore(false)
            , m_discardMarginAfter(false)
            , m_renderNamedFlowFragment(0)
        {
        }

        static LayoutUnit positiveMarginBeforeDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(block->marginBefore(), 0);
        }
        static LayoutUnit negativeMarginBeforeDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(-block->marginBefore(), 0);
        }
        static LayoutUnit positiveMarginAfterDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(block->marginAfter(), 0);
        }
        static LayoutUnit negativeMarginAfterDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(-block->marginAfter(), 0);
        }

        MarginValues m_margins;

        RootInlineBox* m_lineGridBox;

        bool m_discardMarginBefore : 1;
        bool m_discardMarginAfter : 1;
        RenderNamedFlowFragment* m_renderNamedFlowFragment;
    };
    LayoutUnit marginOffsetForSelfCollapsingBlock();

    RenderNamedFlowFragment* renderNamedFlowFragment() const { return m_rareData ? m_rareData->m_renderNamedFlowFragment : 0; }
    void setRenderNamedFlowFragment(RenderNamedFlowFragment*);

protected:
    LayoutUnit maxPositiveMarginBefore() const { return m_rareData ? m_rareData->m_margins.positiveMarginBefore() : RenderBlockFlowRareData::positiveMarginBeforeDefault(this); }
    LayoutUnit maxNegativeMarginBefore() const { return m_rareData ? m_rareData->m_margins.negativeMarginBefore() : RenderBlockFlowRareData::negativeMarginBeforeDefault(this); }
    LayoutUnit maxPositiveMarginAfter() const { return m_rareData ? m_rareData->m_margins.positiveMarginAfter() : RenderBlockFlowRareData::positiveMarginAfterDefault(this); }
    LayoutUnit maxNegativeMarginAfter() const { return m_rareData ? m_rareData->m_margins.negativeMarginAfter() : RenderBlockFlowRareData::negativeMarginAfterDefault(this); }

    void setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg);
    void setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg);

    void setMustDiscardMarginBefore(bool = true);
    void setMustDiscardMarginAfter(bool = true);

    bool mustDiscardMarginBefore() const;
    bool mustDiscardMarginAfter() const;

    bool mustDiscardMarginBeforeForChild(const RenderBox*) const;
    bool mustDiscardMarginAfterForChild(const RenderBox*) const;

    bool mustSeparateMarginBeforeForChild(const RenderBox*) const;
    bool mustSeparateMarginAfterForChild(const RenderBox*) const;

    void initMaxMarginValues()
    {
        if (m_rareData) {
            m_rareData->m_margins = MarginValues(RenderBlockFlowRareData::positiveMarginBeforeDefault(this) , RenderBlockFlowRareData::negativeMarginBeforeDefault(this),
                RenderBlockFlowRareData::positiveMarginAfterDefault(this), RenderBlockFlowRareData::negativeMarginAfterDefault(this));

            m_rareData->m_discardMarginBefore = false;
            m_rareData->m_discardMarginAfter = false;
        }
    }

    virtual ETextAlign textAlignmentForLine(bool endsWithSoftBreak) const;
private:
    virtual LayoutUnit collapsedMarginBefore() const OVERRIDE FINAL { return maxPositiveMarginBefore() - maxNegativeMarginBefore(); }
    virtual LayoutUnit collapsedMarginAfter() const OVERRIDE FINAL { return maxPositiveMarginAfter() - maxNegativeMarginAfter(); }

    LayoutUnit collapseMargins(RenderBox* child, MarginInfo&);
    LayoutUnit clearFloatsIfNeeded(RenderBox* child, MarginInfo&, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos);
    LayoutUnit estimateLogicalTopPosition(RenderBox* child, const MarginInfo&, LayoutUnit& estimateWithoutPagination);
    void marginBeforeEstimateForChild(RenderBox*, LayoutUnit&, LayoutUnit&, bool&) const;
    void handleAfterSideOfBlock(LayoutUnit top, LayoutUnit bottom, MarginInfo&);
    void setCollapsedBottomMargin(const MarginInfo&);

    LayoutUnit applyBeforeBreak(RenderBox* child, LayoutUnit logicalOffset); // If the child has a before break, then return a new yPos that shifts to the top of the next page/column.
    LayoutUnit applyAfterBreak(RenderBox* child, LayoutUnit logicalOffset, MarginInfo&); // If the child has an after break, then return a new offset that shifts to the top of the next page/column.

    LayoutUnit adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox* child, bool atBeforeSideOfBlock);

    // Used to store state between styleWillChange and styleDidChange
    static bool s_canPropagateFloatIntoSibling;

    virtual bool canHaveChildren() const OVERRIDE;
    virtual bool canHaveGeneratedChildren() const OVERRIDE;

    void createRenderNamedFlowFragmentIfNeeded();

    RenderBlockFlowRareData& ensureRareData();

    LayoutUnit m_repaintLogicalTop;
    LayoutUnit m_repaintLogicalBottom;

protected:
    OwnPtr<RenderBlockFlowRareData> m_rareData;
    OwnPtr<FloatingObjects> m_floatingObjects;

    friend class BreakingContext; // FIXME: It uses insertFloatingObject and positionNewFloatOnLine, if we move those out from the private scope/add a helper to LineBreaker, we can remove this friend
    friend class MarginInfo;
    friend class LineBreaker;
    friend class LineWidth; // needs to know FloatingObject

// FIXME-BLOCKFLOW: These methods have implementations in
// RenderBlockLineLayout. They should be moved to the proper header once the
// line layout code is separated from RenderBlock and RenderBlockFlow.
// START METHODS DEFINED IN RenderBlockLineLayout
private:
    InlineFlowBox* createLineBoxes(RenderObject*, const LineInfo&, InlineBox* childBox, bool startsNewSegment);
    RootInlineBox* constructLine(BidiRunList<BidiRun>&, const LineInfo&);
    void setMarginsForRubyRun(BidiRun*, RenderRubyRun*, RenderObject*, const LineInfo&);
    void computeInlineDirectionPositionsForLine(RootInlineBox*, const LineInfo&, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&, WordMeasurements&);
    BidiRun* computeInlineDirectionPositionsForSegment(RootInlineBox*, const LineInfo&, ETextAlign, float& logicalLeft,
        float& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache&, WordMeasurements&);
    void computeBlockDirectionPositionsForLine(RootInlineBox*, BidiRun*, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    BidiRun* handleTrailingSpaces(BidiRunList<BidiRun>&, BidiContext*);
    void appendFloatingObjectToLastLine(FloatingObject*);
    // Helper function for layoutInlineChildren()
    RootInlineBox* createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>&, const InlineIterator& end, LineInfo&, VerticalPositionCache&, BidiRun* trailingSpaceRun, WordMeasurements&);
    void layoutRunsAndFloats(LineLayoutState&, bool hasInlineChild);
    const InlineIterator& restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver&,  const InlineIterator&);
    void layoutRunsAndFloatsInRange(LineLayoutState&, InlineBidiResolver&, const InlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus, unsigned consecutiveHyphenatedLines);
    void updateShapeAndSegmentsForCurrentLine(ShapeInsideInfo*&, const LayoutSize&, LineLayoutState&);
    void updateShapeAndSegmentsForCurrentLineInFlowThread(ShapeInsideInfo*&, LineLayoutState&);
    bool adjustLogicalLineTopAndLogicalHeightIfNeeded(ShapeInsideInfo*, LayoutUnit, LineLayoutState&, InlineBidiResolver&, FloatingObject*, InlineIterator&, WordMeasurements&);
    void linkToEndLineIfNeeded(LineLayoutState&);
    static void repaintDirtyFloats(Vector<FloatWithRect>& floats);
    void checkFloatsInCleanLine(RootInlineBox*, Vector<FloatWithRect>&, size_t& floatIndex, bool& encounteredNewFloat, bool& dirtiedByFloat);
    RootInlineBox* determineStartPosition(LineLayoutState&, InlineBidiResolver&);
    void determineEndPosition(LineLayoutState&, RootInlineBox* startBox, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus);
    bool checkPaginationAndFloatsAtEndLine(LineLayoutState&);
    bool matchedEndLine(LineLayoutState&, const InlineBidiResolver&, const InlineIterator& endLineStart, const BidiStatus& endLineStatus);
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    // Positions new floats and also adjust all floats encountered on the line if any of them
    // have to move to the next page/column.
    bool positionNewFloatOnLine(FloatingObject* newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo&, LineWidth&);


// END METHODS DEFINED IN RenderBlockLineLayout

};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderBlockFlow, isRenderBlockFlow());

} // namespace WebCore

#endif // RenderBlockFlow_h
