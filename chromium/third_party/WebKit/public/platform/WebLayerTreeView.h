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

#ifndef WebLayerTreeView_h
#define WebLayerTreeView_h

#include "WebColor.h"
#include "WebCommon.h"
#include "WebFloatPoint.h"
#include "WebNonCopyable.h"
#include "WebPrivateOwnPtr.h"
#include "WebSize.h"

class SkBitmap;

namespace blink {
class WebGraphicsContext3D;
class WebLayer;
struct WebPoint;
struct WebRect;
struct WebRenderingStats;

class WebLayerTreeView {
public:
    virtual ~WebLayerTreeView() { }

    // Initialization and lifecycle --------------------------------------

    // Indicates that the compositing surface used by this WebLayerTreeView is ready to use.
    // A WebLayerTreeView may request a context from its client before the surface is ready,
    // but it won't attempt to use it.
    virtual void setSurfaceReady() = 0;

    // Sets the root of the tree. The root is set by way of the constructor.
    virtual void setRootLayer(const WebLayer&) = 0;
    virtual void clearRootLayer() = 0;


    // View properties ---------------------------------------------------

    virtual void setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize) = 0;
    // Gives the viewport size in layer space.
    virtual WebSize layoutViewportSize() const = 0;
    // Gives the viewport size in physical device pixels (may be different
    // from the above if there exists page scale, device scale or fixed layout
    // mode).
    virtual WebSize deviceViewportSize() const = 0;

    virtual void setDeviceScaleFactor(float) = 0;
    virtual float deviceScaleFactor() const = 0;

    // Sets the background color for the viewport.
    virtual void setBackgroundColor(WebColor) = 0;

    // Sets the background transparency for the viewport. The default is 'false'.
    virtual void setHasTransparentBackground(bool) = 0;

    // Sets the overhang gutter bitmap.
    virtual void setOverhangBitmap(const SkBitmap&) { }

    // Sets whether this view is visible. In threaded mode, a view that is not visible will not
    // composite or trigger updateAnimations() or layout() calls until it becomes visible.
    virtual void setVisible(bool) = 0;

    // Sets the current page scale factor and minimum / maximum limits. Both limits are initially 1 (no page scale allowed).
    virtual void setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum) = 0;

    // Starts an animation of the page scale to a target scale factor and scroll offset.
    // If useAnchor is true, destination is a point on the screen that will remain fixed for the duration of the animation.
    // If useAnchor is false, destination is the final top-left scroll position.
    virtual void startPageScaleAnimation(const WebPoint& destination, bool useAnchor, float newPageScale, double durationSec) = 0;


    // Flow control and scheduling ---------------------------------------

    // Indicates that an animation needs to be updated.
    virtual void setNeedsAnimate() = 0;

    // Indicates whether a commit is pending.
    virtual bool commitRequested() const = 0;

    // Relays the end of a fling animation.
    virtual void didStopFlinging() { }

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false. Pixel format is 32bit (RGBA), and the provided buffer must be
    // large enough contain viewportSize().width() * viewportSize().height()
    // pixels. The WebLayerTreeView does not assume ownership of the buffer.
    // The buffer is not modified if the false is returned.
    virtual bool compositeAndReadback(void *pixels, const WebRect&) = 0;

    // Blocks until the most recently composited frame has finished rendering on the GPU.
    // This can have a significant performance impact and should be used with care.
    virtual void finishAllRendering() = 0;

    // Prevents updates to layer tree from becoming visible.
    virtual void setDeferCommits(bool deferCommits) { }

    // Take responsiblity for this layer's animations, even if this layer hasn't yet
    // been added to the tree.
    virtual void registerForAnimations(WebLayer* layer) { }

    // Identify key layers to the compositor when using the pinch virtual viewport.
    virtual void registerViewportLayers(
        const WebLayer* pageScaleLayerLayer,
        const WebLayer* innerViewportScrollLayer,
        const WebLayer* outerViewportScrollLayer) { }
    virtual void clearViewportLayers() { }

    // Debugging / dangerous ---------------------------------------------

    // Toggles the FPS counter in the HUD layer
    virtual void setShowFPSCounter(bool) { }

    // Toggles the paint rects in the HUD layer
    virtual void setShowPaintRects(bool) { }

    // Toggles the debug borders on layers
    virtual void setShowDebugBorders(bool) { }

    // Toggles continuous painting
    virtual void setContinuousPaintingEnabled(bool) { }

    // Toggles scroll bottleneck rects on the HUD layer
    virtual void setShowScrollBottleneckRects(bool) { }
};

} // namespace blink

#endif // WebLayerTreeView_h
