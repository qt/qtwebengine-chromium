/*
 * Copyright (C) 2005 Apple Computer
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

#ifndef RenderButton_h
#define RenderButton_h

#include "HTMLNames.h"
#include "core/rendering/RenderFlexibleBox.h"

namespace WebCore {

class RenderTextFragment;

// RenderButtons are just like normal flexboxes except that they will generate an anonymous block child.
// For inputs, they will also generate an anonymous RenderText and keep its style and content up
// to date as the button changes.
class RenderButton FINAL : public RenderFlexibleBox {
public:
    explicit RenderButton(Element*);
    virtual ~RenderButton();

    virtual const char* renderName() const { return "RenderButton"; }
    virtual bool isRenderButton() const { return true; }

    virtual bool canBeSelectionLeaf() const OVERRIDE { return node() && node()->rendererIsEditable(); }

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) { }
    virtual bool createsAnonymousWrapper() const { return true; }

    void setupInnerStyle(RenderStyle*);

    // <button> should allow whitespace even though RenderFlexibleBox doesn't.
    virtual bool canHaveWhitespaceChildren() const OVERRIDE { return true; }

    virtual bool canHaveGeneratedChildren() const OVERRIDE;
    virtual bool hasControlClip() const { return true; }
    virtual LayoutRect controlClipRect(const LayoutPoint&) const;

    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode) const OVERRIDE;

private:
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual bool hasLineIfEmpty() const { return node() && node()->hasTagName(HTMLNames::inputTag); }

    RenderBlock* m_inner;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderButton, isRenderButton());

} // namespace WebCore

#endif // RenderButton_h
