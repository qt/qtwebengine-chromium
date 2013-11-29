/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/chromium/ScrollbarThemeChromiumOverlay.h"

#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/Scrollbar.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/transforms/TransformationMatrix.h"

#include <algorithm>

using namespace std;

namespace WebCore {

static const int scrollbarWidth = 3;
static const int scrollbarMargin = 4;

int ScrollbarThemeChromiumOverlay::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return scrollbarWidth + scrollbarMargin;
}

bool ScrollbarThemeChromiumOverlay::usesOverlayScrollbars() const
{
    return true;
}

int ScrollbarThemeChromiumOverlay::thumbPosition(ScrollbarThemeClient* scrollbar)
{
    if (!scrollbar->totalSize())
        return 0;

    int trackLen = trackLength(scrollbar);
    float proportion = static_cast<float>(scrollbar->currentPos()) / scrollbar->totalSize();
    return round(proportion * trackLen);
}

int ScrollbarThemeChromiumOverlay::thumbLength(ScrollbarThemeClient* scrollbar)
{
    int trackLen = trackLength(scrollbar);

    if (!scrollbar->totalSize())
        return trackLen;

    float proportion = (float)scrollbar->visibleSize() / scrollbar->totalSize();
    int length = round(proportion * trackLen);
    length = min(max(length, minimumThumbLength(scrollbar)), trackLen);
    return length;
}

bool ScrollbarThemeChromiumOverlay::hasThumb(ScrollbarThemeClient* scrollbar)
{
    return true;
}

IntRect ScrollbarThemeChromiumOverlay::backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool)
{
    return IntRect();
}

IntRect ScrollbarThemeChromiumOverlay::forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool)
{
    return IntRect();
}

IntRect ScrollbarThemeChromiumOverlay::trackRect(ScrollbarThemeClient* scrollbar, bool)
{
    IntRect rect = scrollbar->frameRect();
    if (scrollbar->orientation() == HorizontalScrollbar)
        rect.inflateX(-scrollbarMargin);
    else
        rect.inflateY(-scrollbarMargin);
    return rect;
}

void ScrollbarThemeChromiumOverlay::paintThumb(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
    IntRect thumbRect = rect;
    if (scrollbar->orientation() == HorizontalScrollbar)
        thumbRect.setHeight(thumbRect.height() - scrollbarMargin);
    else
        thumbRect.setWidth(thumbRect.width() - scrollbarMargin);
    context->fillRect(thumbRect, Color(128, 128, 128, 128));
}

} // namespace WebCore
