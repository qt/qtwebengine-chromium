/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderPart_h
#define RenderPart_h

#include "core/rendering/RenderWidget.h"

namespace WebCore {

// Renderer for frames via RenderFrame and RenderIFrame, and plug-ins via RenderEmbeddedObject.
class RenderPart : public RenderWidget {
public:
    explicit RenderPart(Element*);
    virtual ~RenderPart();

    virtual void setWidget(PassRefPtr<Widget>) OVERRIDE FINAL;
    virtual void viewCleared();

    bool requiresAcceleratedCompositing() const;

    virtual bool needsPreferredWidthsRecalculation() const OVERRIDE FINAL;
    virtual RenderBox* embeddedContentBox() const OVERRIDE FINAL;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

protected:
    virtual bool requiresLayer() const;

private:
    virtual bool isRenderPart() const OVERRIDE FINAL { return true; }
    virtual const char* renderName() const { return "RenderPart"; }
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderPart, isRenderPart());

}

#endif
