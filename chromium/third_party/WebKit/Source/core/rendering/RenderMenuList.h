/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef RenderMenuList_h
#define RenderMenuList_h

#include "core/rendering/RenderFlexibleBox.h"
#include "platform/PopupMenu.h"
#include "platform/PopupMenuClient.h"
#include "platform/geometry/LayoutRect.h"

namespace WebCore {

class HTMLSelectElement;
class RenderText;

class RenderMenuList FINAL : public RenderFlexibleBox, private PopupMenuClient {

public:
    RenderMenuList(Element*);
    virtual ~RenderMenuList();

public:
    bool popupIsVisible() const { return m_popupIsVisible; }
    void showPopup();
    void hidePopup();

    void setOptionsChanged(bool changed) { m_optionsChanged = changed; }

    void didSetSelectedIndex(int listIndex);

    String text() const;

private:
    HTMLSelectElement* selectElement() const;

    virtual bool isMenuList() const { return true; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual bool createsAnonymousWrapper() const { return true; }

    virtual void updateFromElement();

    virtual LayoutRect controlClipRect(const LayoutPoint&) const;
    virtual bool hasControlClip() const { return true; }
    virtual bool canHaveGeneratedChildren() const OVERRIDE { return false; }

    virtual const char* renderName() const { return "RenderMenuList"; }

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    // PopupMenuClient methods
    virtual void valueChanged(unsigned listIndex, bool fireOnChange = true) OVERRIDE;
    virtual void selectionChanged(unsigned, bool) OVERRIDE { }
    virtual void selectionCleared() OVERRIDE { }
    virtual String itemText(unsigned listIndex) const OVERRIDE;
    virtual String itemLabel(unsigned listIndex) const OVERRIDE;
    virtual String itemIcon(unsigned listIndex) const OVERRIDE;
    virtual String itemToolTip(unsigned listIndex) const OVERRIDE;
    virtual String itemAccessibilityText(unsigned listIndex) const OVERRIDE;
    virtual bool itemIsEnabled(unsigned listIndex) const OVERRIDE;
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const OVERRIDE;
    virtual PopupMenuStyle menuStyle() const OVERRIDE;
    virtual int clientInsetLeft() const OVERRIDE;
    virtual int clientInsetRight() const OVERRIDE;
    virtual LayoutUnit clientPaddingLeft() const OVERRIDE;
    virtual LayoutUnit clientPaddingRight() const OVERRIDE;
    virtual int listSize() const OVERRIDE;
    virtual int selectedIndex() const OVERRIDE;
    virtual void popupDidHide() OVERRIDE;
    virtual bool itemIsSeparator(unsigned listIndex) const OVERRIDE;
    virtual bool itemIsLabel(unsigned listIndex) const OVERRIDE;
    virtual bool itemIsSelected(unsigned listIndex) const OVERRIDE;
    virtual bool valueShouldChangeOnHotTrack() const OVERRIDE { return true; }
    virtual void setTextFromItem(unsigned listIndex) OVERRIDE;
    virtual void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true) OVERRIDE;
    virtual bool multiple() const OVERRIDE;
    virtual FontSelector* fontSelector() const OVERRIDE;
    virtual HostWindow* hostWindow() const OVERRIDE;
    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize) OVERRIDE;

    virtual bool hasLineIfEmpty() const { return true; }

    // Flexbox defines baselines differently than regular blocks.
    // For backwards compatibility, menulists need to do the regular block behavior.
    virtual int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const OVERRIDE
    {
        return RenderBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    virtual int firstLineBoxBaseline() const OVERRIDE { return RenderBlock::firstLineBoxBaseline(); }
    virtual int inlineBlockBaseline(LineDirectionMode direction) const OVERRIDE { return RenderBlock::inlineBlockBaseline(direction); }

    void getItemBackgroundColor(unsigned listIndex, Color&, bool& itemHasCustomBackgroundColor) const;

    void createInnerBlock();
    void adjustInnerStyle();
    void setText(const String&);
    void setTextFromOption(int optionIndex);
    void updateOptionsWidth();

    void didUpdateActiveOption(int optionIndex);

    RenderText* m_buttonText;
    RenderBlock* m_innerBlock;

    bool m_optionsChanged;
    int m_optionsWidth;

    int m_lastActiveIndex;

    RefPtr<RenderStyle> m_optionStyle;

    RefPtr<PopupMenu> m_popup;
    bool m_popupIsVisible;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderMenuList, isMenuList());

}

#endif
