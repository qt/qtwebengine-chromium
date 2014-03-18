/*
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
 */

#ifndef RenderSVGBlock_h
#define RenderSVGBlock_h

#include "core/rendering/RenderBlockFlow.h"

namespace WebCore {

class SVGElement;

class RenderSVGBlock : public RenderBlockFlow {
public:
    explicit RenderSVGBlock(SVGElement*);

    virtual LayoutRect visualOverflowRect() const OVERRIDE FINAL;

protected:
    virtual void willBeDestroyed() OVERRIDE;

private:
    virtual void updateFromStyle() OVERRIDE FINAL;

    virtual bool isRenderSVGBlock() const OVERRIDE FINAL { return true; };

    virtual bool supportsPartialLayout() const OVERRIDE { return false; }

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const OVERRIDE FINAL;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE FINAL;
};

}
#endif
